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
#include "vtkInformation.h"
#include "vtkMPIMoveData.h"
#include "vtkObjectFactory.h"
#include "vtkParallelPieceCacheFilter.h"
#include "vtkProcessModule.h"
#include "vtkPVInformation.h"
#include "vtkSMDoubleVectorProperty.h"
#include "vtkSMIceTMultiDisplayRenderViewProxy.h"
#include "vtkSMIntVectorProperty.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxyProperty.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMStringVectorProperty.h"
#include "vtkSMInputProperty.h"

vtkStandardNewMacro(vtkSMStreamingParallelStrategy);

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
  this->SetEnableLOD(false); //don't try to have LOD

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

  //turn off caching for animation, it interferes with streaming
  vtkSMSourceProxy *cacher =
    vtkSMSourceProxy::SafeDownCast(this->GetSubProxy("CacheKeeper"));
  ivp = vtkSMIntVectorProperty::SafeDownCast(
    cacher->GetProperty("CachingEnabled"));
  ivp->SetElement(0, 0);

  this->Connect(input, this->PieceCache, "Input", outputport);

  this->Superclass::CreatePipeline(this->PieceCache, 0);
  //input->VS->PC->US->Collect->PostCollectUS

  vtkProcessModule *pm = vtkProcessModule::GetProcessModule();
  if (pm->GetNumberOfPartitions(this->GetConnectionID())>1)
    {
    //use streams instead of a proxy property here
    //so that proxyproperty dependencies are not invoked
    //otherwise they end up with an artificial loop
    vtkClientServerStream stream;
    stream << vtkClientServerStream::Invoke
           << this->PostCollectUpdateSuppressor->GetID()
           << "SetMPIMoveData"
           << this->Collect->GetID()
           << vtkClientServerStream::End;
    pm->SendStream(this->GetConnectionID(),
                   vtkProcessModule::CLIENT_AND_SERVERS,
                   stream);
    }

  //On the client only, insert the parallel piece cache filter
  //between the MPIMoveData and the PostCollectUpdateSuppressor
  vtkSMProxyProperty* pp = vtkSMProxyProperty::SafeDownCast(
    this->PostCollectUpdateSuppressor->GetProperty("Input"));
  vtkSMInputProperty* ip = vtkSMInputProperty::SafeDownCast(pp);
  if (!pp)
    {
    vtkErrorMacro("Failed to locate property ");
    }
  if (ip)
    {
    ip->RemoveAllProxies();
    }
  vtkClientServerStream stream;
  // Connect the parallel piece cache and set it up, only on the client.
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
  stream << vtkClientServerStream::Invoke
          << this->PostCollectUpdateSuppressor->GetID()
         << "SetPieceCacheFilter"
         << this->ParallelPieceCache->GetID()
         << vtkClientServerStream::End;
  pm->SendStream(this->ConnectionID, vtkProcessModule::CLIENT, stream);

  vtkClientServerStream stream2;
  stream2  << vtkClientServerStream::Invoke
          << this->Collect->GetID()
          << "GetOutputPort" << 0
          << vtkClientServerStream::End;
  stream2  << vtkClientServerStream::Invoke
          << this->PostCollectUpdateSuppressor->GetID()
          << "SetInputConnection" << 0
          << vtkClientServerStream::LastResult
          << vtkClientServerStream::End;
  pm->SendStream(this->ConnectionID, vtkProcessModule::SERVERS, stream2);

  // Do not supress any updates in the intermediate US's between
  // data and display. We need them to get piece selection back.
  ivp = vtkSMIntVectorProperty::SafeDownCast(
    this->UpdateSuppressor->GetProperty("Enabled"));
  ivp->SetElement(0, 0);
  this->UpdateSuppressor->UpdateVTKObjects();
}

