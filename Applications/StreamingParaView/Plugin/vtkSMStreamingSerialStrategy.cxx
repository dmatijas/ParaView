/*=========================================================================

  Program:   ParaView
  Module:    vtkSMStreamingSerialStrategy.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkSMStreamingSerialStrategy.h"
#include "vtkStreamingOptions.h"
#include "vtkClientServerStream.h"
#include "vtkObjectFactory.h"
#include "vtkProcessModule.h"
#include "vtkPVDataSizeInformation.h"
#include "vtkSMDoubleVectorProperty.h"
#include "vtkSMIntVectorProperty.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMProperty.h"
#include "vtkSMProxyProperty.h"

#define DEBUGPRINT_STRATEGY(arg)\
  if (vtkStreamingOptions::GetEnableStreamMessages()) \
    { \
      arg;\
    }

vtkStandardNewMacro(vtkSMStreamingSerialStrategy);

//----------------------------------------------------------------------------
vtkSMStreamingSerialStrategy::vtkSMStreamingSerialStrategy()
{
  this->EnableCaching = false; //don't try to use animation caching
  this->SetEnableLOD(false); //don't try to have LOD while interacting either
}

//----------------------------------------------------------------------------
vtkSMStreamingSerialStrategy::~vtkSMStreamingSerialStrategy()
{
}

//----------------------------------------------------------------------------
void vtkSMStreamingSerialStrategy::BeginCreateVTKObjects()
{
  this->Superclass::BeginCreateVTKObjects();

  //Get hold of the caching filter proxy
  this->PieceCache =
    vtkSMSourceProxy::SafeDownCast(this->GetSubProxy("PieceCache"));
}

//----------------------------------------------------------------------------
void vtkSMStreamingSerialStrategy::CreatePipeline(vtkSMSourceProxy* input, int outputport)
{
  this->Connect(input, this->PieceCache, "Input", outputport);
  this->Superclass::CreatePipeline(this->PieceCache, 0);
  //input->PieceCache->US
}

//----------------------------------------------------------------------------
void vtkSMStreamingSerialStrategy::GatherInformation(vtkPVInformation* info)
{
  //gather information without requesting the whole data
  vtkSMIntVectorProperty* ivp;
  DEBUGPRINT_STRATEGY(
    cerr << "SSS(" << this << ") Gather Info" << endl;
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
    this->UpdatePipeline();

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
void vtkSMStreamingSerialStrategy::InvalidatePipeline()
{
  // Cache is cleaned up whenever something changes and caching is not currently
  // enabled.
  this->UpdateSuppressor->InvokeCommand("ClearPriorities");
  this->Superclass::InvalidatePipeline();
}

//----------------------------------------------------------------------------
void vtkSMStreamingSerialStrategy::SetPassNumber(int val, int force)
{
  int nPasses = vtkStreamingOptions::GetStreamedPasses();
  vtkSMIntVectorProperty* ivp;
  DEBUGPRINT_STRATEGY(
    cerr << "SSS(" << this << ") SetPassNumber(" << val << "/" << nPasses << (force?"FORCE":"LAZY") << ")" << endl;
                      );

  ivp = vtkSMIntVectorProperty::SafeDownCast(
    this->UpdateSuppressor->GetProperty("PassNumber"));
  ivp->SetElement(0, val);
  ivp->SetElement(1, nPasses);
  if (force)
    {
    ivp->Modified();
    this->UpdateSuppressor->UpdateVTKObjects();
    vtkSMProperty *p = this->UpdateSuppressor->GetProperty("ForceUpdate");
    p->Modified();
    this->UpdateSuppressor->UpdateVTKObjects();
    }
}

//----------------------------------------------------------------------------
int vtkSMStreamingSerialStrategy::ComputePipelinePriorities()
{
  int nPasses = vtkStreamingOptions::GetStreamedPasses();

  vtkSMIntVectorProperty* ivp;

  //put diagnostic settings transfer here in case info not gathered yet
  int cacheLimit = vtkStreamingOptions::GetPieceCacheLimit();

  DEBUGPRINT_STRATEGY(
    cerr << "SSS(" << this << ") ComputePipelinePriorities" << endl;
                      )
  ivp = vtkSMIntVectorProperty::SafeDownCast(
      this->PieceCache->GetProperty("SetCacheSize"));
  ivp->SetElement(0, cacheLimit);
  this->PieceCache->UpdateVTKObjects();

  //let US know NumberOfPasses for CP
  ivp = vtkSMIntVectorProperty::SafeDownCast(
      this->UpdateSuppressor->GetProperty("SetNumberOfPasses"));
  ivp->SetElement(0, nPasses);

  this->UpdateSuppressor->UpdateVTKObjects();

  //ask it to compute the priorities
  vtkSMProperty* cp = this->UpdateSuppressor->GetProperty
    ("ComputeLocalPipelinePriorities");
  cp->Modified();
  this->UpdateSuppressor->UpdateVTKObjects();

  return -1;
}

//----------------------------------------------------------------------------
int vtkSMStreamingSerialStrategy::ComputeViewPriorities()
{
  int nPasses = vtkStreamingOptions::GetStreamedPasses();
  vtkSMIntVectorProperty* ivp;

  //put diagnostic settings transfer here in case info not gathered yet
  int cacheLimit = vtkStreamingOptions::GetPieceCacheLimit();

  DEBUGPRINT_STRATEGY(
    cerr << "SSS(" << this << ") ComputeViewPriorities" << endl;
                      )
  ivp = vtkSMIntVectorProperty::SafeDownCast(
      this->PieceCache->GetProperty("SetCacheSize"));
  ivp->SetElement(0, cacheLimit);
  this->PieceCache->UpdateVTKObjects();

  //let US know NumberOfPasses for CP
  ivp = vtkSMIntVectorProperty::SafeDownCast(
      this->UpdateSuppressor->GetProperty("SetNumberOfPasses"));
  ivp->SetElement(0, nPasses);

  this->UpdateSuppressor->UpdateVTKObjects();

  //ask it to compute the priorities
  vtkSMProperty* cp = this->UpdateSuppressor->GetProperty
    ("ComputeLocalViewPriorities");
  cp->Modified();
  this->UpdateSuppressor->UpdateVTKObjects();


  return -1;
}

//----------------------------------------------------------------------------
void vtkSMStreamingSerialStrategy::ClearStreamCache()
{
  vtkSMProperty *cc = this->PieceCache->GetProperty("EmptyCache");
  cc->Modified();
  this->PieceCache->UpdateVTKObjects();
}

//-----------------------------------------------------------------------------
void vtkSMStreamingSerialStrategy::CopyPieceList(
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

//----------------------------------------------------------------------------
void vtkSMStreamingSerialStrategy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkSMStreamingSerialStrategy::SetViewState(double *camera, double *frustum)
{
  if (!camera || !frustum)
    {
    return;
    }

  //only
  vtkSMDoubleVectorProperty* dvp;
  dvp = vtkSMDoubleVectorProperty::SafeDownCast(
    this->UpdateSuppressor->GetProperty("SetCamera"));
  dvp->SetElements(camera);
  dvp = vtkSMDoubleVectorProperty::SafeDownCast(
    this->UpdateSuppressor->GetProperty("SetFrustum"));
  dvp->SetElements(frustum);
  this->UpdateSuppressor->UpdateVTKObjects();
}

//----------------------------------------------------------------------------
int vtkSMStreamingSerialStrategy::GetNumberNonZeroPriority()
{
  vtkSMIntVectorProperty *ivp;
  ivp = vtkSMIntVectorProperty::SafeDownCast
    (this->UpdateSuppressor->GetProperty("GetNumberNonZeroPriority"));
  this->UpdateSuppressor->UpdatePropertyInformation(ivp);
  return ivp->GetElement(0);
}
