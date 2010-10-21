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
// .NAME vtkStreamingDriver - orchestrates progression of streamed rendering
// .SECTION Description
// vtkStreamingDriver automates the process of streamed rendering. It tells
// vtk's rendering classes to setup for streamed rendering, watches the events
// they fire to start, restart and stop streaming, and cycles through passes
// until completion.
// It controls the pipeline with the help of vtkStreamingHarness classes.
// It delegates the task of deciding what piece to show next to subclasses.

#ifndef __vtkStreamingDriver_h
#define __vtkStreamingDriver_h

#include "vtkObject.h"

class Internals;
class vtkCallbackCommand;
class vtkCollection;
class vtkRenderer;
class vtkRenderWindow;
class vtkStreamingHarness;

class VTK_EXPORT vtkStreamingDriver : public vtkObject
{
public:
  vtkTypeMacro(vtkStreamingDriver,vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Assign what window to automate streaming in.
  void SetRenderWindow(vtkRenderWindow *);
  vtkRenderWindow* GetRenderWindow();

  // Description:
  // Assign what renderer to automate streaming in.
  void SetRenderer(vtkRenderer *);
  vtkRenderer* GetRenderer();

  // Description:
  // Control over the list of things that this renders in streaming fashion.
  void AddHarness(vtkStreamingHarness *);
  void RemoveHarness(vtkStreamingHarness *);
  void RemoveAllHarnesses();
  vtkCollection *GetHarnesses();

  // Description:
  // Assign a function that this driver can call to schedule eventual
  // render calls. This allows automatic streaming to work as part of
  // a GUI event loop so that it can be interruptable.
  void AssignRenderLaterFunction(void (*function)(void));

  // Description:
  // For internal use, window events call back here.
  virtual void StartRenderEvent() = 0;
  virtual void EndRenderEvent() = 0;

protected:
  vtkStreamingDriver();
  ~vtkStreamingDriver();

  Internals *Internal;

  void RenderEventually();

private:
  vtkStreamingDriver(const vtkStreamingDriver&);  // Not implemented.
  void operator=(const vtkStreamingDriver&);  // Not implemented.
};

#endif
