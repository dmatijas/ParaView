/*=========================================================================

  Program:   ParaView
  Module:    vtkStreamingProgression.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkStreamingProgression - calculates progression of streamed pieces
// .SECTION Description

#ifndef __vtkStreamingProgression_h
#define __vtkStreamingProgression_h

#include "vtkObject.h"

class Internals;

class VTK_EXPORT vtkStreamingProgression : public vtkObject
{
public:
  vtkTypeMacro(vtkStreamingProgression,vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent);
  static vtkStreamingProgression *New();

//BTX
protected:
  vtkStreamingProgression();
  ~vtkStreamingProgression();

  Internals *Internal;

private:
  vtkStreamingProgression(const vtkStreamingProgression&);  // Not implemented.
  void operator=(const vtkStreamingProgression&);  // Not implemented.

//ETX
};

#endif
