########################################################
# Test Viewporter
########################################################
set( testViewporter_SRCS
        test_viewporter.cpp
    )
add_executable(testViewporter ${testViewporter_SRCS})
target_link_libraries( testViewporter Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server Wayland::Client Wayland::Server)
add_test(NAME wrapland-testViewporter COMMAND testViewporter)
ecm_mark_as_test(testViewporter)

########################################################
# Test WaylandConnectionThread
########################################################
set( testWaylandConnectionThread_SRCS
        test_wayland_connection_thread.cpp
    )
add_executable(testWaylandConnectionThread ${testWaylandConnectionThread_SRCS})
target_link_libraries( testWaylandConnectionThread Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server Wayland::Client Wayland::Server)
add_test(NAME wrapland-testWaylandConnectionThread COMMAND testWaylandConnectionThread)
ecm_mark_as_test(testWaylandConnectionThread)

########################################################
# Test WaylandRegistry
########################################################
set( testWaylandRegistry_SRCS
        test_wayland_registry.cpp
    )
add_executable(testWaylandRegistry ${testWaylandRegistry_SRCS})
target_link_libraries( testWaylandRegistry Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server Wayland::Client Wayland::Server)
add_test(NAME wrapland-testWaylandRegistry COMMAND testWaylandRegistry)
ecm_mark_as_test(testWaylandRegistry)

########################################################
# Test WaylandFullscreenShell
########################################################
if(Wayland_VERSION VERSION_GREATER "1.4.0")
  find_program(WESTON_EXECUTABLE weston DOC "Path to the weston executable.")
  if(WESTON_EXECUTABLE)
    set( testWaylandFullscreenShell_SRCS
            test_wayland_fullscreen_shell.cpp
        )
    add_executable(testWaylandFullscreenShell ${testWaylandFullscreenShell_SRCS})
    target_link_libraries( testWaylandFullscreenShell Qt5::Test Wrapland::Client Wayland::Client)
    add_test(NAME wrapland-testWaylandFullscreenShell COMMAND testWaylandFullscreenShell)
    ecm_mark_as_test(testWaylandFullscreenShell)
  else()
    message(STATUS "The weston executable was not found. Some autotests will not be executed.")
  endif()
endif()

########################################################
# Test WaylandOutput
########################################################
set(testOutput_SRCS output.cpp)
add_executable(testOutput ${testOutput_SRCS})
target_link_libraries( testOutput
    Qt5::Test Qt5::Gui
    Wrapland::Client Wrapland::Server
    Wayland::Client Wayland::Server
    )
add_test(NAME wrapland-testOutput COMMAND testOutput)
ecm_mark_as_test(testOutput)

# Legacy
#set( testWaylandOutput_SRCS
#        test_wayland_output.cpp
#    )
#add_executable(testWaylandOutput ${testWaylandOutput_SRCS})
#target_link_libraries( testWaylandOutput Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server Wayland::Client Wayland::Server)
#add_test(NAME wrapland-testWaylandOutput COMMAND testWaylandOutput)
#ecm_mark_as_test(testWaylandOutput)

########################################################
# Test WaylandShell
########################################################
set( testWaylandShell_SRCS
        test_wayland_shell.cpp
    )
add_executable(testWaylandShell ${testWaylandShell_SRCS})
target_link_libraries( testWaylandShell Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server Wayland::Client Wayland::Server)
add_test(NAME wrapland-testWaylandShell COMMAND testWaylandShell)
ecm_mark_as_test(testWaylandShell)

########################################################
# Test WaylandSurface
########################################################
set( testWaylandSurface_SRCS
        test_wayland_surface.cpp
    )
add_executable(testWaylandSurface ${testWaylandSurface_SRCS})
target_link_libraries( testWaylandSurface Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server Wayland::Client Wayland::Server)
add_test(NAME wrapland-testWaylandSurface COMMAND testWaylandSurface)
ecm_mark_as_test(testWaylandSurface)

