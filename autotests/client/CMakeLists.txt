# ##################################################################################################
# Test drm_lease_v1
# ##################################################################################################
set(test_drm_lease_v1_SRCS drm_lease_v1.cpp)
add_executable(test_drm_lease_v1 ${test_drm_lease_v1_SRCS})
target_link_libraries(test_drm_lease_v1
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
)
add_test(NAME wrapland-test_drm_lease_v1 COMMAND test_drm_lease_v1)
ecm_mark_as_test(test_drm_lease_v1)

########################################################
# Test Viewporter
########################################################
set(testViewporter_SRCS viewporter.cpp)
add_executable(testViewporter ${testViewporter_SRCS})
target_link_libraries(testViewporter
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
  Wayland::Server
)
add_test(NAME wrapland-testViewporter COMMAND testViewporter)
ecm_mark_as_test(testViewporter)

# ##################################################################################################
# Test WaylandConnectionThread
# ##################################################################################################
set(testConnectionThread_SRCS connection_thread.cpp)
add_executable(testConnectionThread ${testConnectionThread_SRCS})
target_link_libraries(testConnectionThread
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
  Wayland::Server
)
add_test(NAME wrapland-testConnectionThread COMMAND testConnectionThread)
ecm_mark_as_test(testConnectionThread)

# ##################################################################################################
# Test WaylandRegistry
# ##################################################################################################
set(testRegistry_SRCS registry.cpp)
add_executable(testRegistry ${testRegistry_SRCS})
target_link_libraries(testRegistry
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
  Wayland::Server
)
add_test(NAME wrapland-testRegistry COMMAND testRegistry)
ecm_mark_as_test(testRegistry)

# ##################################################################################################
# Test WaylandFullscreenShell
# ##################################################################################################
if(Wayland_VERSION VERSION_GREATER "1.4.0")
  find_program(WESTON_EXECUTABLE weston DOC "Path to the weston executable.")
  if(WESTON_EXECUTABLE)
    set(testFullscreenShell_SRCS fullscreen_shell.cpp)
    add_executable(testFullscreenShell ${testFullscreenShell_SRCS})
    target_link_libraries(testFullscreenShell
      Qt5::Test
      Wrapland::Client
      Wayland::Client
    )
    add_test(NAME wrapland-testFullscreenShell COMMAND testFullscreenShell)
    ecm_mark_as_test(testFullscreenShell)
  else()
    message(STATUS "The weston executable was not found. Some autotests will not be executed.")
  endif()
endif()

# ##################################################################################################
# Test layer-shell
# ##################################################################################################
set(test_layer_shell_SRCS layer_shell.cpp)
add_executable(testLayerShell ${test_layer_shell_SRCS})
target_link_libraries(testLayerShell
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
)
add_test(NAME wrapland-testLayerShell COMMAND testLayerShell)
ecm_mark_as_test(testLayerShell)

# ##################################################################################################
# Test WaylandOutput
# ##################################################################################################
set(testOutput_SRCS output.cpp)
add_executable(testOutput ${testOutput_SRCS})
target_link_libraries(testOutput
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
  Wayland::Server
)
add_test(NAME wrapland-testOutput COMMAND testOutput)
ecm_mark_as_test(testOutput)

# ##################################################################################################
# Test Surface
# ##################################################################################################
set(testSurface_SRCS surface.cpp)
add_executable(testSurface ${testSurface_SRCS})
target_link_libraries(testSurface
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
  Wayland::Server
)
add_test(NAME wrapland-testSurface COMMAND testSurface)
ecm_mark_as_test(testSurface)

# ##################################################################################################
# Test WaylandSeat
# ##################################################################################################
if(HAVE_LINUX_INPUT_H)
  set(testSeat_SRCS seat.cpp)
  add_executable(testSeat ${testSeat_SRCS})
  target_link_libraries(testSeat
    Qt5::Test
    Qt5::Gui
    Wrapland::Client
    Wrapland::Server
    Wayland::Client
    Wayland::Server
)
  add_test(NAME wrapland-testSeat COMMAND testSeat)
  ecm_mark_as_test(testSeat)
endif()

# ##################################################################################################
# Test ShmPool
# ##################################################################################################
set(testShmPool_SRCS shm_pool.cpp)
add_executable(testShmPool ${testShmPool_SRCS})
target_link_libraries(testShmPool
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
)
add_test(NAME wrapland-testShmPool COMMAND testShmPool)
ecm_mark_as_test(testShmPool)

