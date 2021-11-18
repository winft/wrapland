remove_definitions(-DQT_NO_CAST_FROM_BYTEARRAY)
remove_definitions(-DQT_NO_CAST_FROM_ASCII)
remove_definitions(-DQT_NO_CAST_TO_ASCII)

# needed to access QPA
include_directories(SYSTEM ${Qt5Gui_PRIVATE_INCLUDE_DIRS})

set(CLIENT_LIB_SRCS
    appmenu.cpp
    buffer.cpp
    blur.cpp
    compositor.cpp
    connection_thread.cpp
    contrast.cpp
    slide.cpp
    event_queue.cpp
    data_control_v1.cpp
    datadevice.cpp
    datadevicemanager.cpp
    dataoffer.cpp
    datasource.cpp
    dpms.cpp
    drm_lease_v1.cpp
    fakeinput.cpp
    fullscreen_shell.cpp
    idle.cpp
    idleinhibit.cpp
    input_method_v2.cpp
    keyboard.cpp
    keystate.cpp
    keyboard_shortcuts_inhibit.cpp
    layer_shell_v1.cpp
    output.cpp
    output_configuration_v1.cpp
    output_management_v1.cpp
    output_device_v1.cpp
    pointer.cpp
    pointerconstraints.cpp
    pointergestures.cpp
    plasmashell.cpp
    plasmavirtualdesktop.cpp
    plasmawindowmanagement.cpp
    plasmawindowmodel.cpp
    presentation_time.cpp
    primary_selection.cpp
    region.cpp
    registry.cpp
    relativepointer.cpp
    seat.cpp
    server_decoration_palette.cpp
    shadow.cpp
    shell.cpp
    shm_pool.cpp
    subcompositor.cpp
    subsurface.cpp
    surface.cpp
    touch.cpp
    text_input_v2.cpp
    text_input_v3.cpp
    viewporter.cpp
    virtual_keyboard_v1.cpp
    wlr_output_configuration_v1.cpp
    wlr_output_manager_v1.cpp
    xdg_activation_v1.cpp
    xdgdecoration.cpp
    xdg_shell.cpp
    xdg_shell_popup.cpp
    xdg_shell_positioner.cpp
    xdg_shell_toplevel.cpp
    xdgforeign_v2.cpp
    xdgforeign.cpp
    xdgoutput.cpp
    linux_dmabuf_v1.cpp
)

ecm_qt_declare_logging_category(CLIENT_LIB_SRCS
  HEADER logging.h
  IDENTIFIER WRAPLAND_CLIENT
  CATEGORY_NAME wrapland-client
  DEFAULT_SEVERITY Critical
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${WaylandProtocols_DATADIR}/staging/drm-lease/drm-lease-v1.xml
  BASENAME drm-lease-v1
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/fullscreen-shell.xml
  BASENAME fullscreen-shell
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/kwinft-output-management-unstable-v1.xml
  BASENAME output-management-v1
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/kwinft-output-device-unstable-v1.xml
  BASENAME output-device-v1
)
ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/wlr-data-control-unstable-v1.xml
  BASENAME wlr-data-control-v1
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/wlr-output-management-unstable-v1.xml
  BASENAME wlr-output-management-v1
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/wlr-layer-shell-unstable-v1.xml
  BASENAME wlr-layer-shell
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/plasma-shell.xml
  BASENAME plasma-shell
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/plasma-virtual-desktop.xml
  BASENAME plasma-virtual-desktop
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/plasma-window-management.xml
  BASENAME plasma-window-management
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/idle.xml
  BASENAME idle
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/fake-input.xml
  BASENAME fake-input
)
ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/input-method-unstable-v2.xml
  BASENAME input-method-v2
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/shadow.xml
  BASENAME shadow
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/blur.xml
  BASENAME blur
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/contrast.xml
  BASENAME contrast
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/slide.xml
  BASENAME slide
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/dpms.xml
  BASENAME dpms
)
ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/text-input-unstable-v2.xml
  BASENAME text-input-v2
)
ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v3.xml
  BASENAME text-input-v3
)
ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${WaylandProtocols_DATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml
  BASENAME relativepointer-unstable-v1
)
ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${WaylandProtocols_DATADIR}/unstable/pointer-gestures/pointer-gestures-unstable-v1.xml
  BASENAME pointer-gestures-unstable-v1
)
ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${WaylandProtocols_DATADIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml
  BASENAME pointer-constraints-unstable-v1
)
ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${WaylandProtocols_DATADIR}/stable/presentation-time/presentation-time.xml
  BASENAME presentation-time
)
ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${WaylandProtocols_DATADIR}/unstable/primary-selection/primary-selection-unstable-v1.xml
  BASENAME primary-selection-unstable-v1
)
ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${WaylandProtocols_DATADIR}/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml
  BASENAME xdg-foreign-unstable-v2
)
ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${WaylandProtocols_DATADIR}/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml
  BASENAME idle-inhibit-unstable-v1
)
ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/appmenu.xml
  BASENAME appmenu
)
ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/server-decoration-palette.xml
  BASENAME server-decoration-palette
)
ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${WaylandProtocols_DATADIR}/stable/viewporter/viewporter.xml
  BASENAME viewporter
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/virtual-keyboard-unstable-v1.xml
  BASENAME virtual-keyboard-v1
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${WaylandProtocols_DATADIR}/unstable/xdg-output/xdg-output-unstable-v1.xml
  BASENAME xdg-output-unstable-v1
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml
  BASENAME xdg-shell
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${WaylandProtocols_DATADIR}/staging/xdg-activation/xdg-activation-v1.xml
  BASENAME xdg-activation-v1
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${WaylandProtocols_DATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
  BASENAME xdg-decoration-unstable-v1
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${Wrapland_SOURCE_DIR}/src/client/protocols/keystate.xml
  BASENAME keystate
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${WaylandProtocols_DATADIR}/unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml
  BASENAME keyboard-shortcuts-inhibit
)

