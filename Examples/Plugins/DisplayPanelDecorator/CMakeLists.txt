# Create a display panel decorator that adds a new group box to the
# pqDisplayProxyEditor panel.


CMAKE_MINIMUM_REQUIRED(VERSION 2.4)

IF(NOT ParaView_BINARY_DIR)
  FIND_PACKAGE(ParaView REQUIRED)
  INCLUDE(${PARAVIEW_USE_FILE})
ENDIF(NOT ParaView_BINARY_DIR)

IF(PARAVIEW_BUILD_QT_GUI)
  QT4_WRAP_CPP(MOC_SRCS pqMyPanelDecorator.h)
  QT4_WRAP_UI(UI_SRCS pqMyPanelDecorator.ui)

  ADD_PARAVIEW_DISPLAY_PANEL_DECORATOR(
    IFACES IFACE_SRCS
    CLASS_NAME pqMyPanelDecorator
    PANEL_TYPES pqDisplayProxyEditor)

  # create a plugin
  ADD_PARAVIEW_PLUGIN(DisplayPanelDecorator "1.0"
    GUI_INTERFACES ${IFACES}
    SOURCES pqMyPanelDecorator.cxx ${MOC_SRCS} ${UI_SRCS} ${IFACE_SRCS} )
ENDIF(PARAVIEW_BUILD_QT_GUI)
