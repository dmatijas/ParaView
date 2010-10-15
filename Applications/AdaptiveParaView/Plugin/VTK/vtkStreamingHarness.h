/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkStreamingHarness.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkStreamingHarness - a control point for streaming
// .SECTION Description
// This is a control point to orchestrate streamed pipeline passes.
// Insert this filter at the end of a pipeline and give it a number of passes.
// It will split any downstream requests it gets into that number of sub-passes.
// It does not automate iteration over those passes. That is left to the
// application which does so by giving it a particular pass number and optional
// resolution. Controls are also included to simplify the task of asking
// for priority computations and meta information.

#ifndef __vtkStreamingHarness_h
#define __vtkStreamingHarness_h

#include "vtkPassInputTypeAlgorithm.h"

class VTK_EXPORT vtkStreamingHarness : public vtkPassInputTypeAlgorithm
{
public:
  static vtkStreamingHarness *New();
  vtkTypeMacro(vtkStreamingHarness, vtkPassInputTypeAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent);

  //Description:
  //control over pass number
  vtkSetMacro(Piece, int);
  vtkGetMacro(Piece, int);

  //Description:
  //control over number of passes
  vtkSetMacro(NumberOfPieces, int);
  vtkGetMacro(NumberOfPieces, int);

  //Description:
  //control over resolution
  void SetResolution(double r);
  vtkGetMacro(Resolution, double);

  //Description:
  //compute the priority for a particular piece.
  //internal Piece, NumberOfPieces, Resolution are not affected or used
  double ComputePriority(int Pieces, int NumPieces, double Resolution);

  //Description:
  //compute the meta information for a particular piece.
  //internal Piece, NumberOfPieces, Resolution are not affected or used
  //TODO: need a general struct for meta info with type correct bounds,
  //and room for all arrays not just active scalars
  void ComputeMetaInformation
    (int Piece, int NumPieces, double Resolution,
     double bounds[6], double &geometric_confidence,
     double &min, double &max, double &attribute_confidence);

protected:
  vtkStreamingHarness();
  ~vtkStreamingHarness();

  virtual int ProcessRequest(
    vtkInformation *,
    vtkInformationVector **,
    vtkInformationVector *);

  virtual int RequestUpdateExtent(
    vtkInformation *,
    vtkInformationVector **,
    vtkInformationVector *);

  virtual int RequestData(
    vtkInformation *,
    vtkInformationVector **,
    vtkInformationVector *);

  int Piece;
  int NumberOfPieces;
  double Resolution;
  bool ForOther;

private:
  vtkStreamingHarness(const vtkStreamingHarness&);  // Not implemented.
  void operator=(const vtkStreamingHarness&);  // Not implemented.
};


#endif
