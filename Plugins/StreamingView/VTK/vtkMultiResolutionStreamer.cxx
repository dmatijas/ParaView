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
#include "vtkPieceCacheFilter.h"
#include "vtkPieceList.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkStreamingDriver.h"
#include "vtkStreamingHarness.h"

#define DEBUGPRINT_PASSES( arg ) ;
#define DEBUGPRINT_PRIORITY( arg ) ;
#define DEBUGPRINT_REFINE( arg ) ;

vtkStandardNewMacro(vtkMultiResolutionStreamer);

class vtkMultiResolutionStreamer::Internals
{
public:
  Internals(vtkMultiResolutionStreamer *owner)
  {
    this->Owner = owner;
    this->WendDone = true;
    this->CameraMoved = true;
    this->DebugPass = 0;
  }
  ~Internals()
  {
  }
  vtkMultiResolutionStreamer *Owner;
  bool WendDone;
  bool CameraMoved;
  int DebugPass; //used solely for debug messages
};

//----------------------------------------------------------------------------
vtkMultiResolutionStreamer::vtkMultiResolutionStreamer()
{
  this->Internal = new Internals(this);
  this->PipelinePrioritization = 1;
  this->ViewPrioritization = 1;
  this->ProgressionMode = 0;
  this->RefinementDepth = 5;
  this->DepthLimit = 10;
  this->MaxSplits = 8;
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
bool vtkMultiResolutionStreamer::IsCompletelyDone()
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

       //and initialize an empty next frame queue
       pl = vtkPieceList::New();
       harness->SetPieceList2(pl);
       pl->Delete();
       }

     //combine empties that no longer matter
     this->Reap(harness);

     harness->SetNoneToRefine(false); //assume it isn't all done
     //split and refine some of the pieces in nextframe
     int numRefined = this->Refine(harness);
     if (numRefined==0)
       {
       harness->SetNoneToRefine(true);
       }

     //compute priorities for everything we are going to draw this wend
     for (int i = 0; i < ToDo->GetNumberOfPieces(); i++)
       {
       vtkPiece piece = ToDo->GetPiece(i);
       int p = piece.GetPiece();
       int np = piece.GetNumPieces();
       double res = piece.GetResolution();
       double priority = 1.0;
      if (this->PipelinePrioritization)
        {
        priority = harness->ComputePriority(p, np, res);
        }
       DEBUGPRINT_PRIORITY
         (
          if (!priority)
            {
            cerr << "CHECKED PPRI OF " << p << "/" << np << "@" << res
                 << " = " << priority << endl;
            }
          );
       piece.SetPipelinePriority(priority);

       double pbbox[6];
       double gConf = 1.0;
       double aMin = 1.0;
       double aMax = -1.0;
       double aConf = 1.0;
       harness->ComputeMetaInformation
         (p, np, res,
          pbbox, gConf, aMin, aMax, aConf);
       double gPri = 1.0;
       if (this->ViewPrioritization)
         {
         gPri = this->CalculateViewPriority(pbbox);
         }
       DEBUGPRINT_PRIORITY
         (
          if (!gPri)
            {
            cerr << "CHECKED VPRI OF " << p << "/" << np << "@" << res
                 << " = " << gPri << endl;
            }
          );
       piece.SetViewPriority(gPri);

       if (this->Internal->CameraMoved)
         {
         //TODO: this whole forCamera and reapedflag business is a workaround
         //for the way refining/reaping can by cyclic and not terminate.
         //I would like to find a better termination condition and remove this.
         piece.SetReapedFlag(false);
         }

       ToDo->SetPiece(i, piece);
       }

     //sort them
     ToDo->SortPriorities();
     //cerr << "NUM PIECES " << ToDo->GetNumberOfPieces() << endl;
     //ToDo->Print();

#if 0
     //debug code to made sure domain is not overcovered
     if (true)
       {
       vtkPieceList *pl = vtkPieceList::New();
       pl->CopyPieceList(ToDo);

       while (pl->GetNumberOfPieces()>0)
         {
         vtkPiece piece = pl->PopPiece();
         int p = piece.GetPiece();
         int np = piece.GetNumPieces();
         //look for a piece that can be merged with it
         for (int j = 0; j < pl->GetNumberOfPieces(); j++)
           {
           vtkPiece other = pl->GetPiece(j);
           int p2 = other.GetPiece();
           int np2 = other.GetNumPieces();
           //cerr << p << "/" << np << " vs " << p2 << "/" << np2 << endl;
           if (p == p2 && np == np2)
             {
              cerr << "SAME" << endl;
             }
           if (np < np2)
             {
             int ratio = np2/np;
             int base = p * ratio;
             int top = (p+1) * ratio;
             for (int i = base; i < top; i++)
               {
               if (p2 == i)
                 {
                 cerr << "CHILD" << endl;
                 }
               }
             }
           if (np > np2)
             {
             int ratio = np/np2;
             int base = p2 * ratio;
             int top = (p2+1) * ratio;
             for (int i = base; i < top; i++)
               {
               if (p == i)
                 {
                 cerr << "CHILD" << endl;
                 }
               }
             }
           }
         }
       pl->Delete();
       }
