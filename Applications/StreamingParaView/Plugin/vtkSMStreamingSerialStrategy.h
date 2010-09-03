/*=========================================================================

  Program:   ParaView
  Module:    vtkSMStreamingSerialStrategy.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkSMStreamingSerialStrategy
// .SECTION Description
//

#ifndef __vtkSMStreamingSerialStrategy_h
#define __vtkSMStreamingSerialStrategy_h

#include "vtkSMSimpleStrategy.h"

class VTK_EXPORT vtkSMStreamingSerialStrategy : public vtkSMSimpleStrategy
{
public:
  static vtkSMStreamingSerialStrategy* New();
  vtkTypeMacro(vtkSMStreamingSerialStrategy, vtkSMSimpleStrategy);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // LOD and streaming are not working yet.
  virtual void SetEnableLOD(bool vtkNotUsed(enable))
  {}

  // Description:
  // Orders the pieces from most to least important.
  virtual int ComputePipelinePriorities();
  virtual int ComputeViewPriorities();
  virtual int ComputeCachePriorities();
  // Description:
  // Clears the data object cache in the streaming display pipeline.
  virtual void ClearStreamCache();

  virtual void PrepareFirstPass();
  virtual void PrepareAnotherPass();

  virtual void ChooseNextPiece();

//BTX
  // Description:
  // Asks server what piece it is going to show next.
  virtual void GetPieceInfo(int *P, int *NP, double *R, double *PRIORITY, bool *HIT, bool *APPEND);

  // Description:
  // Asks server what piece it is going to show next.
  virtual void GetStateInfo(bool *ALLDONE, bool *WENDDONE);
//ETX

  // Description:
  // Tells the strategy where the camera is so that pieces can be sorted and rejected
  virtual void SetViewState(double *camera, double *frustum);

//BTX
protected:
  vtkSMStreamingSerialStrategy();
  ~vtkSMStreamingSerialStrategy();

  // Description:
  // Copies ordered piece list from one UpdateSupressor to the other.
  virtual void CopyPieceList(vtkClientServerStream *stream,
                             vtkSMSourceProxy *src,
                             vtkSMSourceProxy *dest);

  // Description:
  // Overridden to set the servers correctly on all subproxies.
  virtual void BeginCreateVTKObjects();

  // Description:
  // Create and initialize the data pipeline.
  virtual void CreatePipeline(vtkSMSourceProxy* input, int outputport);

  // Description:
  // Gather the information of the displayed data (non-LOD).
  // Update the part of the pipeline needed to gather full information
  // and then gather that information.
  virtual void GatherInformation(vtkPVInformation*);

  // Description:
  // Invalidates the full resolution pipeline, overridden to clean up cache.
  virtual void InvalidatePipeline();

  vtkSMSourceProxy* PieceCache;

private:
  vtkSMStreamingSerialStrategy(const vtkSMStreamingSerialStrategy&); // Not implemented
  void operator=(const vtkSMStreamingSerialStrategy&); // Not implemented
//ETX
};

#endif
