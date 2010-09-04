/*=========================================================================

  Program:   ParaView
  Module:    vtkSMStreamingRepresentation.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkSMStreamingRepresentation.h"

#include "vtkObjectFactory.h"
#include "vtkPVCompositeDataInformation.h"
#include "vtkPVDataInformation.h"
#include "vtkSmartPointer.h"
#include "vtkSMInputProperty.h"
#include "vtkSMIntVectorProperty.h"
#include "vtkSMOutputPort.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMPropertyIterator.h"
#include "vtkSMProxyProperty.h"
#include "vtkSMRenderViewProxy.h"
#include "vtkSMRepresentationStrategy.h"
#include "vtkSMRepresentationStrategyVector.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMStreamingSerialStrategy.h"
#include "vtkSMStreamingViewProxy.h"
#include "vtkSMStreamingParallelStrategy.h"
#include "vtkStreamingOptions.h"

vtkStandardNewMacro(vtkSMStreamingRepresentation);

#define DEBUGPRINT_REPRESENTATION(arg)\
  {}

//  if (vtkStreamingOptions::GetEnableStreamMessages()) \
//    {                                                 \
//      arg;                                            \
//    }

inline void vtkSMStreamingRepresentationSetInt(
  vtkSMProxy* proxy, const char* pname, int val)
{
  vtkSMIntVectorProperty* ivp = vtkSMIntVectorProperty::SafeDownCast(
    proxy->GetProperty(pname));
  if (ivp)
    {
    ivp->SetElement(0, val);
    proxy->UpdateProperty(pname);
    }
}

//----------------------------------------------------------------------------
vtkSMStreamingRepresentation::vtkSMStreamingRepresentation()
{
  this->PieceBoundsRepresentation = 0;
  this->PieceBoundsVisibility = 0;
  this->AllDone = 1;
}

//----------------------------------------------------------------------------
vtkSMStreamingRepresentation::~vtkSMStreamingRepresentation()
{
}

//----------------------------------------------------------------------------
bool vtkSMStreamingRepresentation::EndCreateVTKObjects()
{
  this->PieceBoundsRepresentation =
    vtkSMDataRepresentationProxy::SafeDownCast(
      this->GetSubProxy("PieceBoundsRepresentation"));

  vtkSMProxy* inputProxy = this->GetInputProxy();

  this->Connect(inputProxy, this->PieceBoundsRepresentation,
                "Input", this->OutputPort);

  vtkSMStreamingRepresentationSetInt(this->PieceBoundsRepresentation,
                                     "Visibility", 0);
  vtkSMStreamingRepresentationSetInt(this->PieceBoundsRepresentation,
                                     "MakeOutlineOfInput", 1);
  vtkSMStreamingRepresentationSetInt(this->PieceBoundsRepresentation,
                                     "UseOutline", 1);

  return this->Superclass::EndCreateVTKObjects();
}

//----------------------------------------------------------------------------
void vtkSMStreamingRepresentation::SetViewInformation(vtkInformation* info)
{
  this->Superclass::SetViewInformation(info);

  if (this->PieceBoundsRepresentation)
    {
    this->PieceBoundsRepresentation->SetViewInformation(info);
    }
}

//----------------------------------------------------------------------------
bool vtkSMStreamingRepresentation::AddToView(vtkSMViewProxy* view)
{
  vtkSMStreamingViewProxy* streamView = vtkSMStreamingViewProxy::SafeDownCast(view);
  if (!streamView)
    {
    vtkErrorMacro("View must be a vtkSMStreamingView.");
    return false;
    }


  //this tells renderview to let view create strategy
  vtkSMRenderViewProxy* renderView = streamView->GetRootView();
  renderView->SetNewStrategyHelper(view);

  //Disabled for now, I need to be able to tell it what piece is current.
  //this->PieceBoundsRepresentation->AddToView(renderView);

  //but still add this to renderView
  return this->Superclass::AddToView(renderView);
}

//----------------------------------------------------------------------------
bool vtkSMStreamingRepresentation::RemoveFromView(vtkSMViewProxy* view)
{
  this->PieceBoundsRepresentation->RemoveFromView(view);
  return this->Superclass::RemoveFromView(view);
}

//----------------------------------------------------------------------------
void vtkSMStreamingRepresentation::SetVisibility(int visible)
{
  if (!visible)
    {
    this->ClearStreamCache();
    }

  vtkSMStreamingRepresentationSetInt(this->PieceBoundsRepresentation,
                                     "Visibility",
                                     visible && this->PieceBoundsVisibility);
  this->PieceBoundsRepresentation->UpdateVTKObjects();

  this->Superclass::SetVisibility(visible);
}

//----------------------------------------------------------------------------
void vtkSMStreamingRepresentation::SetPieceBoundsVisibility(int visible)
{
  this->PieceBoundsVisibility = visible;
  vtkSMStreamingRepresentationSetInt(this->PieceBoundsRepresentation,
                                     "Visibility",
                                     visible && this->GetVisibility());
  this->PieceBoundsRepresentation->UpdateVTKObjects();
}

//----------------------------------------------------------------------------
void vtkSMStreamingRepresentation::Update(vtkSMViewProxy* view)
{
  this->PieceBoundsRepresentation->Update(view);
  this->Superclass::Update(view);
}

//----------------------------------------------------------------------------
bool vtkSMStreamingRepresentation::UpdateRequired()
{
  if (this->PieceBoundsRepresentation->UpdateRequired())
    {
    return true;
    }
  return this->Superclass::UpdateRequired();
}

//----------------------------------------------------------------------------
void vtkSMStreamingRepresentation::SetUpdateTime(double time)
{
  this->Superclass::SetUpdateTime(time);
  this->PieceBoundsRepresentation->SetUpdateTime(time);
}

//----------------------------------------------------------------------------
void vtkSMStreamingRepresentation::SetUseViewUpdateTime(bool use)
{
  this->Superclass::SetUseViewUpdateTime(use);
  this->PieceBoundsRepresentation->SetUseViewUpdateTime(use);
}

//----------------------------------------------------------------------------
void vtkSMStreamingRepresentation::SetViewUpdateTime(double time)
{
  this->Superclass::SetViewUpdateTime(time);
  this->PieceBoundsRepresentation->SetViewUpdateTime(time);
}

//----------------------------------------------------------------------------
void vtkSMStreamingRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

#define TryComputePriorities(type, stage)             \
{ \
  type *ptr = type::SafeDownCast(iter->GetPointer());\
  if (ptr)\
    {\
    int maxpass;\
    if (stage == 0)\
      {\
      maxpass = ptr->ComputePipelinePriorities();\
      }\
    if (stage == 1)\
      {\
      maxpass = ptr->ComputeViewPriorities();\
      }\
    if (stage == 2)\
      {\
      maxpass = ptr->ComputeCachePriorities();\
      }\
    if (maxpass > maxPass)\
      {\
      maxPass = maxpass;\
      }\
    }\
}

//----------------------------------------------------------------------------
int vtkSMStreamingRepresentation::InternalComputePriorities(int stage)
{
  int maxPass = -1;
  vtkSMRepresentationStrategyVector strats;
  this->GetActiveStrategies(strats);
  vtkSMRepresentationStrategyVector::iterator iter;
  for (iter = strats.begin(); iter != strats.end(); ++iter)
    {
    TryComputePriorities(vtkSMStreamingSerialStrategy, stage);
//    TryComputePriorities(vtkSMStreamingParallelStrategy, stage);
    }
  return maxPass;
}

//----------------------------------------------------------------------------
int vtkSMStreamingRepresentation::ComputePipelinePriorities()
{
  DEBUGPRINT_REPRESENTATION(
  cerr << "SR(" << this << ") ComputePipelinePriorities" << endl;
                            );
  return this->InternalComputePriorities(0);
}

//----------------------------------------------------------------------------
int vtkSMStreamingRepresentation::ComputeViewPriorities()
{
  DEBUGPRINT_REPRESENTATION(
  cerr << "SR(" << this << ") ComputeViewPriorities" << endl;
                            );
  return this->InternalComputePriorities(1);
}

//----------------------------------------------------------------------------
int vtkSMStreamingRepresentation::ComputeCachePriorities()
{
  DEBUGPRINT_REPRESENTATION(
  cerr << "SR(" << this << ") ComputeCachePriorities" << endl;
                            );

  return this->InternalComputePriorities(2);
}

//----------------------------------------------------------------------------
#define TryMethod(type,method)                        \
{ \
  type *ptr = type::SafeDownCast(iter->GetPointer());\
  if (ptr)\
    {\
    ptr->method;\
    }\
}

//----------------------------------------------------------------------------
void vtkSMStreamingRepresentation::ClearStreamCache()
{
  vtkSMRepresentationStrategyVector strats;
  this->GetActiveStrategies(strats);
  vtkSMRepresentationStrategyVector::iterator iter;
  for (iter = strats.begin(); iter != strats.end(); ++iter)
    {
    TryMethod(vtkSMStreamingSerialStrategy, ClearStreamCache());
    TryMethod(vtkSMStreamingParallelStrategy, ClearStreamCache());
    }
}


//----------------------------------------------------------------------------
void vtkSMStreamingRepresentation::PrepareFirstPass()
{
  DEBUGPRINT_REPRESENTATION(
    cerr << "SREP("<<this<<")::PrepareFirstPass" << endl;
  );
  vtkSMRepresentationStrategyVector strats;
  this->GetActiveStrategies(strats);
  vtkSMRepresentationStrategyVector::iterator iter;
  for (iter = strats.begin(); iter != strats.end(); ++iter)
    {
    TryMethod(vtkSMStreamingSerialStrategy, PrepareFirstPass());
    }
  this->AllDone = 0;
}

//----------------------------------------------------------------------------
void vtkSMStreamingRepresentation::PrepareAnotherPass()
{
  DEBUGPRINT_REPRESENTATION(
    cerr << "SREP("<<this<<")::PrepareAnotherPass" << endl;
  );
  vtkSMRepresentationStrategyVector strats;
  this->GetActiveStrategies(strats);
  vtkSMRepresentationStrategyVector::iterator iter;
  for (iter = strats.begin(); iter != strats.end(); ++iter)
    {
    TryMethod(vtkSMStreamingSerialStrategy, PrepareAnotherPass());
    }
}

//---------------------------------------------------------------------------
#define TryChooseNextPiece(type) \
{ \
  type *ptr = type::SafeDownCast(iter->GetPointer());\
  if (ptr)\
    {\
    ptr->ChooseNextPiece();\
    ptr->GetPieceInfo(&P, &NP, &R, &PRIORITY, &HIT, &APPEND);   \
    ptr->GetStateInfo(&ALLDONE, &WENDDONE);\
    }\
  }

//---------------------------------------------------------------------------
void vtkSMStreamingRepresentation::ChooseNextPiece()
{
  DEBUGPRINT_REPRESENTATION(
  cerr << "SREP("<<this<<")::ChooseNextPiece" << endl;
  );
  int P, NP;
  double R, PRIORITY;
  bool HIT;
  bool APPEND;

  bool ALLDONE;
  bool WENDDONE;

  vtkSMRepresentationStrategyVector strats;
  this->GetActiveStrategies(strats);
  vtkSMRepresentationStrategyVector::iterator iter;
  for (iter = strats.begin(); iter != strats.end(); ++iter)
    {
    TryChooseNextPiece(vtkSMStreamingSerialStrategy);
    }
  DEBUGPRINT_REPRESENTATION(
  cerr << "STRAT(" << this << ") CHOSE " << P << "/" << NP << "@" << R << endl;
  );

  //this->PieceBoundsRepresentation->SetNextPiece(P, NP, R, PRIORITY, HIT, APPEND);
  this->AllDone = ALLDONE;
}

//---------------------------------------------------------------------------
void vtkSMStreamingRepresentation::SetViewState(double *camera, double *frustum)
{
  vtkSMRepresentationStrategyVector strats;
  this->GetActiveStrategies(strats);
  vtkSMRepresentationStrategyVector::iterator iter;
  for (iter = strats.begin(); iter != strats.end(); ++iter)
    {
    TryMethod(vtkSMStreamingSerialStrategy, SetViewState(camera, frustum));
    TryMethod(vtkSMStreamingParallelStrategy, SetViewState(camera, frustum));
    }
}
