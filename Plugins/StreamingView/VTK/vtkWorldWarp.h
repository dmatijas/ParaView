/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkWorldWarp.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkWorldWarp - Warps flat data onto the surface of the earth
// .SECTION Description
// This filter warps data defined on a flat cartesian x,y,z coordinate space
// onto a sphereical one. The mappings between x,y,z to r,phi,theta are
// controllable. It is meant to allow easy projections of climate data onto
// the surface of the earth.

#ifndef __vtkWorldWarp_h
#define __vtkWorldWarp_h

#include "vtkPolyDataAlgorithm.h"

class VTK_EXPORT vtkWorldWarp : public vtkPolyDataAlgorithm
{
public:
  static vtkWorldWarp *New();
  vtkTypeMacro(vtkWorldWarp, vtkPolyDataAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent);

  //Description:
  //Specifies whether X, Y or Z input varies the output lat
  vtkSetMacro(LatInput, int);
  vtkGetMacro(LatInput, int);

  //Description:
  //Specifies whether X, Y or Z input varies the output lon
  vtkSetMacro(LonInput, int);
  vtkGetMacro(LonInput, int);

  //Description:
  //Specifies whether X, Y or Z input varies the output alt
  vtkSetMacro(AltInput, int);
  vtkGetMacro(AltInput, int);

  // Description:
  //Specifies multiplier for x input so that it can be normalized to 0..1 before mapping
  vtkSetMacro(XScale, double);
  vtkGetMacro(XScale, double);
  // Description:
  //Specifies offset for x input so that it can be normalized to 0..1 before mapping
  vtkSetMacro(XBias, double);
  vtkGetMacro(XBias, double);

  // Description:
  //Specifies multiplier for y input so that it can be normalized to 0..1 before mapping
  vtkSetMacro(YScale, double);
  vtkGetMacro(YScale, double);
  // Description:
  //Specifies offset for y input so that it can be normalized to 0..1 before mapping
  vtkSetMacro(YBias, double);
  vtkGetMacro(YBias, double);

  // Description:
  //Specifies multiplier for z input so that it can be normalized to 0..1 before mapping
  vtkSetMacro(ZScale, double);
  vtkGetMacro(ZScale, double);
  // Description:
  //Specifies offset for z input so that it can be normalized to 0..1 before mapping
  vtkSetMacro(ZBias, double);
  vtkGetMacro(ZBias, double);

protected:
  vtkWorldWarp();
  ~vtkWorldWarp();

  virtual int ProcessRequest(
    vtkInformation *,
    vtkInformationVector **,
    vtkInformationVector *);

  virtual int RequestData(
    vtkInformation *,
    vtkInformationVector **,
    vtkInformationVector *);

  int LatInput;
  int LonInput;
  int AltInput;

  double XScale;
  double XBias;
  double YScale;
  double YBias;
  double ZScale;
  double ZBias;

  void SwapPoint(double inPoint[3], double outPoint[3]);

private:
  vtkWorldWarp(const vtkWorldWarp&);  // Not implemented.
  void operator=(const vtkWorldWarp&);  // Not implemented.
};


#endif
