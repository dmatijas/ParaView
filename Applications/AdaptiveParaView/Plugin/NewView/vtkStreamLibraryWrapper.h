/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkStreamLibraryWrapper.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/*=========================================================================

  Program:   VTK/ParaView Los Alamos National Laboratory Modules (PVLANL)
  Module:    vtkStreamLibraryWrapper.h

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
// .NAME vtkStreamLibraryWrapper - class to enable ParaView CS wrapped classes
// .SECTION Description
// An instance of this class is instantiated on the server to initialize
// the ClientServer wrapped library. It is necessary since the VTK level
// code is in a separate dir and lib from the PV plugin level code.

#ifndef __vtkStreamLibraryWrapper_h
#define __vtkStreamLibraryWrapper_h

#include "vtkObject.h"

class vtkStreamLibraryWrapper : public vtkObject
{
public:
  static vtkStreamLibraryWrapper* New();
  vtkTypeMacro(vtkStreamLibraryWrapper, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent);

  void EnableWrapping();

protected:
  vtkStreamLibraryWrapper();
  ~vtkStreamLibraryWrapper();

  static int WrappingEnabled;

private:
  vtkStreamLibraryWrapper(const vtkStreamLibraryWrapper&);//Not implemented
  void operator=(const vtkStreamLibraryWrapper&);//Not implemented
};

#endif
