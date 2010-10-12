/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkStreamingHarness.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkStreamingHarness.h"

#include "vtkDataObject.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"

vtkStandardNewMacro(vtkStreamingHarness);

//----------------------------------------------------------------------------
vtkStreamingHarness::vtkStreamingHarness()
{
  this->Piece = 0;
  this->NumberOfPieces = 1;
  this->Resolution = 1.0;
}

//----------------------------------------------------------------------------
vtkStreamingHarness::~vtkStreamingHarness()
{
}

//----------------------------------------------------------------------------
void vtkStreamingHarness::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Piece: " << this->Piece << endl;
  os << indent << "NumberOfPieces: " << this->NumberOfPieces << endl;
  os << indent << "Resolution: " << this->Resolution << endl;
}

//----------------------------------------------------------------------------
int vtkStreamingHarness::ProcessRequest(
  vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  return this->Superclass::ProcessRequest
    (request, inputVector, outputVector);
}


//----------------------------------------------------------------------------
int vtkStreamingHarness::RequestUpdateExtentInformation(
  vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  return 1;
}


//----------------------------------------------------------------------------
int vtkStreamingHarness::RequestUpdateExtent(
  vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  //get downstream numpasses
  //multiply by this->NumberOfPieces
  //apply this resolution

  return this->Superclass::RequestUpdateExtent
    (request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
int vtkStreamingHarness::RequestData(
  vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  return this->Superclass::RequestData
    (request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
double ComputePriority(int Piece, int NumPieces, double Resolution)
{
  return 1.0;
}

//----------------------------------------------------------------------------
void ComputeMetaInformation(int Piece, int NumPieces, double Resolution,
                            double bounds[6], double &gconfidence,
                            double &min, double &max, double &aconfidence)
{
  bounds[0] = bounds[2] = bounds[4] = 0;
  bounds[1] = bounds[3] = bounds[5] = -1;
  gconfidence = 0.0;
  min = 0;
  max = -1;
  aconfidence = 0.0;
}
