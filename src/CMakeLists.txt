add_subdirectory(client)
add_subdirectory(tools)

install( FILES org_kde_wrapland.categories DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR} )


if(BUILD_QCH)
    macro(_make_absolute var_name base_path)
        set(_result)
        foreach(_path ${${var_name}})
            if(IS_ABSOLUTE "${_path}")
                list(APPEND _result "${_path}")
            else()
                list(APPEND _result "${base_path}/${_path}")
            endif()
        endforeach()
        set(${var_name} ${_result})
    endmacro()

    _make_absolute(WraplandClient_APIDOX_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/client")
    _make_absolute(WraplandServer_APIDOX_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/../server")

    ecm_add_qch(
        Wrapland_QCH
        NAME Wrapland
        BASE_NAME Wrapland
        VERSION ${PROJECT_VERSION}
        ORG_DOMAIN org.kde
        SOURCES # using only public headers, to cover only public API
            ${WraplandClient_APIDOX_SRCS}
            ${WraplandServer_APIDOX_SRCS}
        MD_MAINPAGE "${CMAKE_SOURCE_DIR}/README.md"
        LINK_QCHS
            Qt5Gui_QCH
        INCLUDE_DIRS
            ${WraplandClient_APIDOX_BUILD_INCLUDE_DIRS}
            ${WraplandServer_APIDOX_BUILD_INCLUDE_DIRS}
        BLANK_MACROS
            WRAPLANDCLIENT_EXPORT
            WRAPLANDSERVER_EXPORT
        TAGFILE_INSTALL_DESTINATION ${KDE_INSTALL_QTQCHDIR}
        QCH_INSTALL_DESTINATION ${KDE_INSTALL_QTQCHDIR}
        COMPONENT Devel
    )
endif()
