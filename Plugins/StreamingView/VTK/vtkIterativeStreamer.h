/*=========================================================================

  Program:   ParaView
  Module:    vtkIterativeStreamer.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkIterativeStreamer - simply iterates through streamed pieces
// .SECTION Description
// This is the simplest of the streaming progression algorithms.
// It is a simple for loop through the pieces with no prioritization at all.
// It allows multiple streamed objects and can be interrupted through the
// render eventually hook.

#ifndef __vtkIterativeStreamer_h
#define __vtkIterativeStreamer_h

#include "vtkStreamingDriver.h"

class VTK_EXPORT vtkIterativeStreamer : public vtkStreamingDriver
{
public:
  vtkTypeMacro(vtkIterativeStreamer,vtkStreamingDriver);
  void PrintSelf(ostream& os, vtkIndent indent);
  static vtkIterativeStreamer *New();

//BTX
protected:
  vtkIterativeStreamer();
  ~vtkIterativeStreamer();

  //Description:
  //Before each render, check and if needed do, initial pass setup
  virtual void StartRenderEvent();
  //Description:
  //After each render advanced to next pass. If last pass do last pass work.
  virtual void EndRenderEvent();

  //Description:
  //Helper to watch for camera motion and restart to first pass.
  bool CameraMoved;

private:
  vtkIterativeStreamer(const vtkIterativeStreamer&);  // Not implemented.
  void operator=(const vtkIterativeStreamer&);  // Not implemented.

//ETX
};

#endif
