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
#include "vtkStreamingMoveData.h"


#include "vtkAppendFilter.h"
#include "vtkAppendPolyData.h"
#include "vtkCellData.h"
#include "vtkCharArray.h"
#include "vtkCommunicator.h"
#include "vtkCompositeDataPipeline.h"
#include "vtkDataObject.h"
#include "vtkDataSetReader.h"
#include "vtkDataSetWriter.h"
#include "vtkImageAppend.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMultiPieceDataSet.h"
#include "vtkObjectFactory.h"
#include "vtkOutlineFilter.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkSmartPointer.h"
#include "vtkSocketController.h"
#include "vtkTimerLog.h"
#include "vtkToolkits.h"
#include "vtkUnstructuredGrid.h"

#include <vtksys/ios/sstream>

#ifdef VTK_USE_MPI
#include "vtkMPICommunicator.h"
#endif

#define EXTENT_HEADER_SIZE 360

vtkCxxRevisionMacro(vtkStreamingMoveData, "$Revision$");
vtkStandardNewMacro(vtkStreamingMoveData);

//-----------------------------------------------------------------------------
vtkStreamingMoveData::vtkStreamingMoveData()
{
}

//-----------------------------------------------------------------------------
vtkStreamingMoveData::~vtkStreamingMoveData()
{
}

//----------------------------------------------------------------------------
int vtkStreamingMoveData::FillOutputPortInformation(int, vtkInformation *info)
{
  if (this->Server == vtkMPIMoveData::CLIENT)
    {
    info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkMultiPieceDataSet");
    }
  else
    {
    info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkDataSet");
    }
  return 1;
}

//-----------------------------------------------------------------------------
void vtkStreamingMoveData::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//-----------------------------------------------------------------------------
int vtkStreamingMoveData::RequestDataObject(vtkInformation*,
                                      vtkInformationVector**,
                                      vtkInformationVector* outputVector)
{
//  cerr << "SMD(" << this << ") RDO" << endl;
  vtkDataObject* output =
    outputVector->GetInformationObject(0)->Get(vtkDataObject::DATA_OBJECT());

  vtkDataObject* outputCopy = 0;
  //------------------------------------------------------------------------
  if (this->Server == vtkMPIMoveData::CLIENT)
    {
    //Client produces a multipiece data set so that client can cache and manager
    //all pieces independently. Each piece is data produced on a different
    //processor
    if (output && output->IsA("vtkMultiPieceDataSet"))
      {
      return 1;
      }
    outputCopy = vtkMultiPieceDataSet::New();
    }
  else
  //------------------------------------------------------------------------
    {
    if (this->OutputDataType == VTK_POLY_DATA)
      {
      if (output && output->IsA("vtkPolyData"))
        {
        return 1;
        }
      outputCopy = vtkPolyData::New();
      }
    else if (this->OutputDataType == VTK_UNSTRUCTURED_GRID)
      {
      if (output && output->IsA("vtkUnstructuredGrid"))
        {
        return 1;
        }
      outputCopy = vtkUnstructuredGrid::New();
      }
    else if (this->OutputDataType == VTK_IMAGE_DATA)
      {
      if (output && output->IsA("vtkImageData"))
        {
        return 1;
        }
      outputCopy = vtkImageData::New();
      }
    else
      {
      vtkErrorMacro("Unrecognized output type: " << this->OutputDataType
                    << ". Cannot create output.");
      return 0;
      }
    }

  outputCopy->SetPipelineInformation(outputVector->GetInformationObject(0));
  outputCopy->Delete();
  return 1;
}

