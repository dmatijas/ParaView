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
    pqStreamingView.h
    )

  QT4_WRAP_UI(UI_SRCS
    )

  ADD_PARAVIEW_VIEW_MODULE(
    VIEW_IFACE VIEW_IFACE_SRCS
    VIEW_TYPE pqStreamingView
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
    ${VIEW_IFACE} ${OPTIONS_IFACE} ${DISPLAY_IFACE}
  GUI_SOURCES
    ${VIEW_IFACE_SRCS}
    ${UI_SRCS}
    ${MOC_SRCS}
    pqStreamingView.cxx
)