//----------------------------------------------------------------------------
void vtkSMStreamingParallelStrategy::SetPassNumber(int val, int force)
{
  int nPasses = vtkStreamingOptions::GetStreamedPasses();
//  cerr << "SPS(" << this << ") SetPassNumber " << val << "/" << nPasses << " " << (force?"FORCE":"LAZY") << endl;

  vtkSMIntVectorProperty* ivp;
  ivp = vtkSMIntVectorProperty::SafeDownCast(
    this->PostCollectUpdateSuppressor->GetProperty("PassNumber"));
  ivp->SetElement(0, val);
  ivp->SetElement(1, nPasses);
  if (force)
    {
    ivp->Modified();

    if (!this->HaveLocalCache())
      {
      cerr << "SHOULD SKIP MPI" << endl;
      }

    this->PostCollectUpdateSuppressor->UpdateVTKObjects();
    vtkSMProperty *p = this->PostCollectUpdateSuppressor->GetProperty("ForceUpdate");
    p->Modified();
    this->PostCollectUpdateSuppressor->UpdateVTKObjects();
    }
}

//----------------------------------------------------------------------------
int vtkSMStreamingParallelStrategy::ComputePriorities()
{
  int nPasses = vtkStreamingOptions::GetStreamedPasses();
  int ret = nPasses;

  vtkSMIntVectorProperty* ivp;

  //put diagnostic settings transfer here in case info not gathered yet
  int cacheLimit = vtkStreamingOptions::GetPieceCacheLimit();
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
  vtkSMProperty* cp =
    this->UpdateSuppressor->GetProperty("ComputePriorities");
  vtkSMIntVectorProperty* rp = vtkSMIntVectorProperty::SafeDownCast(
    this->UpdateSuppressor->GetProperty("GetMaxPass"));
  cp->Modified();
  this->UpdateSuppressor->UpdateVTKObjects();
  //get the result
  this->UpdateSuppressor->UpdatePropertyInformation(rp);
  ret = rp->GetElement(0);

  //now that we've computed the priority and piece ordering, share that
  //with the other UpdateSuppressors to keep them all in synch.
  vtkSMSourceProxy *pcUS = this->PostCollectUpdateSuppressor;

  vtkClientServerStream stream;
  this->CopyPieceList(&stream, this->UpdateSuppressor, pcUS);

  vtkProcessModule *pm = vtkProcessModule::GetProcessModule();
  pm->SendStream(this->GetConnectionID(),
                 vtkProcessModule::SERVERS,
                 stream);

  //now gather list from server to client
  vtkClientServerStream s2c;
  s2c << vtkClientServerStream::Invoke
      << this->UpdateSuppressor->GetID()
      << "SerializePriorities"
      << vtkClientServerStream::End;
  pm->SendStream(this->GetConnectionID(),
                 vtkProcessModule::DATA_SERVER_ROOT,
                 s2c);
  vtkSMStringVectorProperty *svp = vtkSMStringVectorProperty::SafeDownCast(
    this->UpdateSuppressor->GetProperty("SerializedLists"));
  this->UpdateSuppressor->UpdatePropertyInformation(svp);
//  int np = svp->GetNumberOfElements();
//  char *elems = svp->GetElement(0);

/*
  cerr << "SPS(" << this << ") Obtained list " << np << ":";
  for (int i = 0; i < np; i++)
    {
    cerr << elems[i] << " ";
    }
  cerr << endl;

  vtkClientServerStream s3c;
  s3c << vtkClientServerStream::Invoke
      << this->PostCollectUpdateSuppressor->GetID()
      << "UnSerializeLists"
//      << elem
//      << vtkClientServerStream::InsertArray(elems, np)
      << vtkClientServerStream::End;
  pm->SendStream(this->GetConnectionID(),
                 vtkProcessModule::CLIENT,
                 s3c);
*/
  return ret;
}

