cmake_minimum_required(VERSION 3.20)

project(NinjaLogAnalyzer
    VERSION 0.1.0
    DESCRIPTION "Visual analyzer for Ninja .ninja_log build timings"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

# Qt 6.4's macOS package adds AGL to the consuming application's OpenGL
# interface. Xcode 26 removed AGL from the SDK even though the prebuilt QtGui
# framework still carries its own runtime dependency. Remove only that redundant
# application-level item when the active SDK cannot provide it.
if(APPLE AND QT_VERSION_MAJOR EQUAL 6 AND TARGET WrapOpenGL::WrapOpenGL)
    set(_ninja_analyzer_macos_sdk "${CMAKE_OSX_SYSROOT}")
    if(NOT _ninja_analyzer_macos_sdk)
        execute_process(
            COMMAND xcrun --sdk macosx --show-sdk-path
            OUTPUT_VARIABLE _ninja_analyzer_macos_sdk
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()

    if(_ninja_analyzer_macos_sdk
       AND NOT EXISTS "${_ninja_analyzer_macos_sdk}/System/Library/Frameworks/AGL.framework")
        foreach(_ninja_analyzer_gl_target WrapOpenGL::WrapOpenGL OpenGL::GL)
            if(TARGET ${_ninja_analyzer_gl_target})
                get_target_property(
                    _ninja_analyzer_gl_links
                    ${_ninja_analyzer_gl_target}
                    INTERFACE_LINK_LIBRARIES
                )
                if(_ninja_analyzer_gl_links)
                    list(FILTER _ninja_analyzer_gl_links EXCLUDE REGEX "AGL")
                    set_property(
                        TARGET ${_ninja_analyzer_gl_target}
                        PROPERTY INTERFACE_LINK_LIBRARIES "${_ninja_analyzer_gl_links}"
                    )
                endif()
            endif()
        endforeach()
        message(STATUS "Qt6 compatibility: removed unavailable AGL SDK link interface")
    endif()
endif()

add_library(ninja_analyzer_core STATIC
    src/core/BuildAnalyzer.cpp
    src/core/BuildAnalyzer.h
    src/core/LogLocator.cpp
    src/core/LogLocator.h
    src/core/NinjaLogParser.cpp
    src/core/NinjaLogParser.h
    src/core/NinjaManifestParser.cpp
    src/core/NinjaManifestParser.h
    src/core/NinjaLogTypes.cpp
    src/core/NinjaLogTypes.h
)
target_include_directories(ninja_analyzer_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ninja_analyzer_core PUBLIC Qt${QT_VERSION_MAJOR}::Core)
target_compile_features(ninja_analyzer_core PUBLIC cxx_std_17)

set(NINJA_ANALYZER_GUI_SOURCES
    src/gui/CategoryPalette.h
    src/gui/MainWindow.cpp
    src/gui/MainWindow.h
    src/gui/OverviewChartsWidget.cpp
    src/gui/OverviewChartsWidget.h
    src/gui/SlowTasksModel.cpp
    src/gui/SlowTasksModel.h
    src/gui/TimelineWidget.cpp
    src/gui/TimelineWidget.h
)

add_executable(ninja_log_analyzer WIN32 MACOSX_BUNDLE
    src/main.cpp
    ${NINJA_ANALYZER_GUI_SOURCES}
)
target_link_libraries(ninja_log_analyzer
    PRIVATE
        ninja_analyzer_core
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
)
set_target_properties(ninja_log_analyzer PROPERTIES
    OUTPUT_NAME "Ninja Log Analyzer"
    MACOSX_BUNDLE_BUNDLE_NAME "Ninja Log Analyzer"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.codex.ninjaloganalyzer"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
    add_executable(ninja_analyzer_tests
        tests/tst_core.cpp
    )
    target_link_libraries(ninja_analyzer_tests
        PRIVATE
            ninja_analyzer_core
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Test
    )
    add_test(NAME ninja_analyzer_tests COMMAND ninja_analyzer_tests)

    add_executable(ninja_analyzer_gui_tests
        tests/tst_mainwindow.cpp
        ${NINJA_ANALYZER_GUI_SOURCES}
    )
    target_link_libraries(ninja_analyzer_gui_tests
        PRIVATE
            ninja_analyzer_core
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Widgets
            Qt${QT_VERSION_MAJOR}::Test
    )
    add_test(NAME ninja_analyzer_gui_tests COMMAND ninja_analyzer_gui_tests)
    set_tests_properties(ninja_analyzer_gui_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()

install(TARGETS ninja_log_analyzer
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