########################################################
# Test WaylandSeat
########################################################
if (HAVE_LINUX_INPUT_H)
    set( testSeat_SRCS seat.cpp )
    add_executable(testSeat ${testSeat_SRCS})
    target_link_libraries( testSeat
        Qt5::Test Qt5::Gui
        Wrapland::Client Wrapland::Server
        Wayland::Client Wayland::Server)
    add_test(NAME wrapland-testSeat COMMAND testSeat)
    ecm_mark_as_test(testSeat)
endif()

#Legacy
#if (HAVE_LINUX_INPUT_H)
#    set( testWaylandSeat_SRCS
#            test_wayland_seat.cpp
#        )
#    add_executable(testWaylandSeat ${testWaylandSeat_SRCS})
#    target_link_libraries( testWaylandSeat Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server Wayland::Client Wayland::Server)
#    add_test(NAME wrapland-testWaylandSeat COMMAND testWaylandSeat)
#    ecm_mark_as_test(testWaylandSeat)
#endif()

########################################################
# Test ShmPool
########################################################
set( testShmPool_SRCS
        test_shm_pool.cpp
    )
add_executable(testShmPool ${testShmPool_SRCS})
target_link_libraries( testShmPool Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server)
add_test(NAME wrapland-testShmPool COMMAND testShmPool)
ecm_mark_as_test(testShmPool)

########################################################
# Test KWin OutputManagement
########################################################
set( test_output_management_SRCS
        test_output_management.cpp
    )
add_executable(testOutputManagement ${test_output_management_SRCS})
target_link_libraries( testOutputManagement Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server Wayland::Client)
add_test(NAME wrapland-testOutputManagement COMMAND testOutputManagement)
ecm_mark_as_test(testOutputManagement)

########################################################
# Test KWin OutputDevice
########################################################
set( test_output_device_SRCS
        test_output_device.cpp
    )
add_executable(testOutputDevice ${test_output_device_SRCS})
target_link_libraries( testOutputDevice Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server Wayland::Client)
add_test(NAME wrapland-testOutputDevice COMMAND testOutputDevice)
ecm_mark_as_test(testOutputDevice)

########################################################
# Test wlroots based OutputManagement
########################################################
set( test_wlr_output_management_SRCS
        test_wlr_output_management.cpp
    )
add_executable(testWlrOutputManagement ${test_wlr_output_management_SRCS})
target_link_libraries( testWlrOutputManagement
    Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server Wayland::Client)
add_test(NAME wrapland-testWlrOutputManagement COMMAND testWlrOutputManagement)
ecm_mark_as_test(testWlrOutputManagement)

########################################################
# Test Compositor
########################################################
set( testCompositor_SRCS
        test_compositor.cpp
    )
add_executable(testCompositor ${testCompositor_SRCS})
target_link_libraries( testCompositor Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server)
add_test(NAME wrapland-testCompositor COMMAND testCompositor)
ecm_mark_as_test(testCompositor)

########################################################
# Test SubCompositor
########################################################
set( testSubCompositor_SRCS
        test_wayland_subcompositor.cpp
    )
add_executable(testSubCompositor ${testSubCompositor_SRCS})
target_link_libraries( testSubCompositor Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server)
add_test(NAME wrapland-testSubCompositor COMMAND testSubCompositor)
ecm_mark_as_test(testSubCompositor)


########################################################
# Test SubSurface
########################################################
set( testSubSurface_SRCS
        test_wayland_subsurface.cpp
    )
add_executable(testSubSurface ${testSubSurface_SRCS})
target_link_libraries( testSubSurface Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server Wayland::Client)
add_test(NAME wrapland-testSubSurface COMMAND testSubSurface)
ecm_mark_as_test(testSubSurface)

########################################################
# Test Region
########################################################
set( testRegion_SRCS
        test_wayland_region.cpp
    )
add_executable(testRegion ${testRegion_SRCS})
target_link_libraries( testRegion Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server)
add_test(NAME wrapland-testRegion COMMAND testRegion)
ecm_mark_as_test(testRegion)