//-----------------------------------------------------------------------------
void vtkStreamingMoveData::DataServerGatherToZero(vtkDataSet* input,
                                                  vtkDataSet* output)
{
//  cerr << "SMD(" << this << ") DataServerGatherToZero" << endl;

  int numProcs= this->Controller->GetNumberOfProcesses();
  if (numProcs == 1)
    {
//    cerr << "ONLY 1 PROC" << endl;
//    cerr << "NUMBUFFS = " << this->NumberOfBuffers << endl;
    this->ClearBuffer();
    this->MarshalDataToBuffer(input);
    output->ShallowCopy(input);
    return;
    }
//  cerr << "NUMPROCS = " << numProcs << endl;

  vtkTimerLog::MarkStartEvent("Dataserver gathering to 0");

#ifdef VTK_USE_MPI
  cerr << "USING MPI " << endl;
  int idx;
  int myId= this->Controller->GetLocalProcessId();
  vtkMPICommunicator* com = vtkMPICommunicator::SafeDownCast(
                                         this->Controller->GetCommunicator());

  if (com == 0)
    {
    vtkErrorMacro("MPICommunicator neededfor this operation.");
    return;
    }
  this->ClearBuffer();
  this->MarshalDataToBuffer(input);

  // Save a copy of the buffer so we can receive into the buffer.
  // We will be responsiblefor deleting the buffer.
  // This assumes one buffer. MashalData will produce only one buffer
  // One data set, one buffer.
  vtkIdType inBufferLength = this->BufferTotalLength;
  char *inBuffer = this->Buffers;
  this->Buffers = NULL;
  this->ClearBuffer();

  if (myId == 0)
    {
    // Allocate arrays used by the AllGatherV call.
    this->BufferLengths = new vtkIdType[numProcs];
    this->BufferOffsets = new vtkIdType[numProcs];
    }

  // Compute the degenerate input offsets and lengths.
  // Broadcast our size to process 0.
  com->Gather(&inBufferLength, this->BufferLengths, 1, 0);

  // Compute the displacements.
  this->BufferTotalLength = 0;
  if (myId == 0)
    {
    for (idx = 0; idx < numProcs; ++idx)
      {
      this->BufferOffsets[idx] = this->BufferTotalLength;
      this->BufferTotalLength += this->BufferLengths[idx];
      }
    // Gather the marshaled data sets to 0.
    this->Buffers = new char[this->BufferTotalLength];
    }
  com->GatherV(inBuffer, this->Buffers, inBufferLength,
                  this->BufferLengths, this->BufferOffsets, 0);
  this->NumberOfBuffers = numProcs;

  if (myId == 0)
    {
    this->ReconstructDataFromBuffer(output);
    }

  //int fixme; // Do not clear buffers here
  //------------------------------------------------------------------------------
  //Do not clear the buffer, keep it just like it is (parallel)
#if 0
  this->ClearBuffer();
#endif
  //------------------------------------------------------------------------------

  delete [] inBuffer;
  inBuffer = NULL;
#endif

  vtkTimerLog::MarkEndEvent("Dataserver gathering to 0");
}

//-----------------------------------------------------------------------------
void vtkStreamingMoveData::DataServerSendToClient(vtkDataSet* output)
{
//  cerr << "SMD(" << this << ") DataServerSendToClient" << endl;
//  cerr << "TO SEND NUMBUFFS" << this->NumberOfBuffers << endl;

  int myId = this->Controller->GetLocalProcessId();

  if (myId == 0)
    {
    vtkTimerLog::MarkStartEvent("Dataserver sending to client");

    vtkSmartPointer<vtkDataSet> tosend = output;
    if (this->DeliverOutlineToClient)
      {
      // reduce data using outline filter.
      if (output->IsA("vtkPolyData"))
        {
        vtkDataSet* clone = output->NewInstance();
        clone->ShallowCopy(output);

        vtkOutlineFilter* filter = vtkOutlineFilter::New();
        filter->SetInput(clone);
        filter->Update();
        tosend = filter->GetOutput();
        filter->Delete();
        clone->Delete();
        }
      else
        {
        vtkErrorMacro("DeliverOutlineToClient can only be used for vtkPolyData.");
        }
      }

    //------------------------------------------------------------------------------
    //cerr << "DATA SERVER SEND TO CLIENT " << this->NumberOfBuffers << endl;
    //DO NOT CLEAR, JUST SENT THE PARALLEL BUFFER DIRECTLY OVER
#if 0
    this->ClearBuffer();
    this->MarshalDataToBuffer(tosend);
#endif
    //------------------------------------------------------------------------------

//    cerr << "SENDING NUMBUFFS" << this->NumberOfBuffers << endl;
    this->ClientDataServerSocketController->Send(
                                     &(this->NumberOfBuffers), 1, 1, 23490);
    this->ClientDataServerSocketController->Send(this->BufferLengths,
                                     this->NumberOfBuffers, 1, 23491);
    this->ClientDataServerSocketController->Send(this->Buffers,
                                     this->BufferTotalLength, 1, 23492);
    this->ClearBuffer();
    vtkTimerLog::MarkEndEvent("Dataserver sending to client");
    }

}

