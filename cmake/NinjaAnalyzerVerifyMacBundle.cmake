foreach(required_variable APP_BUNDLE EXPECTED_BUNDLE LEGACY_BUNDLE)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "Missing required variable: ${required_variable}")
    endif()
endforeach()

get_filename_component(actual_bundle "${APP_BUNDLE}" ABSOLUTE)
get_filename_component(expected_bundle "${EXPECTED_BUNDLE}" ABSOLUTE)
if(NOT actual_bundle STREQUAL expected_bundle)
    message(FATAL_ERROR
        "macOS app bundle path mismatch: expected '${expected_bundle}', got '${actual_bundle}'")
endif()

if(NOT IS_DIRECTORY "${actual_bundle}")
    message(FATAL_ERROR "macOS app bundle does not exist: ${actual_bundle}")
endif()
if(NOT EXISTS "${actual_bundle}/Contents/Info.plist")
    message(FATAL_ERROR "macOS app bundle is missing Contents/Info.plist")
endif()
if(NOT EXISTS "${actual_bundle}/Contents/MacOS/Ninja Log Analyzer")
    message(FATAL_ERROR "macOS app bundle is missing its main executable")
endif()
if(NOT IS_DIRECTORY "${actual_bundle}/Contents/Frameworks")
    message(FATAL_ERROR
        "macOS app bundle is missing deployed Qt frameworks")
endif()
if(NOT EXISTS
   "${actual_bundle}/Contents/PlugIns/platforms/libqcocoa.dylib")
    message(FATAL_ERROR
        "macOS app bundle is missing the Qt cocoa platform plugin")
endif()
if(EXISTS "${LEGACY_BUNDLE}")
    message(FATAL_ERROR
        "Legacy duplicate macOS app bundle must not exist: ${LEGACY_BUNDLE}")
endif()

set(bundle_executable
    "${actual_bundle}/Contents/MacOS/Ninja Log Analyzer")
execute_process(
    COMMAND otool -l "${bundle_executable}"
    OUTPUT_VARIABLE bundle_load_commands
    RESULT_VARIABLE bundle_otool_result
)
if(NOT bundle_otool_result EQUAL 0)
    message(FATAL_ERROR
        "Cannot inspect macOS app bundle load commands")
endif()
if(NOT bundle_load_commands MATCHES
   "@executable_path/../Frameworks")
    message(FATAL_ERROR
        "macOS app bundle is missing its Frameworks runtime search path")
endif()

message(STATUS
    "Verified self-contained macOS build bundle: ${actual_bundle}")
