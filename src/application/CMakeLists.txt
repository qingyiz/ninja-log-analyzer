add_library(ninja_analyzer_application STATIC
    AnalysisService.cpp
    AnalysisService.h
)

target_include_directories(ninja_analyzer_application PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(ninja_analyzer_application
    PUBLIC
        ninja_analyzer_core
        Qt${QT_VERSION_MAJOR}::Core
)
target_compile_features(ninja_analyzer_application PUBLIC cxx_std_17)
