/*=========================================================================

  Program:   ParaView
  Module:    vtkPVProcessModule.cxx
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

Copyright (c) 2000-2001 Kitware Inc. 469 Clifton Corporate Parkway,
Clifton Park, NY, 12065, USA.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 * Neither the name of Kitware nor the names of any contributors may be used
   to endorse or promote products derived from this software without specific
   prior written permission.

 * Modified source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/
#include "vtkPVProcessModule.h"

#include "vtkCharArray.h"
#include "vtkDataSet.h"
#include "vtkDoubleArray.h"
#include "vtkDummyController.h"
#include "vtkFloatArray.h"
#include "vtkKWLoadSaveDialog.h"
#include "vtkLongArray.h"
#include "vtkMapper.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkPVApplication.h"
#include "vtkPVConfig.h"
#include "vtkPVInformation.h"
#include "vtkPVPart.h"
#include "vtkPVPartDisplay.h"
#include "vtkPVPartDisplay.h"
#include "vtkPVWindow.h"
#include "vtkPolyData.h"
#include "vtkShortArray.h"
#include "vtkSource.h"
#include "vtkString.h"
#include "vtkStringList.h"
#include "vtkTclUtil.h"
#include "vtkToolkits.h"
#include "vtkUnsignedIntArray.h"
#include "vtkUnsignedLongArray.h"
#include "vtkUnsignedShortArray.h"
#include "vtkClientServerStream.h"
#include "vtkClientServerInterpreter.h"

#include <vtkstd/string>

int vtkStringListCommand(ClientData cd, Tcl_Interp *interp,
                         int argc, char *argv[]);

struct vtkPVArgs
{
  int argc;
  char **argv;
  int* RetVal;
};


//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkPVProcessModule);
vtkCxxRevisionMacro(vtkPVProcessModule, "1.24.2.14");

int vtkPVProcessModuleCommand(ClientData cd, Tcl_Interp *interp,
                             int argc, char *argv[]);


//----------------------------------------------------------------------------
vtkPVProcessModule::vtkPVProcessModule()
{
  this->UniqueID.ID = 3;
  this->Controller = NULL;
  this->TemporaryInformation = NULL;
  this->RootResult = NULL;
  this->ClientServerStream = 0;
  this->ClientInterpreter = 0;
}

//----------------------------------------------------------------------------
vtkPVProcessModule::~vtkPVProcessModule()
{
  if (this->Controller)
    {
    this->Controller->Delete();
    this->Controller = NULL;
    }
  this->SetRootResult(NULL);
  if(this->ClientInterpreter)
    {
    this->ClientInterpreter->Delete();
    }
  delete this->ClientServerStream;
}

//----------------------------------------------------------------------------
int vtkPVProcessModule::Start(int argc, char **argv)
{
  if (this->Controller == NULL)
    {
    this->Controller = vtkDummyController::New();
    vtkMultiProcessController::SetGlobalController(this->Controller);
    }

  vtkPVApplication *app = this->GetPVApplication();
  // For SGI pipes option.
  app->SetNumberOfPipes(1);

#ifdef PV_HAVE_TRAPS_FOR_SIGNALS
  app->SetupTrapsForSignals(myId);
#endif // PV_HAVE_TRAPS_FOR_SIGNALS
  app->SetProcessModule(this);
  app->Script("wm withdraw .");

  this->ClientServerStream = new vtkClientServerStream;
  this->ClientInterpreter = vtkClientServerInterpreter::New();
  this->ClientInterpreter->SetLogFile("pvClient.out");
  vtkPVProcessModule::InitializeInterpreter(this->ClientInterpreter);
  this->GetStream()
    << vtkClientServerStream::Assign
    << this->GetApplicationID() << app
    << vtkClientServerStream::End
    << vtkClientServerStream::Assign
    << this->GetProcessModuleID() << this
    << vtkClientServerStream::End;
  this->ClientInterpreter->ProcessStream(this->GetStream());
  this->GetStream().Reset();

  app->Start(argc,argv);

  this->GetStream()
    << vtkClientServerStream::Delete
    << this->GetApplicationID()
    << vtkClientServerStream::End
    << vtkClientServerStream::Delete
    << this->GetProcessModuleID()
    << vtkClientServerStream::End;
  this->ClientInterpreter->ProcessStream(this->GetStream());
  this->GetStream().Reset();

  return app->GetExitStatus();
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::Exit()
{
}

//----------------------------------------------------------------------------
vtkPVApplication* vtkPVProcessModule::GetPVApplication()
{
  return vtkPVApplication::SafeDownCast(this->Application);
}

//----------------------------------------------------------------------------
int vtkPVProcessModule::GetPartitionId()
{
  return 0;
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::BroadcastScript(const char* format, ...)
{
  char event[1600];
  char* buffer = event;

  if (this->Application == NULL)
    {
    vtkErrorMacro("Missing application object.");
    return;
    }

  va_list ap;
  va_start(ap, format);
  int length = this->Application->EstimateFormatLength(format, ap);
  va_end(ap);

  if(length > 1599)
    {
    buffer = new char[length+1];
    }

  va_list var_args;
  va_start(var_args, format);
  vsprintf(buffer, format, var_args);
  va_end(var_args);

  this->BroadcastSimpleScript(buffer);

  if(buffer != event)
    {
    delete [] buffer;
    }
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::ServerScript(const char* format, ...)
{
  char event[1600];
  char* buffer = event;

  if (this->Application == NULL)
    {
    vtkErrorMacro("Missing application object.");
    return;
    }

  va_list ap;
  va_start(ap, format);
  int length = this->Application->EstimateFormatLength(format, ap);
  va_end(ap);

  if(length > 1599)
    {
    buffer = new char[length+1];
    }

  va_list var_args;
  va_start(var_args, format);
  vsprintf(buffer, format, var_args);
  va_end(var_args);

  this->ServerSimpleScript(buffer);

  if(buffer != event)
    {
    delete [] buffer;
    }
}


//----------------------------------------------------------------------------
void vtkPVProcessModule::BroadcastSimpleScript(const char *str)
{
  this->Application->SimpleScript(str);
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::ServerSimpleScript(const char *str)
{
  // Do this so that only the client server process module
  // needs to implement this method.
  this->BroadcastSimpleScript(str);
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::RemoteScript(int id, const char* format, ...)
{
  char event[1600];
  char* buffer = event;

  if (this->Application == NULL)
    {
    vtkErrorMacro("Missing application object.");
    return;
    }

  va_list ap;
  va_start(ap, format);
  int length = this->EstimateFormatLength(format, ap);
  va_end(ap);

  if(length > 1599)
    {
    buffer = new char[length+1];
    }

  va_list var_args;
  va_start(var_args, format);
  vsprintf(buffer, format, var_args);
  va_end(var_args);

  this->RemoteSimpleScript(id, buffer);

  if(buffer != event)
    {
    delete [] buffer;
    }
}


//----------------------------------------------------------------------------
void vtkPVProcessModule::RemoteSimpleScript(int remoteId, const char *str)
{
  // send string to evaluate.
  if (vtkString::Length(str) <= 0)
    {
    return;
    }

  if (remoteId == 0)
    {
    this->Application->SimpleScript(str);
    }
  }


//----------------------------------------------------------------------------
int vtkPVProcessModule::GetNumberOfPartitions()
{
  return 1;
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::GatherInformation(vtkPVInformation* info,
                                           vtkClientServerID id)
{
  // Just a simple way of passing the information object to the next
  // method.
  this->TemporaryInformation = info;
  this->GetStream()
    << vtkClientServerStream::Invoke
    << this->GetApplicationID() << "GetProcessModule"
    << vtkClientServerStream::End
    << vtkClientServerStream::Invoke
    << vtkClientServerStream::LastResult
    << "GatherInformationInternal" << info->GetClassName() << id
    << vtkClientServerStream::End;
  this->SendStreamToServer();
  this->TemporaryInformation = NULL;
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::GatherInformationInternal(const char*,
                                                   vtkObject* object)
{
  // This class is used only for one processes.
  if (this->TemporaryInformation == NULL)
    {
    vtkErrorMacro("Information argument not set.");
    return;
    }
  if (object == NULL)
    {
    vtkErrorMacro("Object tcl name must be wrong.");
    return;
    }

  this->TemporaryInformation->CopyFromObject(object);
 }


//----------------------------------------------------------------------------
void vtkPVProcessModule::RootScript(const char* format, ...)
{
  char event[1600];
  char* buffer = event;

  va_list ap;
  va_start(ap, format);
  int length = this->EstimateFormatLength(format, ap);
  va_end(ap);

  if(length > 1599)
    {
    buffer = new char[length+1];
    }

  va_list var_args;
  va_start(var_args, format);
  vsprintf(buffer, format, var_args);
  va_end(var_args);

  this->RootSimpleScript(buffer);

  if(buffer != event)
    {
    delete [] buffer;
    }
}



//----------------------------------------------------------------------------
void vtkPVProcessModule::RootSimpleScript(const char *script)
{
  // Default implementation just executes locally.
  this->Script(script);
  this->SetRootResult(this->Application->GetMainInterp()->result);
}

//----------------------------------------------------------------------------
const char* vtkPVProcessModule::GetRootResult()
{
  return this->RootResult;
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::SetApplication(vtkKWApplication* arg)
{
  this->Superclass::SetApplication(arg);
}

//----------------------------------------------------------------------------
int vtkPVProcessModule::GetDirectoryListing(const char* dir,
                                            vtkStringList* dirs,
                                            vtkStringList* files,
                                            int save)
{
  // Get the listing from the server.
  vtkClientServerID lid = this->NewStreamObject("vtkPVServerFileListing");
  this->GetStream() << vtkClientServerStream::Invoke
                    << lid << "GetFileListing" << dir << save
                    << vtkClientServerStream::End;
  this->SendStreamToServer();
  vtkClientServerStream result;
  if(!this->GetLastServerResult().GetArgument(0, 0, &result))
    {
    vtkErrorMacro("Error getting file list result from server.");
    this->DeleteStreamObject(lid);
    this->SendStreamToServer();
    return 0;
    }
  this->DeleteStreamObject(lid);
  this->SendStreamToServer();

  // Parse the listing.
  dirs->RemoveAllItems();
  files->RemoveAllItems();
  if(result.GetNumberOfMessages() == 2)
    {
    int i;
    // The first message lists directories.
    for(i=0; i < result.GetNumberOfArguments(0); ++i)
      {
      const char* d;
      if(result.GetArgument(0, i, &d))
        {
        dirs->AddString(d);
        }
      else
        {
        vtkErrorMacro("Error getting directory name from listing.");
        }
      }

    // The second message lists files.
    for(i=0; i < result.GetNumberOfArguments(1); ++i)
      {
      const char* f;
      if(result.GetArgument(1, i, &f))
        {
        files->AddString(f);
        }
      else
        {
        vtkErrorMacro("Error getting file name from listing.");
        }
      }
    return 1;
    }
  else
    {
    return 0;
    }
}

//----------------------------------------------------------------------------
vtkKWLoadSaveDialog* vtkPVProcessModule::NewLoadSaveDialog()
{
  vtkKWLoadSaveDialog* dialog = vtkKWLoadSaveDialog::New();
  return dialog;
}

//----------------------------------------------------------------------------
int vtkPVProcessModule::ReceiveRootPolyData(const char* tclName,
                                            vtkPolyData* out)
{
  // Make sure we have a named Tcl VTK object.
  if(!tclName || !tclName[0])
    {
    return 0;
    }

  // We are the server.  Just return the object from the local
  // interpreter.
  vtkstd::string name = this->GetPVApplication()->EvaluateString(tclName);
  vtkObject* obj = this->GetPVApplication()->TclToVTKObject(name.c_str());
  vtkPolyData* dobj = vtkPolyData::SafeDownCast(obj);
  if(dobj)
    {
    out->DeepCopy(dobj);
    }
  else
    {
    return 0;
    }
  return 1;

}

//----------------------------------------------------------------------------
void vtkPVProcessModule::SendStreamToClient()
{
  this->SendStreamToServer();
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::SendStreamToServer()
{
  this->ClientInterpreter->ProcessStream(*this->ClientServerStream);
  this->ClientServerStream->Reset();
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::SendStreamToClientAndServer()
{
  this->SendStreamToServer();
}

//----------------------------------------------------------------------------
vtkClientServerID vtkPVProcessModule::NewStreamObject(const char* type)
{
  vtkClientServerStream& stream = this->GetStream();
  vtkClientServerID id = this->GetUniqueID();
  stream << vtkClientServerStream::New << type
         << id <<  vtkClientServerStream::End;
  return id;
}

vtkObjectBase* vtkPVProcessModule::GetObjectFromID(vtkClientServerID id)
{
  return this->ClientInterpreter->GetObjectFromID(id);
}


//----------------------------------------------------------------------------
void vtkPVProcessModule::DeleteStreamObject(vtkClientServerID id)
{
  vtkClientServerStream& stream = this->GetStream();
  stream << vtkClientServerStream::Delete << id
         <<  vtkClientServerStream::End;
}

//----------------------------------------------------------------------------
const vtkClientServerStream& vtkPVProcessModule::GetLastServerResult()
{
  return this->ClientInterpreter->GetLastResult();
}

//----------------------------------------------------------------------------
const vtkClientServerStream& vtkPVProcessModule::GetLastClientResult()
{
  return this->GetLastServerResult();
}

//----------------------------------------------------------------------------
vtkClientServerInterpreter* vtkPVProcessModule::GetLocalInterpreter()
{
  return this->ClientInterpreter;
}

//----------------------------------------------------------------------------
vtkClientServerID vtkPVProcessModule::GetUniqueID()
{
  this->UniqueID.ID++;
  return this->UniqueID;
}

//----------------------------------------------------------------------------
vtkClientServerID vtkPVProcessModule::GetApplicationID()
{
  vtkClientServerID id = {1};
  return id;
}

//----------------------------------------------------------------------------
vtkClientServerID vtkPVProcessModule::GetProcessModuleID()
{
  vtkClientServerID id = {2};
  return id;
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
  os << indent << "Controller: " << this->Controller << endl;
  if (this->RootResult)
    {
    os << indent << "RootResult: " << this->RootResult << endl;
    }
}

// ClientServer wrapper initialization functions.
extern void vtkCommonCS_Initialize(vtkClientServerInterpreter*);
extern void vtkFilteringCS_Initialize(vtkClientServerInterpreter*);
extern void vtkImagingCS_Initialize(vtkClientServerInterpreter*);
extern void vtkGraphicsCS_Initialize(vtkClientServerInterpreter*);
extern void vtkIOCS_Initialize(vtkClientServerInterpreter*);
extern void vtkRenderingCS_Initialize(vtkClientServerInterpreter*);
extern void vtkHybridCS_Initialize(vtkClientServerInterpreter*);
extern void vtkParallelCS_Initialize(vtkClientServerInterpreter*);
#ifdef VTK_USE_PATENTED
extern void vtkPatentedCS_Initialize(vtkClientServerInterpreter*);
#endif
extern void vtkPVFiltersCS_Initialize(vtkClientServerInterpreter*);
extern void vtkParaViewServerCS_Initialize(vtkClientServerInterpreter*);

//----------------------------------------------------------------------------
void
vtkPVProcessModule::InitializeInterpreter(vtkClientServerInterpreter* interp)
{
  vtkCommonCS_Initialize(interp);
  vtkFilteringCS_Initialize(interp);
  vtkImagingCS_Initialize(interp);
  vtkGraphicsCS_Initialize(interp);
  vtkIOCS_Initialize(interp);
  vtkRenderingCS_Initialize(interp);
  vtkHybridCS_Initialize(interp);
  vtkParallelCS_Initialize(interp);
#ifdef VTK_USE_PATENTED
  vtkPatentedCS_Initialize(interp);
#endif
  vtkPVFiltersCS_Initialize(interp);
  vtkParaViewServerCS_Initialize(interp);
}
