include(ECMMarkAsTest)

include_directories(SYSTEM ${QtCore_PRIVATE_INCLUDE_DIRS})
set(testServer_SRCS waylandservertest.cpp)
add_executable(testServer ${testServer_SRCS})
target_link_libraries(testServer
  Qt6::GuiPrivate
  Wrapland::Server
)
ecm_mark_as_test(testServer)

find_package(Qt6Widgets ${QT_MIN_VERSION} CONFIG QUIET)
if(Qt6Widgets_FOUND)
  set(testRenderingServer_SRCS renderingservertest.cpp)
  add_executable(testRenderingServer ${testRenderingServer_SRCS})
  target_link_libraries(testRenderingServer
    Wrapland::Server
    Qt6::Concurrent
    Qt6::Widgets
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
  Qt6::Concurrent
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
    Qt6::Core
    Qt6::Gui
    Wrapland::Client
  )
  ecm_mark_as_test(qtwayland-integration-test)

  add_executable(subsurface-test subsurfacetest.cpp)
  target_link_libraries(subsurface-test
    Qt6::Core
    Qt6::Gui
    Wrapland::Client
  )
  ecm_mark_as_test(subsurface-test)
endif()

add_executable(shadowTest shadowtest.cpp)
target_link_libraries(shadowTest
  Wrapland::Client
)
ecm_mark_as_test(shadowTest)

if(Qt6Widgets_FOUND)
  add_executable(dpmsTest dpmstest.cpp)
  target_link_libraries(dpmsTest
    Wrapland::Client
    Qt6::Widgets
  )
  ecm_mark_as_test(dpmsTest)
endif()

add_executable(plasmasurface-test plasmasurfacetest.cpp)
target_link_libraries(plasmasurface-test
  Qt6::Gui
  Wrapland::Client
)
ecm_mark_as_test(plasmasurface-test)

add_executable(xdgforeign-test xdgforeigntest.cpp)
target_link_libraries(xdgforeign-test
  Qt6::Gui
  Wrapland::Client
)
ecm_mark_as_test(xdgforeign-test)

add_executable(xdg-test xdgtest.cpp)
target_link_libraries(xdg-test
  Qt6::Gui
  Wrapland::Client
)
ecm_mark_as_test(xdg-test)
