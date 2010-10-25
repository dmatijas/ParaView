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
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkStreamingDriver.h"
#include "vtkStreamingHarness.h"

vtkStandardNewMacro(vtkPrioritizedStreamer);

class Internals
{
public:
  Internals(vtkPrioritizedStreamer *owner)
  {
    this->Owner = owner;
    this->FirstPass = true;
  }
  ~Internals()
  {
  }
  vtkPrioritizedStreamer *Owner;
  bool FirstPass;
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
bool vtkPrioritizedStreamer::IsFirstPass()
{
  if (this->Internal->FirstPass)
    {
    return true;
    }
  return false;
}

//----------------------------------------------------------------------------
bool vtkPrioritizedStreamer::IsRestart()
{
  //TODO: compare stored last camera with current camera
  return false;
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
    int pieceNow = next->GetPiece();
    int maxPiece = next->GetNumberOfPieces();
    if (pieceNow < maxPiece)
      {
      everyone_done = false;
      break;
      }
    }
  iter->Delete();

  return everyone_done;
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
    vtkStreamingHarness *next = vtkStreamingHarness::SafeDownCast
      (iter->GetCurrentObject());
    next->SetPiece(0);
    iter->GoToNextItem();
    }
  iter->Delete();
}

//----------------------------------------------------------------------------
void vtkPrioritizedStreamer::BumpEveryone()
{
  vtkCollection *harnesses = this->GetHarnesses();
  if (!harnesses)
    {
    return;
    }

  vtkCollectionIterator *iter = harnesses->NewIterator();
  iter->InitTraversal();
  bool everyone_done = true;
  while(!iter->IsDoneWithTraversal())
    {
    vtkStreamingHarness *next = vtkStreamingHarness::SafeDownCast
      (iter->GetCurrentObject());
    iter->GoToNextItem();
    int maxPiece = next->GetNumberOfPieces();
    int pieceNow = next->GetPiece();
    int pieceNext = pieceNow;
    if (pieceNow < maxPiece)
      {
      pieceNext++;
      }
    next->SetPiece(pieceNext);
    }
  iter->Delete();
}

//----------------------------------------------------------------------------
void vtkPrioritizedStreamer::FinalizeEveryone()
{
  return;
}

//----------------------------------------------------------------------------
void vtkPrioritizedStreamer::StartRenderEvent()
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
    //start off by clearing the screen
    ren->EraseOn();
    rw->EraseOn();

    //and telling all the harnesses to get ready
    this->ResetEveryone();

    //don't swap back to front automatically, only show what we've
    //drawn when the entire domain is covered
    rw->SwapBuffersOff();
    //TODO:Except for diagnostic mode where it is helpful to see
    //everything as it is drawn
    }

  this->Internal->FirstPass = false;
}

//----------------------------------------------------------------------------
void vtkPrioritizedStreamer::EndRenderEvent()
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

  this->BumpEveryone();

  if (this->IsEveryoneDone())
    {
    //we just drew the last frame everyone has to start over next time
    this->FinalizeEveryone();

    //bring back buffer forward to show what we drew
    rw->SwapBuffersOn();
    rw->Frame();

    //and mark that the next render should start over
    this->Internal->FirstPass = true;
    }
  else
    {
    //we haven't finished yet so schedule the next pass
    this->RenderEventually();
    }
}
