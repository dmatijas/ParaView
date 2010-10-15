/*=========================================================================

  Program:   Visualization Toolkit
  Module:    TestPriorityStreaming.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

// Tests that the stream harness class works as expected.

#include "vtkActor.h"
#include "vtkArrayCalculator.h"
#include "vtkCamera.h"
#include "vtkContourFilter.h"
#include "vtkDataSetMapper.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkInformationExecutivePortKey.h"
#include "vtkInformationVector.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkSmartPointer.h"
#include "vtkStreamedMandelbrot.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkStreamingDriver.h"
#include "vtkStreamingHarness.h"
#include "vtkTesting.h"

#include "vtksys/SystemTools.hxx"

//---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
  // create a streaming capable source.
  // it provides data and meta data (including world space bounds and
  // possibly scalar ranges) for any requested piece at any requested
  // resolution
  vtkSmartPointer<vtkStreamedMandelbrot> sms =
    vtkSmartPointer<vtkStreamedMandelbrot>::New();
  sms->SetWholeExtent(0,127,0,127,0,127);
  sms->SetOriginCX(-1.75,-1.25,0,0);

  // Set up a sample pipeline containing filters that pass meta info
  // downstream
  vtkSmartPointer<vtkContourFilter> contour =
    vtkSmartPointer<vtkContourFilter>::New();
  //contour->SetInput(id);
  contour->SetInputConnection(sms->GetOutputPort());
  contour->SetValue(0,0.0);

  // An access point to inject resolution into the pipeline
  vtkSmartPointer<vtkStreamingHarness> harness=
    vtkSmartPointer<vtkStreamingHarness>::New();
  harness->SetInputConnection(contour->GetOutputPort());
  harness->SetNumberOfPieces(16);
  harness->SetPiece(0);
  harness->SetResolution(0.0);

  //cerr << "PRIORITY IS" << harness->ComputePriority(0, 4, 0) << endl;
  harness->Update();

  //cerr << "PRIORITY NOW" << harness->ComputePriority(0, 4, 0) << endl;
  contour->SetValue(0,50.0);

  double bounds[6];
  double gconf;
  double min;
  double max;
  double aconf;

  harness->ComputeMetaInformation(0,16,0.0,
                                  bounds, gconf,
                                  min, max, aconf);
  cerr << "META INFO IS ";
  for (int i = 0; i < 6; i++)
    {
    cerr << bounds[i] << ",";
    }
  cerr << gconf << "\n " << min << " to " << max << " " << aconf << endl;

  harness->ComputeMetaInformation(0,16,1.0,
                                  bounds, gconf,
                                  min, max, aconf);
  cerr << "META INFO IS ";
  for (int i = 0; i < 6; i++)
    {
    cerr << bounds[i] << ",";
    }
  cerr << gconf << "\n " << min << " to " << max << " " << aconf << endl;

  harness->ComputeMetaInformation(0,4,0.5,
                                  bounds, gconf,
                                  min, max, aconf);
  cerr << "META INFO IS ";
  for (int i = 0; i < 6; i++)
    {
    cerr << bounds[i] << ",";
    }
  cerr << gconf << "\n" << min << " to " << max << " " << aconf << endl;

  harness->ComputeMetaInformation(1,4,0.5,
                                  bounds, gconf,
                                  min, max, aconf);
  cerr << "META INFO IS ";
  for (int i = 0; i < 6; i++)
    {
    cerr << bounds[i] << ",";
    }
  cerr << gconf << "\n" << min << " to " << max << " " << aconf << endl;

  harness->SetNumberOfPieces(4);
  harness->SetPiece(1);
  harness->SetResolution(0.5);
  harness->Update();

  harness->ComputeMetaInformation(0,4,0.5,
                                  bounds, gconf,
                                  min, max, aconf);
  cerr << "META INFO IS ";
  for (int i = 0; i < 6; i++)
    {
    cerr << bounds[i] << ",";
    }
  cerr << gconf << "\n" << min << " to " << max << " " << aconf << endl;

  harness->ComputeMetaInformation(1,4,0.5,
                                  bounds, gconf,
                                  min, max, aconf);
  cerr << "META INFO IS ";
  for (int i = 0; i < 6; i++)
    {
    cerr << bounds[i] << ",";
    }
  cerr << gconf << "\n" << min << " to " << max << " " << aconf << endl;

  vtkSmartPointer<vtkDataSetMapper> map1 =
    vtkSmartPointer<vtkDataSetMapper>::New();
  map1->SetInputConnection(harness->GetOutputPort());

  vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
  vtkSmartPointer<vtkRenderWindow> renWin =
    vtkSmartPointer<vtkRenderWindow>::New();
  renWin->AddRenderer(renderer);
  renderer->GetActiveCamera()->SetPosition( 0, 0, 10);
  renderer->GetActiveCamera()->SetFocalPoint(0, 0, 0);
  renderer->GetActiveCamera()->SetViewUp(0, 1, 0);
  renderer->SetBackground(0.0,0.0,0.0);
  renWin->SetSize(300,300);
  vtkSmartPointer<vtkRenderWindowInteractor> iren =
    vtkSmartPointer<vtkRenderWindowInteractor>::New();
  iren->SetRenderWindow(renWin);

  vtkSmartPointer<vtkActor> act1 = vtkSmartPointer<vtkActor>::New();
  act1->SetMapper(map1);
  renderer->AddActor(act1);

  vtkSmartPointer<vtkStreamingDriver> sd =
    vtkSmartPointer<vtkStreamingDriver>::New();
  sd->SetRenderWindow(renWin);
  sd->AddHarness(harness);

  renWin->Render();
  iren->Start();

  return 0;
}