# ##################################################################################################
# Test KWin OutputManagement
# ##################################################################################################
set(testOutputManagement_SRCS output_management.cpp)
add_executable(testOutputManagement ${testOutputManagement_SRCS})
target_link_libraries(testOutputManagement
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
)
add_test(NAME wrapland-testOutputManagement COMMAND testOutputManagement)
ecm_mark_as_test(testOutputManagement)

# ##################################################################################################
# Test KWin OutputDevice
# ##################################################################################################
set(testOutputDevice_SRCS output_device.cpp)
add_executable(testOutputDevice ${testOutputDevice_SRCS})
target_link_libraries(testOutputDevice
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
)
add_test(NAME wrapland-testOutputDevice COMMAND testOutputDevice)
ecm_mark_as_test(testOutputDevice)

# ##################################################################################################
# Test wlroots based OutputManagement
# ##################################################################################################
set(testWlrOutputManagement_SRCS wlr_output_management.cpp)
add_executable(testWlrOutputManagement ${testWlrOutputManagement_SRCS})
target_link_libraries(testWlrOutputManagement
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
)
add_test(NAME wrapland-testWlrOutputManagement COMMAND testWlrOutputManagement)
ecm_mark_as_test(testWlrOutputManagement)

# ##################################################################################################
# Test Compositor
# ##################################################################################################
set(testCompositor_SRCS compositor.cpp)
add_executable(testCompositor ${testCompositor_SRCS})
target_link_libraries(testCompositor
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
)
add_test(NAME wrapland-testCompositor COMMAND testCompositor)
ecm_mark_as_test(testCompositor)

# ##################################################################################################
# Test Subcompositor
# ##################################################################################################
set(testSubcompositor_SRCS subcompositor.cpp)
add_executable(testSubcompositor ${testSubcompositor_SRCS})
target_link_libraries(testSubcompositor
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
)
add_test(NAME wrapland-testSubcompositor COMMAND testSubcompositor)
ecm_mark_as_test(testSubcompositor)

# ##################################################################################################
# Test Subsurface
# ##################################################################################################
set(testSubsurface_SRCS subsurface.cpp)
add_executable(testSubsurface ${testSubsurface_SRCS})
target_link_libraries(testSubsurface
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
)
add_test(NAME wrapland-testSubsurface COMMAND testSubsurface)
ecm_mark_as_test(testSubsurface)

# ##################################################################################################
# Test Region
# ##################################################################################################
set(testRegion_SRCS region.cpp)
add_executable(testRegion ${testRegion_SRCS})
target_link_libraries(testRegion
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
)
add_test(NAME wrapland-testRegion COMMAND testRegion)
ecm_mark_as_test(testRegion)

# ##################################################################################################
# Test Blur
# ##################################################################################################
set(testBlur_SRCS blur.cpp)
add_executable(testBlur ${testBlur_SRCS})
target_link_libraries(testBlur
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
)
add_test(NAME wrapland-testBlur COMMAND testBlur)
ecm_mark_as_test(testBlur)

# ##################################################################################################
# Test LinuxDmabuf
# ##################################################################################################
set(testLinuxDmabuf_SRCS linux_dmabuf.cpp)
add_executable(testLinuxDmabuf ${testLinuxDmabuf_SRCS})
target_link_libraries(testLinuxDmabuf
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
)
add_test(NAME wrapland-testLinuxDmabuf COMMAND testLinuxDmabuf)
ecm_mark_as_test(testLinuxDmabuf)

# ##################################################################################################
# Test Contrast
# ##################################################################################################
set(testContrast_SRCS contrast.cpp)
add_executable(testContrast ${testContrast_SRCS})
target_link_libraries(testContrast
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
)
add_test(NAME wrapland-testContrast COMMAND testContrast)
ecm_mark_as_test(testContrast)

# ##################################################################################################
# Test Slide
# ##################################################################################################
set(testSlide_SRCS slide.cpp)
add_executable(testSlide ${testSlide_SRCS})
target_link_libraries(testSlide
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
)
add_test(NAME wrapland-testSlide COMMAND testSlide)
ecm_mark_as_test(testSlide)