ecm_add_wayland_client_protocol(CLIENT_LIB_SRCS
  PROTOCOL ${WaylandProtocols_DATADIR}/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml
  BASENAME linux-dmabuf-unstable-v1
)

set(CLIENT_GENERATED_FILES
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-drm-lease-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-fullscreen-shell-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-output-management-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-output-device-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-wlr-data-control-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-wlr-output-management-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-shell-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-shell-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-window-management-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-idle-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-input-method-v2-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-fake-input-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-shadow-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-blur-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-contrast-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-slide-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-dpms-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-server-decoration-palette-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-text-input-v2-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-viewporter-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-virtual-keyboard-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-relativepointer-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-pointer-gestures-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-pointer-constraints-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-presentation-time-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-primary-selection-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-foreign-unstable-v2-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-idle-inhibit-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-wlr-layer-shell-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-output-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-activation-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-decoration-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-linux-dmabuf-unstable-v1-client-protocol.h
)

set_source_files_properties(${CLIENT_GENERATED_FILES} PROPERTIES SKIP_AUTOMOC ON)

add_library(WraplandClient ${CLIENT_LIB_SRCS})
add_library(Wrapland::Client ALIAS WraplandClient)

ecm_generate_export_header(WraplandClient
  BASE_NAME WraplandClient
  EXPORT_FILE_NAME Wrapland/Client/wraplandclient_export.h
  GROUP_BASE_NAME KF
  VERSION ${PROJECT_VERSION}
)

target_include_directories(WraplandClient
  INTERFACE "$<INSTALL_INTERFACE:${INCLUDE_INSTALL_DIR}/Wrapland/Client>"
)

target_link_libraries(WraplandClient
  PUBLIC
    Qt5::Gui
  PRIVATE
    Wayland::Client
    Qt5::Concurrent
)

set_target_properties(WraplandClient PROPERTIES
  VERSION ${WRAPLAND_VERSION_STRING}
  SOVERSION ${WRAPLAND_SOVERSION}
  EXPORT_NAME WraplandClient
)

install(
  TARGETS WraplandClient
  EXPORT WraplandTargets
  ${INSTALL_TARGETS_DEFAULT_ARGS}
)

set(CLIENT_LIB_HEADERS
    ${CMAKE_CURRENT_BINARY_DIR}/Wrapland/Client/wraplandclient_export.h
    appmenu.h
    blur.h
    buffer.h
    compositor.h
    connection_thread.h
    contrast.h
    event_queue.h
    data_control_v1.h
    datadevice.h
    datadevicemanager.h
    dataoffer.h
    datasource.h
    dpms.h
    drm_lease_v1.h
    fakeinput.h
    fullscreen_shell.h
    idle.h
    idleinhibit.h
    input_method_v2.h
    keyboard.h
    keystate.h
    keyboard_shortcuts_inhibit.h
    layer_shell_v1.h
    output.h
    output_configuration_v1.h
    output_management_v1.h
    output_device_v1.h
    wlr_output_configuration_v1.h
    wlr_output_manager_v1.h
    pointer.h
    pointerconstraints.h
    plasmashell.h
    plasmavirtualdesktop.h
    plasmawindowmanagement.h
    plasmawindowmodel.h
    presentation_time.h
    primary_selection.h
    pointergestures.h
    region.h
    registry.h
    relativepointer.h
    seat.h
    server_decoration_palette.h
    shadow.h
    shell.h
    shm_pool.h
    slide.h
    subcompositor.h
    subsurface.h
    surface.h
    touch.h
    text_input_v2.h
    text_input_v3.h
    viewporter.h
    virtual_keyboard_v1.h
    xdg_activation_v1.h
    xdgdecoration.h
    xdg_shell.h
    xdg_shell_popup.h
    xdg_shell_positioner.h
    xdg_shell_toplevel.h
    xdgforeign.h
    xdgoutput.h
    linux_dmabuf_v1.h
)

install(
  FILES ${CLIENT_LIB_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/Wrapland/Client
  COMPONENT Devel
)

# make available to ecm_add_qch in parent folder
set(WraplandClient_APIDOX_SRCS
    ${CLIENT_LIB_HEADERS}
    PARENT_SCOPE
)
set(WraplandClient_APIDOX_BUILD_INCLUDE_DIRS
    ${CMAKE_CURRENT_BINARY_DIR}
    PARENT_SCOPE
)