########################################################
# Test Blur
########################################################
set( testBlur_SRCS
        test_wayland_blur.cpp
    )
add_executable(testBlur ${testBlur_SRCS})
target_link_libraries( testBlur Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server)
add_test(NAME wrapland-testBlur COMMAND testBlur)
ecm_mark_as_test(testBlur)

########################################################
# Test Contrast
########################################################
set( testContrast_SRCS
        test_wayland_contrast.cpp
    )
add_executable(testContrast ${testContrast_SRCS})
target_link_libraries( testContrast Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server Wayland::Client)
add_test(NAME wrapland-testContrast COMMAND testContrast)
ecm_mark_as_test(testContrast)

########################################################
# Test Slide
########################################################
set( testSlide_SRCS
        test_wayland_slide.cpp
    )
add_executable(testSlide ${testSlide_SRCS})
target_link_libraries( testSlide Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server Wayland::Client)
add_test(NAME wrapland-testSlide COMMAND testSlide)
ecm_mark_as_test(testSlide)

########################################################
# Test Window Management
########################################################
set( testWindowmanagement_SRCS
        test_wayland_windowmanagement.cpp
    )
add_executable(testWindowmanagement ${testWindowmanagement_SRCS})
target_link_libraries( testWindowmanagement Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server Wayland::Client)
add_test(NAME wrapland-testWindowmanagement COMMAND testWindowmanagement)
ecm_mark_as_test(testWindowmanagement)

########################################################
# Test DataSource
########################################################
set( testDataSource_SRCS data_source.cpp )
add_executable(testDataSource ${testDataSource_SRCS})
target_link_libraries( testDataSource
    Qt5::Test
    Qt5::Gui
    Wrapland::Client
    Wrapland::Server
    Wayland::Client
)
add_test(NAME wrapland-testDataSource COMMAND testDataSource)
ecm_mark_as_test(testDataSource)

########################################################
# Test DataDevice
########################################################
set( testDataDevice_SRCS data_device.cpp )
add_executable(testDataDevice ${testDataDevice_SRCS})
target_link_libraries( testDataDevice
    Qt5::Test
    Qt5::Gui
    Wrapland::Client
    Wrapland::Server
    Wayland::Client)
add_test(NAME wrapland-testDataDevice COMMAND testDataDevice)
ecm_mark_as_test(testDataDevice)

########################################################
# Test ServerSideDecoration
########################################################
set( testServerSideDecoration_SRCS
        test_server_side_decoration.cpp
    )
add_executable(testServerSideDecoration ${testServerSideDecoration_SRCS})
target_link_libraries( testServerSideDecoration Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server Wayland::Client)
add_test(NAME wrapland-testServerSideDecoration COMMAND testServerSideDecoration)
ecm_mark_as_test(testServerSideDecoration)

########################################################
# Test Drag'N'Drop
########################################################
set( testDragAndDrop_SRCS drag_and_drop.cpp )
add_executable(testDragAndDrop ${testDragAndDrop_SRCS})
target_link_libraries( testDragAndDrop
    Qt5::Test
    Qt5::Gui
    Wrapland::Client
    Wrapland::Server
    Wayland::Client
)
add_test(NAME wrapland-testDragAndDrop COMMAND testDragAndDrop)
ecm_mark_as_test(testDragAndDrop)

########################################################
# Test PlasmaShell
########################################################
set( testPlasmaShell_SRCS
        test_plasmashell.cpp
    )
add_executable(testPlasmaShell ${testPlasmaShell_SRCS})
target_link_libraries( testPlasmaShell Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server Wayland::Client)
add_test(NAME wrapland-testPlasmaShell COMMAND testPlasmaShell)
ecm_mark_as_test(testPlasmaShell)

########################################################
# Test Idle
########################################################
set( testIdle_SRCS
        test_idle.cpp
    )
add_executable(testIdle ${testIdle_SRCS})
target_link_libraries( testIdle Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server)
add_test(NAME wrapland-testIdle COMMAND testIdle)
ecm_mark_as_test(testIdle)

