/*=========================================================================

  Program:   ParaView
  Module:    vtkSMStreamingViewProxy.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkSMStreamingViewProxy.h"
#include "vtkSMStreamingOptionsProxy.h"
#include "vtkStreamingOptions.h"
#include "vtkCamera.h"
#include "vtkClientServerStream.h"
#include "vtkCollectionIterator.h"
#include "vtkImageData.h"
#include "vtkObjectFactory.h"
#include "vtkProcessModule.h"
#include "vtkPVGenericRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkSmartPointer.h"
#include "vtkSMInputProperty.h"
#include "vtkSMIntVectorProperty.h"
//#include "vtkSMPropertyHelper.h"
#include "vtkSMProxyManager.h"
#include "vtkSMRenderViewProxy.h"
#include "vtkSMRepresentationStrategy.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMStreamingRepresentation.h"
#include "vtkSMStreamingViewHelper.h"
#include "vtkSMUtilities.h"
#include "vtkTimerLog.h"
#include "vtkUnsignedCharArray.h"
#include "vtkWindowToImageFilter.h"

#include <vtkstd/vector>
#include <vtkstd/string>
#include <vtksys/ios/sstream>

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSMStreamingViewProxy);

#define DEBUGPRINT_VIEW(arg) arg;
/*
  if (vtkStreamingOptions::GetEnableStreamMessages()) \
    { \
      arg;\
    }
*/
//-----------------------------------------------------------------------------
class vtkSMStreamingViewProxy::vtkInternals
{
public:
  vtkInternals()
  {
    for (int i =0; i < 9; i++)
      {
      this->CamState[i] = 0.0;
      }
  }

  vtkstd::vector<vtkSMStreamingRepresentation*> Representations;
  vtkSmartPointer<vtkSMRenderViewProxy> RootView;
  double CamState[9];
  double Frustum[32];
  vtkstd::string SuggestedViewType;
};

//-----------------------------------------------------------------------------
vtkSMStreamingViewProxy::vtkSMStreamingViewProxy()
{
  this->Internals = new vtkInternals();

  this->Pass = 0;
  this->DisplayDone = 1;

  this->PixelArray = NULL;

  this->RenderViewHelper = vtkSMStreamingViewHelper::New();
  this->RenderViewHelper->SetStreamingView(this); //not reference counted.

  this->IsSerial = true;

  // Make sure the helper proxy exists
  this->GetStreamingOptionsProxy();
}

//-----------------------------------------------------------------------------
vtkSMStreamingViewProxy::~vtkSMStreamingViewProxy()
{
  this->RenderViewHelper->SetStreamingView(0);
  this->RenderViewHelper->Delete();
  if (this->PixelArray)
    {
    this->PixelArray->Delete();
    }
  delete this->Internals;
}

//-----------------------------------------------------------------------------
vtkSMStreamingOptionsProxy* vtkSMStreamingViewProxy::GetStreamingOptionsProxy()
{
  return vtkSMStreamingOptionsProxy::GetProxy();
}

//STUFF TO MAKE THIS VIEW PRETEND TO BE A RENDERVIEW
//-----------------------------------------------------------------------------
bool vtkSMStreamingViewProxy::BeginCreateVTKObjects()
{
  this->Internals->RootView = vtkSMRenderViewProxy::SafeDownCast(
    this->GetSubProxy("RootView"));
  if (!this->Internals->RootView)
    {
    vtkErrorMacro("Subproxy \"Root\" must be defined in the xml configuration.");
    return false;
    }

  if (!strcmp("StreamingRenderView", this->GetXMLName()))
    {
    DEBUGPRINT_VIEW(cerr << "SV(" << this << ") Created serial view" << endl;);
    this->IsSerial = true;
    }
  else
    {
    DEBUGPRINT_VIEW(cerr << "SV(" << this << ") Created parallel view type " << this->GetXMLName() << endl;);
    this->IsSerial = false;
    }

  return this->Superclass::BeginCreateVTKObjects();
}

