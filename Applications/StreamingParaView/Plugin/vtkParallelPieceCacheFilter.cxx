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
#include "vtkParallelPieceCacheFilter.h"


#include "vtkAppendPolyData.h"
#include "vtkCellData.h"
#include "vtkDataObject.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMultiPieceDataSet.h"
#include "vtkObjectFactory.h"
#include "vtkPiece.h"
#include "vtkPieceList.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"

//Choose the comparison operation that establishes identify for the map
bool operator<(vtkPiece a, vtkPiece b)
{
  return a.CompareHandle(b);
}

vtkCxxRevisionMacro(vtkParallelPieceCacheFilter, "$Revision$");
vtkStandardNewMacro(vtkParallelPieceCacheFilter);
vtkCxxSetObjectMacro(vtkParallelPieceCacheFilter,RequestedPieces,vtkPieceList);

//-----------------------------------------------------------------------------
vtkParallelPieceCacheFilter::vtkParallelPieceCacheFilter()
{
  this->AppendFilter = vtkAppendPolyData::New();
  this->AppendFilter->UserManagedInputsOn();
  this->RequestedPieces = NULL;
  this->AppendResult = NULL;
  this->CacheSize = -1;
}

//-----------------------------------------------------------------------------
vtkParallelPieceCacheFilter::~vtkParallelPieceCacheFilter()
{
  this->EmptyCache();
  this->AppendFilter->Delete();
  if (this->RequestedPieces)
    {
    this->RequestedPieces->Delete();
    }
  if (this->AppendResult != NULL)
    {
    this->AppendResult->Delete();
    this->AppendResult = NULL;
    }
}

//-----------------------------------------------------------------------------
void vtkParallelPieceCacheFilter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
int vtkParallelPieceCacheFilter::FillOutputPortInformation(int, vtkInformation *info)
{
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkPolyData");
  return 1;
}

//----------------------------------------------------------------------------
int vtkParallelPieceCacheFilter::FillInputPortInformation(int, vtkInformation *info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkMultiPieceDataSet");
  return 1;
}

//----------------------------------------------------------------------------
void vtkParallelPieceCacheFilter::SetCacheSize(int size)
{
  this->CacheSize = size;
  if (this->Cache.size() == static_cast<unsigned long>(size))
    {
    return;
    }
  this->EmptyCache();
}

//----------------------------------------------------------------------------
void vtkParallelPieceCacheFilter::EmptyCache()
{
  //cerr << this << " PPCF : EMPTYCACHE" << endl;
  CacheType::iterator pos;
  for (pos = this->Cache.begin(); pos != this->Cache.end(); )
    {
    pos->second->Delete();
    this->Cache.erase(pos++);
    }
}

//-----------------------------------------------------------------------------
int vtkParallelPieceCacheFilter::RequestDataObject(vtkInformation*,
                                            vtkInformationVector** inputVector,
                                            vtkInformationVector* outputVector)
{
  // for each output
  for(int i=0; i < this->GetNumberOfOutputPorts(); ++i)
    {
    vtkInformation* info = outputVector->GetInformationObject(i);
    vtkDataSet *output = vtkDataSet::SafeDownCast(
                                                  info->Get(vtkDataObject::DATA_OBJECT()));

    if (!output || !output->IsA("vtkPolyData"))
      {
      vtkPolyData* newOutput = vtkPolyData::New();
      newOutput->SetPipelineInformation(info);
      newOutput->Delete();
      }
    }
  return 1;
}