########################################################
# Test Shadow
########################################################
set( testShadow_SRCS
        test_shadow.cpp
    )
add_executable(testShadow ${testShadow_SRCS})
target_link_libraries( testShadow Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server)
add_test(NAME wrapland-testShadow COMMAND testShadow)
ecm_mark_as_test(testShadow)

########################################################
# Test FakeInput
########################################################
if (HAVE_LINUX_INPUT_H)
    set( testFakeInput_SRCS
            test_fake_input.cpp
        )
    add_executable(testFakeInput ${testFakeInput_SRCS})
    target_link_libraries( testFakeInput Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server)
    add_test(NAME wrapland-testFakeInput COMMAND testFakeInput)
    ecm_mark_as_test(testFakeInput)
endif()

########################################################
# Test PlasmaWindowModel
########################################################
if (HAVE_LINUX_INPUT_H)
    set( testPlasmaWindowModel_SRCS
            test_plasma_window_model.cpp
        )
    add_executable(testPlasmaWindowModel ${testPlasmaWindowModel_SRCS})
    target_link_libraries( testPlasmaWindowModel Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server)
    add_test(NAME wrapland-testPlasmaWindowModel COMMAND testPlasmaWindowModel)
    ecm_mark_as_test(testPlasmaWindowModel)
endif()

########################################################
# Test TextInput
########################################################
set( testTextInput_SRCS
        test_text_input.cpp
    )
add_executable(testTextInput ${testTextInput_SRCS})
target_link_libraries( testTextInput Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server)
add_test(NAME wrapland-testTextInput COMMAND testTextInput)
ecm_mark_as_test(testTextInput)

########################################################
# Test Error
########################################################
set( testError_SRCS
        test_error.cpp
    )
add_executable(testError ${testError_SRCS})
target_link_libraries( testError Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server Wayland::Client)
add_test(NAME wrapland-testError COMMAND testError)
ecm_mark_as_test(testError)

########################################################
# Test Selection
########################################################
set( testSelection_SRCS selection.cpp )
add_executable(testSelection ${testSelection_SRCS})
target_link_libraries( testSelection
    Qt5::Test
    Qt5::Gui
    Wrapland::Client
    Wrapland::Server
    Wayland::Client)
add_test(NAME wrapland-testSelection COMMAND testSelection)
ecm_mark_as_test(testSelection)

########################################################
# Test XdgShellV5
########################################################
set( testXdgShellV5_SRCS
      test_xdg_shell.cpp
      test_xdg_shell_v5.cpp
      )
add_executable(testXdgShellV5 ${testXdgShellV5_SRCS})
target_link_libraries( testXdgShellV5 Qt5::Test Qt5::Gui Wrapland::Server Wrapland::Client Wayland::Client)
add_test(NAME wrapland-testXdgShellV5 COMMAND testXdgShellV5)
ecm_mark_as_test(testXdgShellV5)

########################################################
# Test XdgForeign
########################################################
set( testXdgForeign_SRCS
      test_xdg_foreign.cpp
      )
add_executable(testXdgForeign ${testXdgForeign_SRCS})
target_link_libraries( testXdgForeign Qt5::Test Qt5::Gui Wrapland::Server Wrapland::Client Wayland::Client)
add_test(NAME wrapland-testXdgForeign COMMAND testXdgForeign)
ecm_mark_as_test(testXdgForeign)

########################################################
# Test XdgShellV6
########################################################
set( testXdgShellV6_SRCS
      test_xdg_shell.cpp
      test_xdg_shell_v6.cpp
      )
add_executable(testXdgShellV6 ${testXdgShellV6_SRCS})
target_link_libraries( testXdgShellV6 Qt5::Test Qt5::Gui Wrapland::Server Wrapland::Client Wayland::Client)
add_test(NAME wrapland-testXdgShellV6 COMMAND testXdgShellV6)
ecm_mark_as_test(testXdgShellV6)
########################################################
# Test XdgShellStable
########################################################
set( testXdgShellStable_SRCS
      test_xdg_shell.cpp
      test_xdg_shell_stable.cpp
      )
