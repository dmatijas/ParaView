/*=========================================================================

  Program:   ParaView
  Module:    vtkPrioritizedStreamer.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkMultiResolutionStreamer.h"

#include "vtkCollection.h"
#include "vtkCollectionIterator.h"
#include "vtkObjectFactory.h"
#include "vtkPieceList.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkStreamingDriver.h"
#include "vtkStreamingHarness.h"

vtkStandardNewMacro(vtkMultiResolutionStreamer);

class Internals
{
public:
  Internals(vtkMultiResolutionStreamer *owner)
  {
    this->Owner = owner;
    this->WendDone = true;
  }
  ~Internals()
  {
  }
  vtkMultiResolutionStreamer *Owner;
  bool WendDone;
};

//----------------------------------------------------------------------------
vtkMultiResolutionStreamer::vtkMultiResolutionStreamer()
{
  this->Internal = new Internals(this);
}

//----------------------------------------------------------------------------
vtkMultiResolutionStreamer::~vtkMultiResolutionStreamer()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkMultiResolutionStreamer::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
bool vtkMultiResolutionStreamer::IsFirstPass()
{
  if (this->Internal->WendDone)
    {
    return true;
    }
  return false;
}

//----------------------------------------------------------------------------
bool vtkMultiResolutionStreamer::IsRestart()
{
  //TODO: compare stored last camera with current camera
  return false;
}

//----------------------------------------------------------------------------
bool vtkMultiResolutionStreamer::IsWendDone()
{
  vtkCollection *harnesses = this->GetHarnesses();
  if (!harnesses)
    {
    return true;
    }

  bool everyone_finished_wend = true;
  vtkCollectionIterator *iter = harnesses->NewIterator();
  iter->InitTraversal();
  while(!iter->IsDoneWithTraversal())
    {
    vtkStreamingHarness *harness = vtkStreamingHarness::SafeDownCast
      (iter->GetCurrentObject());
    iter->GoToNextItem();

    //check if anyone hasn't reached the end of the visible domain
    vtkPieceList *ToDo = harness->GetPieceList1();
    if (ToDo->GetNumberNonZeroPriority() > 0)
      {
      everyone_finished_wend = false;
      break;
      }
    }
  iter->Delete();

  return everyone_finished_wend;
}

//----------------------------------------------------------------------------
bool vtkMultiResolutionStreamer::IsEveryoneDone()
{
  vtkCollection *harnesses = this->GetHarnesses();
  if (!harnesses)
    {
    return true;
    }

  bool everyone_completely_done = true;
  vtkCollectionIterator *iter = harnesses->NewIterator();
  iter->InitTraversal();
  while(!iter->IsDoneWithTraversal())
    {
    vtkStreamingHarness *harness = vtkStreamingHarness::SafeDownCast
      (iter->GetCurrentObject());
    iter->GoToNextItem();

    vtkPieceList *ToDo = harness->GetPieceList1();
    if (ToDo->GetNumberNonZeroPriority() > 0)
      {
      everyone_completely_done = false;
      break;
      }

    if (harness->GetNoneToRefine() == false)
      {
      everyone_completely_done = false;
      break;
      }
    }
  iter->Delete();

  return everyone_completely_done;
}

//----------------------------------------------------------------------------
void vtkMultiResolutionStreamer::PrepareFirstPass()
{
  vtkCollection *harnesses = this->GetHarnesses();
  if (!harnesses)
    {
    return;
    }

  vtkCollectionIterator *iter = harnesses->NewIterator();
  iter->InitTraversal();
  while(!iter->IsDoneWithTraversal())
    {
    vtkStreamingHarness *harness = vtkStreamingHarness::SafeDownCast
      (iter->GetCurrentObject());
    iter->GoToNextItem();

    vtkPieceList *ToDo = harness->GetPieceList1();
    if (!ToDo)
      {
      //very first pass, start off with one piece at lowest res
      vtkPieceList *pl = vtkPieceList::New();
      vtkPiece p;
      p.SetResolution(0.0);
      p.SetPiece(0);
      p.SetNumPieces(1);
      pl->AddPiece(p);
      harness->SetPieceList1(pl);
      pl->Delete();
      ToDo = pl;

      //and initialize the other two queues
      pl = vtkPieceList::New();
      harness->SetPieceList2(pl);
      pl->Delete();
      }

    harness->SetNoneToRefine(false); //assume it isn't all done
    //split and refine some of the pieces in nextframe
    int numRefined = this->Refine(harness);
    if (numRefined==0)
      {
      harness->SetNoneToRefine(true);
      //cerr << "NONE TO REFINE" << endl;
      }

    //compute priorities for everything we are going to draw this wend
    for (int i = 0; i < ToDo->GetNumberOfPieces(); i++)
      {
      int p = ToDo->GetPiece(i).GetPiece();
      int np = ToDo->GetPiece(i).GetNumPieces();
      double res = ToDo->GetPiece(i).GetResolution();
      double priority = harness->ComputePriority(p, np, res);
      ToDo->GetPiece(i).SetPipelinePriority(priority);
      }

    //sort them
    ToDo->SortPriorities();
    //cerr << "NUM PIECES " << ToDo->GetNumberOfPieces() << endl;

    //TODO: combine empties that no longer matter
    //this->Reap();
    }

  iter->Delete();
}

//----------------------------------------------------------------------------
void vtkMultiResolutionStreamer::ChooseNextPieces()
{
  vtkCollection *harnesses = this->GetHarnesses();
  if (!harnesses)
    {
    return;
    }

  vtkCollectionIterator *iter = harnesses->NewIterator();
  iter->InitTraversal();
  while(!iter->IsDoneWithTraversal())
    {
    vtkStreamingHarness *harness = vtkStreamingHarness::SafeDownCast
      (iter->GetCurrentObject());
    iter->GoToNextItem();

    vtkPieceList *ToDo = harness->GetPieceList1();
    vtkPieceList *NextFrame = harness->GetPieceList2();
    if (ToDo->GetNumberNonZeroPriority() > 0)
      {
      vtkPiece p = ToDo->PopPiece();
      NextFrame->AddPiece(p);
      //adjust pipeline to draw the chosen piece
      //cerr << "CHOSE "
      //     << p.GetPiece() << "/" << p.GetNumPieces()
      //     << "@" << p.GetResolution() << endl;
      harness->SetPiece(p.GetPiece());
      harness->SetNumberOfPieces(p.GetNumPieces());
      harness->SetResolution(p.GetResolution());
      }
    }

  iter->Delete();
}

//----------------------------------------------------------------------------
int vtkMultiResolutionStreamer::Refine(vtkStreamingHarness *harness)
{
  vtkPieceList *ToDo = harness->GetPieceList1();
  vtkPieceList *NextFrame = harness->GetPieceList2();
  vtkPieceList *ToSplit = vtkPieceList::New();

  int numSplittable = 0;
  while (NextFrame->GetNumberOfPieces() != 0)
    {
    vtkPiece piece = NextFrame->PopPiece();
    double res = piece.GetResolution();
    double priority = piece.GetPriority();
    double mdr = 1.0;
    if (priority > 0.0 &&
        res < 1.0)
      {
      numSplittable++;
      ToSplit->AddPiece(piece);
      }
    else
      {
      ToDo->AddPiece(piece);
      }
    }

  //loop through the splittable pieces, and split some of them
  int numSplit = 0;
  int maxSplits = 2;
  int maxHeight = 5;
  int degree = 2;
  for (;
       numSplit < maxSplits && ToSplit->GetNumberOfPieces() != 0;
       numSplit++)
  {
    //get the next piece
    vtkPiece piece = ToSplit->PopPiece();
    int p = piece.GetPiece();
    int np = piece.GetNumPieces();
    double res = piece.GetResolution();

    //compute next resolution to request for it
    double resolution = res + (1.0/maxHeight);

    //split it N times
    int nrNP = np*degree;
    int gPieces = nrNP;
    for (int child=0; child < degree; child++)
      {
      int nrA = p * degree + child;
      int gPieceA = nrNP + nrA;

      vtkPiece pA;
      pA.SetPiece(nrA);
      pA.SetNumPieces(nrNP);
      pA.SetResolution(resolution);
      double priority = harness->ComputePriority(gPieceA, gPieces, resolution);
      pA.SetPipelinePriority(priority);

      ToDo->AddPiece(pA);
      }
    }

  //put any pieces we did not split back into next wend
  ToDo->MergePieceList(ToSplit);
  ToSplit->Delete();

  return numSplit;
}

//----------------------------------------------------------------------------
void vtkMultiResolutionStreamer::StartRenderEvent()
{
  vtkCollection *harnesses = this->GetHarnesses();
  vtkRenderer *ren = this->GetRenderer();
  vtkRenderWindow *rw = this->GetRenderWindow();
  if (!harnesses || !ren || !rw)
    {
    return;
    }

  if (this->IsFirstPass() || this->IsRestart())
    {
    //cerr << "FIRST PASS" << endl;

    //start off by clearing the screen
    ren->EraseOn();
    rw->EraseOn();

    //and telling all the harnesses to get ready
    this->PrepareFirstPass();

    //don't swap back to front automatically, only show what we've
    //drawn when the entire domain is covered
    rw->SwapBuffersOff();
    //TODO:Except for diagnostic mode where it is helpful to see
    //everything as it is drawn
    }

  //ask everyone to choose the next piece to draw
  this->ChooseNextPieces();

  //assume that we are not done covering all the domains
  this->Internal->WendDone = false;
}

//----------------------------------------------------------------------------
void vtkMultiResolutionStreamer::EndRenderEvent()
{
  vtkCollection *harnesses = this->GetHarnesses();
  vtkRenderer *ren = this->GetRenderer();
  vtkRenderWindow *rw = this->GetRenderWindow();
  if (!harnesses || !ren || !rw)
    {
    return;
    }

  //after first pass all subsequent renders can not clear
  //otherwise we erase the partial results we drew before
  ren->EraseOff();
  rw->EraseOff();

 if (this->IsEveryoneDone())
    {
    //cerr << "EVERYONE DONE" << endl;
    //next pass should start with clear screen
    this->Internal->WendDone = true;

    //bring back buffer forward to show what we drew
    rw->SwapBuffersOn();
    rw->Frame();
    }
  else
    {
    if (this->IsWendDone())
      {
      //cerr << "START NEXT WEND" << endl;
      //next pass should start with clear screen
      this->Internal->WendDone = true;

      //everyone has covered their domain
      //bring back buffer forward to show what we drew
      rw->SwapBuffersOn();
      rw->Frame();
      }

    //there is more to draw, so keep going
    this->RenderEventually();
    }
}
