/*=========================================================================

  Program:   ParaView
  Module:    $RCSfile$

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkSMStreamingParallelStrategy
// .SECTION Description

#ifndef __vtkSMStreamingParallelStrategy_h
#define __vtkSMStreamingParallelStrategy_h

#include "vtkSMSimpleParallelStrategy.h"

class VTK_EXPORT vtkSMStreamingParallelStrategy
: public vtkSMSimpleParallelStrategy
{
public:
  static vtkSMStreamingParallelStrategy* New();
  vtkTypeMacro(vtkSMStreamingParallelStrategy,
               vtkSMSimpleParallelStrategy);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Tells server side to work with a particular piece until further notice.
  virtual void SetPassNumber(int Pass, int force);
  // Description:
  // Orders the pieces from most to least important.
  virtual int ComputePriorities();

  // Description:
  // Clears the data object cache in the streaming display pipeline.
  virtual void ClearStreamCache();
  // Description:
  // Tells the strategy where the camera is so that pieces can be sorted and rejected
  virtual void SetViewState(double *camera, double *frustum);

//BTX
protected:
  vtkSMStreamingParallelStrategy();
  ~vtkSMStreamingParallelStrategy();

  // Description:
  // Overridden to swap in StreamingUpdateSupressors and add the PieceCache.
  virtual void BeginCreateVTKObjects();

  // Description:
  // Overridden to insert piececache in front of render pipeline.
  virtual void CreatePipeline(vtkSMSourceProxy* input, int outputport);

  // Description:
  // Copies ordered piece list from one UpdateSupressor to the other.
  virtual void CopyPieceList(vtkClientServerStream *stream,
                             vtkSMSourceProxy *src,
                             vtkSMSourceProxy *dest);

  // Description:
  // Overridden to gather information incrementally.
  virtual void GatherInformation(vtkPVInformation*);

  // Description:
  // Overridden to clean piececache too.
  virtual void InvalidatePipeline();

  // Description:
  // Updates the data pipeline (non-LOD only).
  // Overridden to operate in a way that is OK with streaming.
  virtual void UpdatePipeline();

  vtkSMSourceProxy* PieceCache;
  vtkSMSourceProxy* ParallelPieceCache;

  bool HaveLocalCache();
private:
  vtkSMStreamingParallelStrategy(const vtkSMStreamingParallelStrategy&); // Not implemented
  void operator=(const vtkSMStreamingParallelStrategy&); // Not implemented

//ETX
};

#endif
