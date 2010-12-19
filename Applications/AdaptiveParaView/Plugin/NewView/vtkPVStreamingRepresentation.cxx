/*=========================================================================

  Program:   ParaView
  Module:    vtkPVStreamingRepresentation.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/*=========================================================================

  Program:   VTK/ParaView Los Alamos National Laboratory Modules (PVLANL)
  Module:    vtkPVStreamingRepresentation.cxx

Copyright (c) 2007, Los Alamos National Security, LLC

All rights reserved.

Copyright 2007. Los Alamos National Security, LLC.
This software was produced under U.S. Government contract DE-AC52-06NA25396
for Los Alamos National Laboratory (LANL), which is operated by
Los Alamos National Security, LLC for the U.S. Department of Energy.
The U.S. Government has rights to use, reproduce, and distribute this software.
NEITHER THE GOVERNMENT NOR LOS ALAMOS NATIONAL SECURITY, LLC MAKES ANY WARRANTY,
EXPRESS OR IMPLIED, OR ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE.
If software is modified to produce derivative works, such modified software
should be clearly marked, so as not to confuse it with the version available
from LANL.

Additionally, redistribution and use in source and binary forms, with or
without modification, are permitted provided that the following conditions
are met:
-   Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
-   Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
-   Neither the name of Los Alamos National Security, LLC, Los Alamos National
    Laboratory, LANL, the U.S. Government, nor the names of its contributors
    may be used to endorse or promote products derived from this software
    without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY LOS ALAMOS NATIONAL SECURITY, LLC AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL LOS ALAMOS NATIONAL SECURITY, LLC OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/

#include "vtkPVStreamingRepresentation.h"
#include "vtkObjectFactory.h"

#include "vtkPieceCacheFilter.h"
#include "vtkStreamingHarness.h"

vtkStandardNewMacro(vtkPVStreamingRepresentation);

class vtkPVStreamingRepresentation::Internals
{
public:
  Internals()
  {
    this->PCF = vtkPieceCacheFilter::New();
    this->Harness = vtkStreamingHarness::New();
    this->Harness->SetNumberOfPieces(10);
    this->PVCR = vtkPVCompositeRepresentation::New();

    this->Harness->SetInputConnection(0, this->PCF->GetOutputPort());
    this->PVCR->SetInputConnection(0, this->Harness->GetOutputPort());
  }

  ~Internals()
  {
    this->PVCR->Delete();
    this->PCF->Delete();
    this->Harness->Delete();
  }

  vtkPVCompositeRepresentation *PVCR;
  vtkPieceCacheFilter *PCF;
  vtkStreamingHarness *Harness;
};

//----------------------------------------------------------------------------
vtkPVStreamingRepresentation::vtkPVStreamingRepresentation()
{
  cerr << "PVSR(" << this << ") ()" << endl;
  this->Internal = new Internals;
}

//----------------------------------------------------------------------------
vtkPVStreamingRepresentation::~vtkPVStreamingRepresentation()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkPVStreamingRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkPVStreamingRepresentation::SetInputConnection
  (int port, vtkAlgorithmOutput* input)
{
  if (port == 0)
    {
    this->Internal->PCF->SetInputConnection(port, input);
    }
  else
    {
    //this->Internal->PVCR->SetInputConnection(port, input);
    }
}

//----------------------------------------------------------------------------
void vtkPVStreamingRepresentation::SetInputConnection
  (vtkAlgorithmOutput* input)
{
  this->Internal->PCF->SetInputConnection(input);
}

//----------------------------------------------------------------------------
void vtkPVStreamingRepresentation::AddInputConnection
  (int port, vtkAlgorithmOutput* input)
{
  if (port == 0)
    {
    this->Internal->PCF->AddInputConnection(port, input);
    }
  else
    {
    //this->Internal->PVCR->AddInputConnection(port, input);
    }
}

//----------------------------------------------------------------------------
void vtkPVStreamingRepresentation::AddInputConnection
  (vtkAlgorithmOutput* input)
{
  this->Internal->PCF->AddInputConnection(input);
}

//----------------------------------------------------------------------------
void vtkPVStreamingRepresentation::RemoveInputConnection
  (int port, vtkAlgorithmOutput* input)
{
  if (port == 0)
    {
    this->Internal->PCF->RemoveInputConnection(port, input);
    }
  else
    {
    //this->Internal->PVCR->RemoveInputConnection(port, input);
    }
}

//----------------------------------------------------------------------------
void vtkPVStreamingRepresentation::Update()
{
  this->Internal->Harness->Update();
  this->Internal->PVCR->Update();
  this->Superclass::Update();
}
