/*=========================================================================

  Program:   ParaView
  Module:    vtkSMStreamingRepresentation.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkSMStreamingRepresentation - Superclass for representations that
// can stream the display of their inputs.
// .SECTION Description

#ifndef __vtkSMStreamingRepresentation_h
#define __vtkSMStreamingRepresentation_h

#include "vtkSMPVRepresentationProxy.h"

class vtkSMViewProxy;
class vtkInformation;

class VTK_EXPORT vtkSMStreamingRepresentation : 
  public vtkSMPVRepresentationProxy
{
public:
  static vtkSMStreamingRepresentation* New();
  vtkTypeMacro(vtkSMStreamingRepresentation,
    vtkSMPVRepresentationProxy);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Orders the pieces from most to least important based on the data
  // processing pipeline.
  virtual int ComputePipelinePriorities();

  // Description:
  // Orders the pieces from most to least important based on the
  // rendering pipeline.
  virtual int ComputeViewPriorities();

  // Description:
  // Allows skipping of pieces that are already rendered in the cache
  virtual int ComputeCachePriorities();

  // Description:
  // Clears the data object cache in the streaming display pipeline.
  virtual void ClearStreamCache();

  virtual bool AddToView(vtkSMViewProxy *view);

  // Description:
  // Set piece bounds visibility. This flag is considered only if
  // this->GetVisibility() == true, otherwise, cube axes is not shown.
  void SetPieceBoundsVisibility(int);
  vtkGetMacro(PieceBoundsVisibility, int);

  // Description:
  // This tells the display pipeline that a new wend is starting.
  virtual void PrepareFirstPass();

  // Description:
  // This tells the display pipeline that the next piece is starting
  virtual void PrepareAnotherPass();

  // Description:
  // This tells the display pipeline to choose the next most important piece to render.
  virtual void ChooseNextPiece();

  // Description:
  // Obtain state flags that server computes about multipass render progress.
  vtkGetMacro(AllDone, int);
  vtkSetMacro(AllDone, int);

//BTX
  virtual bool UpdateRequired();
  virtual void SetViewInformation(vtkInformation*);
  virtual void SetVisibility(int visible);
  virtual void Update(vtkSMViewProxy* view);
  virtual void SetUpdateTime(double time);
  virtual void SetUseViewUpdateTime(bool);
  virtual void SetViewUpdateTime(double time);

  virtual void SetViewState(double *camera, double *frustum);

protected:
  vtkSMStreamingRepresentation();
  ~vtkSMStreamingRepresentation();

  virtual bool EndCreateVTKObjects();
  virtual bool RemoveFromView(vtkSMViewProxy* view); 

  vtkSMDataRepresentationProxy* PieceBoundsRepresentation;
  int PieceBoundsVisibility;

  int InternalComputePriorities(int type);

  int AllDone;

private:
  vtkSMStreamingRepresentation(const vtkSMStreamingRepresentation&); // Not implemented
  void operator=(const vtkSMStreamingRepresentation&); // Not implemented
//ETX
};

#endif