# ##################################################################################################
# Test Window Management
# ##################################################################################################
set(testPlasmaWindowManagement_SRCS plasma_window_management.cpp)
add_executable(testPlasmaWindowManagement ${testPlasmaWindowManagement_SRCS})
target_link_libraries(testPlasmaWindowManagement
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
)
add_test(NAME wrapland-testPlasmaWindowManagement COMMAND testPlasmaWindowManagement)
ecm_mark_as_test(testPlasmaWindowManagement)

# ##################################################################################################
# Test DataSource
# ##################################################################################################
set(testDataSource_SRCS data_source.cpp)
add_executable(testDataSource ${testDataSource_SRCS})
target_link_libraries(testDataSource
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
)
add_test(NAME wrapland-testDataSource COMMAND testDataSource)
ecm_mark_as_test(testDataSource)

# ##################################################################################################
# Test DataDevice
# ##################################################################################################
set(testDataDevice_SRCS data_device.cpp)
add_executable(testDataDevice ${testDataDevice_SRCS})
target_link_libraries(testDataDevice
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
)
add_test(NAME wrapland-testDataDevice COMMAND testDataDevice)
ecm_mark_as_test(testDataDevice)

# ##################################################################################################
# Test data-control
# ##################################################################################################
set(test_data_control_SRCS data_control.cpp)
add_executable(test-data-control ${test_data_control_SRCS})
target_link_libraries(test-data-control
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
)
add_test(NAME wrapland-test-data-control COMMAND test-data-control)
ecm_mark_as_test(test-data-control)

# ##################################################################################################
# Test Drag'N'Drop
# ##################################################################################################
set(testDragAndDrop_SRCS drag_and_drop.cpp)
add_executable(testDragAndDrop ${testDragAndDrop_SRCS})
target_link_libraries(testDragAndDrop
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
)
add_test(NAME wrapland-testDragAndDrop COMMAND testDragAndDrop)
ecm_mark_as_test(testDragAndDrop)

# ##################################################################################################
# Test external sources
# ##################################################################################################
set(test_external_sources_SRCS external_sources.cpp)
add_executable(test-external-sources ${test_external_sources_SRCS})
target_link_libraries(test-external-sources
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
)
add_test(NAME wrapland-test-external-sources COMMAND test-external-sources)
ecm_mark_as_test(test-external-sources)

# ##################################################################################################
# Test KWin Primary Selection
# ##################################################################################################
set(testPrimarySelection_SRCS primary_selection.cpp)
add_executable(testPrimarySelection ${testPrimarySelection_SRCS})
target_link_libraries(testPrimarySelection
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
)
add_test(NAME wrapland-testPrimarySelection COMMAND testPrimarySelection)
ecm_mark_as_test(testPrimarySelection)

# ##################################################################################################
# Test input-method-unstable-v2
# ##################################################################################################
set(test_input_method_v2_SRCS input_method_v2.cpp)
add_executable(test_input_method_v2 ${test_input_method_v2_SRCS})
target_link_libraries(test_input_method_v2
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
)
add_test(NAME wrapland-test_input_method_v2 COMMAND test_input_method_v2)
ecm_mark_as_test(test_input_method_v2)

# ##################################################################################################
# Test PlasmaShell
# ##################################################################################################
set(testPlasmaShell_SRCS plasma_shell.cpp)
add_executable(testPlasmaShell ${testPlasmaShell_SRCS})
target_link_libraries(testPlasmaShell
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
)
add_test(NAME wrapland-testPlasmaShell COMMAND testPlasmaShell)
ecm_mark_as_test(testPlasmaShell)

# ##################################################################################################
# Test KdeIdle
# ##################################################################################################
set(testKdeIdle_SRCS kde_idle.cpp)
add_executable(testKdeIdle ${testKdeIdle_SRCS})
target_link_libraries(testKdeIdle
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
)
add_test(NAME wrapland-testKdeIdle COMMAND testKdeIdle)
ecm_mark_as_test(testKdeIdle)

# ##################################################################################################
# Test Keyboard Shortcuts Inhibitor
# ##################################################################################################
set(testKeyboardShortcutsInhibitor_SRCS keyboard_shortcuts_inhibitor.cpp)
add_executable(testKeyboardShortcutsInhibitor ${testKeyboardShortcutsInhibitor_SRCS})
target_link_libraries(testKeyboardShortcutsInhibitor
  Qt5::Test
  Qt5::Gui Wrapland::Client
  Wrapland::Server
  Wayland::Client
)
add_test(NAME wrapland-testKeyboardShortcutsInhibitor COMMAND testKeyboardShortcutsInhibitor)
ecm_mark_as_test(testKeyboardShortcutsInhibitor)

