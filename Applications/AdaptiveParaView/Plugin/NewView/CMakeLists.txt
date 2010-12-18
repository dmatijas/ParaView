IF (ParaView_SOURCE_DIR)
  INCLUDE_DIRECTORIES(
    ${VTK_INCLUDE_DIR}
    ${PARAVIEW_INCLUDE_DIRS}
  )
ELSE (ParaView_SOURCE_DIR)
  FIND_PACKAGE(ParaView REQUIRED)
  INCLUDE(${PARAVIEW_USE_FILE})
ENDIF (ParaView_SOURCE_DIR)

IF(PARAVIEW_BUILD_QT_GUI)
  INCLUDE_DIRECTORIES(
    ${PARAVIEW_GUI_INCLUDE_DIRS}
    )
  INCLUDE(${QT_USE_FILE})

  QT4_WRAP_CPP(MOC_SRCS
    StreamingView.h
    )

  ADD_PARAVIEW_VIEW_MODULE( VIEW_IFACE VIEW_IFACE_SRCS
    VIEW_TYPE StreamingView
    VIEW_NAME StreamingView
    VIEW_XML_GROUP views
    VIEW_XML_NAME Streaming
    )

ENDIF()

add_paraview_plugin(
  StreamingView "2.0"
  REQUIRED_ON_SERVER
  REQUIRED_ON_CLIENT
  SERVER_MANAGER_SOURCES
    vtkPVStreamingRepresentation.cxx
    vtkPVStreamingView.cxx
    vtkSMStreamingRepresentationProxy.cxx
    vtkSMStreamingViewProxy.cxx
  SERVER_MANAGER_XML
    StreamingView.xml
  GUI_INTERFACES
    ${VIEW_IFACE}
  GUI_SOURCES
    ${MOC_SRCS}
    ${VIEW_IFACE_SRCS}
    StreamingView.cxx
)
