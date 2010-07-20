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
// .NAME vtkParallelPieceCacheFilter - Moves/redistributes data between processes.
// .SECTION Description

#ifndef __vtkParallelPieceCacheFilter_h
#define __vtkParallelPieceCacheFilter_h

#include "vtkDataSetAlgorithm.h"
#include "vtkPiece.h" //need access for cache index
#include <vtkstd/map> //implementation of the cache

class vtkDataSet;
class vtkAppendPolyData;
class vtkPieceList;
class vtkPolyData;
class vtkPiece;

class VTK_EXPORT vtkParallelPieceCacheFilter : public vtkDataSetAlgorithm
{
public:
  static vtkParallelPieceCacheFilter *New();
  vtkTypeRevisionMacro(vtkParallelPieceCacheFilter, vtkDataSetAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // This is the maximum number of pieces that can be retained in memory.
  // It defaults to -1, meaning unbounded.
  void SetCacheSize(int size);
  vtkGetMacro(CacheSize,int);

  //Description:
  //Called to clear out the cache of pieces.
  void EmptyCache();

//BTX
  //Description:
  //Called to check if a particular piece is locally cached
  bool HasPiece(vtkPiece *p);

  //Description:
  //Called to ask for a particular set of locally cached pieces on the next update.
  void SetRequestedPieces(vtkPieceList *npl);
  vtkGetObjectMacro(RequestedPieces, vtkPieceList);
//ETX

  //Description:
  //Call to check if all of the requested pieces are cached.
  int HasRequestedPieces();

  //Description:
  //Called by Executive to check if upstream needs to execute.
  bool HasPieceList() { return this->GetRequestedPieces()!=NULL; }

  //Description:
  //Call to append all cached vtkPolyDatas into one
  void AppendPieces();

  //Description:
  //Call to obtain the appended result.
  vtkPolyData *GetAppendedData();

  //Description:
  //for testing
  void MakeDummyList();

protected:
  vtkParallelPieceCacheFilter();
  ~vtkParallelPieceCacheFilter();

  virtual int FillOutputPortInformation(int port, vtkInformation* info);
  virtual int FillInputPortInformation(int port, vtkInformation* info);
  virtual int RequestDataObject(vtkInformation* request,
                          vtkInformationVector** inputVector,
                          vtkInformationVector* outputVector);
  virtual int RequestData(vtkInformation* request,
                          vtkInformationVector** inputVector,
                          vtkInformationVector* outputVector);


//BTX
  typedef vtkstd::map<
   vtkPiece,
   vtkDataSet * //data
  > CacheType;
  CacheType Cache;
//ETX

  vtkAppendPolyData *AppendFilter;
  vtkPolyData *AppendResult;

  vtkPieceList *RequestedPieces;

  int CacheSize;

private:

  vtkParallelPieceCacheFilter(const vtkParallelPieceCacheFilter&); // Not implemented
  void operator=(const vtkParallelPieceCacheFilter&); // Not implemented
};

#endif
