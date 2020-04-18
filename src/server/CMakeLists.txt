set(SERVER_LIB_SRCS
    ../compat/wayland-xdg-shell-v5-protocol.c
    appmenu_interface.cpp
    blur_interface.cpp
    buffer_interface.cpp
    clientconnection.cpp
    compositor_interface.cpp
    contrast_interface.cpp
    display.cpp
    eglstream_controller_interface.cpp
    fakeinput_interface.cpp
    filtered_display.cpp
    global.cpp
    idle_interface.cpp
    idleinhibit_interface.cpp
    idleinhibit_interface_v1.cpp
    keystate_interface.cpp
    linuxdmabuf_v1_interface.cpp
    output_interface.cpp
    output_changeset_v1.cpp
    output_configuration_v1_interface.cpp
    output_device_v1_interface.cpp
    output_management_v1_interface.cpp
    plasmashell_interface.cpp
    plasmavirtualdesktop_interface.cpp
    plasmawindowmanagement_interface.cpp
    qtsurfaceextension_interface.cpp
    region_interface.cpp
    remote_access_interface.cpp
    resource.cpp
    seat_interface.cpp
    server_decoration_interface.cpp
    server_decoration_palette_interface.cpp
    shadow_interface.cpp
    shell_interface.cpp
    slide_interface.cpp
    subcompositor_interface.cpp
    surface_interface.cpp
    surfacerole.cpp
    textinput_interface.cpp
    textinput_interface_v0.cpp
    textinput_interface_v2.cpp
    touch_interface.cpp
    viewporter_interface.cpp
    xdgdecoration_interface.cpp
    xdgforeign_interface.cpp
    xdgforeign_v2_interface.cpp
    xdgoutput_interface.cpp
    xdgshell_interface.cpp
    xdgshell_stable_interface.cpp
    xdgshell_v5_interface.cpp
    xdgshell_v5_interface.cpp
    xdgshell_v6_interface.cpp


    ../../server/client.cpp
    ../../server/data_device.cpp
    ../../server/data_device_manager.cpp
    ../../server/data_offer.cpp
    ../../server/data_source.cpp
    ../../server/display.cpp
    ../../server/dpms.cpp
    ../../server/output.cpp
    ../../server/pointer.cpp
    ../../server/pointer_constraints_v1.cpp
    ../../server/pointer_gestures_v1.cpp
    ../../server/relative_pointer_v1.cpp
    ../../server/seat.cpp
    ../../server/wayland/client.cpp
    ../../server/wayland/display.cpp

    ../../server/keyboard.cpp
)

ecm_qt_declare_logging_category(SERVER_LIB_SRCS HEADER logging.h IDENTIFIER WRAPLAND_SERVER CATEGORY_NAME wrapland-server DEFAULT_SEVERITY Critical)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/kwinft-output-management-unstable-v1.xml
    BASENAME output-management-v1
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/kwinft-output-device-unstable-v1.xml
    BASENAME output-device-v1
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/plasma-shell.xml
    BASENAME plasma-shell
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/plasma-virtual-desktop.xml
    BASENAME plasma-virtual-desktop
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/plasma-window-management.xml
    BASENAME plasma-window-management
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/surface-extension.xml
    BASENAME qt-surface-extension
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/idle.xml
    BASENAME idle
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/fake-input.xml
    BASENAME fake-input
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/shadow.xml
    BASENAME shadow
)
ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/dpms.xml
    BASENAME dpms
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/blur.xml
    BASENAME blur
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/contrast.xml
    BASENAME contrast
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml
    BASENAME relativepointer-unstable-v1
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/slide.xml
    BASENAME slide
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/server-decoration.xml
    BASENAME server_decoration
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/text-input.xml
    BASENAME text
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/text-input-unstable-v2.xml
    BASENAME text-input-unstable-v2
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/xdg-shell/xdg-shell-unstable-v6.xml
    BASENAME xdg-shell-v6
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/pointer-gestures/pointer-gestures-unstable-v1.xml
    BASENAME pointer-gestures-unstable-v1
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml
    BASENAME pointer-constraints-unstable-v1
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml
    BASENAME xdg-foreign-unstable-v2
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml
    BASENAME idle-inhibit-unstable-v1
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/appmenu.xml
    BASENAME appmenu
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/server-decoration-palette.xml
    BASENAME server_decoration_palette
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/remote-access.xml
    BASENAME remote-access
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/stable/viewporter/viewporter.xml
    BASENAME viewporter
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/xdg-output/xdg-output-unstable-v1.xml
    BASENAME xdg-output
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml
    BASENAME xdg-shell
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
    BASENAME xdg-decoration
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/wayland-eglstream-controller.xml
    BASENAME eglstream-controller
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/keystate.xml
    BASENAME keystate
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml
    BASENAME linux-dmabuf-unstable-v1
)

