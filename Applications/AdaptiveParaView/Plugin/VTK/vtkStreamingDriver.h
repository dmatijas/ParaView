/*=========================================================================

  Program:   ParaView
  Module:    vtkStreamingDriver.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkStreamingDriver - orchestrates progression of streamed pieces
// .SECTION Description
// vtkStreamingDriver automates the process of streamed rendering. It tells
// vtk's rendering classes to setup for streamed rendering, watches the events
// they fire to start, restart and stop streaming, and cycles through passes
// until completion.
// It controls the pipeline with the help of vtkStreamingHarness classes.
// It delegates the task of deciding what piece to show next to a helper class,
// namely vtkStreamingProgression, and it's specialized subclasses.

#ifndef __vtkStreamingDriver_h
#define __vtkStreamingDriver_h

#include "vtkObject.h"

class vtkRenderWindow;
class vtkCallbackCommand;
class vtkStreamingHarness;
class vtkStreamingProgression;
class Internals;

class VTK_EXPORT vtkStreamingDriver : public vtkObject
{
public:
  vtkTypeMacro(vtkStreamingDriver,vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent);
  static vtkStreamingDriver *New();

  // Description:
  // Assign what window to automate streaming in.
  void SetRenderWindow(vtkRenderWindow *);

  // Description:
  // Assign the helper that decides on the ordering of pieces
  void SetProgression(vtkStreamingProgression *);

  // Description:
  // Control over the list of things that this renders in streaming fashion.
  void AddHarness(vtkStreamingHarness *);
  void RemoveHarness(vtkStreamingHarness *);
  void RemoveAllHarnesses();

  // Description:
  // For internal use, window events call back here.
  void RenderEvent();

protected:
  vtkStreamingDriver();
  ~vtkStreamingDriver();

  Internals *Internal;

private:
  vtkStreamingDriver(const vtkStreamingDriver&);  // Not implemented.
  void operator=(const vtkStreamingDriver&);  // Not implemented.
};

#endif