//-----------------------------------------------------------------------------
int vtkParallelPieceCacheFilter::RequestData(vtkInformation*,
                                            vtkInformationVector** inputVector,
                                            vtkInformationVector* outputVector)
{
  vtkDataObject* doout =
    outputVector->GetInformationObject(0)->Get(vtkDataObject::DATA_OBJECT());
  vtkDataSet *output = vtkDataSet::SafeDownCast(doout);

  if (this->HasRequestedPieces())
    {
    cerr << "PPCF(" << this << ") UPDATE FROM CACHE" << endl;
    int numProcessors = this->RequestedPieces->GetNumberOfPieces();
    this->AppendFilter->SetNumberOfInputs(numProcessors);

    for (int i = 0; i < numProcessors; i++)
      {
      vtkPiece ph = this->RequestedPieces->GetPiece(i);
      CacheType::iterator pos = this->Cache.find(ph);
      if (pos == this->Cache.end())
        {
        cerr << "UH OH: MISS" << endl;
        //TODO: Something intelligent here, it should never get here because we've
        //checked above
        return 0;
        }
      vtkDataSet *ds = pos->second;
      this->AppendFilter->SetInputByNumber(i, vtkPolyData::SafeDownCast(ds));
      }
    }
  else
    {
    cerr << "PPCF(" << this << ") UPDATE FROM INPUT" << endl;
    vtkDataObject* input =
      inputVector[0]->GetInformationObject(0)->Get(vtkDataObject::DATA_OBJECT());
    vtkMultiPieceDataSet *dsIn = vtkMultiPieceDataSet::SafeDownCast(input);

    int numProcessors = dsIn->GetNumberOfPieces();
    this->AppendFilter->SetNumberOfInputs(numProcessors);

    for (unsigned int i = 0; i < dsIn->GetNumberOfPieces(); i++)
      {
      vtkDataSet *ds = dsIn->GetPiece(i);
      vtkDataSet *ds2 = ds->NewInstance();
      ds2->ShallowCopy(ds);
      ds2->GetInformation()->Copy(ds->GetInformation());

      vtkInformation* dataInfo = ds2->GetInformation();
      int pNum = -1;
      int nPieces = -1;
      double res = 0.0;
      if (dataInfo->Has(vtkDataObject::DATA_PIECE_NUMBER()))
        {
        pNum = dataInfo->Get(vtkDataObject::DATA_PIECE_NUMBER());
        nPieces = dataInfo->Get(vtkDataObject::DATA_NUMBER_OF_PIECES());
        res = dataInfo->Get(vtkDataObject::DATA_RESOLUTION());
        }

      vtkPiece ph;
      ph.SetProcessor(i);
      ph.SetPiece(pNum);
      ph.SetNumPieces(nPieces);
      CacheType::iterator pos = this->Cache.find(ph);
      cerr << "PPCF(" << this << ") looking for ("
           << ph.GetProcessor() << ":"
           << ph.GetPiece() << "/"
           << ph.GetNumPieces() << "@"
           << ph.GetResolution() << "->["
           << ph.GetBounds()[0] << "-"
           << ph.GetBounds()[1] << ","
           << ph.GetBounds()[2] << "-"
           << ph.GetBounds()[3] << ","
           << ph.GetBounds()[4] << "-"
           << ph.GetBounds()[5] << "]=("
           << ph.GetPipelinePriority() << " "
           << ph.GetViewPriority() << " "
           << ph.GetCachedPriority() << ")" << endl;
      if (pos != this->Cache.end())
        {
        cerr << this << " PPCF UP " << i << ":" << pNum << "/" << nPieces << " HIT" << endl;
        vtkPiece ph2 = pos->first;
        cerr << "PPCF(" << this << ") found ("
             << ph2.GetProcessor() << ":"
             << ph2.GetPiece() << "/"
             << ph2.GetNumPieces() << "@"
             << ph2.GetResolution() << "->["
             << ph2.GetBounds()[0] << "-"
             << ph2.GetBounds()[1] << ","
             << ph2.GetBounds()[2] << "-"
             << ph2.GetBounds()[3] << ","
             << ph2.GetBounds()[4] << "-"
             << ph2.GetBounds()[5] << "]=("
             << ph2.GetPipelinePriority() << " "
             << ph2.GetViewPriority() << " "
             << ph2.GetCachedPriority() << ")" << endl;
        pos->second->Delete();
        }
      else
        {
        cerr << this << " PPCF UP " << i << ":" << pNum << "/" << nPieces << " MISS" << endl;
        }
      if ((this->CacheSize < 0 ||
           this->Cache.size() < static_cast<unsigned long>(this->CacheSize)))
        {
        cerr << this << " HAD ROOM" << endl;
        this->Cache[ph] = ds2;
        }
      else
        {
        cerr << this << " NO ROOM" << endl;
        }
      this->AppendFilter->SetInputByNumber(i, vtkPolyData::SafeDownCast(ds2));
      }
    }

  this->AppendFilter->Update();
  vtkPolyData *pdout = vtkPolyData::SafeDownCast(this->AppendFilter->GetOutput());
  output->CopyStructure(pdout);
  output->GetPointData()->PassData(pdout->GetPointData());
  output->GetCellData()->PassData(pdout->GetCellData());

  return 1;
}

