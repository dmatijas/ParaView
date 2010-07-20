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
#include "vtkParallelPieceCacheExecutive.h"

#include "vtkInformationIntegerKey.h"
#include "vtkInformationIntegerVectorKey.h"
#include "vtkObjectFactory.h"

#include "vtkStreamingOptions.h"
#include "vtkAlgorithm.h"
#include "vtkAlgorithmOutput.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkPointData.h"
#include "vtkParallelPieceCacheFilter.h"

vtkCxxRevisionMacro(vtkParallelPieceCacheExecutive, "$Revision$");
vtkStandardNewMacro(vtkParallelPieceCacheExecutive);

#define DEBUGPRINT_CACHING(arg) \
  if (vtkStreamingOptions::GetEnableStreamMessages()) \
    { \
      arg;\
    }

//----------------------------------------------------------------------------
vtkParallelPieceCacheExecutive
::vtkParallelPieceCacheExecutive()
{
}

//----------------------------------------------------------------------------
vtkParallelPieceCacheExecutive
::~vtkParallelPieceCacheExecutive()
{
}

//----------------------------------------------------------------------------
int vtkParallelPieceCacheExecutive
::NeedToExecuteData(int outputPort,
                    vtkInformationVector** inInfoVec,
                    vtkInformationVector* outInfoVec)
{
  vtkParallelPieceCacheFilter *myPCF =
    vtkParallelPieceCacheFilter::SafeDownCast(this->GetAlgorithm());

  // If no port is specified, check all ports.  This behavior is
  // implemented by the superclass.
  if(outputPort < 0 || !myPCF)
    {
    return this->Superclass::NeedToExecuteData(outputPort,
                                               inInfoVec, outInfoVec);
    }

  // Has the algorithm asked to be executed again?
  if(this->ContinueExecuting)
    {
    return 1;
    }

  cerr << "CHECKING CACHE FOR REQUESTED PIECES" << endl;
  if (myPCF->HasRequestedPieces())
    {
    //pipeline request can terminate now, yeah!
    cerr << "HAVE THEM ALL, NO NEED TO EXECUTE" << endl;
    return 0;
    }
  cerr << "SOMETHING MISSING" << endl;

  // Leave it up to the superclass
  return this->Superclass::NeedToExecuteData(outputPort,
                                             inInfoVec, outInfoVec);

}
