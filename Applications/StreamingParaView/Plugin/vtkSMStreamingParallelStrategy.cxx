/*=========================================================================

  Program:   ParaView
  Module:    vtkSMStreamingParallelStrategy.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkSMStreamingParallelStrategy.h"
#include "vtkStreamingOptions.h"

#include "vtkClientServerStream.h"
#include "vtkDataObjectTypes.h"
#include "vtkInformation.h"
#include "vtkMPIMoveData.h"
#include "vtkObjectFactory.h"
#include "vtkParallelPieceCacheFilter.h"
#include "vtkProcessModule.h"
#include "vtkPVInformation.h"
#include "vtkSMDoubleVectorProperty.h"
#include "vtkSMIceTMultiDisplayRenderViewProxy.h"
#include "vtkSMInputProperty.h"
#include "vtkSMIntVectorProperty.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxyProperty.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMStringVectorProperty.h"

vtkStandardNewMacro(vtkSMStreamingParallelStrategy);

#define DEBUGPRINT_STRATEGY(arg)\
  arg;

//----------------------------------------------------------------------------
vtkSMStreamingParallelStrategy::vtkSMStreamingParallelStrategy()
{
  this->EnableCaching = false; //don't try to use animation caching
}

//----------------------------------------------------------------------------
vtkSMStreamingParallelStrategy::~vtkSMStreamingParallelStrategy()
{
}

//----------------------------------------------------------------------------
void vtkSMStreamingParallelStrategy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkSMStreamingParallelStrategy::BeginCreateVTKObjects()
{
  this->Superclass::BeginCreateVTKObjects();
  //this->SetEnableLOD(false); //don't try to have LOD

  //Get hold of the caching filter proxies
  this->PieceCache =
    vtkSMSourceProxy::SafeDownCast(this->GetSubProxy("PieceCache"));
  this->PieceCache->SetServers(vtkProcessModule::DATA_SERVER);

  this->ParallelPieceCache =
    vtkSMSourceProxy::SafeDownCast(this->GetSubProxy("ParallelPieceCache"));
  this->ParallelPieceCache->SetServers(vtkProcessModule::CLIENT);
}

//----------------------------------------------------------------------------
void vtkSMStreamingParallelStrategy::CreatePipeline(vtkSMSourceProxy* input, int outputport)
{
  vtkSMIntVectorProperty *ivp;
  vtkClientServerStream stream;
  vtkProcessModule *pm = vtkProcessModule::GetProcessModule();

  //tell collect filters where they are and what datatype they should to produce
  int data_type_id;
  //on server
  //parent class does this already
  data_type_id =
    vtkDataObjectTypes::GetTypeIdFromClassName("vtkPolyData");
  stream << vtkClientServerStream::Invoke
         << this->Collect->GetID()
         << "SetMoveModeToCollect"
         << vtkClientServerStream::End;
  stream << vtkClientServerStream::Invoke
         << this->Collect->GetID()
         << "SetOutputDataType"
         << data_type_id
         << vtkClientServerStream::End;
  stream << vtkClientServerStream::Invoke
         << this->Collect->GetID()
         << "SetServerToDataServer"
         << vtkClientServerStream::End;
  pm->SendStream(this->ConnectionID, vtkProcessModule::SERVERS, stream);
  //on client
  //data_type_id =
  //  vtkDataObjectTypes::GetTypeIdFromClassName("vtkMultiPieceDataSet");
  stream << vtkClientServerStream::Invoke
         << this->Collect->GetID()
         << "SetMoveModeToCollect"
         << vtkClientServerStream::End;
  stream << vtkClientServerStream::Invoke
         << this->Collect->GetID()
         << "SetOutputDataType"
         << data_type_id
         << vtkClientServerStream::End;
  stream << vtkClientServerStream::Invoke
         << this->Collect->GetID()
         << "SetServerToClient"
         << vtkClientServerStream::End;
  pm->SendStream(this->ConnectionID, vtkProcessModule::CLIENT, stream);

  //set up the pipeline
  this->Connect(input, this->PieceCache, "Input", outputport);
  this->Superclass::CreatePipeline(this->PieceCache, 0);
  //we now have: input->PC->US->Collect->PostCollectUS

  //On the client, connect the parallel piece cache
  //first break the existing connection between collect un PCUS
  vtkSMProxyProperty* pp = vtkSMProxyProperty::SafeDownCast(
    this->PostCollectUpdateSuppressor->GetProperty("Input"));
  vtkSMInputProperty* ip = vtkSMInputProperty::SafeDownCast(pp);
  if (!ip)
    {
    abort();
    }
  ip->RemoveAllProxies();
  //reconnect the server as it was
  stream  << vtkClientServerStream::Invoke
          << this->Collect->GetID()
          << "GetOutputPort" << 0
          << vtkClientServerStream::End;
  stream  << vtkClientServerStream::Invoke
          << this->PostCollectUpdateSuppressor->GetID()
          << "SetInputConnection" << 0
          << vtkClientServerStream::LastResult
          << vtkClientServerStream::End;
  pm->SendStream(this->ConnectionID, vtkProcessModule::SERVERS, stream);
  stream.Reset();
  //now connect up the client with the PPCF inserted between collect and PCUS
  stream << vtkClientServerStream::Invoke
         << this->Collect->GetID()
         << "GetOutputPort" << 0
         << vtkClientServerStream::End;
  stream << vtkClientServerStream::Invoke
         << this->ParallelPieceCache->GetID()
         << "SetInputConnection" << 0
         << vtkClientServerStream::LastResult
         << vtkClientServerStream::End;
  stream << vtkClientServerStream::Invoke
         << this->ParallelPieceCache->GetID()
          << "GetOutputPort" << 0
          << vtkClientServerStream::End;
  stream  << vtkClientServerStream::Invoke
          << this->PostCollectUpdateSuppressor->GetID()
          << "SetInputConnection" << 0
          << vtkClientServerStream::LastResult
          << vtkClientServerStream::End;
  pm->SendStream(this->ConnectionID, vtkProcessModule::CLIENT, stream);
  stream.Reset();

  //setup the filters along the pipeline

  //turn off animation cache, it interferes with streaming
  vtkSMSourceProxy *cacher =
    vtkSMSourceProxy::SafeDownCast(this->GetSubProxy("CacheKeeper"));
  ivp = vtkSMIntVectorProperty::SafeDownCast(
    cacher->GetProperty("CachingEnabled"));
  ivp->SetElement(0, 0);

  // Do not let the server's US suppress pipeline updates so that priorities
  // can be computed downstream
  ivp = vtkSMIntVectorProperty::SafeDownCast(
    this->UpdateSuppressor->GetProperty("Enabled"));
  ivp->SetElement(0, 0);
  this->UpdateSuppressor->UpdateVTKObjects();

  //give US access to the PCF that feeds into it
  stream  << vtkClientServerStream::Invoke
          << this->UpdateSuppressor->GetID()
          << "SetPieceCacheFilter"
          << this->PieceCache->GetID()
          << vtkClientServerStream::End;
  pm->SendStream(this->ConnectionID, vtkProcessModule::DATA_SERVER, stream);
  stream.Reset();

  //give all PCUS's direct access to the collect filter
  stream << vtkClientServerStream::Invoke
         << this->PostCollectUpdateSuppressor->GetID()
         << "SetMPIMoveData"
         << this->Collect->GetID()
         << vtkClientServerStream::End;
  pm->SendStream(this->GetConnectionID(), vtkProcessModule::CLIENT_AND_SERVERS, stream);
  stream.Reset();

  //give the client's PCUS direct access to the PPCF
  stream  << vtkClientServerStream::Invoke
          << this->PostCollectUpdateSuppressor->GetID()
          << "SetParallelPieceCacheFilter"
          << this->ParallelPieceCache->GetID()
          << vtkClientServerStream::End;
  pm->SendStream(this->ConnectionID, vtkProcessModule::CLIENT, stream);
  stream.Reset();
}

//----------------------------------------------------------------------------
void vtkSMStreamingParallelStrategy::ClearStreamCache()
{
  vtkSMProperty *cc = this->PieceCache->GetProperty("EmptyCache");
  cc->Modified();
  this->PieceCache->UpdateVTKObjects();
  cc = this->ParallelPieceCache->GetProperty("EmptyCache");
  cc->Modified();
  this->ParallelPieceCache->UpdateVTKObjects();
}

//----------------------------------------------------------------------------
void vtkSMStreamingParallelStrategy::GatherInformation(vtkPVInformation* info)
{
  //gather information in multiple passes so as never to request
  //everything at once.
  //gather information without requesting the whole data
  vtkSMIntVectorProperty* ivp;
  DEBUGPRINT_STRATEGY(
    cerr << "SPS(" << this << ") Gather Info" << endl;
    );

  int cacheLimit = vtkStreamingOptions::GetPieceCacheLimit();
  ivp = vtkSMIntVectorProperty::SafeDownCast(
    this->PieceCache->GetProperty("SetCacheSize"));
  ivp->SetElement(0, cacheLimit);
  this->PieceCache->UpdateVTKObjects();

  //let US know NumberOfPasses for CP
  ivp = vtkSMIntVectorProperty::SafeDownCast(
    this->UpdateSuppressor->GetProperty("SetNumberOfPasses"));
  int nPasses = vtkStreamingOptions::GetStreamedPasses();
  ivp->SetElement(0, nPasses);

  this->UpdateSuppressor->UpdateVTKObjects();
  vtkSMProperty *p = this->UpdateSuppressor->GetProperty
    ("ComputeLocalPipelinePriorities");
  p->Modified();
  this->UpdateSuppressor->UpdateVTKObjects();
  for (int i = 0; i < 1; i++)
    {
    vtkPVInformation *sinfo = vtkPVInformation::SafeDownCast(info->NewInstance());
    ivp = vtkSMIntVectorProperty::SafeDownCast(
      this->UpdateSuppressor->GetProperty("PassNumber"));
    ivp->SetElement(0, i);
    ivp->SetElement(1, nPasses);

    this->UpdateSuppressor->UpdateVTKObjects();
    this->UpdateSuppressor->InvokeCommand("ForceUpdate");

    // For simple strategy information sub-pipline is same as the full pipeline
    // so no data movements are involved.
    vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
    pm->GatherInformation(this->ConnectionID,
                          vtkProcessModule::DATA_SERVER_ROOT,
                          sinfo,
                          this->UpdateSuppressor->GetID());
    info->AddInformation(sinfo);
    sinfo->Delete();
    }
}

//----------------------------------------------------------------------------
void vtkSMStreamingParallelStrategy::InvalidatePipeline()
{
  this->Superclass::InvalidatePipeline();

  vtkSMIntVectorProperty* ivp =
    vtkSMIntVectorProperty::SafeDownCast(
      this->PieceCache->GetProperty("SetCacheSize"));
  int cacheLimit = vtkStreamingOptions::GetPieceCacheLimit();
  ivp->SetElement(0, cacheLimit);
  this->PieceCache->UpdateVTKObjects();

  //let US know NumberOfPasses for CP
  ivp = vtkSMIntVectorProperty::SafeDownCast(
    this->UpdateSuppressor->GetProperty("SetNumberOfPasses"));
  int nPasses = vtkStreamingOptions::GetStreamedPasses();
  ivp->SetElement(0, nPasses);
  this->UpdateSuppressor->UpdateVTKObjects();
  ivp = vtkSMIntVectorProperty::SafeDownCast(
    this->PostCollectUpdateSuppressor->GetProperty("SetNumberOfPasses"));
  ivp->SetElement(0, nPasses);
  this->PostCollectUpdateSuppressor->UpdateVTKObjects();

  //clear all caches and priority lists
  this->ClearStreamCache();
  this->UpdateSuppressor->InvokeCommand("ClearPriorities");
  this->UpdateSuppressor->UpdateVTKObjects();
  this->PostCollectUpdateSuppressor->InvokeCommand("ClearPriorities");
  this->PostCollectUpdateSuppressor->UpdateVTKObjects();

  //compute the pipeline priorities
  this->UpdateSuppressor->InvokeCommand("ComputeLocalPipelinePriorities");

  //deliver the pipeline priorities to the client
  this->UpdateSuppressor->InvokeCommand("Serialize");
  this->PostCollectUpdateSuppressor->UpdateVTKObjects();

  vtkSMStringVectorProperty *svp =
    vtkSMStringVectorProperty::SafeDownCast
    (this->UpdateSuppressor->GetProperty("SerializedLists"));
  this->UpdateSuppressor->UpdatePropertyInformation(svp);
  vtkSMStringVectorProperty *svp2 =
    vtkSMStringVectorProperty::SafeDownCast
    (this->PostCollectUpdateSuppressor->GetProperty("UnSerialize"));
  svp2->SetElement(0, svp->GetElement(0));
  svp2->Modified(); //even if string same as last time, clears above necessitate
  this->PostCollectUpdateSuppressor->UpdateVTKObjects();
}

//----------------------------------------------------------------------------
void vtkSMStreamingParallelStrategy::SetViewState(double *camera, double *frustum)
{
  if (!camera || !frustum)
    {
    return;
    }

  vtkSMDoubleVectorProperty* dvp;
  dvp = vtkSMDoubleVectorProperty::SafeDownCast(
    this->PostCollectUpdateSuppressor->GetProperty("SetCamera"));
  dvp->SetElements(camera);
  dvp = vtkSMDoubleVectorProperty::SafeDownCast(
    this->PostCollectUpdateSuppressor->GetProperty("SetFrustum"));
  dvp->SetElements(frustum);
  this->PostCollectUpdateSuppressor->UpdateVTKObjects();
}

//----------------------------------------------------------------------------
int vtkSMStreamingParallelStrategy::ComputePipelinePriorities()
{
  DEBUGPRINT_STRATEGY(
    cerr << "SPS(" << this << ") ComputePipelinePriorities" << endl;
                      )
  return -1;
}

//----------------------------------------------------------------------------
int vtkSMStreamingParallelStrategy::ComputeViewPriorities()
{
  DEBUGPRINT_STRATEGY(
    cerr << "SPS(" << this << ") ComputeViewPriorities" << endl;
                      )

  vtkProcessModule *pm = vtkProcessModule::GetProcessModule();

  //ask it to compute the priorities
  vtkClientServerStream stream;
  stream  << vtkClientServerStream::Invoke
          << this->PostCollectUpdateSuppressor->GetID()
          << "ComputeGlobalViewPriorities"
          << vtkClientServerStream::End;
  pm->SendStream(this->ConnectionID, vtkProcessModule::CLIENT, stream);
  stream.Reset();

  return -1;
}

//----------------------------------------------------------------------------
int vtkSMStreamingParallelStrategy::ComputeCachePriorities()
{
  DEBUGPRINT_STRATEGY(
    cerr << "SPS(" << this << ") ComputeCachePriorities" << endl;
                      )

  vtkProcessModule *pm = vtkProcessModule::GetProcessModule();

  //ask it to compute the priorities
  vtkClientServerStream stream;
  stream  << vtkClientServerStream::Invoke
          << this->PostCollectUpdateSuppressor->GetID()
          << "ComputeGlobalCachePriorities"
          << vtkClientServerStream::End;
  pm->SendStream(this->ConnectionID, vtkProcessModule::CLIENT, stream);
  stream.Reset();

  //deliver the pipeline priorities back to the server
  stream << vtkClientServerStream::Invoke
         << this->PostCollectUpdateSuppressor->GetID()
         << "SerializeLists"
         << vtkClientServerStream::End;
  pm->SendStream(this->ConnectionID, vtkProcessModule::CLIENT, stream);
  stream.Reset();

  stream << vtkClientServerStream::Invoke
         << this->PostCollectUpdateSuppressor->GetID()
         << "GetSerializedLists"
         << vtkClientServerStream::End;
  pm->SendStream(this->ConnectionID, vtkProcessModule::CLIENT, stream);
  stream.Reset();

  // Get the result
  const vtkClientServerStream& res =
    pm->GetLastResult(this->ConnectionID, vtkProcessModule::GetRootId(vtkProcessModule::CLIENT));
  bool fail = false;
  int numMsgs = res.GetNumberOfMessages();
  if (numMsgs < 1)
    {
    fail = true;
    }
  int numArgs = res.GetNumberOfArguments(0);
  if (numArgs < 1)
    {
    fail = true;
    }
  int argType = res.GetArgumentType(0, 0);
  const char* sres;
  if (argType == vtkClientServerStream::string_value)
    {
    int retVal = res.GetArgument(0, 0, &sres);
    if (!retVal)
      {
      fail = true;
      }
    }
  else
    {
    fail = true;
    }
  if (fail)
    {
    cerr << "Could not get prioritization lists" << endl;
    abort();
    }

  //send it back to the server
  vtkSMStringVectorProperty *svp2;
  svp2 =
    vtkSMStringVectorProperty::SafeDownCast
    (this->UpdateSuppressor->GetProperty("UnSerialize"));
  svp2->SetElement(0, sres);
//  this->UpdateSuppressor->UpdateVTKObjects();
  svp2 =
    vtkSMStringVectorProperty::SafeDownCast
    (this->PostCollectUpdateSuppressor->GetProperty("UnSerialize"));
  svp2->SetElement(0, sres);
  this->PostCollectUpdateSuppressor->UpdateVTKObjects();

  return -1;
}

//------------------------------------------------------------------------------
bool vtkSMStreamingParallelStrategy::HaveLocalCache()
{
  bool ret = false;
  vtkParallelPieceCacheFilter *pcf =
    vtkParallelPieceCacheFilter::SafeDownCast
    (this->ParallelPieceCache->GetClientSideObject());
  if (pcf)
    {
    ret = pcf->HasRequestedPieces();
    cerr << "SPS(" << this << ") I " << (ret?"HAVE":"DO NOT HAVE") << " cache" << endl;
    }

  return ret;
}

//----------------------------------------------------------------------------
void vtkSMStreamingParallelStrategy::GetPieceInfo(
  int *P, int *NP, double *R, double *PRIORITY, bool *HIT, bool *APPEND)
{
  vtkSMDoubleVectorProperty* dp = vtkSMDoubleVectorProperty::SafeDownCast(
    this->PostCollectUpdateSuppressor->GetProperty("GetPieceInfo"));
  //get the result
  this->PostCollectUpdateSuppressor->UpdatePropertyInformation(dp);

  *P = (int)dp->GetElement(0);
  *NP = (int)dp->GetElement(1);
  *R = dp->GetElement(2);
  *PRIORITY = dp->GetElement(3);
  *HIT = dp->GetElement(4)!=0.0;
  *APPEND = dp->GetElement(5)!=0.0;
}

//----------------------------------------------------------------------------
void vtkSMStreamingParallelStrategy::GetStateInfo(
  bool *ALLDONE, bool *WENDDONE)
{
  vtkSMIntVectorProperty* ivp = vtkSMIntVectorProperty::SafeDownCast(
    this->PostCollectUpdateSuppressor->GetProperty("GetStateInfo"));
  //get the result
  this->PostCollectUpdateSuppressor->UpdatePropertyInformation(ivp);
  *ALLDONE = (ivp->GetElement(0)!=0);
  *WENDDONE = (ivp->GetElement(1)!=0);
}

//----------------------------------------------------------------------------
void vtkSMStreamingParallelStrategy::PrepareFirstPass()
{
  DEBUGPRINT_STRATEGY(
    cerr << "SPS("<<this<<")::PrepareFirstPass" << endl;
    );

  vtkProcessModule *pm = vtkProcessModule::GetProcessModule();
  vtkClientServerStream stream;
  stream  << vtkClientServerStream::Invoke
          << this->PostCollectUpdateSuppressor->GetID()
          << "PrepareFirstPass"
          << vtkClientServerStream::End;
  pm->SendStream(this->ConnectionID, vtkProcessModule::CLIENT, stream);
  stream.Reset();
}

//----------------------------------------------------------------------------
void vtkSMStreamingParallelStrategy::PrepareAnotherPass()
{
  DEBUGPRINT_STRATEGY(
    cerr << "SPS("<<this<<")::PrepareAnotherPass" << endl;
    );

  vtkProcessModule *pm = vtkProcessModule::GetProcessModule();
  vtkClientServerStream stream;
  stream  << vtkClientServerStream::Invoke
          << this->PostCollectUpdateSuppressor->GetID()
          << "PrepareAnotherPass"
          << vtkClientServerStream::End;
  pm->SendStream(this->ConnectionID, vtkProcessModule::CLIENT, stream);
  stream.Reset();
}

//----------------------------------------------------------------------------
void vtkSMStreamingParallelStrategy::ChooseNextPiece()
{
  DEBUGPRINT_STRATEGY(
    cerr << "SPS("<<this<<")::ChooseNextPiece" << endl;
    );

  vtkProcessModule *pm = vtkProcessModule::GetProcessModule();
  vtkClientServerStream stream;
  stream  << vtkClientServerStream::Invoke
          << this->PostCollectUpdateSuppressor->GetID()
          << "ChooseNextPieces"
          << vtkClientServerStream::End;
  pm->SendStream(this->ConnectionID, vtkProcessModule::CLIENT, stream);
  stream.Reset();

  //see if we need to update in parallel
  vtkSMIntVectorProperty* ivp = vtkSMIntVectorProperty::SafeDownCast(
    this->PostCollectUpdateSuppressor->GetProperty("GetStateInfo"));
  this->PostCollectUpdateSuppressor->UpdatePropertyInformation(ivp);
  bool local = (ivp->GetElement(3)==1);
  int pass = ivp->GetElement(4);
  int nPasses = vtkStreamingOptions::GetStreamedPasses();

  if (!local)
    {
    cerr << "SPS("<<this<<") UPDATING FROM SERVER FOR " << pass << "/" << nPasses << endl;

    ivp = vtkSMIntVectorProperty::SafeDownCast(
      this->UpdateSuppressor->GetProperty("PassNumber"));
    ivp->SetElement(0, pass);
    ivp->SetElement(1, nPasses);
    ivp->Modified();
    this->UpdateSuppressor->UpdateVTKObjects();

    ivp = vtkSMIntVectorProperty::SafeDownCast(
      this->PostCollectUpdateSuppressor->GetProperty("PassNumber"));
    ivp->SetElement(0, pass);
    ivp->SetElement(1, nPasses);
    ivp->Modified();
    this->PostCollectUpdateSuppressor->UpdateVTKObjects();
    }
  else
    {
    cerr << "SPS("<<this<<") FOUND LOCALLY " << pass << "/" << nPasses << endl;
    }
  this->UpdatePipeline();

}

//----------------------------------------------------------------------------
void vtkSMStreamingParallelStrategy::UpdatePipeline()
{
  DEBUGPRINT_STRATEGY(
                      cerr << "SPS("<<this<<")::UpdatePipeline" << endl;
                      );

  //this->vtkSMSimpleParallelStrategy::UpdatePipeline();

  vtkSMIntVectorProperty* ivp = vtkSMIntVectorProperty::SafeDownCast(
    this->PostCollectUpdateSuppressor->GetProperty("GetStateInfo"));
  bool local = (ivp->GetElement(3)==1);

  if (local)
    {
    cerr << "SPS("<<this<<") Using local cache" << endl;
    vtkProcessModule *pm = vtkProcessModule::GetProcessModule();
    vtkClientServerStream stream;
    stream  << vtkClientServerStream::Invoke
            << this->PostCollectUpdateSuppressor->GetID()
            << "ForceUpdate"
            << vtkClientServerStream::End;
    pm->SendStream(this->ConnectionID, vtkProcessModule::CLIENT, stream);

    this->CollectedDataValid = true;
    }
  else
    {
    cerr << "SPS("<<this<<") Updating from server" << endl;

    // Update the CacheKeeper.
    this->DataValid = true;
    this->InformationValid = false;

    this->UpdateSuppressor->InvokeCommand("ForceUpdate");
    // This is called for its side-effects; i.e. to force a PostUpdateData()
    this->UpdateSuppressor->UpdatePipeline();

    vtkSMPropertyHelper(this->Collect, "MoveMode").Set(1); //collect
    this->Collect->UpdateProperty("MoveMode");

    // It is essential to mark the Collect filter explicitly modified.
    vtkClientServerStream stream;
    stream  << vtkClientServerStream::Invoke
            << this->Collect->GetID()
            << "Modified"
            << vtkClientServerStream::End;
    vtkProcessModule::GetProcessModule()->SendStream
      (
       this->ConnectionID, this->Collect->GetServers(), stream
       );

    this->PostCollectUpdateSuppressor->InvokeCommand("ForceUpdate");

    this->CollectedDataValid = true;
    }
}

//-----------------------------------------------------------------------------
void vtkSMStreamingParallelStrategy::CopyPieceList(
  vtkClientServerStream *stream,
  vtkSMSourceProxy *src, vtkSMSourceProxy *dest)
{
  if (src && dest)
    {
    (*stream)
      << vtkClientServerStream::Invoke
      << src->GetID()
      << "GetPieceList"
      << vtkClientServerStream::End
      << vtkClientServerStream::Invoke
      << dest->GetID()
      << "SetPieceList"
      << vtkClientServerStream::LastResult
      << vtkClientServerStream::End;
    }
}