//-----------------------------------------------------------------------------
void vtkSMStreamingViewProxy::EndCreateVTKObjects()
{
  this->Superclass::EndCreateVTKObjects();
  this->Internals->RootView->SetLODThreshold(1000000);

  //replace the real view's interactor, which points to the real view
  //with one that points to this so that mouse events will result in streaming
  //renders
  vtkPVGenericRenderWindowInteractor *iren =
    this->Internals->RootView->GetInteractor();
  iren->SetPVRenderView(this->RenderViewHelper);

  //turn off the axes widgets by default
  vtkSMIntVectorProperty *vis;
  vtkSMProxy *annotation;
  annotation = this->Internals->RootView->GetOrientationWidgetProxy();
  vis = vtkSMIntVectorProperty::SafeDownCast(annotation->GetProperty("Visibility"));
  vis->SetElement(0,0);
  annotation = this->Internals->RootView->GetCenterAxesProxy();
  vis = vtkSMIntVectorProperty::SafeDownCast(annotation->GetProperty("Visibility"));
  vis->SetElement(0,0);

  this->UpdateVTKObjects();
}

//-----------------------------------------------------------------------------
vtkSMRenderViewProxy *vtkSMStreamingViewProxy::GetRootView()
{
  return this->Internals->RootView;
}

//----------------------------------------------------------------------------
void vtkSMStreamingViewProxy::AddRepresentation(vtkSMRepresentationProxy* rep)
{
  vtkSMStreamingRepresentation *repr =
    vtkSMStreamingRepresentation::SafeDownCast(rep);
  if (!repr)
    {
    //quietly ignore
    return;
    }

  vtkSMViewProxy* RVP = this->GetRootView();
  if (repr && !RVP->Representations->IsItemPresent(repr))
    {
    //There is magic inside AddToView, such that it actually adds rep to RVP,
    //but uses this to to create repr's streaming strategy.
    if (repr->AddToView(this))
      {
      RVP->AddRepresentationInternal(repr);
      }
    else
      {
      vtkErrorMacro(<< repr->GetClassName() << " cannot be added to view "
        << "of type " << this->GetClassName());
      }
    }
}

//----------------------------------------------------------------------------
void vtkSMStreamingViewProxy::RemoveRepresentation(vtkSMRepresentationProxy* repr)
{
  this->GetRootView()->RemoveRepresentation(repr);
}

//----------------------------------------------------------------------------
void vtkSMStreamingViewProxy::RemoveAllRepresentations()
{
  this->GetRootView()->RemoveAllRepresentations();
}

//----------------------------------------------------------------------------
void vtkSMStreamingViewProxy::StillRender()
{
  static bool in_still_render = false;
  if (in_still_render)
    {
    return;
    }

  in_still_render = true;

  this->BeginStillRender();
  this->GetRootView()->BeginStillRender();

  this->PrepareRenderPass();
  this->UpdateAllRepresentations();
  this->PerformRender();
  this->FinalizeRenderPass();

  this->GetRootView()->EndStillRender();
  this->EndStillRender();
  in_still_render = false;
}

//----------------------------------------------------------------------------
void vtkSMStreamingViewProxy::InteractiveRender()
{
  //a hack to disable LOD
  this->Internals->RootView->SetLODThreshold(1000000);

  static bool in_interactive_render = false;
  if (in_interactive_render)
    {
    return;
    }

  in_interactive_render = true;

  this->BeginInteractiveRender();
  this->GetRootView()->BeginInteractiveRender();

  this->PrepareRenderPass();
  this->UpdateAllRepresentations();
  this->PerformRender();
  this->FinalizeRenderPass();

  this->GetRootView()->EndInteractiveRender();
  this->EndInteractiveRender();
  in_interactive_render = false;
}

