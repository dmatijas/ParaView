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
#include "vtkObjectFactory.h"
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
    this->RenderWindow = NULL;
    this->WindowWatcher = NULL;
    this->Progression = vtkStreamingProgression::New();
    this->Harnesses = vtkCollection::New();
  }
  ~Internals()
  {
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
  vtkRenderWindow *RenderWindow;
  vtkCallbackCommand *WindowWatcher;
  vtkCollection *Harnesses;
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
  //TODO: Intialize swap off

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
  cerr << "RENDER CALLED" << endl;
  //TODO:
  //for every harness, set to next piece
  //call render again
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
  //TODO: Initialize harnesses to first piece
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
