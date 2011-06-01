/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkWorldWarp.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkWorldWarp.h"
#include "vtkObjectFactory.h"

#include "vtkBoundingBox.h"
#include "vtkCellData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkInformationExecutivePortKey.h"
#include "vtkMath.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkSmartPointer.h"

vtkStandardNewMacro(vtkWorldWarp);

//----------------------------------------------------------------------------
vtkWorldWarp::vtkWorldWarp()
{
  this->LatInput = 0;
  this->LonInput = 1;
  this->AltInput = 2;
  this->XScale = 1.0;
  this->XBias = 0.0;
  this->YScale = 1.0;
  this->YBias = 0.0;
  this->ZScale = 1.0;
  this->ZBias = 0.0;

  this->GetInformation()->Set(vtkAlgorithm::MANAGES_METAINFORMATION(), 1);
}

//----------------------------------------------------------------------------
vtkWorldWarp::~vtkWorldWarp()
{
}

//----------------------------------------------------------------------------
void vtkWorldWarp::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkWorldWarp::SwapPoint(double inPoint[3], double outPoint[3])
{
  inPoint[0] = (inPoint[0] * this->XScale) + this->XBias;
  inPoint[1] = (inPoint[1] * this->YScale) + this->YBias;
  inPoint[2] = (inPoint[2] * this->ZScale) + this->ZBias;

  double r = inPoint[this->AltInput];
  double sinphi = sin(inPoint[this->LatInput]);
  double cosphi = cos(inPoint[this->LatInput]);
  double sintheta = sin(inPoint[this->LonInput]);
  double costheta = cos(inPoint[this->LonInput]);

  outPoint[2] = r*sinphi*costheta;
  outPoint[1] = r*sinphi*sintheta;
  outPoint[0] = r*cosphi;
}


//----------------------------------------------------------------------------
int vtkWorldWarp::ProcessRequest(
  vtkInformation *request,
  vtkInformationVector **inputVector,
  vtkInformationVector *outputVector)
{
  if(!request->Has(vtkStreamingDemandDrivenPipeline::REQUEST_MANAGE_INFORMATION()))
    {
    return this->Superclass::ProcessRequest(request, inputVector, outputVector);
    }

  // copy attributes across unmodified
  vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation *outInfo = outputVector->GetInformationObject(0);
  if (inInfo->Has(vtkDataObject::CELL_DATA_VECTOR()))
    {
    outInfo->CopyEntry(inInfo, vtkDataObject::CELL_DATA_VECTOR(), 1);
    }
  if (inInfo->Has(vtkDataObject::POINT_DATA_VECTOR()))
    {
    outInfo->CopyEntry(inInfo, vtkDataObject::POINT_DATA_VECTOR(), 1);
    }

  vtkSmartPointer<vtkPoints> inPts = vtkSmartPointer<vtkPoints>::New();
  int i;
  vtkIdType ptId, numPts;
  double inpt[3], outpt[3];

  double *pbounds = inInfo->Get(vtkStreamingDemandDrivenPipeline::PIECE_BOUNDING_BOX());
  //convert to 8 corner Points
  //todo: subdivide bounds to get a more accurate estimate
  for (int a=0; a<2; a++)
    {
    for (int b=0; b<2; b++)
      {
      for (int c=0; c<2; c++)
        {
        inPts->InsertNextPoint(pbounds[a], pbounds[2+b], pbounds[4+c]);

        }
      }
    }

  numPts = 8;
  vtkBoundingBox bbox;
  // Loop over 8 corners, adjusting locations by min attribute
  for (ptId=0; ptId < numPts; ptId++)
    {
    inPts->GetPoint(ptId, inpt);
    this->SwapPoint(inpt, outpt);
    bbox.AddPoint(outpt);
    }

  double obounds[6];
  bbox.GetBounds(obounds);
  outInfo->Set(vtkStreamingDemandDrivenPipeline::PIECE_BOUNDING_BOX(), obounds, 6);
  return 1;
}

//----------------------------------------------------------------------------
int vtkWorldWarp::RequestData(
  vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation* outInfo = outputVector->GetInformationObject(0);
  vtkPolyData* input = vtkPolyData::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));
  vtkPolyData* output = vtkPolyData::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));

  output->CopyStructure(input);
  output->GetFieldData()->ShallowCopy(input->GetFieldData());
  output->GetCellData()->ShallowCopy(input->GetCellData());
  output->GetPointData()->ShallowCopy(input->GetPointData());

  vtkPoints *opts = vtkPoints::New();
  vtkIdType npts = input->GetNumberOfPoints();
  opts->SetNumberOfPoints(npts);
  double nextipt[3];
  double nextopt[3];
  for(vtkIdType i = 0; i < npts; i++)
    {
    input->GetPoint(i, nextipt);
    this->SwapPoint(nextipt, nextopt);
    opts->SetPoint(i, nextopt);
    }
  output->SetPoints(opts);
  opts->Delete();
  return 1;
}
