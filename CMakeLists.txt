cmake_minimum_required(VERSION 3.16)

project(Wrapland)
set(PROJECT_VERSION "0.527.0")
set(PROJECT_VERSION_MAJOR 0)

# ECM setup
include(FeatureSummary)
find_package(ECM 5.91.0 NO_MODULE)
set_package_properties(ECM PROPERTIES
  TYPE REQUIRED
  DESCRIPTION "Extra CMake Modules."
  URL "https://commits.kde.org/extra-cmake-modules"
)
feature_summary(WHAT REQUIRED_PACKAGES_NOT_FOUND FATAL_ON_MISSING_REQUIRED_PACKAGES)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/Modules)

include(BuildType)
include(FeatureSummary)
include(ECMGenerateExportHeader)
include(CMakePackageConfigHelpers)
include(ECMSetupVersion)
include(ECMGenerateHeaders)
include(CMakeFindFrameworks)
include(ECMQtDeclareLoggingCategory)
include(ECMPoQmTools)
include(ECMAddQch)

option(BUILD_QCH
       "Build API documentation in QCH format (for e.g. Qt Assistant, Qt Creator & KDevelop)" OFF
)
add_feature_info(
  QCH ${BUILD_QCH} "API documentation in QCH format (for e.g. Qt Assistant, Qt Creator & KDevelop)"
)


ecm_setup_version(PROJECT
  VARIABLE_PREFIX WRAPLAND
  VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/wrapland_version.h"
  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/WraplandConfigVersion.cmake"
  SOVERSION 0
)

# Dependencies
set(REQUIRED_QT_VERSION 5.15.0)
set(KDE_COMPILERSETTINGS_LEVEL "5.84")
find_package(Qt5 ${REQUIRED_QT_VERSION} CONFIG REQUIRED Concurrent Gui)

find_package(Wayland 1.18 COMPONENTS Client Server)
set_package_properties(Wayland PROPERTIES TYPE REQUIRED)

find_package(WaylandScanner)

find_package(WaylandProtocols 1.27)
set_package_properties(WaylandProtocols PROPERTIES TYPE REQUIRED)

find_package(EGL)
set_package_properties(EGL PROPERTIES TYPE REQUIRED)

include(KDEInstallDirs)
include(CheckIncludeFile)
include(CTest)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)
set(CMAKE_LINK_DEPENDS_NO_SHARED ON)
option(BUILD_SHARED_LIBS "If enabled, shared libs will be built by default, otherwise static libs" ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

check_include_file("linux/input.h" HAVE_LINUX_INPUT_H)
configure_file(config-wrapland.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-wrapland.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# adjusting CMAKE_C_FLAGS to get wayland protocols to compile
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu90")

add_definitions(-DQT_NO_FOREACH)
add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x050d00)
# Subdirectories
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/po")
  ecm_install_po_files_as_qm(po)
endif()

add_subdirectory(extern)
add_subdirectory(server)
add_subdirectory(src)

if(BUILD_TESTING)
  add_subdirectory(autotests)
  add_subdirectory(tests)
endif()

# create a Config.cmake and a ConfigVersion.cmake file and install them
set(CMAKECONFIG_INSTALL_DIR "${CMAKECONFIG_INSTALL_PREFIX}/Wrapland")

if(BUILD_QCH)
  ecm_install_qch_export(
    TARGETS Wrapland_QCH
    FILE WraplandQchTargets.cmake
    DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
    COMPONENT Devel
  )
  set(PACKAGE_INCLUDE_QCHTARGETS "include(\"\${CMAKE_CURRENT_LIST_DIR}/WraplandQchTargets.cmake\")")
endif()

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/WraplandConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/WraplandConfig.cmake" INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR}
)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/WraplandConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/WraplandConfigVersion.cmake"
  DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
  COMPONENT Devel
)

install(
  EXPORT WraplandTargets
  DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
  FILE WraplandTargets.cmake
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/wrapland_version.h
  DESTINATION ${INCLUDE_INSTALL_DIR}
  COMPONENT Devel
)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
