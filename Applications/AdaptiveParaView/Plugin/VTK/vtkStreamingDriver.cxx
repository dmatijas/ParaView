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
#include "vtkCamera.h"
#include "vtkCollection.h"
#include "vtkCollectionIterator.h"
#include "vtkInteractorStyle.h"
#include "vtkObjectFactory.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkStreamingHarness.h"
#include "vtkVisibilityPrioritizer.h"

//TODO:
//consider multiple renderers and non harnessed sources carefully
//consider other UI events
//interface to schedule render callbacks at a later time
//interface to progressor

class vtkStreamingDriver::Internals
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
    this->RenderLaterArgument = NULL;

    //auxilliary functionality, that help view sorting sublasses
    this->ViewSorter = vtkVisibilityPrioritizer::New();
    this->CameraTime = 0;
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
  this->ViewSorter->Delete();
  }

  vtkStreamingDriver *Owner;
  vtkRenderer *Renderer;
  vtkRenderWindow *RenderWindow;
  vtkCallbackCommand *WindowWatcher;
  vtkCollection *Harnesses;
  void (*RenderLaterFunction) (void *);
  void *RenderLaterArgument;

  //auxilliary functionality, that help view sorting sublasses
  vtkVisibilityPrioritizer *ViewSorter;
  unsigned long CameraTime;
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
void vtkStreamingDriver::AssignRenderLaterFunction(void (*foo)(void *),
                                                   void*bar)
{
  this->Internal->RenderLaterFunction = foo;
  this->Internal->RenderLaterArgument = bar;
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
  this->Internal->RenderWindow = rw;
  if (!rw)
    {
    return;
    }
  rw->Register(this);
  vtkRenderWindowInteractor *iren = rw->GetInteractor();
  if(iren)
    {
    vtkInteractorStyle *istyle = vtkInteractorStyle::SafeDownCast
      (iren->GetInteractorStyle());
    if (istyle)
      {
      //cerr << "SET OFF" << endl;
      istyle->AutoAdjustCameraClippingRangeOff();
      }
    }

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
    this->Internal->RenderLaterFunction
      (this->Internal->RenderLaterArgument);
    return;
    }

  if (this->Internal->RenderWindow)
    {
    this->Internal->RenderWindow->Render();
    }
}

//----------------------------------------------------------------------------
bool vtkStreamingDriver::IsRestart()
{
  vtkRenderer *ren = this->GetRenderer();
  if (ren)
    {
    vtkCamera * cam = ren->GetActiveCamera();
    if (cam)
      {
      unsigned long mtime = cam->GetMTime();
      if (mtime > this->Internal->CameraTime)
        {
        this->Internal->CameraTime = mtime;

        double camState[9];
        cam->GetPosition(&camState[0]);
        cam->GetViewUp(&camState[3]);
        cam->GetFocalPoint(&camState[6]);

        this->Internal->ViewSorter->SetCameraState(camState);

        //convert screen rectangle to world frustum
        const double HALFEXT=1.0; /*1.0 means all way to edge of screen*/
        const double XMAX=HALFEXT;
        const double XMIN=-HALFEXT;
        const double YMAX=HALFEXT;
        const double YMIN=-HALFEXT;
        const double viewP[32] = {
          XMIN, YMIN,  0.0, 1.0,
          XMIN, YMIN,  1.0, 1.0,
          XMIN, YMAX,  0.0, 1.0,
          XMIN, YMAX,  1.0, 1.0,
          XMAX, YMIN,  0.0, 1.0,
          XMAX, YMIN,  1.0, 1.0,
          XMAX, YMAX,  0.0, 1.0,
          XMAX, YMAX,  1.0, 1.0
        };

        double frust[32];
        memcpy(frust, viewP, 32*sizeof(double));
        for (int index=0; index<8; index++)
          {
          ren->ViewToWorld(frust[index*4+0],
                           frust[index*4+1],
                           frust[index*4+2]);
          }

        this->Internal->ViewSorter->SetFrustum(frust);
        return true;
        }
      }
    }

  return false;
}

//------------------------------------------------------------------------------
double vtkStreamingDriver::CalculateViewPriority(double *pbbox)
{
  return this->Internal->ViewSorter->CalculatePriority(pbbox);
}