//-----------------------------------------------------------------------------
void vtkStreamingMoveData::ClientReceiveFromDataServer(vtkDataObject* doutput)
{
//  cerr << "SMD(" << this << ") ClientReceiveFromDataServer" << endl;

  vtkMultiPieceDataSet *output = vtkMultiPieceDataSet::SafeDownCast(doutput);
  if (!output)
    {
    vtkErrorMacro("I was expecting to make a multi piece dataset.");
    return;
    }

  vtkCommunicator* com = 0;
  com = this->ClientDataServerSocketController->GetCommunicator();
  if (com == 0)
    {
    vtkErrorMacro("Missing socket controler on cleint.");
    return;
    }

  this->ClearBuffer();
  com->Receive(&(this->NumberOfBuffers), 1, 1, 23490);
//  cerr << "RECEIVED NUMBUFFS " << this->NumberOfBuffers << endl;

  this->BufferLengths = new vtkIdType[this->NumberOfBuffers];
  com->Receive(this->BufferLengths, this->NumberOfBuffers,
                                  1, 23491);
  // Compute additional buffer information.
  this->BufferOffsets = new vtkIdType[this->NumberOfBuffers];
  this->BufferTotalLength = 0;
  for (int idx = 0; idx < this->NumberOfBuffers; ++idx)
    {
    this->BufferOffsets[idx] = this->BufferTotalLength;
    this->BufferTotalLength += this->BufferLengths[idx];
    }
  this->Buffers = new char[this->BufferTotalLength];
  com->Receive(this->Buffers, this->BufferTotalLength,
                                  1, 23492);
  this->ClientReconstructDataFromBuffer(doutput);
  this->ClearBuffer();
}