//-----------------------------------------------------------------------------
vtkImageData* vtkSMStreamingViewProxy::CaptureWindow(int magnification)
{
  vtkRenderWindow *renWin = this->GetRootView()->GetRenderWindow();

  vtkWindowToImageFilter* w2i = vtkWindowToImageFilter::New();
  w2i->SetInput(renWin);
  w2i->SetMagnification(magnification);
  w2i->ReadFrontBufferOn();
  w2i->ShouldRerenderOff();
  w2i->Update();

  vtkImageData* capture = vtkImageData::New();
  capture->ShallowCopy(w2i->GetOutput());
  w2i->Delete();

  // Update image extents based on ViewPosition
  int extents[6];
  capture->GetExtent(extents);
  for (int cc=0; cc < 4; cc++)
    {
    extents[cc] += this->ViewPosition[cc/2]*magnification;
    }
  capture->SetExtent(extents);

  return capture;
}

//-----------------------------------------------------------------------------
int vtkSMStreamingViewProxy::WriteImage(const char* filename, int magnification)
{
  vtkSmartPointer<vtkImageData> shot;
  shot.TakeReference(this->CaptureWindow(magnification));
  return vtkSMUtilities::SaveImage(shot, filename);
}

//STUFF TO MAKE A PLUGIN VIEW WITH SPECIALIZED STREAMING REPS and STRATS
//-----------------------------------------------------------------------------
vtkSMRepresentationProxy* vtkSMStreamingViewProxy::CreateDefaultRepresentation(
  vtkSMProxy* source, int opport)
{
  if (!source)
    {
    return 0;
    }

  DEBUGPRINT_VIEW(
    cerr << "SV(" << this << ") CreateDefaultRepresentation" << endl;
    );

  vtkSMProxyManager* pxm = vtkSMProxyManager::GetProxyManager();

  // Update with time to avoid domains updating without time later.
  vtkSMSourceProxy* sproxy = vtkSMSourceProxy::SafeDownCast(source);
  if (sproxy)
    {
    sproxy->UpdatePipeline(this->GetViewUpdateTime());
    }

  // Choose which type of representation proxy to create.
  vtkSMProxy* prototype = pxm->GetPrototypeProxy("representations",
    "StreamingGeometryRepresentation");
  vtkSMInputProperty *pp = vtkSMInputProperty::SafeDownCast(
    prototype->GetProperty("Input"));
  pp->RemoveAllUncheckedProxies();
  pp->AddUncheckedInputConnection(source, opport);
  bool g = (pp->IsInDomains()>0);
  pp->RemoveAllUncheckedProxies();
  if (g)
    {
    DEBUGPRINT_VIEW(
      cerr << "SV(" << this << ") Created StreamingGeometryRepresentation" << endl;
      );
    return vtkSMRepresentationProxy::SafeDownCast(
      pxm->NewProxy("representations", "StreamingGeometryRepresentation"));
    }

  return 0;
}

