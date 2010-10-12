cmake_minimum_required(VERSION 2.8)

INCLUDE_DIRECTORIES(../..)
ADD_EXECUTABLE(AdaptiveStreaming AdaptiveMain.cxx)
TARGET_LINK_LIBRARIES(AdaptiveStreaming vtkAdaptive vtkRendering vtkFiltering vtkGraphics vtkCommon vtkImaging)