//-----------------------------------------------------------------------------
void vtkStreamingMoveData::MarshalDataToBuffer(vtkDataSet* data)
{
//  cerr << "SMD(" << this << ") MashalData" << endl;
  // Protect from empty data.
  if (data->GetNumberOfPoints() == 0)
    {
    this->NumberOfBuffers = 0;
    }

  vtkImageData* imageData = vtkImageData::SafeDownCast(data);

  // Copy input to isolate reader from the pipeline.
  vtkDataSet* d = data->NewInstance();
  d->CopyStructure(data);
  d->GetPointData()->PassData(data->GetPointData());
  d->GetCellData()->PassData(data->GetCellData());
  // Marshal with writer.
  vtkDataSetWriter *writer = vtkDataSetWriter::New();
  writer->SetFileTypeToBinary();
  writer->WriteToOutputStringOn();
  writer->SetInput(d);
  writer->Write();
  // Get string.
  this->NumberOfBuffers = 1;
  this->BufferLengths = new vtkIdType[1];
  this->BufferLengths[0] = writer->GetOutputStringLength();
  this->BufferOffsets = new vtkIdType[1];
  this->BufferOffsets[0] = 0;

  if (imageData)
    {
    // we need to add marshall extents separately, since the writer doesn't
    // preserve extents.
    int *extent = imageData->GetExtent();
    double* origin = imageData->GetOrigin();
    vtksys_ios::ostringstream stream;
    stream << "EXTENT " << extent[0] << " " <<
      extent[1] << " " <<
      extent[2] << " " <<
      extent[3] << " " <<
      extent[4] << " " <<
      extent[5];
    stream << " ORIGIN: " << origin[0] << " " << origin[1] << " " << origin[2];

    if (stream.str().size() >= EXTENT_HEADER_SIZE)
      {
      vtkErrorMacro("Extent message too long!");
      this->Buffers = writer->RegisterAndGetOutputString();
      }
    else
      {
      char extentHeader[EXTENT_HEADER_SIZE];
      strcpy(extentHeader, stream.str().c_str());
      this->BufferLengths[0] += EXTENT_HEADER_SIZE;
      this->Buffers = new char[this->BufferLengths[0]];
      memcpy(this->Buffers, extentHeader, EXTENT_HEADER_SIZE);
      memcpy(this->Buffers+EXTENT_HEADER_SIZE, writer->GetOutputString(),
        writer->GetOutputStringLength());
      }
    }
  else
    {
    vtkInformation* dataInfo = data->GetInformation();
    //--------------------------------------------------------------------------
    //Have to recover individual pieces from the serialized whole.
    //This code inserts that meta-info that identifies each piece
    //into the stream ahead of the serialized data.
    if (dataInfo->Has(vtkDataObject::DATA_PIECE_NUMBER()))
      {
      cerr << "HAS PIECE NUMBER" << endl;

      int myId = this->Controller->GetLocalProcessId();
      int numProcs = this->Controller->GetNumberOfProcesses();

      int pnum = dataInfo->Get(vtkDataObject::DATA_PIECE_NUMBER());
      int np = dataInfo->Get(vtkDataObject::DATA_NUMBER_OF_PIECES());
      double res = dataInfo->Get(vtkDataObject::DATA_RESOLUTION());

      vtksys_ios::ostringstream stream;
      stream << "PIECEINFO " << myId << " " << numProcs << " "
             << pnum << " " << np << " " << res;
      cerr << "SENDING PIECEINFO " << myId << " " << numProcs << " "
             << pnum << " " << np << " " << res;
      char extentHeader[256];
      strcpy(extentHeader, stream.str().c_str());
      this->BufferLengths[0] += 256;
      this->Buffers = new char[this->BufferLengths[0]];
      memcpy(this->Buffers, extentHeader, 256);
      memcpy(this->Buffers+256, writer->GetOutputString(),
        writer->GetOutputStringLength());
      }
    else
      {
      cerr << "NO PIECE NUMBER" << endl;
      this->Buffers = writer->RegisterAndGetOutputString();
      }
    }
    //--------------------------------------------------------------------------

  this->BufferTotalLength = this->BufferLengths[0];


  d->Delete();
  d = 0;
  writer->Delete();
  writer = 0;
}