set(SERVER_GENERATED_SRCS
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-blur-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-blur-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-contrast-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-contrast-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-dpms-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-dpms-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-eglstream-controller-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-fake-input-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-fake-input-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-idle-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-idle-inhibit-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-idle-inhibit-unstable-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-idle-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-linux-dmabuf-unstable-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-output-device-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-output-device-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-output-management-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-output-management-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-shell-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-shell-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-shell-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-virtual-desktop-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-virtual-desktop-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-window-management-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-window-management-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-pointer-constraints-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-pointer-constraints-unstable-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-pointer-gestures-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-pointer-gestures-unstable-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-qt-surface-extension-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-qt-surface-extension-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-relativepointer-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-relativepointer-unstable-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-server_decoration-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-server_decoration-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-server_decoration_palette-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-server_decoration_palette-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-shadow-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-shadow-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-slide-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-slide-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-text-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-text-input-unstable-v2-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-text-input-unstable-v2-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-text-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-viewporter-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-viewporter-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-decoration-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-decoration-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-foreign-unstable-v2-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-foreign-unstable-v2-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-shell-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-shell-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-shell-v6-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-shell-v6-server-protocol.h
)

set_source_files_properties(${SERVER_GENERATED_SRCS} PROPERTIES SKIP_AUTOMOC ON)

add_library(WraplandServer ${SERVER_LIB_SRCS})
add_library(Wrapland::Server ALIAS WraplandServer)

ecm_generate_export_header(WraplandServer
    BASE_NAME
        WraplandServer
    EXPORT_FILE_NAME
        Wrapland/Server/wraplandserver_export.h
    VERSION ${PROJECT_VERSION}
)

target_include_directories(WraplandServer INTERFACE
    "$<INSTALL_INTERFACE:${INCLUDE_INSTALL_DIR}/Wrapland/Server>")

target_link_libraries(WraplandServer
    PUBLIC Qt5::Gui
    PRIVATE
        Wayland::Server
        EGL::EGL
        Qt5::Concurrent
)

set_target_properties(WraplandServer PROPERTIES VERSION   ${WRAPLAND_VERSION_STRING}
                                                 SOVERSION ${WRAPLAND_SOVERSION}
                                                 EXPORT_NAME WraplandServer
)
kde_target_enable_exceptions(WraplandServer PUBLIC)

install(TARGETS WraplandServer EXPORT WraplandTargets ${INSTALL_TARGETS_DEFAULT_ARGS})

set(SERVER_LIB_HEADERS
  ${CMAKE_CURRENT_BINARY_DIR}/Wrapland/Server/wraplandserver_export.h
  appmenu_interface.h
  blur_interface.h
  buffer_interface.h
  clientconnection.h
  compositor_interface.h
  contrast_interface.h
  display.h
  eglstream_controller_interface.h
  fakeinput_interface.h
  filtered_display.h
  global.h
  idle_interface.h
  idleinhibit_interface.h
  keystate_interface.h
  linuxdmabuf_v1_interface.h
  output_interface.h
  output_changeset_v1.h
  output_configuration_v1_interface.h
  output_device_v1_interface.h
  output_management_v1_interface.h
  plasmashell_interface.h
  plasmavirtualdesktop_interface.h
  plasmawindowmanagement_interface.h
  qtsurfaceextension_interface.h
  region_interface.h
  remote_access_interface.h
  resource.h
  server_decoration_interface.h
  server_decoration_palette_interface.h
  shadow_interface.h
  shell_interface.h
  slide_interface.h
  subcompositor_interface.h
  surface_interface.h
  textinput_interface.h
  touch_interface.h
  viewporter_interface.h
  xdgdecoration_interface.h
  xdgforeign_interface.h
  xdgoutput_interface.h
  xdgshell_interface.h

  ../../server/client.h
  ../../server/data_device.h
  ../../server/data_device_manager.h
  ../../server/data_offer.h
  ../../server/data_source.h
  ../../server/display.h
  ../../server/dpms.h
  ../../server/keyboard.h
  ../../server/output.h
  ../../server/pointer.h
  ../../server/pointer_constraints_v1.h
  ../../server/pointer_gestures_v1.h
  ../../server/relative_pointer_v1.h
  ../../server/seat.h
)

install(FILES
  ${SERVER_LIB_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/Wrapland/Server COMPONENT Devel
)

# make available to ecm_add_qch in parent folder
set(WraplandServer_APIDOX_SRCS ${SERVER_LIB_HEADERS} PARENT_SCOPE)
set(WraplandServer_APIDOX_BUILD_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE)
