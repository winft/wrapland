set(SERVER_LIB_SRCS
    appmenu.cpp
    blur.cpp
    buffer.cpp
    client.cpp
    compositor.cpp
    contrast.cpp
    data_device.cpp
    data_device_manager.cpp
    data_offer.cpp
    data_source.cpp
    display.cpp
    dpms.cpp
    egl_stream_controller.cpp
    fake_input.cpp
    filtered_display.cpp
    idle_inhibit_v1.cpp
    kde_idle.cpp
    keyboard.cpp
    keystate.cpp
    linux_dmabuf_v1.cpp
    output.cpp
    output_changeset_v1.cpp
    output_configuration_v1.cpp
    output_device_v1.cpp
    output_management_v1.cpp
    plasma_shell.cpp
    plasma_virtual_desktop.cpp
    plasma_window.cpp
    pointer.cpp
    pointer_constraints_v1.cpp
    pointer_gestures_v1.cpp
    region.cpp
    relative_pointer_v1.cpp
    remote_access.cpp
    seat.cpp
    server_decoration_palette.cpp
    shadow.cpp
    slide.cpp
    subcompositor.cpp
    surface.cpp
    text_input_v2.cpp
    touch.cpp
    viewporter.cpp
    wayland/buffer_manager.cpp
    wayland/client.cpp
    wayland/display.cpp
    xdg_decoration.cpp
    xdg_foreign.cpp
    xdg_foreign_v2.cpp
    xdg_output.cpp
    xdg_shell.cpp
    xdg_shell_popup.cpp
    xdg_shell_positioner.cpp
    xdg_shell_surface.cpp
    xdg_shell_toplevel.cpp
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
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/text-input.xml
    BASENAME text
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/text-input-unstable-v2.xml
    BASENAME text-input-unstable-v2
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
  appmenu.h
  blur.h
  buffer.h
  client.h
  compositor.h
  contrast.h
  data_device.h
  data_device_manager.h
  data_offer.h
  data_source.h
  display.h
  dpms.h
  egl_stream_controller.h
  fake_input.h
  filtered_display.h
  idle_inhibit_v1.h
  kde_idle.h
  keyboard.h
  keystate.h
  linux_dmabuf_v1.h
  output.h
  output_changeset_v1.h
  output_configuration_v1.h
  output_device_v1.h
  output_management_v1.h
  plasma_shell.h
  plasma_virtual_desktop.h
  plasma_window.h
  pointer.h
  pointer_constraints_v1.h
  pointer_gestures_v1.h
  region.h
  relative_pointer_v1.h
  remote_access.h
  seat.h
  server_decoration_palette.h
  shadow.h
  slide.h
  subcompositor.h
  surface.h
  text_input_v2.h
  touch.h
  viewporter.h
  xdg_decoration.h
  xdg_output.h
  xdg_foreign.h
  xdg_shell.h
  xdg_shell_popup.h
  xdg_shell_surface.h
  xdg_shell_toplevel.h
)

install(FILES
  ${SERVER_LIB_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/Wrapland/Server COMPONENT Devel
)

# make available to ecm_add_qch in parent folder
set(WraplandServer_APIDOX_SRCS ${SERVER_LIB_HEADERS} PARENT_SCOPE)
set(WraplandServer_APIDOX_BUILD_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE)