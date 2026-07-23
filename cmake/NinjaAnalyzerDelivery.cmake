function(ninja_analyzer_configure_delivery target)
    install(TARGETS ${target}
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
    )

    if(NOT APPLE)
        return()
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            "-DAPP_BUNDLE=$<TARGET_BUNDLE_DIR:${target}>"
            "-DEXPECTED_BUNDLE=${CMAKE_BINARY_DIR}/Ninja Log Analyzer.app"
            "-DLEGACY_BUNDLE=${CMAKE_BINARY_DIR}/bin/Ninja Log Analyzer.app"
            -P "${PROJECT_SOURCE_DIR}/cmake/NinjaAnalyzerVerifyMacBundle.cmake"
        VERBATIM
    )

    set(qt_dir_variable "Qt${QT_VERSION_MAJOR}_DIR")
    set(qt_package_dir "${${qt_dir_variable}}")
    get_filename_component(qt_prefix "${qt_package_dir}/../../.." ABSOLUTE)
    find_program(NINJA_ANALYZER_MACDEPLOYQT
        NAMES macdeployqt
        HINTS "${qt_prefix}/bin"
        NO_DEFAULT_PATH
        REQUIRED
    )

    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/NinjaAnalyzerDeployInstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/NinjaAnalyzerDeployInstall.cmake"
        @ONLY
    )
    install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/NinjaAnalyzerDeployInstall.cmake")
endfunction()
