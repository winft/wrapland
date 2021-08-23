include(ECMMarkAsTest)

# find_package(Qt5Core ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENT Private _check_private)
include_directories(SYSTEM ${Qt5Core_PRIVATE_INCLUDE_DIRS})
set(testServer_SRCS waylandservertest.cpp)
add_executable(testServer ${testServer_SRCS})
target_link_libraries(testServer
  Wrapland::Server
)
ecm_mark_as_test(testServer)

find_package(Qt5Widgets ${QT_MIN_VERSION} CONFIG QUIET)
if(Qt5Widgets_FOUND)
  set(testRenderingServer_SRCS renderingservertest.cpp)
  add_executable(testRenderingServer ${testRenderingServer_SRCS})
  target_link_libraries(testRenderingServer
    Wrapland::Server
    Qt5::Concurrent
    Qt5::Widgets
  )
  ecm_mark_as_test(testRenderingServer)
endif()

add_executable(copyClient copyclient.cpp)
target_link_libraries(copyClient
  Wrapland::Client
)
ecm_mark_as_test(copyClient)

add_executable(pasteClient pasteclient.cpp)
target_link_libraries(pasteClient
  Qt5::Concurrent
  Wrapland::Client
)
ecm_mark_as_test(pasteClient)

if(HAVE_LINUX_INPUT_H)
  add_executable(touchClientTest touchclienttest.cpp)
  target_link_libraries(touchClientTest
    Wrapland::Client
  )

  add_executable(panelTest paneltest.cpp)
  target_link_libraries(panelTest
    Wrapland::Client
  )
  ecm_mark_as_test(panelTest)

  add_executable(qtwayland-integration-test qtwaylandintegrationtest.cpp)
  target_link_libraries(qtwayland-integration-test
    Qt5::Core
    Qt5::Gui
    Wrapland::Client
  )
  ecm_mark_as_test(qtwayland-integration-test)

  add_executable(subsurface-test subsurfacetest.cpp)
  target_link_libraries(subsurface-test
    Qt5::Core
    Qt5::Gui
    Wrapland::Client
  )
  ecm_mark_as_test(subsurface-test)
endif()

add_executable(shadowTest shadowtest.cpp)
target_link_libraries(shadowTest
  Wrapland::Client
)
ecm_mark_as_test(shadowTest)

if(Qt5Widgets_FOUND)
  add_executable(dpmsTest dpmstest.cpp)
  target_link_libraries(dpmsTest
    Wrapland::Client
    Qt5::Widgets
  )
  ecm_mark_as_test(dpmsTest)
endif()

add_executable(plasmasurface-test plasmasurfacetest.cpp)
target_link_libraries(plasmasurface-test
  Qt5::Gui
  Wrapland::Client
)
ecm_mark_as_test(plasmasurface-test)

add_executable(xdgforeign-test xdgforeigntest.cpp)
target_link_libraries(xdgforeign-test
  Qt5::Gui
  Wrapland::Client
)
ecm_mark_as_test(xdgforeign-test)

add_executable(xdg-test xdgtest.cpp)
target_link_libraries(xdg-test
  Qt5::Gui
  Wrapland::Client
)
ecm_mark_as_test(xdg-test)
