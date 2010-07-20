/*=========================================================================

  Program:   ParaView
  Module:    $RCSfile$

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkStreamingUpdateSuppressor -
// .SECTION Description

#ifndef __vtkStreamingUpdateSuppressor_h
#define __vtkStreamingUpdateSuppressor_h

#include "vtkPVUpdateSuppressor.h"

class vtkPieceList;
class vtkCharArray;
class vtkMPIMoveData;
class vtkPieceCacheFilter;
class vtkParallelPieceCacheFilter;
class vtkExtractSelectedFrustum;

class VTK_EXPORT vtkStreamingUpdateSuppressor : public vtkPVUpdateSuppressor
{
public:
  vtkTypeRevisionMacro(vtkStreamingUpdateSuppressor,vtkPVUpdateSuppressor);
  void PrintSelf(ostream& os, vtkIndent indent);
  static vtkStreamingUpdateSuppressor *New();

  //EXECUTION ----------------------------------------------------------
  // Description:
  // Force update on the input.
  virtual void ForceUpdate();

  //Description:
  //Define the current pass and the number of passes for streaming.
  //NumberOfPasses is essentially number of pieces local to this processor
  //Pass is essentially the current Piece
  vtkSetMacro(Pass, int);
  vtkGetMacro(Pass, int);
  vtkSetMacro(NumberOfPasses, int);
  vtkGetMacro(NumberOfPasses, int);
  void SetPassNumber(int Pass, int NPasses);

  //Description:
  //Tells the next forceupdate to try the append slot of the cache
  vtkSetMacro(UseAppend, int);
  vtkGetMacro(UseAppend, int);

  //PRIORITY COMPUTATION -----------------------------------------------
  //Description:
  //Camera and screen parameters needed for View Prioritization
  void SetCameraState(double *EyeUpAt);
  vtkGetVectorMacro(CameraState, double, 9);
  void SetFrustum(double *frustum);
  vtkGetVectorMacro(Frustum, double, 32);

  //Description:
  //Reset the piece ordering.
  void ClearPriorities();

  //Description:
  //Computes a priority for every piece by filter characteristics
  //This can only be called on the update supressor that is connected
  //to the pipeline that has the reader at its source (ie not on the client)
  void ComputeLocalPipelinePriorities();

  //Description:
  //Computes a priority for every piece based on its position in the viewing
  //frustum.
  void ComputeLocalViewPriorities();
  void ComputeGlobalViewPriorities();

  //Description:
  //Computes a priority that reflects the cached state
  void ComputeLocalCachePriorities();
  void ComputeGlobalCachePriorities();

  //Description:
  //Returns the number of pieces with non zero priorities in PieceList
  //TODO: This does not take into account view or cache priority
  //vtkGetMacro(MaxPass, int);

  //PIECE LIST TRANSFERS-------------------------------------------------
  //Description:
  //Get a hold of my PieceList, so it can be shared within a process
  vtkGetObjectMacro(PieceList, vtkPieceList);

  //Description:
  //Sets my PieceList to be the same as another
  void SetPieceList(vtkPieceList *other);

  //Description:
  //Copy the parallel piece lists across the network
  void SerializeLists();
  char * GetSerializedLists();
  void UnSerializeLists(char *);

  //PIPELINE CONTROL -----------------------------------------------------
  //Description:
  //Give direct access to the data transfer filter upstream
  void SetMPIMoveData(vtkMPIMoveData *mp)
  {
    this->MPIMoveData = mp;
  }
  //Description:
  //Make sure the data tranfer filter flows on all proessors
  void MarkMoveDataModified();

  //Description:
  //Give direct access to the caching filter upstream
  void SetPieceCacheFilter(vtkPieceCacheFilter *pcf)
  {
    this->PieceCacheFilter = pcf;
  }

  //Description:
  //Give direct access to the client side caching filter upstream
  void SetParallelPieceCacheFilter(vtkParallelPieceCacheFilter *pcf)
  {
    this->ParallelPieceCacheFilter = pcf;
  }

  //Description:
  //Ask the caching filter if all of the pieces for a given pass
  //are locally cached yet
  bool HasSliceCached(int pass);

  // DEBUGGING AIDS-------------------------------------------------------
  //Description:
  //For debugging
  void AddPieceList(vtkPieceList *newPL);
  void PrintPieceLists();
  void CopyBuddy(vtkStreamingUpdateSuppressor *buddy);
  void PrintSerializedLists();

//BTX

protected:
  vtkStreamingUpdateSuppressor();
  ~vtkStreamingUpdateSuppressor();

  //Description:
  //Determine the piece number for a given render pass
  //if Unspecified, will use this->Pass
  int GetPiece(int Pass = -1);

  //Description:
  //Send parallel priority lists to the root node of the server
  void GatherPriorities();

  //Description:
  //Receive parallel priority lists from the root node of the server
  void ScatterPriorities();

  //Description:
  //Number of passes to stream over
  int NumberOfPasses;

  //Description:
  //Current pass in working on
  int Pass;

  //Description:
  //The ordered list of pieces, which says which piece corresponds to which pass
  vtkPieceList *PieceList;

  //Description:
  //The root node of the server, and the client keep lists for all processors
  vtkPieceList **ParPLs;
  int NumPLs;

  //A copy of the piece list which server sends to client to stay in synch.
  char *SerializedLists;

  vtkMPIMoveData *MPIMoveData;
  vtkPieceCacheFilter *PieceCacheFilter;
  vtkParallelPieceCacheFilter *ParallelPieceCacheFilter;


  vtkExtractSelectedFrustum *FrustumTester;

  double *CameraState;
  double *Frustum;

  //Description::
  //Internal method used to compute view priority for each piece
  void ComputeViewPriorities(vtkPieceList *);
  double ComputeViewPriority(double *bbox);

  //Control
  int UseAppend;
private:
  vtkStreamingUpdateSuppressor(const vtkStreamingUpdateSuppressor&);  // Not implemented.
  void operator=(const vtkStreamingUpdateSuppressor&);  // Not implemented.

  enum
    {
    PRIORITY_COMMUNICATION_TAG=197001
    };

  //Description:
  //For debugging, helps to identify which US is spewing out messages
  void PrintPipe(vtkAlgorithm *);
//ETX

};

#endif
