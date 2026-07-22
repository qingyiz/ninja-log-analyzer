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

set_target_properties(ninja_log_analyzer PROPERTIES
    OUTPUT_NAME "${ninja_analyzer_output_name}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    MACOSX_BUNDLE_BUNDLE_NAME "Ninja Log Analyzer"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.codex.ninjaloganalyzer"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
)

if(APPLE)
    set(NINJA_ANALYZER_EXECUTABLE_NAME "Ninja Log Analyzer")
    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/NinjaAnalyzerInfo.plist.in"
        "${CMAKE_CURRENT_BINARY_DIR}/NinjaAnalyzerInfo.plist"
        @ONLY
    )
    set_target_properties(ninja_log_analyzer PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/NinjaAnalyzerInfo.plist"
    )
endif()

include(NinjaAnalyzerDelivery)
ninja_analyzer_configure_delivery(ninja_log_analyzer)