//-----------------------------------------------------------------------------
void vtkStreamingMoveData::ReconstructDataFromBuffer(vtkDataSet* data)
{
//  cerr << "SMD(" << this << ") ReconstructDataFromBuffer" << endl;
  if (this->NumberOfBuffers == 0 || this->Buffers == 0)
    {
    data->Initialize();
    return;
    }

  // PolyData and Unstructured grid need different append filters.
  vtkAppendPolyData* appendPd = NULL;
  vtkAppendFilter*   appendUg = NULL;
  vtkImageAppend* appendId = NULL;
  if (this->NumberOfBuffers > 1)
    {
    if (data->IsA("vtkPolyData"))
      {
      appendPd = vtkAppendPolyData::New();
      }
    else if (data->IsA("vtkUnstructuredGrid"))
      {
      appendUg = vtkAppendFilter::New();
      }
    else if (data->IsA("vtkImageData"))
      {
      appendId = vtkImageAppend::New();
      appendId->PreserveExtentsOn();
      }
    else
      {
      vtkErrorMacro("This filter only handles unstructured data.");
      return;
      }
    }

  for (int idx = 0; idx < this->NumberOfBuffers; ++idx)
    {
    // Setup a reader.
    vtkDataSetReader *reader = vtkDataSetReader::New();
    reader->ReadFromInputStringOn();

    char* bufferArray = this->Buffers+this->BufferOffsets[idx];
    vtkIdType bufferLength = this->BufferLengths[idx];

    int extent[6]= {0, 0, 0, 0, 0, 0};
    float origin[3] = {0, 0, 0};
    bool extentAvailable = false;
    if (bufferLength >= EXTENT_HEADER_SIZE &&
      strncmp(bufferArray, "EXTENT", 6)==0)
      {
      sscanf(bufferArray, "EXTENT %d %d %d %d %d %d ORIGIN %f %f %f", &extent[0], &extent[1],
        &extent[2], &extent[3], &extent[4], &extent[5],
        &origin[0], &origin[1], &origin[2]);
      bufferArray += EXTENT_HEADER_SIZE;
      bufferLength -= EXTENT_HEADER_SIZE;
      extentAvailable = true;
      }

    //--------------------------------------------------------------------------
    //Since the server marshals piece meta inforamtion into the buffer,
    //it also has to skip over the same information when it processes the stream
    //locally
    if (bufferLength >= EXTENT_HEADER_SIZE &&
      strncmp(bufferArray, "PIECEINFO", 9)==0)
      {
      int pId;
      int nProcs;
      int pNum;
      int nPieces;
      double res;
      sscanf(bufferArray, "PIECEINFO %d %d %d %d %lf",
             &pId, &nProcs, &pNum, &nPieces, &res);
//      cerr << "GOT " << pId << " " << nProcs << " " << pNum << " " << nPieces << " " << res << endl;
      bufferArray += 256;
      bufferLength -= 256;
      }
    //--------------------------------------------------------------------------

    vtkCharArray* mystring = vtkCharArray::New();
    mystring->SetArray(bufferArray, bufferLength, 1);
    reader->SetInputArray(mystring);
    reader->Modified(); // For append loop
    reader->Update();
    if (appendPd)
      {
      appendPd->AddInput(reader->GetPolyDataOutput());
      }
    else if (appendUg)
      {
      appendUg->AddInput(reader->GetUnstructuredGridOutput());
      }
    else if (appendId)
      {
      vtkImageData* curInput = vtkImageData::SafeDownCast(
        reader->GetOutput());
      if (curInput->GetNumberOfPoints() >0)
        {
        if (extentAvailable)
          {
          vtkImageData* clone = vtkImageData::New();
          clone->ShallowCopy(curInput);
          clone->SetOrigin(origin[0], origin[1], origin[2]);
          clone->SetExtent(extent);
          appendId->AddInput(clone);
          clone->Delete();
          }
        else
          {
          appendId->AddInput(curInput);
          }
        }
      }
    else
      {
      vtkDataSet* out = reader->GetOutput();
      data->CopyStructure(out);
      data->GetPointData()->PassData(out->GetPointData());
      data->GetCellData()->PassData(out->GetCellData());
      }
    mystring->Delete();
    mystring = 0;
    reader->Delete();
    reader = NULL;
    }

  if (appendPd)
    {
    vtkDataSet* out = appendPd->GetOutput();
    out->Update();
    data->CopyStructure(out);
    data->GetPointData()->PassData(out->GetPointData());
    data->GetCellData()->PassData(out->GetCellData());
    appendPd->Delete();
    appendPd = NULL;
    }
  if (appendUg)
    {
    vtkDataSet* out = appendUg->GetOutput();
    out->Update();
    data->CopyStructure(out);
    data->GetPointData()->PassData(out->GetPointData());
    data->GetCellData()->PassData(out->GetCellData());
    appendUg->Delete();
    appendUg = NULL;
    }
  if (appendId)
    {
    appendId->Update();
    vtkDataSet* out = appendId->GetOutput();
    data->CopyStructure(out);
    data->GetPointData()->PassData(out->GetPointData());
    data->GetCellData()->PassData(out->GetCellData());
    appendId->Delete();
    appendId = NULL;
    }
}

