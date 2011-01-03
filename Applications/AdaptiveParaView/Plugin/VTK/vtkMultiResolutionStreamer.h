/*=========================================================================

  Program:   ParaView
  Module:    vtkMultiResolutionStreamer.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkMultiResolutionStreamer - streams pieces with increasing refinement
// .SECTION Description
// Besides rendering pieces in a most to least important order, this streamer
// also continuously increases the resolution of the pieces it draws. That is,
// it draws the entire domain at a low level of resolution, then clears the
// screen and redraws the domain with one or more of the original pieces
// split and refined at a higher level of resolution. Groups of unimportant
// pieces, for instance those offscreen are merged back together into a lower
// resolution whole.

#ifndef __vtkMultiResolutionStreamer_h
#define __vtkMultiResolutionStreamer_h

#include "vtkStreamingDriver.h"

class VTK_EXPORT vtkMultiResolutionStreamer : public vtkStreamingDriver
{
public:
  vtkTypeMacro(vtkMultiResolutionStreamer,vtkStreamingDriver);
  void PrintSelf(ostream& os, vtkIndent indent);
  static vtkMultiResolutionStreamer *New();

//BTX
protected:
  vtkMultiResolutionStreamer();
  ~vtkMultiResolutionStreamer();

  virtual void StartRenderEvent();
  virtual void EndRenderEvent();

  virtual bool IsWendDone();
  virtual bool IsEveryoneDone();

  virtual void PrepareFirstPass();
  virtual void ChooseNextPieces();
  virtual int Refine(vtkStreamingHarness *);
  virtual void Reap(vtkStreamingHarness *);

private:
  vtkMultiResolutionStreamer(const vtkMultiResolutionStreamer&);  // Not implemented.
  void operator=(const vtkMultiResolutionStreamer&);  // Not implemented.

  class Internals;
  Internals *Internal;

//ETX
};

#endif
