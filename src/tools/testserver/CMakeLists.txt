add_executable(org-kde-kf5-wrapland-testserver main.cpp testserver.cpp)
target_link_libraries(org-kde-kf5-wrapland-testserver
  Qt5::Core
  Wrapland::Server
)

install(TARGETS org-kde-kf5-wrapland-testserver DESTINATION ${LIBEXEC_INSTALL_DIR})
