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
// .NAME vtkIterativeStreamer - calculates progression of streamed pieces
// .SECTION Description

#ifndef __vtkIterativeStreamer_h
#define __vtkIterativeStreamer_h

#include "vtkStreamingDriver.h"

class Internals;

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

  void RenderInternal();
  void RenderEventInternal();

  Internals *Internal;

private:
  vtkIterativeStreamer(const vtkIterativeStreamer&);  // Not implemented.
  void operator=(const vtkIterativeStreamer&);  // Not implemented.

//ETX
};

#endif