//------------------------------------------------------------------------------
bool vtkParallelPieceCacheFilter::HasPiece(vtkPiece *ph)
{
  CacheType::iterator pos = this->Cache.find(*ph);
  if (pos != this->Cache.end())
    {
    return true;
    }
  return false;
}

//------------------------------------------------------------------------------
void vtkParallelPieceCacheFilter::MakeDummyList()
{
  //cerr << "MADE DUMMY LIST" << endl;
  vtkPieceList *pl = vtkPieceList::New();
  vtkPiece ph;
  ph.SetProcessor(0);
  ph.SetPiece(0);
  ph.SetNumPieces(4);
  pl->AddPiece(ph);
  ph.SetProcessor(1);
  ph.SetPiece(0);
  ph.SetNumPieces(4);
  pl->AddPiece(ph);
  pl->Print();
  this->SetRequestedPieces(pl);
  pl->Delete();
}

//-----------------------------------------------------------------------------
vtkPolyData *vtkParallelPieceCacheFilter::GetAppendedData()
{
  return this->AppendResult;
}

//-----------------------------------------------------------------------------
void vtkParallelPieceCacheFilter::AppendPieces()
{
  if (this->AppendResult)
    {
    this->AppendResult->Delete();
    this->AppendResult = NULL;
    }

  if (!this->Cache.size())
    {
    return;
    }

  CacheType::iterator pos;
  vtkIdType cnt = 0;
  this->AppendFilter->SetNumberOfInputs(this->Cache.size());
  for (pos = this->Cache.begin(); pos != this->Cache.end(); )
    {
    vtkPolyData *content = vtkPolyData::SafeDownCast(pos->second);
    if (content)
      {
      this->AppendFilter->SetInputByNumber(cnt++, content);
      }
    pos++;
    }

  this->AppendFilter->SetNumberOfInputs(cnt);
  this->AppendFilter->Update();
  this->AppendResult = vtkPolyData::New();
  this->AppendResult->ShallowCopy(this->AppendFilter->GetOutput());
}

//------------------------------------------------------------------------------
int vtkParallelPieceCacheFilter::HasRequestedPieces()
{
  cerr << "PPCF(" << this << ") HasRequestedPieces?" << endl;
  if (this->RequestedPieces == NULL)
    {
    cerr << "NO PIECE LIST" << endl;
    }
  else
    {
    //cerr << "CHECKING:" << endl;
    //this->RequestedPieces->Print();
    }
  if (this->RequestedPieces != 0
      && this->RequestedPieces->GetNumberOfPieces() != 0)
    {
    int numProcessors = this->RequestedPieces->GetNumberOfPieces();
    for (int i = 0; i < numProcessors; i++)
      {
      vtkPiece ph = this->RequestedPieces->GetPiece(i);
      cerr << "PPCF(" << this << ") looking for ("
           << ph.GetProcessor() << ":"
           << ph.GetPiece() << "/"
           << ph.GetNumPieces() << "@"
           << ph.GetResolution() << "->["
           << ph.GetBounds()[0] << "-"
           << ph.GetBounds()[1] << ","
           << ph.GetBounds()[2] << "-"
           << ph.GetBounds()[3] << ","
           << ph.GetBounds()[4] << "-"
           << ph.GetBounds()[5] << "]=("
           << ph.GetPipelinePriority() << " "
           << ph.GetViewPriority() << " "
           << ph.GetCachedPriority() << ")" << endl;

      CacheType::iterator pos = this->Cache.find(ph);
      if (pos == this->Cache.end())
        {
        cerr << "PPCF(" << this << ")"
             << " MISSING " << ph.GetProcessor() << ":" << ph.GetPiece() << "/" << ph.GetNumPieces() << endl;
        return 0;
        }
      }
    cerr << "PPCF(" << this << ") FOUND EVERYTHING" << endl;
    return 1;
    }
  cerr << "PPCF(" << this << ") NO PIECES TO REQUEST" << endl;
  return 0;
}