//----------------------------------------------------------------------------
void vtkSMStreamingParallelStrategy::ClearStreamCache()
{
  vtkSMProperty *cc = this->PieceCache->GetProperty("EmptyCache");
  cc->Modified();
  this->PieceCache->UpdateVTKObjects();
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

//----------------------------------------------------------------------------
void vtkSMStreamingParallelStrategy::GatherInformation(vtkPVInformation* info)
{
  //gather information in multiple passes so as never to request
  //everything at once.
  vtkSMIntVectorProperty* ivp;

  //put diagnostic setting transfer here because this happens early
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
  vtkSMProperty *p = this->UpdateSuppressor->GetProperty("ComputePriorities");
  p->Modified();
  this->UpdateSuppressor->UpdateVTKObjects();

  for (int i = 0; i < 1; i++)
    {
    vtkPVInformation *sinfo =
      vtkPVInformation::SafeDownCast(info->NewInstance());
    ivp = vtkSMIntVectorProperty::SafeDownCast(
      this->UpdateSuppressor->GetProperty("PassNumber"));
    ivp->SetElement(0, i);
    ivp->SetElement(1, nPasses);

    this->UpdateSuppressor->UpdateVTKObjects();
    this->UpdateSuppressor->InvokeCommand("ForceUpdate");

    vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
    pm->GatherInformation(this->ConnectionID,
                          vtkProcessModule::DATA_SERVER_ROOT,
                          sinfo,
                          this->PostCollectUpdateSuppressor->GetID());
    info->AddInformation(sinfo);
    sinfo->Delete();
    }
}

//----------------------------------------------------------------------------
void vtkSMStreamingParallelStrategy::InvalidatePipeline()
{
  // Cache is cleaned up whenever something changes and caching is not currently
  // enabled.
  if (this->PostCollectUpdateSuppressor)
    {
    this->PostCollectUpdateSuppressor->InvokeCommand("ClearPriorities");
    }
  this->Superclass::InvalidatePipeline();
}

//----------------------------------------------------------------------------
void vtkSMStreamingParallelStrategy::SetViewState(double *camera, double *frustum)
{
  if (!camera || !frustum)
    {
    return;
    }

  //TODO: When server side rendering, call on server only, when client side, set on client
  //only
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
void vtkSMStreamingParallelStrategy::UpdatePipeline()
{
  //I have inlined the code that normally happens in the parent classes.
  //Each parent class checks locally if data is valid and if not calls
  //superclass Update before updating the parts of the pipeline it is
  //responsible for. Doing it that way squeezes the data downward along
  //the intestine, as it were, toward the client.
  //
  //I had to do all this here just to bypass the blockage that
  //streamingoutputport makes on the MPIMoveData filter.

  //this->vtkSMSimpleParallelStrategy::UpdatePipeline();
  if (!this->HaveLocalCache())
    {
    if (this->vtkSMSimpleParallelStrategy::GetDataValid())
      {
      return;
      }

    //this->vtkSMSimpleStrategy::UpdatePipeline();
    {
    // We check to see if the part of the pipeline that will up updated by this
    // class needs any update. Then alone do we call update.
    if (this->vtkSMSimpleStrategy::GetDataValid())
      {
      return;
      }

    //this->vtkSMRepresentationStrategy::UpdatePipeline()
    {
    // Update the CacheKeeper.
    this->DataValid = true;
    this->InformationValid = false;
    }

    this->UpdateSuppressor->InvokeCommand("ForceUpdate");
    // This is called for its side-effects; i.e. to force a PostUpdateData()
    this->UpdateSuppressor->UpdatePipeline();
    }

    vtkSMPropertyHelper(this->Collect, "MoveMode").Set(this->GetMoveMode());
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

//------------------------------------------------------------------------------
bool vtkSMStreamingParallelStrategy::HaveLocalCache()
{
  bool ret = false;
  vtkParallelPieceCacheFilter *pcf =
    vtkParallelPieceCacheFilter::SafeDownCast
    (this->ParallelPieceCache->GetClientSideObject());
  if (pcf)
    {
    ret = pcf->HasPieceList();
    cerr << "SPS(" << this << ") I " << (ret?"HAVE":"DO NOT HAVE") << " cache" << endl;
    }

  return ret;
}