//-----------------------------------------------------------------------------
vtkSMRepresentationStrategy* vtkSMStreamingViewProxy::NewStrategyInternal(
  int dataType)
{
  vtkSMProxyManager* pxm = vtkSMObject::GetProxyManager();
  vtkSMRepresentationStrategy* strategy = 0;

  if (this->IsSerial)
    {
    if (dataType == VTK_POLY_DATA || dataType == VTK_UNIFORM_GRID ||
        dataType == VTK_IMAGE_DATA || dataType == VTK_UNSTRUCTURED_GRID )
      {
      DEBUGPRINT_VIEW(
        cerr << "SV(" << this << ") Creating StreamingStrategy" << endl;
        );
      strategy = vtkSMRepresentationStrategy::SafeDownCast(
        pxm->NewProxy("strategies", "StreamingPolyDataStrategy"));
      }
    else
      {
      vtkWarningMacro("This view does not provide a suitable strategy for "
                      << dataType);
      }
    }
  else
    {
    if (dataType == VTK_POLY_DATA || dataType == VTK_UNIFORM_GRID ||
        dataType == VTK_IMAGE_DATA || dataType == VTK_UNSTRUCTURED_GRID )
      {
      DEBUGPRINT_VIEW(
        cerr << "SV(" << this << ") Creating StreamingParallelStrategy" << endl;
        );
      strategy = vtkSMRepresentationStrategy::SafeDownCast(
         pxm->NewProxy("strategies", "StreamingParallelStrategy"));
      }
    else
      {
      vtkWarningMacro("This view does not provide a suitable strategy for "
                      << dataType);
      }
    }

  vtkSMIntVectorProperty *ivp = vtkSMIntVectorProperty::SafeDownCast(
    strategy->GetProperty("SetNumberOfPasses"));
  if (ivp)
    {
    int nPasses = vtkStreamingOptions::GetStreamedPasses();
    ivp->SetElement(0,nPasses);
    }

  return strategy;
}

#define ForEveryStreamingRep(command)\
{\
vtkSmartPointer<vtkCollectionIterator> iter;\
iter.TakeReference(this->GetRootView()->Representations->NewIterator());\
for (iter->InitTraversal();\
     !iter->IsDoneWithTraversal();\
     iter->GoToNextItem())\
  {\
  vtkSMStreamingRepresentation* srep =\
    vtkSMStreamingRepresentation::SafeDownCast(iter->GetCurrentObject());\
  if (srep && srep->GetVisibility())\
    {\
    command;\
    }\
  }\
}\

//STUFF THAT ACTUALLY MATTERS FOR MULTIPASS RENDERING
//----------------------------------------------------------------------------
void vtkSMStreamingViewProxy::PrepareRenderPass()
{
  static bool firstpass=true;
  vtkRenderWindow *renWin = this->GetRootView()->GetRenderWindow();
  vtkRenderer *ren = this->GetRootView()->GetRenderer();

  bool CamChanged = this->CameraChanged();
  if (CamChanged)
    {
    this->Pass = 0;
    }

  if (this->Pass == 0)
    {
    //prepare for incremental rendering

    if (firstpass)
      {
      //force a render, just to make sure we have a graphics context
      renWin->Render();
      firstpass = false;

      //Setup to for multi pass render process
      //don't cls automatically whenever renderwindow->render() starts
      renWin->EraseOff();
      ren->EraseOff();
      //don't swap back to front automatically whenever renderwindow->render() finishes
      renWin->SwapBuffersOff();
      }

    ren->Clear();

    if (CamChanged)
      {
      DEBUGPRINT_VIEW(
      cerr << "SV(" << this << ") compute VIEW priority" << endl;
                      )
      ForEveryStreamingRep(
                           srep->SetViewState(this->Internals->CamState, this->Internals->Frustum);
                           srep->ComputeViewPriorities();
                           );
      }

    DEBUGPRINT_VIEW(
    cerr << "SV(" << this << ") compute CACHE priority" << endl;
                    )
    ForEveryStreamingRep(
                         srep->SetViewState(this->Internals->CamState, this->Internals->Frustum);
                         srep->ComputeCachePriorities();
                         );

    DEBUGPRINT_VIEW(
    cerr << "SV(" << this << ") Prepare first pass" << endl;
                    )
    ForEveryStreamingRep(
                         srep->PrepareFirstPass();
                         );
    }
  else
    {
    //continue incremental rendering

    DEBUGPRINT_VIEW(
    cerr << "SV(" << this << ") Prepare next pass" << endl;
                    )

    ForEveryStreamingRep(
                         srep->PrepareAnotherPass();
                         );
    }

  this->DisplayDone = 0;
}