#endif
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
       /*
       DEBUGPRINT_PRIORITY
         (
          cerr << "CHOSE "
          << p.GetPiece() << "/" << p.GetNumPieces()
          << "@" << p.GetResolution() << endl;
          );
       */
       harness->SetPiece(p.GetPiece());
       harness->SetNumberOfPieces(p.GetNumPieces());
       harness->SetResolution(p.GetResolution());

       //TODO:
       //This should not be necessary, but the PieceCacheFilter is silently
       //producing the stale (lower res?) results without it.
       harness->ComputePriority(p.GetPiece(), p.GetNumPieces(),
                                p.GetResolution());
       }
     }

   iter->Delete();
 }

 //----------------------------------------------------------------------------
 int vtkMultiResolutionStreamer::Refine(vtkStreamingHarness *harness)
{
  double res_delta = (1.0/this->RefinementDepth);

  vtkPieceList *ToDo = harness->GetPieceList1();
  vtkPieceList *NextFrame = harness->GetPieceList2();
  vtkPieceList *ToSplit = vtkPieceList::New();

  //cerr << "PRE REFINE:" << endl;
  //cerr << "TODO:" << endl;
  //ToDo->Print();
  //cerr << "NEXT:" << endl;
  //NextFrame->Print();

  //find candidates for refinement
  double maxRes = 1.0;
  if (this->DepthLimit>0.0)
    {
    maxRes = res_delta*this->DepthLimit;
    maxRes = (maxRes<1.0?maxRes:1.0);
    }
  int numSplittable = 0;
  while (NextFrame->GetNumberOfPieces() != 0)
    {
    vtkPiece piece = NextFrame->PopPiece();
    double res = piece.GetResolution();
    double priority = piece.GetPriority();
    bool reaped = piece.GetReapedFlag();
    if (!reaped &&
        priority > 0.0 &&
        (res+res_delta <= maxRes))
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
  //TODO:
  //Degree is potentially variable, but it would take some work
  int degree = 2;
  for (;
       numSplit < this->MaxSplits
         && ToSplit->GetNumberOfPieces() != 0;
       numSplit++)
  {
    //get the next piece
    vtkPiece piece = ToSplit->PopPiece();
    int p = piece.GetPiece();
    int np = piece.GetNumPieces();
    double res = piece.GetResolution();

    DEBUGPRINT_REFINE
      (
       cerr << "SPLIT "
       << p << "/" << np
       << " -> ";
       );

    //compute next resolution to request for it
    double resolution = res + res_delta;
    if (resolution > 1.0)
      {
      resolution = 1.0;
      }

    //split it N times
    int nrNP = np*degree;
    for (int child=0; child < degree; child++)
      {
      int nrA = p * degree + child;

      vtkPiece pA;
      pA.SetPiece(nrA);
      pA.SetNumPieces(nrNP);
      pA.SetResolution(resolution);

      DEBUGPRINT_REFINE
        (
         cerr
         << nrA << "/" << nrNP << "@" << resolution
         << (child<degree-1?" & ":"");
         );

      //put the split piece back
      ToDo->AddPiece(pA);
      }

    DEBUGPRINT_REFINE
      (
       cerr << endl;
       );
    }

  //put any pieces we did not split back
  ToDo->MergePieceList(ToSplit);
  ToSplit->Delete();

  //cerr << "POST REFINE:" << endl;
  //cerr << "TODO:" << endl;
  //ToDo->Print();
  //cerr << "NEXT:" << endl;
  //NextFrame->Print();

  return numSplit;
}

//----------------------------------------------------------------------------
void vtkMultiResolutionStreamer::Reap(vtkStreamingHarness *harness)
{
  vtkPieceList *ToDo = harness->GetPieceList1();
  int important = ToDo->GetNumberNonZeroPriority();
  int total = ToDo->GetNumberOfPieces();
  if (important == total)
    {
    return;
    }

  //cerr << "PRE REAP:" << endl;
  //cerr << "TODO:" << endl;
  //ToDo->Print();

  double res_delta = (1.0/this->RefinementDepth);

  vtkPieceList *toMerge = vtkPieceList::New();
  for (int i = total-1; i>=important; --i)
    {
    vtkPiece piece = ToDo->PopPiece(i);
    toMerge->AddPiece(piece);
    }

  vtkPieceList *merged = vtkPieceList::New();

  bool done = false;
  while (!done)
    {
    int mcount = 0;
    //pick a piece
    while (toMerge->GetNumberOfPieces()>0)
      {
      vtkPiece piece = toMerge->PopPiece();
      int p = piece.GetPiece();
      int np = piece.GetNumPieces();
      bool found = false;

      //look for a piece that can be merged with it
      for (int j = 0; j < toMerge->GetNumberOfPieces(); j++)
        {
        vtkPiece other = toMerge->GetPiece(j);
        int p2 = other.GetPiece();
        int np2 = other.GetNumPieces();
        if ((np==np2) &&
            (p/2==p2/2) )
          //TODO, when Degree==N!=2, have to round up all N sibs
          {
          //make the parent of the two pieces
          piece.SetPiece(p/2);
          piece.SetNumPieces(np/2);
          double res = piece.GetResolution()-res_delta;
          if (res < 0.0)
            {
            res = 0.0;
            }
          piece.SetResolution(res);
          piece.SetPipelinePriority(0.0);
          piece.SetReapedFlag(true); //this avoids an infinite loop
          DEBUGPRINT_REFINE
            (
             cerr << "REAP "
             << p << "&" << p2 << "/" << np
             << " -> "
             << p/2 << "/" << np/2 << "@" << res << endl;
             );

          //save it
          merged->AddPiece(piece);
          //get rid of the second half of the piece
          toMerge->RemovePiece(j);
          found = true;
          mcount++;

          //remove the cached data for the merged pieces
          vtkPieceCacheFilter *pcf = harness->GetCacheFilter();
          if (pcf)
            {
            int index;
            index = pcf->ComputeIndex(p,np);
            pcf->DeletePiece(index);
            index = pcf->ComputeIndex(p2,np);
            pcf->DeletePiece(index);
            }
          break;
          }
        }
      if (!found)
        {
        //put the candidate back
        merged->AddPiece(piece);
        }
      }
    if (mcount==0)
      {
      done = true;
      }
    //else
    //{
    //  try to merge the ones we just merged
    //}

    toMerge->MergePieceList(merged);
    }

  //add the merged and nonmergable pieces back
  ToDo->MergePieceList(toMerge);
  toMerge->Delete();
  merged->Delete();

  //cerr << "POST REAP:" << endl;
  //cerr << "TODO:" << endl;
  //ToDo->Print();
}

//----------------------------------------------------------------------------
void vtkMultiResolutionStreamer::StartRenderEvent()
{
  DEBUGPRINT_PASSES
    (
     cerr << "SR " << this->Internal->DebugPass << endl;
     );

  vtkRenderer *ren = this->GetRenderer();
  vtkRenderWindow *rw = this->GetRenderWindow();
  if (!ren || !rw)
    {
    return;
    }

  this->Internal->CameraMoved = this->HasCameraMoved();
  if (this->Internal->CameraMoved || this->Internal->WendDone)
    {
    DEBUGPRINT_PASSES
      (
       cerr << "RESTART" << endl;
       this->Internal->DebugPass = 0;
       );

    //show whatever we partially drew before the camera moved
    rw->SwapBuffersOn();
    rw->Frame();

    //start off initial pass by clearing the screen
    ren->EraseOn();
    rw->EraseOn();

    //compute priority of subsequent passes
    //set pipeline to show the most important one this pass
    this->PrepareFirstPass();
    }
  else
    {
    //ask everyone to choose the next piece to draw
    this->ChooseNextPieces();
    }

  //don't swap back to front automatically
  //only update the screen once the last piece is drawn
  rw->SwapBuffersOff(); //comment this out to see each piece rendered

  //assume that we are not done covering all the domains
  this->Internal->WendDone = false;
}

//----------------------------------------------------------------------------
void vtkMultiResolutionStreamer::EndRenderEvent()
{
  DEBUGPRINT_PASSES
    (
     cerr << "ER " << this->Internal->DebugPass << endl;
     this->Internal->DebugPass++;
     );

  vtkRenderer *ren = this->GetRenderer();
  vtkRenderWindow *rw = this->GetRenderWindow();
  if (!ren || !rw)
    {
    return;
    }

  //after first pass all subsequent renders can not clear
  //otherwise we erase the partial results we drew before
  ren->EraseOff();
  rw->EraseOff();

  if (this->IsCompletelyDone())
    {
    DEBUGPRINT_PASSES
      (
       cerr << "ALL DONE" << endl;
       );

    //next pass should start with clear screen
    this->Internal->WendDone = true;

    //bring back buffer forward to show what we drew
    rw->SwapBuffersOn();
    rw->Frame();
    }
  else
    {
    if (this->IsWendDone())// || this->Internal->CameraMoved)
      {
      DEBUGPRINT_PASSES
        (
         cerr << "WEND DONE" << endl;
         );

      //next pass should start with clear screen
      this->Internal->WendDone = true;
      }

    //there is more to draw, so keep going
    this->RenderEventually();
    }
}

//------------------------------------------------------------------------------
void vtkMultiResolutionStreamer::StopStreaming()
{
  //cerr << "STOP STREAMING" << endl;
}

//------------------------------------------------------------------------------
void vtkMultiResolutionStreamer::Refine()
{
  //cerr << "REFINE" << endl;
}

//------------------------------------------------------------------------------
void vtkMultiResolutionStreamer::Coarsen()
{
  //cerr << "COARSEN" << endl;
}
