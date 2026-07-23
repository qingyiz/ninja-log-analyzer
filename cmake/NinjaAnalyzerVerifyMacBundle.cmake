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
if(EXISTS "${LEGACY_BUNDLE}")
    message(FATAL_ERROR
        "Legacy duplicate macOS app bundle must not exist: ${LEGACY_BUNDLE}")
endif()

message(STATUS "Verified macOS build bundle: ${actual_bundle}")