//----------------------------------------------------------------------------
void vtkSMStreamingViewProxy::UpdateAllRepresentations()
{
  vtkSMRenderViewProxy *RVP = this->GetRootView();
  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();

  DEBUGPRINT_VIEW(
    cerr << "SV(" << this << ")::UpdateAllRepresentations" << endl;
    );

  //Update pipeline for each representation.
  //For representations that allow streaming, compute a piece priority order
  vtkSmartPointer<vtkCollectionIterator> iter;
  iter.TakeReference(RVP->Representations->NewIterator());
  bool enable_progress = false;
  for (iter->InitTraversal();
       !iter->IsDoneWithTraversal();
       iter->GoToNextItem())
    {
    vtkSMRepresentationProxy* repr =
      vtkSMRepresentationProxy::SafeDownCast(iter->GetCurrentObject());
    if (!repr->GetVisibility())
      {
      // Invisible representations are not updated.
      continue;
      }

    if (!enable_progress && repr->UpdateRequired())
      {
      // If a representation required an update, than it implies that the
      // update will result in progress events. We don't want to ignore
      // those progress events, hence we enable progress handling.
      pm->SendPrepareProgress(this->ConnectionID,
        vtkProcessModule::CLIENT | vtkProcessModule::DATA_SERVER);
      enable_progress = true;
      }

    vtkSMStreamingRepresentation* srep =
      vtkSMStreamingRepresentation::SafeDownCast(repr);
    if (srep)
      {
      if (!srep->GetAllDone())
        {
        srep->ChooseNextPiece();
        }
      }
    else
      {
      if (this->Pass == 0)
        {
        repr->Update(RVP);
        }
      }
    }

  if (enable_progress)
    {
    pm->SendCleanupPendingProgress(this->ConnectionID);
    }
}

//-----------------------------------------------------------------------------
void vtkSMStreamingViewProxy::PerformRender()
{
  DEBUGPRINT_VIEW(
    cerr << "SV(" << this << ")::PerformRender" << endl;
    );

  vtkSMRenderViewProxy *RVP = this->GetRootView();
  if ( RVP->GetMeasurePolygonsPerSecond() )
    {
    this->RenderTimer->StartTimer();
    }

  vtkSMProxy *RWProxy = RVP->GetRenderWindowProxy();
  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke
         << RWProxy->GetID()
         << "Render"
         << vtkClientServerStream::End;
  vtkProcessModule::GetProcessModule()->SendStream(
    this->ConnectionID,
    vtkProcessModule::CLIENT,
    stream);

  if ( RVP->GetMeasurePolygonsPerSecond() )
    {
    this->RenderTimer->StopTimer();
    RVP->CalculatePolygonsPerSecond(this->RenderTimer->GetElapsedTime());
    }
}

//-----------------------------------------------------------------------------
int vtkSMStreamingViewProxy::GetDisplayDone()
{
  return this->DisplayDone;
}

//-----------------------------------------------------------------------------
void vtkSMStreamingViewProxy::CopyBackBufferToFrontBuffer()
{
  vtkRenderWindow *renWin = this->GetRootView()->GetRenderWindow();

  //allocate pixel storage
  int *size = renWin->GetSize();
  if (!this->PixelArray)
    {
    this->PixelArray = vtkUnsignedCharArray::New();
    }
  this->PixelArray->Initialize();
  this->PixelArray->SetNumberOfComponents(4);
  this->PixelArray->SetNumberOfTuples(size[0]*size[1]);

  //capture back buffer
  renWin->GetRGBACharPixelData(0, 0, size[0]-1, size[1]-1, 0, this->PixelArray);

  //copy into the front buffer
  renWin->SetRGBACharPixelData(0, 0, size[0]-1, size[1]-1, this->PixelArray, 1);
}

