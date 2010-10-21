/*=========================================================================

  Program:   ParaView
  Module:    vtkIterativeStreamer.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkIterativeStreamer.h"

#include "vtkCollection.h"
#include "vtkCollectionIterator.h"
#include "vtkObjectFactory.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkStreamingDriver.h"
#include "vtkStreamingHarness.h"

vtkStandardNewMacro(vtkIterativeStreamer);

class Internals
{
public:
  Internals(vtkIterativeStreamer *owner)
  {
    this->Owner = owner;
  }
  ~Internals()
  {
  }
  vtkIterativeStreamer *Owner;
};

//----------------------------------------------------------------------------
vtkIterativeStreamer::vtkIterativeStreamer()
{
  this->Internal = new Internals(this);
}

//----------------------------------------------------------------------------
vtkIterativeStreamer::~vtkIterativeStreamer()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkIterativeStreamer::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
void vtkIterativeStreamer::RenderEventInternal()
{
  vtkCollection *harnesses = this->GetHarnesses();
  vtkRenderer *ren = this->GetRenderer();
  vtkRenderWindow *rw = this->GetRenderWindow();
  if (!harnesses || !ren || !rw)
    {
    return;
    }

  vtkCollectionIterator *iter = harnesses->NewIterator();
  iter->InitTraversal();
  bool someone_not_ready_to_flip = false;
  bool someone_not_done = false;
  while(!iter->IsDoneWithTraversal())
    {
    vtkStreamingHarness *next = vtkStreamingHarness::SafeDownCast
      (iter->GetCurrentObject());
    iter->GoToNextItem();
    int maxPiece = next->GetNumberOfPieces();
    int pieceNow = next->GetPiece();

    if (pieceNow+2 < maxPiece)
      {
      someone_not_ready_to_flip = true;
      }

    int pieceNext = pieceNow;
    if (pieceNow+1 < maxPiece)
      {
      someone_not_done = true;
      pieceNext++;
      }
    next->SetPiece(pieceNext);
    }

  if (!someone_not_ready_to_flip && someone_not_done)
    {
    //we are at next to last frame, the next frame (last one) has to swap
    //b2f to show what's been drawn
    rw->SwapBuffersOn();
    }
  else
    {
    //don't allow swapping otherwise so we can keep adding to image without
    //showing progress
    rw->SwapBuffersOff();
    }
  if (!someone_not_done)
    {
    //we just drew the last frame, next frame should start by clearing
    ren->EraseOn();
    iter->InitTraversal();
    //and everyone should start over
    while(!iter->IsDoneWithTraversal())
      {
      vtkStreamingHarness *next = vtkStreamingHarness::SafeDownCast
        (iter->GetCurrentObject());
      next->SetPiece(0);
      iter->GoToNextItem();
      }
    }
  else
    {
    ren->EraseOff();
    //TODO: In a GUI, this should be scheduled at a future point in time
    //rather than called (recursively) here.
    rw->Render();
    }
}

//----------------------------------------------------------------------------
void vtkIterativeStreamer::RenderInternal()
{
  vtkRenderer *ren = this->GetRenderer();
  vtkRenderWindow *rw = this->GetRenderWindow();
  vtkCollection *harnesses = this->GetHarnesses();
  if (!harnesses || !rw || !ren)
    {
    return;
    }

  //find max number of passes
  vtkCollectionIterator *iter = harnesses->NewIterator();
  iter->InitTraversal();
  int maxpieces = -1;
  while(!iter->IsDoneWithTraversal())
    {
    vtkStreamingHarness *next = vtkStreamingHarness::SafeDownCast
      (iter->GetCurrentObject());
    iter->GoToNextItem();
    int maxpiece = next->GetNumberOfPieces();
    if (maxpiece > maxpieces)
      {
      maxpieces = maxpiece;
      }
    }

  for (int i = 0; i < maxpieces; i++)
    {
    iter->InitTraversal();
    cerr << "RENDER " << i << endl;
    while(!iter->IsDoneWithTraversal())
      {
      vtkStreamingHarness *next = vtkStreamingHarness::SafeDownCast
        (iter->GetCurrentObject());
      iter->GoToNextItem();
      if (i < next->GetNumberOfPieces())
        {
        next->SetPiece(i);
        }
      }
    if (i == 0)
      {
      cerr << "FIRST" << endl;
      //first pass has to begin by clearing background
      ren->EraseOn();
      rw->SwapBuffersOff();
      }
    else
      {
      //rest can not clear
      ren->EraseOff();
      }
    if (i == maxpieces-1)
      {
      cerr << "next to last" << endl;
      rw->SwapBuffersOn();
      }
    rw->Render();
    }
}
