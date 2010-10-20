/*=========================================================================

  Program:   ParaView
  Module:    vtkStreamingDriver.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkStreamingDriver.h"

#include "vtkCallbackCommand.h"
#include "vtkCollection.h"
#include "vtkCollectionIterator.h"
#include "vtkObjectFactory.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkStreamingHarness.h"
#include "vtkStreamingProgression.h"

vtkStandardNewMacro(vtkStreamingDriver);

class Internals
{
public:
  Internals(vtkStreamingDriver *owner)
  {
    this->Owner = owner;
    this->Renderer = NULL;
    this->RenderWindow = NULL;
    this->WindowWatcher = NULL;
    this->Progression = vtkStreamingProgression::New();
    this->Harnesses = vtkCollection::New();
    this->WatchRenders = true;
  }
  ~Internals()
  {
  this->Owner->SetRenderer(NULL);
  this->Owner->SetRenderWindow(NULL);
  if (this->WindowWatcher)
    {
    this->WindowWatcher->Delete();
    }
  this->Progression->Delete();
  this->Harnesses->Delete();
  }
  vtkStreamingDriver *Owner;
  vtkStreamingProgression *Progression;
  vtkRenderer *Renderer;
  vtkRenderWindow *RenderWindow;
  vtkCallbackCommand *WindowWatcher;
  vtkCollection *Harnesses;
  bool WatchRenders;
};

static void VTKSD_RenderEvent(vtkObject *vtkNotUsed(caller),
                              unsigned long vtkNotUsed(eventid),
                              void *who,
                              void *)
{
  vtkStreamingDriver *self = reinterpret_cast<vtkStreamingDriver*>(who);
  self->RenderEvent();
}

//----------------------------------------------------------------------------
vtkStreamingDriver::vtkStreamingDriver()
{
  this->Internal = new Internals(this);
}

//----------------------------------------------------------------------------
vtkStreamingDriver::~vtkStreamingDriver()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkStreamingDriver::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
void vtkStreamingDriver::SetRenderWindow(vtkRenderWindow *rw)
{
  if (this->Internal->RenderWindow)
    {
    this->Internal->RenderWindow->Delete();
    }
  if (!rw)
    {
    return;
    }
  rw->Register(this);
  this->Internal->RenderWindow = rw;

  //set up for multipass renders in which we continuously add to the
  //the back buffer until done and then show it
  rw->SwapBuffersOff();

  if (this->Internal->WindowWatcher)
    {
    this->Internal->WindowWatcher->Delete();
    }
  vtkCallbackCommand *cbc = vtkCallbackCommand::New();
  cbc->SetCallback(VTKSD_RenderEvent);
  cbc->SetClientData((void*)this);
  rw->AddObserver(vtkCommand::EndEvent,cbc);
  this->Internal->WindowWatcher = cbc;
}

//----------------------------------------------------------------------------
void vtkStreamingDriver::SetRenderer(vtkRenderer *ren)
{
  if (this->Internal->Renderer)
    {
    this->Internal->Renderer->Delete();
    }
  if (!ren)
    {
    return;
    }
  //set up for multipass renders in which we continuously add to the
  //the back buffer until done and then show it
  ren->EraseOff();

  ren->Register(this);
  this->Internal->Renderer = ren;
}

//----------------------------------------------------------------------------
void vtkStreamingDriver::SetProgression(vtkStreamingProgression *p)
{
  if (this->Internal->Progression)
    {
    this->Internal->Progression->Delete();
    }
  if (!p)
    {
    return;
    }
  p->Register(this);
  this->Internal->Progression = p;
}

//----------------------------------------------------------------------------
void vtkStreamingDriver::RenderEvent()
{
  if (!this->Internal->WatchRenders)
    {
    //if asked to stream directly, don't autostream
    return;
    }

  vtkCollectionIterator *iter = this->Internal->Harnesses->NewIterator();
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

  vtkRenderer *ren = this->Internal->Renderer;
  vtkRenderWindow *rw = this->Internal->RenderWindow;
  if (!ren || !rw)
    {
    return;
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
void vtkStreamingDriver::AddHarness(vtkStreamingHarness *harness)
{
  if (!harness)
    {
    return;
    }
  if (this->Internal->Harnesses->IsItemPresent(harness))
    {
    return;
    }
  this->Internal->Harnesses->AddItem(harness);
}

//----------------------------------------------------------------------------
void vtkStreamingDriver::RemoveHarness(vtkStreamingHarness *harness)
{
  if (!harness)
    {
    return;
    }
  this->Internal->Harnesses->RemoveItem(harness);
}

//----------------------------------------------------------------------------
void vtkStreamingDriver::RemoveAllHarnesses()
{
  this->Internal->Harnesses->RemoveAllItems();
}

//----------------------------------------------------------------------------
void vtkStreamingDriver::Render()
{
  bool wrNow = this->Internal->WatchRenders;
  this->Internal->WatchRenders = false;

  vtkRenderWindow *rw = this->Internal->RenderWindow;
  vtkRenderer *ren = this->Internal->Renderer;
  if (!rw || !ren)
    {
    return;
    }

  //find max number of passes
  vtkCollectionIterator *iter = this->Internal->Harnesses->NewIterator();
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

  this->Internal->WatchRenders = wrNow;
}