add_executable(testXdgShellStable ${testXdgShellStable_SRCS})
target_link_libraries( testXdgShellStable Qt5::Test Qt5::Gui Wrapland::Server Wrapland::Client Wayland::Client)
add_test(NAME wrapland-testXdgShellStable COMMAND testXdgShellStable)
ecm_mark_as_test(testXdgShellStable)

########################################################
# Test Pointer Constraints
########################################################
add_executable(testPointerConstraints test_pointer_constraints.cpp)
target_link_libraries( testPointerConstraints Qt5::Test Qt5::Gui Wrapland::Server Wrapland::Client Wayland::Client)
add_test(NAME wrapland-testPointerConstraints COMMAND testPointerConstraints)
ecm_mark_as_test(testPointerConstraints)


########################################################
# Test Filter
########################################################
set( testFilter_SRCS
        test_wayland_filter.cpp
    )
add_executable(testFilter ${testFilter_SRCS})
target_link_libraries( testFilter Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server Wayland::Server)
add_test(NAME wrapland-testFilter COMMAND testFilter)
ecm_mark_as_test(testFilter)

########################################################
# Test Appmenu
########################################################
set( testAppmenu_SRCS
        test_wayland_appmenu.cpp
    )
add_executable(testAppmenu ${testAppmenu_SRCS})
target_link_libraries( testAppmenu Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server)
add_test(NAME wrapland-testAppmenu COMMAND testAppmenu)
ecm_mark_as_test(testAppmenu)

########################################################
# Test Appmenu
########################################################
set( testServerSideDecorationPalette_SRCS
        test_server_side_decoration_palette.cpp
    )
add_executable(testServerSideDecorationPalette ${testServerSideDecorationPalette_SRCS})
target_link_libraries( testServerSideDecorationPalette Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server)
add_test(NAME wrapland-testServerSideDecorationPalette COMMAND testServerSideDecorationPalette)
ecm_mark_as_test(testServerSideDecorationPalette)

########################################################
# Test RemoteAccess
########################################################
set( testRemoteAccess_SRCS
        test_remote_access.cpp
    )
add_executable(testRemoteAccess ${testRemoteAccess_SRCS})
target_link_libraries( testRemoteAccess Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server)
add_test(NAME wrapland-testRemoteAccess COMMAND testRemoteAccess)
ecm_mark_as_test(testRemoteAccess)

########################################################
# Test VirtualDesktop
########################################################
set( testPlasmaVirtualDesktop_SRCS
        test_plasma_virtual_desktop.cpp
    )
add_executable(testPlasmaVirtualDesktop ${testPlasmaVirtualDesktop_SRCS})
target_link_libraries( testPlasmaVirtualDesktop Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server)
add_test(NAME wrapland-testPlasmaVirtualDesktop COMMAND testPlasmaVirtualDesktop)
ecm_mark_as_test(testPlasmaVirtualDesktop)

########################################################
# Test XDG Output
########################################################
set( testXdgOutput_SRCS
        test_xdg_output.cpp
    )
add_executable(testXdgOutput ${testXdgOutput_SRCS})
target_link_libraries( testXdgOutput Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server Wayland::Client Wayland::Server)
add_test(NAME wrapland-testXdgOutput COMMAND testXdgOutput)
ecm_mark_as_test(testXdgOutput)

########################################################
# Test XDG Decoration
########################################################
set( testXdgdecoration_SRCS
        test_xdg_decoration.cpp
    )
add_executable(testXdgDecoration ${testXdgdecoration_SRCS})
target_link_libraries( testXdgDecoration Qt5::Test Qt5::Gui Wrapland::Client Wrapland::Server Wayland::Client Wayland::Server)
add_test(NAME wrapland-testXdgDecoration COMMAND testXdgDecoration)
ecm_mark_as_test(testXdgDecoration)

