/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile$

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkParallelPieceCacheExecutive - An executive for the ParallelPieceCacheFilter
// .SECTION Description
// vtkParallelPieceCacheExecutive is used along with the ParallelPieceCacheFilter to cache
// data pieces. The filter stores the data. The executive prevents requests
// that can be satisfied by the cache from causing upstream pipeline updates.
// .SEE ALSO
// vtkParallelPieceCacheFilter

#ifndef __vtkParallelPieceCacheExecutive_h
#define __vtkParallelPieceCacheExecutive_h

#include "vtkCompositeDataPipeline.h"

class VTK_EXPORT vtkParallelPieceCacheExecutive :
  public vtkCompositeDataPipeline
{
public:
  static vtkParallelPieceCacheExecutive* New();
  vtkTypeRevisionMacro(vtkParallelPieceCacheExecutive,
                       vtkCompositeDataPipeline);

protected:
  vtkParallelPieceCacheExecutive();
  ~vtkParallelPieceCacheExecutive();

  //overridden to short circuit upstream requests when cache has data
  virtual int NeedToExecuteData(int outputPort,
                                vtkInformationVector** inInfoVec,
                                vtkInformationVector* outInfoVec);

private:
  vtkParallelPieceCacheExecutive(const vtkParallelPieceCacheExecutive&);  // Not implemented.
  void operator=(const vtkParallelPieceCacheExecutive&);  // Not implemented.
};

#endif