//----------------------------------------------------------------------------
void vtkSMStreamingViewProxy::FinalizeRenderPass()
{
  DEBUGPRINT_VIEW(
                   cerr << "SV(" << this << ") FinalizeRenderPass" << endl;
                   );

  //we are done if every representation is done
  int alldone = 1;
  ForEveryStreamingRep(
                       int rAllDone = srep->GetAllDone();
                       DEBUGPRINT_VIEW(
                       if (!rAllDone)
                         {
                         cerr << "SV(" << this << ") got NOT done " << endl;
                         }
                                       );
                       alldone &= rAllDone;
                       );

  if (alldone)
    {
    DEBUGPRINT_VIEW(cerr<<"SV("<<this<<") everone DONE" << endl;);
    this->DisplayDone = 1;
    }

  //or if we are at the maximum pass count
  int lastPass = vtkStreamingOptions::GetPieceRenderCutoff();
  if (lastPass > -1 && lastPass < this->Pass)
    {
    DEBUGPRINT_VIEW(
    cerr << "SV(" << this << ") early exit" << endl;
                    );
    this->DisplayDone = 1;
    }

  //once done, get reset for the next wend
  if (this->DisplayDone)
    {
    this->Pass = 0;
    }
  else
    {
    this->Pass++;
    }

  vtkRenderWindow *renWin = this->GetRootView()->GetRenderWindow();

  this->CopyBackBufferToFrontBuffer();
  renWin->SwapBuffersOn();
  renWin->Frame();
  renWin->SwapBuffersOff();

//  cerr << "---------------------<any key>----------------------" << endl;
//  vtkstd::string s;
//  cin >> s;
}

//-----------------------------------------------------------------------------
void vtkSMStreamingViewProxy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//-----------------------------------------------------------------------------
bool vtkSMStreamingViewProxy::CameraChanged()
{
  bool changed = false;
  vtkSMRenderViewProxy *RVP = this->GetRootView();
  vtkCamera *cam = RVP->GetActiveCamera();
  if (cam)
    {
    double camState[9];
    cam->GetPosition(&camState[0]);
    cam->GetViewUp(&camState[3]);
    cam->GetFocalPoint(&camState[6]);
    for (int i = 0; i < 9; i++)
      {
      if (camState[i] != this->Internals->CamState[i])
        {
        changed = true;
        break;
        }
      }
    memcpy(this->Internals->CamState, camState, 9*sizeof(double));

    if (changed)
      {
      vtkRenderer *renderer = RVP->GetRenderer();
      //convert screen rectangle to world frustum
      const double XMAX=1.0;
      const double XMIN=-1.0;
      const double YMAX=1.0;
      const double YMIN=-1.0;
      static double viewP[32] = {
        XMIN, YMIN,  0.0, 1.0,
        XMIN, YMIN,  1.0, 1.0,
        XMIN,  YMAX,  0.0, 1.0,
        XMIN,  YMAX,  1.0, 1.0,
        XMAX, YMIN,  0.0, 1.0,
        XMAX, YMIN,  1.0, 1.0,
        XMAX,  YMAX,  0.0, 1.0,
        XMAX,  YMAX,  1.0, 1.0
        };
      memcpy(this->Internals->Frustum, viewP, 32*sizeof(double));
      for (int index=0; index<8; index++)
        {
        renderer->ViewToWorld(this->Internals->Frustum[index*4+0],
                              this->Internals->Frustum[index*4+1],
                              this->Internals->Frustum[index*4+2]);
        }
      }
    }
  return changed;
}

//----------------------------------------------------------------------------
const char* vtkSMStreamingViewProxy::GetSuggestedViewType(vtkIdType connectionID)
{
  vtkSMViewProxy* rootView = vtkSMViewProxy::SafeDownCast(this->GetSubProxy("RootView"));
  if (rootView)
    {
    vtksys_ios::ostringstream stream;
    stream << "Streaming" << rootView->GetSuggestedViewType(connectionID);
    this->Internals->SuggestedViewType = stream.str();
    return this->Internals->SuggestedViewType.c_str();
    }

  return this->Superclass::GetSuggestedViewType(connectionID);
}