# ##################################################################################################
# Test Shadow
# ##################################################################################################
set(testShadow_SRCS shadow.cpp)
add_executable(testShadow ${testShadow_SRCS})
target_link_libraries(testShadow
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
)
add_test(NAME wrapland-testShadow COMMAND testShadow)
ecm_mark_as_test(testShadow)

# ##################################################################################################
# Test FakeInput
# ##################################################################################################
if(HAVE_LINUX_INPUT_H)
  set(testFakeInput_SRCS fake_input.cpp)
  add_executable(testFakeInput ${testFakeInput_SRCS})
  target_link_libraries(testFakeInput
    Qt5::Test
    Qt5::Gui
    Wrapland::Client
    Wrapland::Server
  )
  add_test(NAME wrapland-testFakeInput COMMAND testFakeInput)
  ecm_mark_as_test(testFakeInput)
endif()

# ##################################################################################################
# Test PlasmaWindowModel
# ##################################################################################################
if(HAVE_LINUX_INPUT_H)
  set(testPlasmaWindowModel_SRCS plasma_window_model.cpp)
  add_executable(testPlasmaWindowModel ${testPlasmaWindowModel_SRCS})
  target_link_libraries(testPlasmaWindowModel
    Qt5::Test
    Qt5::Gui
    Wrapland::Client
    Wrapland::Server
  )
  add_test(NAME wrapland-testPlasmaWindowModel COMMAND testPlasmaWindowModel)
  ecm_mark_as_test(testPlasmaWindowModel)
endif()

# ##################################################################################################
# Test TextInput
# ##################################################################################################
set(test_text_input_v2_SRCS text_input_v2.cpp)
add_executable(test-text-input-v2 ${test_text_input_v2_SRCS})
target_link_libraries(test-text-input-v2
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
)
add_test(NAME wrapland-test-text-input-v2 COMMAND test-text-input-v2)
ecm_mark_as_test(test-text-input-v2)

# ##################################################################################################
# Test text-input v3
# ##################################################################################################
set(test_text_input_v3_SRCS text_input_v3.cpp)
add_executable(test-text-input-v3 ${test_text_input_v3_SRCS})
target_link_libraries(test-text-input-v3
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
)
add_test(NAME wrapland-test-text-input-v3 COMMAND test-text-input-v3)
ecm_mark_as_test(test-text-input-v3)

# ##################################################################################################
# Test text-input with input-method synchronization
# ##################################################################################################
set(test_text_input_method_sync_SRCS text_input_method_sync.cpp)
add_executable(test-text-input-method-sync ${test_text_input_method_sync_SRCS})
target_link_libraries(test-text-input-method-sync
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
)
add_test(NAME wrapland-test-text-input-method-sync COMMAND test-text-input-method-sync)
ecm_mark_as_test(test-text-input-method-sync)

# ##################################################################################################
# Test Error
# ##################################################################################################
set(testError_SRCS error.cpp)
add_executable(testError ${testError_SRCS})
target_link_libraries(testError
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
)
add_test(NAME wrapland-testError COMMAND testError)
ecm_mark_as_test(testError)

# ##################################################################################################
# Test Selection
# ##################################################################################################
set(testSelection_SRCS selection.cpp)
add_executable(testSelection ${testSelection_SRCS})
target_link_libraries(testSelection
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
)
add_test(NAME wrapland-testSelection COMMAND testSelection)
ecm_mark_as_test(testSelection)

# ##################################################################################################
# Test XdgForeign
# ##################################################################################################
set(testXdgForeign_SRCS xdg_foreign.cpp)
add_executable(testXdgForeign ${testXdgForeign_SRCS})
target_link_libraries(testXdgForeign
  Qt5::Test
  Qt5::Gui
  Wrapland::Server
  Wrapland::Client
  Wayland::Client
)
add_test(NAME wrapland-testXdgForeign COMMAND testXdgForeign)
ecm_mark_as_test(testXdgForeign)