//-----------------------------------------------------------------------------
void vtkStreamingMoveData::ClientReconstructDataFromBuffer(vtkDataObject* dsdata)
{
//  cerr << "SMD(" << this << ") ClientReconstructDataFromBuffer" << endl;
  //Client takes the N serialied datasets out of the buffer
  //and populates the multipiece data set it produces from them.
  vtkMultiPieceDataSet *data = vtkMultiPieceDataSet::SafeDownCast(dsdata);
  if (!data)
    {
    vtkErrorMacro("I was expecting to make a multi piece dataset.");
    return;
    }

  if (this->NumberOfBuffers == 0 || this->Buffers == 0)
    {
/*
    cerr << "EARLY EXIT"
         << " NUMBUFFS = " << this->NumberOfBuffers
         << " BUFFERS =" << this->Buffers << endl;
*/
    data->Initialize();
    return;
    }
  data->SetNumberOfPieces(this->NumberOfBuffers);

  // PolyData and Unstructured grid need different append filters.
  for (int idx = 0; idx < this->NumberOfBuffers; ++idx)
    {
//    cerr << "READ " << idx << endl;

    // Setup a reader.
    vtkDataSetReader *reader = vtkDataSetReader::New();
    reader->ReadFromInputStringOn();

    char* bufferArray = this->Buffers+this->BufferOffsets[idx];
    vtkIdType bufferLength = this->BufferLengths[idx];

    int extent[6]= {0, 0, 0, 0, 0, 0};
    float origin[3] = {0, 0, 0};
    bool extentAvailable = false;
    if (bufferLength >= EXTENT_HEADER_SIZE &&
      strncmp(bufferArray, "EXTENT", 6)==0)
      {
      sscanf(bufferArray, "EXTENT %d %d %d %d %d %d ORIGIN %f %f %f", &extent[0], &extent[1],
        &extent[2], &extent[3], &extent[4], &extent[5],
        &origin[0], &origin[1], &origin[2]);
      bufferArray += EXTENT_HEADER_SIZE;
      bufferLength -= EXTENT_HEADER_SIZE;
      extentAvailable = true;
      }

    //--------------------------------------------------------------------------
    // Parse the serialized meta info to figure out who sent the next piece
    // and which exact piece it is
    int pId = idx;
    int nProcs = this->NumberOfBuffers;
    int pNum = 0;
    int nPieces = 1;
    double res = -1.0;

    if (bufferLength >= 256 &&
      strncmp(bufferArray, "PIECEINFO", 9)==0)
      {
      cerr << "I SEE PIECEINFO" << endl;
      sscanf(bufferArray, "PIECEINFO %d %d %d %d %lf",
             &pId, &nProcs, &pNum, &nPieces, &res);
      cerr << idx << " GOT " << pId << " " << nProcs << " " << pNum << " " << nPieces << " " << res << endl;
      cerr << "convert to local index for later reuse" << endl;
      nPieces = nPieces/nProcs;
      pNum = pNum % nPieces;
      cerr << idx << " NOW " << pId << " " << nProcs << " " << pNum << " " << nPieces << " " << res << endl;
      bufferArray += 256;
      bufferLength -= 256;
      }
    else
      {
      cerr << "NO PIECEINFO" << endl;
      }
    //--------------------------------------------------------------------------

    vtkCharArray* mystring = vtkCharArray::New();
    mystring->SetArray(bufferArray, bufferLength, 1);
    reader->SetInputArray(mystring);
    reader->Modified(); // For append loop
    reader->Update();
    if (this->OutputDataType == VTK_POLY_DATA)
      {
      vtkDataSet *ds = reader->GetPolyDataOutput();
      vtkInformation* dataInfo = ds->GetInformation();
      dataInfo->Set(vtkDataObject::DATA_PIECE_NUMBER(), pNum);
      dataInfo->Set(vtkDataObject::DATA_NUMBER_OF_PIECES(), nPieces);
      dataInfo->Set(vtkDataObject::DATA_RESOLUTION(), res);

      data->SetPiece(idx, ds);

      cerr << "RECONSTRUCTING FROM BUFFER " << idx << " " ;
      cerr << "NPTS = " << reader->GetPolyDataOutput()->GetNumberOfPoints() << " ";
      double bds[6];
      reader->GetPolyDataOutput()->GetBounds(bds);
      cerr << "BDS = "
           << bds[0] << "," << bds[1] << ","
           << bds[2] << "," << bds[3] << ","
           << bds[4] << "," << bds[5] << endl;

      }
    else if (this->OutputDataType == VTK_UNSTRUCTURED_GRID)
      {
//      cerr << "UG" << endl;
      data->SetPiece(idx, reader->GetUnstructuredGridOutput());
      }
    else if (this->OutputDataType == VTK_IMAGE_DATA)
      {
//      cerr << "ID" << endl;
      vtkImageData* curInput = vtkImageData::SafeDownCast(
        reader->GetOutput());
      if (curInput->GetNumberOfPoints() >0)
        {
        if (extentAvailable)
          {
          vtkImageData* clone = vtkImageData::New();
          clone->ShallowCopy(curInput);
          clone->SetOrigin(origin[0], origin[1], origin[2]);
          clone->SetExtent(extent);
          data->SetPiece(idx, clone);
          clone->Delete();
          }
        else
          {
          data->SetPiece(idx, curInput);
          }
        }
      }
    else
      {
//      cerr << "WEIRDO" << endl;
      vtkDataSet* out = reader->GetOutput();
      vtkDataSet* mine = out->NewInstance();
      mine->CopyStructure(out);
      mine->GetPointData()->PassData(out->GetPointData());
      mine->GetCellData()->PassData(out->GetCellData());
      data->SetPiece(idx, mine);
      mine->Delete();
      }

    mystring->Delete();
    mystring = 0;
    reader->Delete();
    reader = NULL;
    }
}

