# Internal helper library only for compilation of the other exported libraries.
add_library(external INTERFACE)
target_include_directories(external
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
add_library(external::external ALIAS external)
