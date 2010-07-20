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
// .NAME vtkStreamingMoveData - Moves/redistributes data between processes.
// .SECTION Description
// This class produces a multipiece data set on the client. Each piece
// in the multipiece data set originates in a different processor on the
// server.

#ifndef __vtkStreamingMoveData_h
#define __vtkStreamingMoveData_h

#include "vtkMPIMoveData.h"

class vtkMultiPieceDataSet;

class VTK_EXPORT vtkStreamingMoveData : public vtkMPIMoveData
{
public:
  static vtkStreamingMoveData *New();
  vtkTypeRevisionMacro(vtkStreamingMoveData, vtkMPIMoveData);
  void PrintSelf(ostream& os, vtkIndent indent);

  vtkGetMacro(Server, int);

  // Description:
  // Get the output data object for a port on this algorithm.
  vtkMultiPieceDataSet* GetMultiPieceOutput();
  vtkMultiPieceDataSet* GetMultiPieceOutput(int);

  virtual void PrintMe();
protected:
  vtkStreamingMoveData();
  ~vtkStreamingMoveData();

  virtual int FillOutputPortInformation(int port, vtkInformation* info);
  virtual int RequestDataObject(vtkInformation* request,
                           vtkInformationVector** inputVector,
                           vtkInformationVector* outputVector);


  virtual void DataServerGatherToZero(vtkDataSet* input, vtkDataSet* output);
  virtual void DataServerSendToClient(vtkDataSet* output);
  virtual void ClientReceiveFromDataServer(vtkDataObject* output);

  virtual void MarshalDataToBuffer(vtkDataSet* data);
  virtual void ReconstructDataFromBuffer(vtkDataSet* data);
  virtual void ClientReconstructDataFromBuffer(vtkDataObject* data);

  // Create a default executive.
  virtual vtkExecutive* CreateDefaultExecutive();

private:

  vtkStreamingMoveData(const vtkStreamingMoveData&); // Not implemented
  void operator=(const vtkStreamingMoveData&); // Not implemented
};

#endif