//------------------------------------------------------------------------------
void vtkStreamingMoveData::PrintMe()
{
//  cerr << "SMD(" << this << ") PrintMe" << endl;
  this->PrintSelf(cerr, vtkIndent(0));
  cerr << "MY OUTPUT ------------------------" << endl;
  if (this->GetMultiPieceOutput())
    {
    cerr << this->GetMultiPieceOutput()->GetClassName() << endl;
    cerr << this->GetMultiPieceOutput()->GetNumberOfPoints() << endl;
    }
  else if (this->GetOutput())
    {
    cerr << this->GetOutput()->GetClassName() << endl;
    vtkDataSet*ds = vtkDataSet::SafeDownCast(this->GetOutput());
    if (ds)
      {
      cerr << ds->GetNumberOfPoints() << endl;
      }
    }
  else
    {
    cerr << "NO OUTPUT" << endl;
    }
}


//----------------------------------------------------------------------------
vtkExecutive* vtkStreamingMoveData::CreateDefaultExecutive()
{
  return vtkCompositeDataPipeline::New();
}

//----------------------------------------------------------------------------
vtkMultiPieceDataSet* vtkStreamingMoveData::GetMultiPieceOutput()
{
  return this->GetMultiPieceOutput(0);
}

//----------------------------------------------------------------------------
vtkMultiPieceDataSet* vtkStreamingMoveData::GetMultiPieceOutput(int port)
{
  vtkDataObject* output =
    vtkCompositeDataPipeline::SafeDownCast(this->GetExecutive())->
    GetCompositeOutputData(port);
  return vtkMultiPieceDataSet::SafeDownCast(output);
}
