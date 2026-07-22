add_library(ninja_analyzer_core STATIC
    BuildAnalyzer.cpp
    BuildAnalyzer.h
    LogLocator.cpp
    LogLocator.h
    NinjaLogParser.cpp
    NinjaLogParser.h
    NinjaManifestParser.cpp
    NinjaManifestParser.h
    NinjaLogTypes.cpp
    NinjaLogTypes.h
)

target_include_directories(ninja_analyzer_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(ninja_analyzer_core PUBLIC Qt${QT_VERSION_MAJOR}::Core)
target_compile_features(ninja_analyzer_core PUBLIC cxx_std_17)
