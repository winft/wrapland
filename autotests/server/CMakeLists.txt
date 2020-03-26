########################################################
# Test WaylandServerDisplay
########################################################
set( testServerDisplay_SRCS display.cpp )
add_executable(testServerDisplay ${testServerDisplay_SRCS})
target_link_libraries( testServerDisplay Qt5::Test Qt5::Gui Wrapland::Server Wayland::Server)
add_test(NAME wrapland-testServerDisplay COMMAND testServerDisplay)
ecm_mark_as_test(testServerDisplay)

# Legacy
#set( testWaylandServerDisplay_SRCS
#        test_display.cpp
#    )
#add_executable(testWaylandServerDisplay ${testWaylandServerDisplay_SRCS})
#target_link_libraries( testWaylandServerDisplay Qt5::Test Qt5::Gui Wrapland::Server Wayland::Server)
#add_test(NAME wrapland-testWaylandServerDisplay COMMAND testWaylandServerDisplay)
#ecm_mark_as_test(testWaylandServerDisplay)

########################################################
# Test WaylandServerSeat
########################################################
set( testWaylandServerSeat_SRCS
        test_seat.cpp
    )
add_executable(testWaylandServerSeat ${testWaylandServerSeat_SRCS})
target_link_libraries( testWaylandServerSeat Qt5::Test Qt5::Gui Wrapland::Server Wayland::Server)
add_test(NAME wrapland-testWaylandServerSeat COMMAND testWaylandServerSeat)
ecm_mark_as_test(testWaylandServerSeat)

########################################################
# QtSurfaceExtenstion Helper
########################################################
add_executable(surfaceExtensionHelper surfaceextension_helper.cpp)
target_link_libraries( surfaceExtensionHelper Qt5::Gui)
ecm_mark_as_test(surfaceExtensionHelper)

########################################################
# Test QtSurfaceExtenstion
########################################################
set( testQtSurfaceExtension_SRCS
        test_qt_surface_extension.cpp
    )
add_executable(testQtSurfaceExtension ${testQtSurfaceExtension_SRCS})
target_link_libraries( testQtSurfaceExtension Qt5::Test Qt5::Gui Wrapland::Server)
add_test(NAME wrapland-testQtSurfaceExtension COMMAND testQtSurfaceExtension)
ecm_mark_as_test(testQtSurfaceExtension)

########################################################
# Test No XDG_RUNTIME_DIR
########################################################
add_executable(testNoXdgRuntimeDir test_no_xdg_runtime_dir.cpp)
target_link_libraries( testNoXdgRuntimeDir Qt5::Test Wrapland::Server)
add_test(NAME wrapland-testNoXdgRuntimeDir COMMAND testNoXdgRuntimeDir)
ecm_mark_as_test(testNoXdgRuntimeDir)
