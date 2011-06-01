/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkKakaduReader.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkKakaduReader - read ImageNetCDF files
// .Author Joshua Wu 09.15.2009
// .SECTION Description
// vtkKakaduReader is a source object that reads ImageNetCDF files.
// It should be able to read most any ImageNetCDF file that wants to output a
// rectilinear grid.  The ordering of the variables is changed such that
// the ImageNetCDF x, y, z directions correspond to the vtkRectilinearGrid
// z, y, x directions, respectively.  The striding is done with
// respect to the vtkRectilinearGrid ordering.  Additionally, the
// z coordinates of the vtkRectilinearGrid are negated so that the
// first slice/plane has the highest z-value and the last slice/plane
// has the lowest z-value.

#ifndef __vtkKakaduReader_h
#define __vtkKakaduReader_h

#include "vtkImageAlgorithm.h"

class vtkDataArraySelection;
class vtkCallbackCommand;

class VTK_EXPORT vtkKakaduReader : public vtkImageAlgorithm
{
public:
  static vtkKakaduReader *New();
  vtkTypeMacro(vtkKakaduReader, vtkImageAlgorithm);
  virtual void PrintSelf(ostream& os, vtkIndent indent);

  //Description:
  //The file to open
  vtkSetStringMacro(FileName);
  vtkGetStringMacro(FileName);

  vtkSetVector3Macro(Origin, double);
  vtkGetVector3Macro(Origin, double);

  vtkSetVector3Macro(Spacing, double);
  vtkGetVector3Macro(Spacing, double);

  // Description:
  // Variable array selection
  virtual int GetNumberOfVariableArrays();
  virtual const char *GetVariableArrayName(int index);
  virtual int GetVariableArrayStatus(const char *name);
  virtual void SetVariableArrayStatus(const char *name, int status);

protected:
  vtkKakaduReader();
  ~vtkKakaduReader();

  virtual int ProcessRequest
    (vtkInformation *request,
     vtkInformationVector** inputVector,
     vtkInformationVector* outputVector);

  virtual int RequestData
    (vtkInformation*,
     vtkInformationVector**,
     vtkInformationVector*);

  virtual int RequestInformation
    (vtkInformation* request,
     vtkInformationVector** inputVector,
     vtkInformationVector* outputVector);

  static void SelectionModifiedCallback(vtkObject *caller, unsigned long eid,
                              void *clientdata, void *calldata);

  static void EventCallback(vtkObject* caller, unsigned long eid,
                      void* clientdata, void* calldata);

  vtkCallbackCommand* SelectionObserver;

  char *FileName;
  double Origin[3];
  double Spacing[3];

private:
  vtkKakaduReader(const vtkKakaduReader&);  // Not implemented.
  void operator=(const vtkKakaduReader&);  // Not implemented.

  class Internal;
  Internal* Internals;
};

#endif
