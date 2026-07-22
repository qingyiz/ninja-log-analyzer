add_executable(ninja_analyzer_tests tst_core.cpp)
target_link_libraries(ninja_analyzer_tests
    PRIVATE
        ninja_analyzer_core
        Qt${QT_VERSION_MAJOR}::Test
)
add_test(NAME ninja_analyzer_tests COMMAND ninja_analyzer_tests)

add_executable(ninja_analyzer_application_tests tst_application.cpp)
target_link_libraries(ninja_analyzer_application_tests
    PRIVATE
        ninja_analyzer_application
        Qt${QT_VERSION_MAJOR}::Test
)
add_test(NAME ninja_analyzer_application_tests COMMAND ninja_analyzer_application_tests)

add_executable(ninja_analyzer_gui_tests tst_mainwindow.cpp)
target_link_libraries(ninja_analyzer_gui_tests
    PRIVATE
        ninja_analyzer_gui
        Qt${QT_VERSION_MAJOR}::Test
)
add_test(NAME ninja_analyzer_gui_tests COMMAND ninja_analyzer_gui_tests)
set_tests_properties(ninja_analyzer_gui_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
