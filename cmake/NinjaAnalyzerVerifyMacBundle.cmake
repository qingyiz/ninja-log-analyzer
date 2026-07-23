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
set(bundle_icon
    "${actual_bundle}/Contents/Resources/NinjaLogAnalyzer.icns")
if(NOT EXISTS "${bundle_icon}")
    message(FATAL_ERROR
        "macOS app bundle is missing Contents/Resources/NinjaLogAnalyzer.icns")
endif()
execute_process(
    COMMAND /usr/libexec/PlistBuddy
            -c "Print :CFBundleIconFile"
            "${actual_bundle}/Contents/Info.plist"
    OUTPUT_VARIABLE bundle_icon_file
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE bundle_icon_plist_result
)
if(NOT bundle_icon_plist_result EQUAL 0
   OR NOT bundle_icon_file STREQUAL "NinjaLogAnalyzer.icns")
    message(FATAL_ERROR
        "macOS app bundle Info.plist has an invalid CFBundleIconFile")
endif()
get_filename_component(bundle_parent "${actual_bundle}" DIRECTORY)
set(icon_verify_dir
    "${bundle_parent}/.NinjaLogAnalyzer-verify.iconset")
file(REMOVE_RECURSE "${icon_verify_dir}")
execute_process(
    COMMAND /usr/bin/iconutil
            --convert iconset
            "${bundle_icon}"
            --output "${icon_verify_dir}"
    RESULT_VARIABLE bundle_iconutil_result
)
if(NOT bundle_iconutil_result EQUAL 0)
    message(FATAL_ERROR
        "macOS app bundle icon cannot be expanded by iconutil")
endif()
foreach(icon_representation
        icon_16x16.png
        icon_16x16@2x.png
        icon_32x32.png
        icon_32x32@2x.png
        icon_128x128.png
        icon_128x128@2x.png
        icon_256x256.png
        icon_256x256@2x.png
        icon_512x512.png
        icon_512x512@2x.png)
    if(NOT EXISTS "${icon_verify_dir}/${icon_representation}")
        message(FATAL_ERROR
            "macOS app bundle icon is missing ${icon_representation}")
    endif()
endforeach()
file(REMOVE_RECURSE "${icon_verify_dir}")
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
    "Verified self-contained macOS build bundle and icon: ${actual_bundle}")
