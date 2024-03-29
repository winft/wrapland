########################################################
# Test WaylandServerDisplay
########################################################
set(testServerDisplay_SRCS display.cpp)
add_executable(testServerDisplay ${testServerDisplay_SRCS})
target_link_libraries(testServerDisplay
  Qt6::Test
  Qt6::Gui
  Wrapland::Server
  Wayland::Server
)
add_test(NAME wrapland-testServerDisplay COMMAND testServerDisplay)
ecm_mark_as_test(testServerDisplay)

# ##################################################################################################
# Test WaylandServerSeat
# ##################################################################################################
set(testWaylandServerSeat_SRCS test_seat.cpp)
add_executable(testWaylandServerSeat ${testWaylandServerSeat_SRCS})
target_link_libraries(testWaylandServerSeat
  Qt6::Test
  Qt6::Gui
  Wrapland::Server
  Wayland::Server
)
add_test(NAME wrapland-testWaylandServerSeat COMMAND testWaylandServerSeat)
ecm_mark_as_test(testWaylandServerSeat)

# ##################################################################################################
# Test No XDG_RUNTIME_DIR
# ##################################################################################################
add_executable(testNoXdgRuntimeDir test_no_xdg_runtime_dir.cpp)
target_link_libraries(testNoXdgRuntimeDir
  Qt6::Test
  Wrapland::Server
)
add_test(NAME wrapland-testNoXdgRuntimeDir COMMAND testNoXdgRuntimeDir)
ecm_mark_as_test(testNoXdgRuntimeDir)
