function(ninja_analyzer_apply_qt_compatibility)
    # Qt 6.4's macOS package can add AGL to the consuming application's
    # OpenGL interface. Xcode 26 removed AGL, so remove only that redundant
    # application-level item when the active SDK does not provide it.
    if(NOT APPLE OR NOT QT_VERSION_MAJOR EQUAL 6 OR NOT TARGET WrapOpenGL::WrapOpenGL)
        return()
    endif()

    set(macos_sdk "${CMAKE_OSX_SYSROOT}")
    if(NOT macos_sdk)
        execute_process(
            COMMAND xcrun --sdk macosx --show-sdk-path
            OUTPUT_VARIABLE macos_sdk
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()
    if(NOT macos_sdk OR EXISTS "${macos_sdk}/System/Library/Frameworks/AGL.framework")
        return()
    endif()

    foreach(gl_target WrapOpenGL::WrapOpenGL OpenGL::GL)
        if(TARGET ${gl_target})
            get_target_property(gl_links ${gl_target} INTERFACE_LINK_LIBRARIES)
            if(gl_links)
                list(FILTER gl_links EXCLUDE REGEX "AGL")
                set_property(TARGET ${gl_target} PROPERTY INTERFACE_LINK_LIBRARIES "${gl_links}")
            endif()
        endif()
    endforeach()
    message(STATUS "Qt6 compatibility: removed unavailable AGL SDK link interface")
endfunction()
