/*=========================================================================

  Program:   ParaView
  Module:    vtkStreamingProgression.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkStreamingProgression.h"

#include "vtkObjectFactory.h"

vtkStandardNewMacro(vtkStreamingProgression);

class Internals
{
public:
  Internals(vtkStreamingProgression *owner)
  {
    this->Owner = owner;
  }
  ~Internals()
  {
  }
  vtkStreamingProgression *Owner;
};

//----------------------------------------------------------------------------
vtkStreamingProgression::vtkStreamingProgression()
{
  this->Internal = new Internals(this);
}

//----------------------------------------------------------------------------
vtkStreamingProgression::~vtkStreamingProgression()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkStreamingProgression::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
