/*=========================================================================

  Program:   ParaView
  Module:    $RCSfile$

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkStreamingUpdateSuppressor.h"

#include "vtkAlgorithmOutput.h"
#include "vtkBoundingBox.h"
#include "vtkCharArray.h"
#include "vtkDataSet.h"
#include "vtkExtractSelectedFrustum.h"
#include "vtkInformation.h"
#include "vtkInformationExecutivePortKey.h"
#include "vtkInformationVector.h"
#include "vtkMath.h"
#include "vtkMultiProcessController.h"
#include "vtkMPIMoveData.h"
#include "vtkObjectFactory.h"
#include "vtkParallelPieceCacheFilter.h"
#include "vtkPiece.h"
#include "vtkPieceCacheFilter.h"
#include "vtkPieceList.h"
#include "vtkPolyData.h"
#include "vtkProcessModule.h"
#include "vtkStreamingOptions.h"
#include "vtkUpdateSuppressorPipeline.h"

#include <vtksys/ios/sstream>
#include <string.h>
#include <sstream>
#include <iostream>

vtkCxxRevisionMacro(vtkStreamingUpdateSuppressor, "$Revision$");
vtkStandardNewMacro(vtkStreamingUpdateSuppressor);

#define LOG(arg)\
  {\
  std::ostringstream stream;\
  stream << arg;\
  vtkStreamingOptions::Log(stream.str().c_str());\
  }

#define DEBUGPRINT_EXECUTION(arg)\
  if (vtkStreamingOptions::GetEnableStreamMessages()) \
    { \
      arg;\
    }

#define DEBUGPRINT_PRIORITY(arg)\
  if (vtkStreamingOptions::GetEnableStreamMessages())\
    { \
      arg;\
    }

//----------------------------------------------------------------------------
vtkStreamingUpdateSuppressor::vtkStreamingUpdateSuppressor()
{
  this->Pass = 0;
  this->NumberOfPasses = 1;

  this->PieceList = NULL;

  this->ParPLs = NULL;
  this->NumPLs = 0;

  this->SerializedLists = NULL;

  this->MPIMoveData = NULL;
  this->PieceCacheFilter = NULL;
  this->ParallelPieceCacheFilter = NULL;

  this->CameraState = new double[9];
  const double caminit[9] = {0.0,0.0,-1.0, 0.0,1.0,0.0, 0.0,0.0,0.0};
  memcpy(this->CameraState, caminit, 9*sizeof(double));
  this->Frustum = new double[32];
  const double frustinit[32] = {
    0.0, 0.0, 0.0, 1.0,
    0.0, 0.0, 1.0, 1.0,
    0.0, 1.0, 0.0, 1.0,
    0.0, 1.0, 1.0, 1.0,
    1.0, 0.0, 0.0, 1.0,
    1.0, 0.0, 1.0, 1.0,
    1.0, 1.0, 0.0, 1.0,
    1.0, 1.0, 1.0, 1.0};
  memcpy(this->Frustum, frustinit, 32*sizeof(double));
  this->FrustumTester = vtkExtractSelectedFrustum::New();

  this->UseAppend = 0;
}

//----------------------------------------------------------------------------
vtkStreamingUpdateSuppressor::~vtkStreamingUpdateSuppressor()
{
  if (this->PieceList)
    {
    this->PieceList->Delete();
    }
  if (this->ParPLs)
    {
    for (int i = 0; i < NumPLs; i++)
      {
      this->ParPLs[i]->Delete();
      }
    delete[] this->ParPLs;
    }
  if (this->SerializedLists)
    {
    delete[] this->SerializedLists;
    }

  this->FrustumTester->Delete();
  delete[] this->CameraState;
  delete[] this->Frustum;
}

//----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
  cerr << "PPC = ";
  if (this->ParallelPieceCacheFilter)
    {
    cerr << this->ParallelPieceCacheFilter;
    }
  else
    {
    cerr << "NONE";
    }
  cerr << endl;
  cerr << "PCF = ";
  if (this->PieceCacheFilter)
    {
    cerr << this->PieceCacheFilter;
    }
  else
    {
    cerr << "NONE";
    }
  cerr << endl;
  this->PrintPieceLists();
  os << "EYE:" << this->CameraState[0] << "," << this->CameraState[1] << "," << this->CameraState[2] << endl;
  os << "UP: " << this->CameraState[3] << "," << this->CameraState[4] << "," << this->CameraState[5] << endl;
  os << "AT: " << this->CameraState[6] << "," << this->CameraState[7] << "," << this->CameraState[8] << endl;
  os << "FRUST: " << endl;
  for (int i = 0; i < 8; i++)
    {
    os << this->Frustum[i*4+0] << ","
       << this->Frustum[i*4+1] << ","
       << this->Frustum[i*4+2] << ","
       << this->Frustum[i*4+3] << endl;
    }
}

//----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::ForceUpdate()
{
  cerr << "SUS(" << this << ") FU!" << endl;
  if (this->UseAppend)
    {
    cerr << "TRYING APPEND" << endl;
    this->UseAppend = 0;
    if (this->ParallelPieceCacheFilter)
      {
      cerr << "TRYING PPCF" << endl;
      if (this->ParallelPieceCacheFilter->GetAppendedData())
        {
        cerr << "GOT SOMETHING" << endl;
        vtkPolyData *output = vtkPolyData::SafeDownCast(this->GetOutput());
        output->ShallowCopy(this->ParallelPieceCacheFilter->GetAppendedData());
        this->PipelineUpdateTime.Modified();
        return;
        }
      }
    if (this->PieceCacheFilter)
      {
      LOG("TRYING PCF" << endl;)
      if (this->PieceCacheFilter->GetAppendedData())
        {
        LOG("GOT SOMETHING" << endl;)
        vtkDataObject *output = this->GetOutput();
        output->ShallowCopy(this->PieceCacheFilter->GetAppendedData());
        this->PipelineUpdateTime.Modified();
        return;
        }
      }
    }

  int gPiece = this->UpdatePiece*this->NumberOfPasses + this->GetPiece();
  int gPieces = this->UpdateNumberOfPieces*this->NumberOfPasses;

  //DEBUGPRINT_EXECUTION(
  LOG("US(" << this << ") ForceUpdate " << this->Pass << "/" << this->NumberOfPasses << "->" << gPiece << "/" << gPieces << endl;)
  //                     );

  // Make sure that output type matches input type
  this->UpdateInformation();

  vtkDataObject *input = this->GetInput();
  if (input == 0)
    {
    vtkErrorMacro("No valid input.");
    return;
    }
  vtkDataObject *output = this->GetOutput();
  vtkPiece cachedPiece;
  if (this->PieceList)
    {
    cachedPiece = this->PieceList->GetPiece(this->Pass);
    }

  // int fixme; // I do not like this hack.  How can we get rid of it?
  // Assume the input is the collection filter.
  // Client needs to modify the collection filter because it is not
  // connected to a pipeline.
  vtkAlgorithm *source = input->GetProducerPort()->GetProducer();
  if (source &&
      (source->IsA("vtkMPIMoveData") ||
       source->IsA("vtkCollectPolyData") ||
       source->IsA("vtkM2NDuplicate") ||
       source->IsA("vtkM2NCollect") ||
       source->IsA("vtkOrderedCompositeDistributor") ||
       source->IsA("vtkClientServerMoveData")))
    {
    source->Modified();
    }

  vtkInformation* info = input->GetPipelineInformation();
  vtkStreamingDemandDrivenPipeline* sddp =
    vtkStreamingDemandDrivenPipeline::SafeDownCast(
      vtkExecutive::PRODUCER()->GetExecutive(info));

  if (sddp)
    {
    sddp->SetUpdateExtent(info,
                          gPiece,
                          gPieces,
                          0);
    }
  else
    {
    vtkErrorMacro("Uh oh");
    return;
    }

  if (this->UpdateTimeInitialized)
    {
    info->Set(vtkCompositeDataPipeline::UPDATE_TIME_STEPS(), &this->UpdateTime, 1);
    }

  DEBUGPRINT_EXECUTION(
  cerr << "US(" << this << ") Update " << this->Pass << "->" << gPiece << endl;
  );

  input->Update();
  output->ShallowCopy(input);

  if (cachedPiece.IsValid())
    {
    vtkDataSet *ds = vtkDataSet::SafeDownCast(input);
    cachedPiece.SetBounds(ds->GetBounds());
    double bds[6];
    cachedPiece.GetBounds(bds);
    LOG("SUS(" << this << ") UPDATE PRODUCED"
        << ds->GetClassName() << " "
        << ds->GetNumberOfPoints() << " "
        << bds[0] << ".." << bds[1] << ","
        << bds[2] << ".." << bds[3] << ","
        << bds[4] << ".." << bds[5] << endl;)
    }
  this->PipelineUpdateTime.Modified();
}

//----------------------------------------------------------------------------
int vtkStreamingUpdateSuppressor::GetPiece(int p)
{
  int piece;
  int pass = p;

  //check validity
  if (pass < 0 || pass >= this->NumberOfPasses)
    {
    pass = this->Pass;
    }

  //lookup piece corresponding to pass
  vtkPiece pStruct;
  if (!this->PieceList)
    {
    piece = pass;
    }
  else
    {
    pStruct = this->PieceList->GetPiece(pass);
    if (!pStruct.IsValid())
      {
      piece = pass;
      }
    else
      {
      piece = pStruct.GetPiece();
      LOG("LOOKUP FOUND " << piece << "@" << pStruct.GetPriority() << endl;)
      }
    }
  return piece;
}

//----------------------------------------------------------------------------
bool vtkStreamingUpdateSuppressor::HasSliceCached(int pass)
{
  bool ret = false;
  if (this->ParallelPieceCacheFilter)
    {
    vtkPieceList *pl = vtkPieceList::New();

    for (int i = 0; i < this->NumPLs; i++)
      {
      vtkPiece pStruct = this->ParPLs[i]->GetPiece(pass);
      pStruct.SetProcessor(i);
      pl->AddPiece(pStruct);
      }

    this->ParallelPieceCacheFilter->SetRequestedPieces(pl);
    if (this->ParallelPieceCacheFilter->HasRequestedPieces())
      {
      cerr << "IT HAS THEM ALL" << endl;
      ret = true;
      }
    else
      {
      cerr << "SOME ARE MISSING" << endl;
      }
    pl->Delete();
    }
  return ret;
}

//----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::MarkMoveDataModified()
{
  if (this->MPIMoveData)
    {
    //We have to ensure that communication isn't halted when a piece is reused.
    //This is called whenever we set the pass, because any processor might
    //(because of view dependent reordering) rerender the same piece in two
    //subsequent frames in which case the pipeline would not update and
    //communications will hang.
    this->MPIMoveData->Modified();
    }
}

//----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::SetPassNumber(int pass, int NPasses)
{
  DEBUGPRINT_EXECUTION(
  cerr << "US(" << this << ") SetPassNumber " << Pass << "/" << NPasses << endl;
                       );
  this->SetPass(pass);
  this->SetNumberOfPasses(NPasses);
  //Calling this to check if we client can go alone
  this->GetPiece();
  if (!this->ParallelPieceCacheFilter || !this->ParallelPieceCacheFilter->HasPieceList())
    {
    this->MarkMoveDataModified();
    }
}

//-----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::SetPieceList(vtkPieceList *other)
{
  DEBUGPRINT_EXECUTION(
  cerr << "US(" << this << ") SET PIECE LIST" << endl;
  );
  if (this->PieceList)
    {
    this->PieceList->Delete();
    }
  this->PieceList = other;
  if (other)
    {
    other->Register(this);
    }
}

//-----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::SerializeLists()
{
  //cerr << "SUS(" << this << ") SerializeLists" << endl;
  for (int i = 0; i < this->NumPLs; i++)
    {
    this->ParPLs[i]->Serialize();
    }
}

//-----------------------------------------------------------------------------
char * vtkStreamingUpdateSuppressor::GetSerializedLists()
{
  if (this->SerializedLists)
    {
    delete[] this->SerializedLists;
    }

  vtksys_ios::ostringstream temp;
  int np = this->NumPLs;
  temp << np << " ";

  char *buffer;
  int len=0;
  for (int i = 0; i < this->NumPLs; i++)
    {
    this->ParPLs[i]->GetSerializedList(&buffer, &len);
    temp << buffer << " ";
    }

  int total_len = strlen(temp.str().c_str());
  this->SerializedLists = new char[total_len];
  strcpy(this->SerializedLists, temp.str().c_str());

  return this->SerializedLists;
}

//-----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::UnSerializeLists(char *buffer)
{
  LOG("SUS(" << this << ") UnSerializeLists" << endl;)
  if (!buffer)
    {
    return;
    }
  vtksys_ios::istringstream temp;
  temp.str(buffer);

  int numProcs;
  temp >> numProcs;
  //allocate the piecelists if necessary
  if (this->ParPLs == NULL || this->NumPLs != numProcs)
    {
    for (int i = 0; i < this->NumPLs; i++)
      {
      this->ParPLs[i]->Delete();
      }
    delete[] this->ParPLs;

    this->NumPLs = numProcs;
    this->ParPLs = new vtkPieceList*[numProcs];
    for (int i = 0; i < numProcs; i++)
      {
      this->ParPLs[i] = vtkPieceList::New();
      }
    }

  //serialize each one's own region of the buffer into itself
  int pos = temp.tellg();
  for (int i = 0; i < numProcs; i++)
    {
    int len;
    this->ParPLs[i]->UnSerialize(buffer+pos, &len);
    if (i == this->UpdatePiece)
      {
      //copy mine to where I will use it for future updates
      if (!this->PieceList)
        {
        this->PieceList = vtkPieceList::New();
        this->PieceList->Clear();
        }
      this->PieceList->CopyPieceList(this->ParPLs[i]);
      }
    pos = pos + len;
    }
}

//-----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::ClearPriorities()
{
  DEBUGPRINT_EXECUTION(
  cerr << "US(" << this << ") CLEAR PRIORITIES" << endl;
  );

  if (this->PieceList)
    {
    this->PieceList->Delete();
    this->PieceList = NULL;
    }

  if (this->ParPLs)
    {
    for (int i = 0; i < this->NumPLs; i++)
      {
      this->ParPLs[i]->Delete();
      }
    delete[] this->ParPLs;
    }
  this->NumPLs = 0;
}

//-----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::ComputeLocalPipelinePriorities()
{
  DEBUGPRINT_EXECUTION(
  cerr << "US(" << this << ") COMPUTE PRIORITIES ";
  this->PrintPipe(this);
  cerr << endl;
  );
  vtkDataObject *input = this->GetInput();
  if (input == 0)
    {
    cerr << "NO INPUT" << endl;
    return;
    }
  if (this->PieceList)
    {
    this->PieceList->Delete();
    }
  this->PieceList = vtkPieceList::New();
  this->PieceList->Clear();
  vtkInformation* info = input->GetPipelineInformation();
  vtkStreamingDemandDrivenPipeline* sddp =
    vtkStreamingDemandDrivenPipeline::SafeDownCast(
      vtkExecutive::PRODUCER()->GetExecutive(info));
  if (sddp)
    {
    for (int i = 0; i < this->NumberOfPasses; i++)
      {
      double priority = 1.0;
      vtkPiece piece;
      int gPiece = this->UpdatePiece*this->NumberOfPasses + i;
      int gPieces = this->UpdateNumberOfPieces*this->NumberOfPasses;
      double pBounds[6] = {0,-1,0,-1,0,-1};
      if (vtkStreamingOptions::GetUsePrioritization())
        {
        DEBUGPRINT_EXECUTION(
        cerr << "US(" << this << ") COMPUTE PRIORITY ON " << gPiece << endl;
        );
        sddp->SetUpdateExtent(info, gPiece, gPieces, 0);
        priority = sddp->ComputePriority();
        sddp->GetPieceBoundingBox(0, pBounds);
        DEBUGPRINT_EXECUTION(
        cerr << "US(" << this << ") result was " << priority << endl;
        );
        }
      piece.SetPiece(i);
      piece.SetNumPieces(this->NumberOfPasses);
      piece.SetPipelinePriority(priority);
      piece.SetBounds(pBounds);
      this->PieceList->AddPiece(piece);
      }
    }
  DEBUGPRINT_EXECUTION(
  cerr << "US(" << this << ") PRESORT:" << endl;
  this->PieceList->Print();
  );

  //sorts pieces from priority 1.0 down to 0.0
  this->PieceList->SortPriorities();

  //Send piecelists to root node where client can get it
  this->GatherPriorities();
}

//-----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::ComputeLocalViewPriorities()
{
  LOG("COMPUTING VIEW PRIORITIES" << endl;)
  this->ComputeViewPriorities(this->PieceList);
  this->PieceList->SortPriorities();
}

//-----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::ComputeGlobalViewPriorities()
{
  for (int i = 0; i < this->NumPLs; i++)
    {
    this->ComputeViewPriorities(this->ParPLs[i]);
    this->ParPLs[i]->SortPriorities();
    }
}

//-----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::ComputeViewPriorities(vtkPieceList *pl)
{
  if (!pl)
    {
    return;
    }

  for (int i = 0; i < pl->GetNumberOfPieces(); i++)
    {
    vtkPiece p = pl->GetPiece(i);
    double pp = this->ComputeViewPriority(p.GetBounds());
    p.SetViewPriority(pp);
    pl->SetPiece(i, p);
    }
}

//-----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::ComputeLocalCachePriorities()
{
  if (!this->PieceCacheFilter || !this->PieceList)
    {
    return;
    }

  vtkPieceList *pl = this->PieceList;
  for (int j = 0; j < pl->GetNumberOfPieces(); j++)
    {
    vtkPiece p = pl->GetPiece(j);
    int piece = p.GetPiece();
    int pieces = p.GetNumPieces();
    double res = p.GetResolution();
    if (this->PieceCacheFilter->InCache(piece,pieces,res))
      {
      p.SetCachedPriority(0.0);
      pl->SetPiece(j, p);
      }
    }
}

//-----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::ComputeGlobalCachePriorities()
{
  if (!this->ParallelPieceCacheFilter)
    {
    return;
    }

  for (int i = 0; i < this->NumPLs; i++)
    {
    vtkPieceList *pl = this->ParPLs[i];
    if (!pl)
      {
      continue;
      }

    for (int j = 0; j < pl->GetNumberOfPieces(); j++)
      {
      vtkPiece p = pl->GetPiece(j);
      if (this->ParallelPieceCacheFilter->HasPiece(&p))
        {
        p.SetCachedPriority(0.0);
        pl->SetPiece(j, p);
        }
      }
    }
}

//----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::PrintPipe(vtkAlgorithm *alg)
{
  //for debugging, this helps me understand which US is which in the messages
  vtkAlgorithm *algptr = alg;
  if (!algptr) return;
  if (algptr->GetNumberOfInputPorts() &&
      algptr->GetNumberOfInputConnections(0))
    {
    vtkAlgorithmOutput *ao = algptr->GetInputConnection(0,0);
    if (ao)
      {
      algptr = ao->GetProducer();
      this->PrintPipe(algptr);
      }
    cerr << "->";
    }
  cerr << alg->GetClassName();
}

//----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::GatherPriorities()
{
  if (!this->PieceList)
    {
    //will someone ever not have a piecelist?
    return;
    }

  int procid = 0;
  int numProcs = 1;
  vtkMultiProcessController *controller =
    vtkMultiProcessController::GetGlobalController();
  if (controller)
    {
    procid = controller->GetLocalProcessId();
    numProcs = controller->GetNumberOfProcesses();
    DEBUGPRINT_EXECUTION(
    cerr << "US(" << this << ") PREGATHER:" << endl;
    this->PieceList->Print();
    );
    }

  //serialize locally computed priorities
  this->PieceList->Serialize();
  char *buffer;
  int len;
  this->PieceList->GetSerializedList(&buffer, &len);

  if (procid)
    {
    //send locally computed priorities to root
    controller->Send(&len, 1, 0, PRIORITY_COMMUNICATION_TAG);
    controller->Send(buffer,
                     len,
                     0, PRIORITY_COMMUNICATION_TAG);
    }
  else
    {
    if (this->ParPLs == NULL)
      {
      this->ParPLs = new vtkPieceList*[numProcs];
      this->NumPLs = numProcs;
      for (int i = 0; i < numProcs; i++)
        {
        this->ParPLs[i] = vtkPieceList::New();
        }
      }
    //Unserialize my own
    this->ParPLs[0]->UnSerialize(buffer, &len);
    //Unserialize everyone else's
    for (int j = 1; j < numProcs; j++)
      {
      int rlen;
      controller->Receive(&rlen, 1, j, PRIORITY_COMMUNICATION_TAG);
      char *remotes = new char[rlen];
      controller->Receive(remotes, rlen, j, PRIORITY_COMMUNICATION_TAG);
      this->ParPLs[j]->UnSerialize(remotes, &rlen);
      delete[] remotes;
      }
    }
}

//----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::ScatterPriorities()
{
  int procid = 0;
  int numProcs = 1;
  vtkMultiProcessController *controller =
    vtkMultiProcessController::GetGlobalController();
  if (controller)
    {
    procid = controller->GetLocalProcessId();
    numProcs = controller->GetNumberOfProcesses();
    DEBUGPRINT_EXECUTION(
    cerr << "US(" << this << ") PREGATHER:" << endl;
    this->PieceList->Print();
    );
    }

  char *buffer;
  int len;
  if (procid)
    {
    controller->Receive(&len, 1, 0, PRIORITY_COMMUNICATION_TAG);
    controller->Receive(buffer,
                        len,
                        0, PRIORITY_COMMUNICATION_TAG);

    if (this->PieceList)
      {
      this->PieceList->Delete();
      }
    this->PieceList = vtkPieceList::New();
    this->PieceList->Clear();
    this->PieceList->UnSerialize(buffer, &len);
    }
  else
    {
    for (int j = 0; j < numProcs; j++)
      {
      int rlen;
      char *remotes;
      this->ParPLs[j]->Serialize();
      this->ParPLs[j]->GetSerializedList(&remotes, &rlen);
      if (j == 0)
        {
        this->PieceList->UnSerialize(remotes, &rlen);
        }
      else
        {
        controller->Send(&rlen, 1, j, PRIORITY_COMMUNICATION_TAG);
        controller->Send(remotes, rlen, j, PRIORITY_COMMUNICATION_TAG);
        }
      }
    }
}

//-----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::SetFrustum(double *frustum)
{
  int i;
  for (i=0; i<32; i++)
    {
    if ( frustum[i] != this->Frustum[i] )
      {
      break;
      }
    }
  if ( i < 32 )
    {
    for (i=0; i<32; i++)
      {
      this->Frustum[i] = frustum[i];
      }
    DEBUGPRINT_PRIORITY(
    cerr << "FRUST" << endl;
    for (i=0; i<8; i++)
      {
      cerr << this->Frustum[i*4+0] << "," << this->Frustum[i*4+1] << "," << this->Frustum[i*4+2] << endl;
      }
    );

    this->FrustumTester->CreateFrustum(frustum);

    //No! camera changes are low priority, should NOT cause
    //reexecution of pipeline or invalidate cache filter
    //this->Modified();
    }
}

//-----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::SetCameraState(double *cameraState)
{
  int i;
  for (i=0; i<9; i++)
    {
    if ( cameraState[i] != this->CameraState[i] )
      {
      break;
      }
    }
  if ( i < 9 )
    {
    for (i=0; i<9; i++)
      {
      this->CameraState[i] = cameraState[i];
      }
    DEBUGPRINT_PRIORITY(
    cerr << "EYE" << this->CameraState[0] << "," << this->CameraState[1] << "," << this->CameraState[2] << endl;
                        );
    }
}

//------------------------------------------------------------------------------
double vtkStreamingUpdateSuppressor::ComputeViewPriority(double *pbbox)
{
  if (!pbbox)
    {
    return 1.0;
    }

  double outPriority = 1.0;

  if (pbbox[0] <= pbbox[1] &&
      pbbox[2] <= pbbox[3] &&
      pbbox[4] <= pbbox[5])
    {
    // use the frustum extraction filter to reject pieces that do not intersect the view frustum
    if (!this->FrustumTester->OverallBoundsTest(pbbox))
      {
      outPriority = 0.0;
      }
    else
      {
      //for those that are not rejected, compute a priority from the bounds such that pieces
      //nearest to camera eye have highest priority 1 and those furthest away have lowest 0.
      //Must do this using only information about current piece.
      vtkBoundingBox box(pbbox);
      double center[3];
      box.GetCenter(center);

      double dbox=sqrt(vtkMath::Distance2BetweenPoints(&this->CameraState[0], center));
      const double *farlowerleftcorner = &this->Frustum[1*4];
      double dfar=sqrt(vtkMath::Distance2BetweenPoints(&this->CameraState[0], farlowerleftcorner));


      double dist = 1.0-dbox/dfar;
      if (dist < 0.0)
        {
        DEBUGPRINT_PRIORITY(cerr << "VS(" << this << ") reject too far" << endl;);
        dist = 0.0;
        }
      if (dist > 1.0)
        {
        DEBUGPRINT_PRIORITY(cerr << "VS(" << this << ") reject too near" << endl;);
        dist = 0.0;
        }
      outPriority = dist;
      }
    }

  return outPriority;
}


//-----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::AddPieceList(vtkPieceList *newPL)
{
  //This method is for testing. It makes it possible to manually add
  //piecelists with known content. That can later be inspected.
  cerr << "SUS(" << this << ") AddPieceList " << newPL << endl;
  if (!newPL)
    {
    return;
    }
  vtkPieceList **newPLs = new vtkPieceList*[this->NumPLs+1];
  for (int i = 0; i < this->NumPLs; i++)
    {
    newPLs[i] = this->ParPLs[i];
    }
  newPLs[this->NumPLs] = newPL;
  if (this->ParPLs)
    {
    delete[] this->ParPLs;
    }
  this->ParPLs = newPLs;
  this->NumPLs++;
}

//-----------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::PrintPieceLists()
{
  //This method is for testing. It makes it possible to
  //inspected piecelists for veracity.
  LOG("SUS(" << this << ") PrintPieceLists " << endl;)

  int procid = 0;
  int numProcs = 1;
  vtkMultiProcessController *controller =
    vtkMultiProcessController::GetGlobalController();
  if (controller)
    {
    procid = controller->GetLocalProcessId();
    numProcs = controller->GetNumberOfProcesses();
    }

  if(this->PieceList)
    {
    this->PieceList->Print();
    }
  else
    {
    LOG("NO LOCAL PL" << endl;);
    }
  LOG("NPLS = " << this->NumPLs << endl;)
  for (int i = 0; i < this->NumPLs; i++)
    {
    LOG("Printing " << i << " " << this->ParPLs[i] << endl;)
    this->ParPLs[i]->Print();
    }
  LOG("DONE" << endl;)
}

//------------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::CopyBuddy(vtkStreamingUpdateSuppressor *buddy)
{
  //This method is for testing. It makes it possible to simulate
  //piecelist copying across the network so test serialization.
  cerr << "SUS(" << this << ") CopyBuddy " << buddy << endl;
  if (!buddy)
    {
    return;
    }
  buddy->SerializeLists();

  char *buffer;
  int len;
  buffer = buddy->GetSerializedLists();
  len = strlen(buffer);
  cerr << "LEN = " << len << endl;
  cerr << buffer << endl;

  this->UnSerializeLists(buffer);
  this->PrintPieceLists();
}

//------------------------------------------------------------------------------
void vtkStreamingUpdateSuppressor::PrintSerializedLists()
{
  LOG("SUS(" << this << ") PrintSerialized" << endl;)
  char *buffer;
  buffer = this->GetSerializedLists();
  LOG("LEN = " << strlen(buffer) << endl;);
  LOG(cerr << buffer << endl;)
}
