add_library(ninja_analyzer_gui STATIC
    AnalysisResultsWidget.cpp
    AnalysisResultsWidget.h
    AppStyle.cpp
    AppStyle.h
    CategoryPalette.h
    MainWindow.cpp
    MainWindow.h
    OverviewChartsWidget.cpp
    OverviewChartsWidget.h
    OverviewPage.cpp
    OverviewPage.h
    SlowTasksModel.cpp
    SlowTasksModel.h
    SlowTasksPage.cpp
    SlowTasksPage.h
    TimelinePage.cpp
    TimelinePage.h
    TimelineWidget.cpp
    TimelineWidget.h
)

target_include_directories(ninja_analyzer_gui PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(ninja_analyzer_gui
    PUBLIC
        ninja_analyzer_application
        Qt${QT_VERSION_MAJOR}::Widgets
    PRIVATE
        ninja_analyzer_core
)
target_compile_features(ninja_analyzer_gui PUBLIC cxx_std_17)
