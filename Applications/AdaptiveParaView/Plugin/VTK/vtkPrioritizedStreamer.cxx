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
#include "vtkPrioritizedStreamer.h"

#include "vtkCollection.h"
#include "vtkCollectionIterator.h"
#include "vtkObjectFactory.h"
#include "vtkPieceList.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkStreamingDriver.h"
#include "vtkStreamingHarness.h"

#define DEBUGPRINT_PASSES( arg ) ;
#define DEBUGPRINT_PRIORITY( arg ) ;

vtkStandardNewMacro(vtkPrioritizedStreamer);

class vtkPrioritizedStreamer::Internals
{
public:
  Internals(vtkPrioritizedStreamer *owner)
  {
    this->Owner = owner;
    this->StartOver = true;
    this->DebugPass = 0;
  }
  ~Internals()
  {
  }

  vtkPrioritizedStreamer *Owner;
  bool StartOver;
  int DebugPass;  //used solely for debug messages
};

//----------------------------------------------------------------------------
vtkPrioritizedStreamer::vtkPrioritizedStreamer()
{
  this->Internal = new Internals(this);
}

//----------------------------------------------------------------------------
vtkPrioritizedStreamer::~vtkPrioritizedStreamer()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkPrioritizedStreamer::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
void vtkPrioritizedStreamer::ResetEveryone()
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
    //get a hold of the pipeline for this object
    vtkStreamingHarness *harness = vtkStreamingHarness::SafeDownCast
      (iter->GetCurrentObject());
    iter->GoToNextItem();

    //make it start over
    harness->SetPass(0);

    //compute pipeline priority to render in most to least important order
    //get a hold of the priority list
    vtkPieceList *pl = harness->GetPieceList1();
    if (!pl)
      {
      //make one if never created before
      pl = vtkPieceList::New();
      harness->SetPieceList1(pl);
      pl->Delete();
      }
    //start off cleanly
    pl->Clear();

    //compute a priority for each piece and sort them
    int max = harness->GetNumberOfPieces();
    for (int i = 0; i < max; i++)
      {
      vtkPiece p;
      p.SetPiece(i);
      p.SetNumPieces(max);
      p.SetResolution(1.0);
      double priority = harness->ComputePriority(i,max,1.0);
      DEBUGPRINT_PRIORITY
        (
         if (!priority)
           {
           cerr << "CHECKED PPRI OF " << i << "/" << max << "@" << 1.0
                << " = " << priority << endl;
           }
         );
      p.SetPipelinePriority(priority);

      double pbbox[6];
      double gConf = 1.0;
      double aMin = 1.0;
      double aMax = -1.0;
      double aConf = 1.0;
      harness->ComputeMetaInformation
        (i, max, 1.0,
         pbbox, gConf, aMin, aMax, aConf);
      double gPri = this->CalculateViewPriority(pbbox);
      DEBUGPRINT_PRIORITY
        (
         if (true || !gPri)
           {
           cerr << "CHECKED VPRI OF " << i << "/" << max << "@" << 1.0
                << " [" << pbbox[0] << "," << pbbox[1] << " "
                << pbbox[2] << "," << pbbox[3] << " "
                << pbbox[4] << "," << pbbox[5] << "]"
                << " = " << gPri << endl;
           }
         );
      p.SetViewPriority(gPri);

      pl->AddPiece(p);
      }
    pl->SortPriorities();
    //pl->Print();

    //convert pass number to absolute piece number
    int pieceNext = pl->GetPiece(0).GetPiece();
    DEBUGPRINT_PRIORITY
      (
       cerr << harness << " P0 PASS " << 0 << " PIECE " << pieceNext << endl;
       );
    harness->SetPiece(pieceNext);
    //this is a hack
    harness->SetPass(-1);
    //but I can't get the first piece to show consistently without it
    }
  iter->Delete();
}

//----------------------------------------------------------------------------
void vtkPrioritizedStreamer::AdvanceEveryone()
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

    //increment to the next pass
    int maxPass = harness->GetNumberOfPieces();
    int passNow = harness->GetPass();
    int passNext = passNow;
    if (passNow < maxPass)
      {
      passNext++;
      }
    harness->SetPass(passNext);

    //map that to an absolute piece number, but don't got beyond end
    vtkPieceList *pl = harness->GetPieceList1();
    double priority = pl->GetPiece(passNext).GetPriority();
    if (priority)
      {
      int pieceNext = pl->GetPiece(passNext).GetPiece();
      DEBUGPRINT_PRIORITY
        (
         cerr <<harness<<" NP PASS "<<passNext<<" PIECE "<<pieceNext<<endl;
         );
      harness->SetPiece(pieceNext);
      }
   }

  iter->Delete();
}

//----------------------------------------------------------------------------
bool vtkPrioritizedStreamer::IsEveryoneDone()
{
  vtkCollection *harnesses = this->GetHarnesses();
  if (!harnesses)
    {
    return true;
    }

  bool everyone_done = true;
  vtkCollectionIterator *iter = harnesses->NewIterator();
  iter->InitTraversal();
  while(!iter->IsDoneWithTraversal())
    {
    vtkStreamingHarness *next = vtkStreamingHarness::SafeDownCast
      (iter->GetCurrentObject());
    iter->GoToNextItem();

    //check if anyone hasn't drawn the last necessary pass
    int passNow = next->GetPass();
    int maxPass = next->GetNumberOfPieces();
    if (passNow <= maxPass-2)
      {
      vtkPieceList *pl = next->GetPieceList1();
      if (pl)
        {
        double priority = pl->GetPiece(passNow+1).GetPriority();
        if (priority == 0.0)
          {
          //if this object finished early, don't keep going just for it's sake
          continue;
          }
        }
      everyone_done = false;
      break;
      }
    }
  iter->Delete();

  return everyone_done;
}

//----------------------------------------------------------------------------
void vtkPrioritizedStreamer::StartRenderEvent()
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

  if (this->HasCameraMoved() || this->Internal->StartOver)
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
    this->ResetEveryone();
    }
  else
    {
    //subsequent passes pick next less important piece each pass
    this->AdvanceEveryone();
    }

  //don't swap back to front automatically
  //only update the screen once the last piece is drawn
  rw->SwapBuffersOff(); //comment this out to see each piece rendered

  //assume that we are not done covering all the domains
  this->Internal->StartOver = false;
}

//----------------------------------------------------------------------------
void vtkPrioritizedStreamer::EndRenderEvent()
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

  //don't clear the screen, keep adding to what we already drew
  ren->EraseOff();
  rw->EraseOff();

  if (this->IsEveryoneDone()) //stop when there are no more non culled pieces
    {
    DEBUGPRINT_PASSES
      (
       cerr << "ALL DONE" << endl;
       );

    //we just drew the last frame, everyone has to start over next time
    this->Internal->StartOver = true;

    //bring back buffer forward to show what we drew
    rw->SwapBuffersOn();
    rw->Frame();
    }
  else
    {
    //we haven't finished yet so schedule the next pass
    this->RenderEventually();
    }
}
