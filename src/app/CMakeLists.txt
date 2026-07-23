add_executable(ninja_log_analyzer WIN32 MACOSX_BUNDLE main.cpp)

target_link_libraries(ninja_log_analyzer
    PRIVATE
        ninja_analyzer_gui
        Qt${QT_VERSION_MAJOR}::Widgets
)
target_compile_definitions(ninja_log_analyzer
    PRIVATE NINJA_ANALYZER_VERSION="${PROJECT_VERSION}"
)

if(APPLE OR WIN32)
    set(ninja_analyzer_output_name "Ninja Log Analyzer")
else()
    set(ninja_analyzer_output_name "ninja_log_analyzer")
endif()

set(ninja_analyzer_runtime_output_directory "${CMAKE_BINARY_DIR}/bin")

set_target_properties(ninja_log_analyzer PROPERTIES
    OUTPUT_NAME "${ninja_analyzer_output_name}"
    RUNTIME_OUTPUT_DIRECTORY "${ninja_analyzer_runtime_output_directory}"
    MACOSX_BUNDLE_BUNDLE_NAME "Ninja Log Analyzer"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.codex.ninjaloganalyzer"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
)

if(APPLE)
    set(ninja_analyzer_icon
        "${PROJECT_SOURCE_DIR}/resources/icons/NinjaLogAnalyzer.icns")
    set_source_files_properties("${ninja_analyzer_icon}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(ninja_log_analyzer PRIVATE "${ninja_analyzer_icon}")

    foreach(configuration DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(ninja_log_analyzer PROPERTIES
            "RUNTIME_OUTPUT_DIRECTORY_${configuration}"
            "${ninja_analyzer_runtime_output_directory}"
        )
    endforeach()

    set(NINJA_ANALYZER_EXECUTABLE_NAME "Ninja Log Analyzer")
    set(NINJA_ANALYZER_ICON_FILE "NinjaLogAnalyzer.icns")
    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/NinjaAnalyzerInfo.plist.in"
        "${CMAKE_CURRENT_BINARY_DIR}/NinjaAnalyzerInfo.plist"
        @ONLY
    )
    set_target_properties(ninja_log_analyzer PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/NinjaAnalyzerInfo.plist"
        MACOSX_BUNDLE_ICON_FILE "${NINJA_ANALYZER_ICON_FILE}"
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
endif()

include(NinjaAnalyzerDelivery)
ninja_analyzer_configure_delivery(ninja_log_analyzer)