# ##################################################################################################
# Test XdgShell
# ##################################################################################################
set(testXdgShell_SRCS xdg_shell.cpp)
add_executable(testXdgShell ${testXdgShell_SRCS})
target_link_libraries(testXdgShell
  Qt5::Test
  Qt5::Gui
  Wrapland::Server
  Wrapland::Client
  Wayland::Client
)
add_test(NAME wrapland-testXdgShell COMMAND testXdgShell)
ecm_mark_as_test(testXdgShell)

# ##################################################################################################
# Test Pointer Constraints
# ##################################################################################################
add_executable(testPointerConstraints pointer_constraints.cpp)
target_link_libraries(testPointerConstraints
  Qt5::Test
  Qt5::Gui
  Wrapland::Server
  Wrapland::Client
  Wayland::Client
)
add_test(NAME wrapland-testPointerConstraints COMMAND testPointerConstraints)
ecm_mark_as_test(testPointerConstraints)

# ##################################################################################################
# Test Filter
# ##################################################################################################
set(testFilter_SRCS filter.cpp)
add_executable(testFilter ${testFilter_SRCS})
target_link_libraries(testFilter
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Server
)
add_test(NAME wrapland-testFilter COMMAND testFilter)
ecm_mark_as_test(testFilter)

# ##################################################################################################
# Test Appmenu
# ##################################################################################################
set(testAppmenu_SRCS appmenu.cpp)
add_executable(testAppmenu ${testAppmenu_SRCS})
target_link_libraries(testAppmenu
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
)
add_test(NAME wrapland-testAppmenu COMMAND testAppmenu)
ecm_mark_as_test(testAppmenu)

# ##################################################################################################
# Test Appmenu
# ##################################################################################################
set(testServerSideDecorationPalette_SRCS server_side_decoration_palette.cpp)
add_executable(testServerSideDecorationPalette ${testServerSideDecorationPalette_SRCS})
target_link_libraries(testServerSideDecorationPalette
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
)
add_test(NAME wrapland-testServerSideDecorationPalette COMMAND testServerSideDecorationPalette)
ecm_mark_as_test(testServerSideDecorationPalette)

# ##################################################################################################
# Test PresentationTime
# ##################################################################################################
set(testPresentationTime_SRCS presentation_time.cpp)
add_executable(testPresentationTime ${testPresentationTime_SRCS})
target_link_libraries(testPresentationTime
  Qt5::Test
  Wrapland::Client
  Wrapland::Server
)
add_test(NAME wrapland-testPresentationTime COMMAND testPresentationTime)
ecm_mark_as_test(testPresentationTime)

# ##################################################################################################
# Test VirtualDesktop
# ##################################################################################################
set(testPlasmaVirtualDesktop_SRCS plasma_virtual_desktop.cpp)
add_executable(testPlasmaVirtualDesktop ${testPlasmaVirtualDesktop_SRCS})
target_link_libraries(testPlasmaVirtualDesktop
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
)
add_test(NAME wrapland-testPlasmaVirtualDesktop COMMAND testPlasmaVirtualDesktop)
ecm_mark_as_test(testPlasmaVirtualDesktop)

# ##################################################################################################
# Test XDG Activation
# ##################################################################################################
set(testXdgActivation_SRCS xdg_activation.cpp)
add_executable(testXdgActivation ${testXdgActivation_SRCS})
target_link_libraries(testXdgActivation
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
  Wayland::Server
)
add_test(NAME wrapland-testXdgActivation COMMAND testXdgActivation)
ecm_mark_as_test(testXdgActivation)

# ##################################################################################################
# Test XDG Output
# ##################################################################################################
set(testXdgOutput_SRCS xdg_output.cpp)
add_executable(testXdgOutput ${testXdgOutput_SRCS})
target_link_libraries(testXdgOutput
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
  Wayland::Server
)
add_test(NAME wrapland-testXdgOutput COMMAND testXdgOutput)
ecm_mark_as_test(testXdgOutput)

# ##################################################################################################
# Test XDG Decoration
# ##################################################################################################
set(testXdgdecoration_SRCS xdg_decoration.cpp)
add_executable(testXdgDecoration ${testXdgdecoration_SRCS})
target_link_libraries(testXdgDecoration
  Qt5::Test
  Qt5::Gui
  Wrapland::Client
  Wrapland::Server
  Wayland::Client
  Wayland::Server
)
add_test(NAME wrapland-testXdgDecoration COMMAND testXdgDecoration)
ecm_mark_as_test(testXdgDecoration)
