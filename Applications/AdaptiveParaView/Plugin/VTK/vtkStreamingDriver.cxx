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

//TODO:
//consider multiple renderers and non harnesses sources carefully
//consider other UI events
//interface to schedule render callbacks at a later time
//interface to progressor

class Internals
{
public:
  Internals(vtkStreamingDriver *owner)
  {
    this->Owner = owner;
    this->Renderer = NULL;
    this->RenderWindow = NULL;
    this->WindowWatcher = NULL;
    this->Harnesses = vtkCollection::New();
    this->RenderLaterFunction = NULL;
  }
  ~Internals()
  {
  this->Owner->SetRenderer(NULL);
  this->Owner->SetRenderWindow(NULL);
  if (this->WindowWatcher)
    {
    this->WindowWatcher->Delete();
    }
  this->Harnesses->Delete();
  }
  vtkStreamingDriver *Owner;
  vtkRenderer *Renderer;
  vtkRenderWindow *RenderWindow;
  vtkCallbackCommand *WindowWatcher;
  vtkCollection *Harnesses;
  void (*RenderLaterFunction) (void);
};

static void VTKSD_RenderEvent(vtkObject *vtkNotUsed(caller),
                              unsigned long eventid,
                              void *who,
                              void *)
{
  vtkStreamingDriver *self = reinterpret_cast<vtkStreamingDriver*>(who);
  if (eventid == vtkCommand::StartEvent)
    {
    self->StartRenderEvent();
    }
  if (eventid == vtkCommand::EndEvent)
    {
    self->EndRenderEvent();
    }
}

//----------------------------------------------------------------------------
void vtkStreamingDriver::AssignRenderLaterFunction(void (*foo)(void))
{
  this->Internal->RenderLaterFunction = foo;
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

  if (this->Internal->WindowWatcher)
    {
    this->Internal->WindowWatcher->Delete();
    }
  vtkCallbackCommand *cbc = vtkCallbackCommand::New();
  cbc->SetCallback(VTKSD_RenderEvent);
  cbc->SetClientData((void*)this);
  rw->AddObserver(vtkCommand::StartEvent,cbc);
  rw->AddObserver(vtkCommand::EndEvent,cbc);
  this->Internal->WindowWatcher = cbc;
}

//----------------------------------------------------------------------------
vtkRenderWindow *vtkStreamingDriver::GetRenderWindow()
{
  return this->Internal->RenderWindow;
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

  ren->Register(this);
  this->Internal->Renderer = ren;
}

//----------------------------------------------------------------------------
vtkRenderer *vtkStreamingDriver::GetRenderer()
{
  return this->Internal->Renderer;
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
vtkCollection * vtkStreamingDriver::GetHarnesses()
{
  return this->Internal->Harnesses;
}

//----------------------------------------------------------------------------
void vtkStreamingDriver::RenderEventually()
{
  if (this->Internal->RenderLaterFunction)
    {
    this->Internal->RenderLaterFunction();
    return;
    }

  if (this->Internal->RenderWindow)
    {
    this->Internal->RenderWindow->Render();
    }
}
