# Included by app/CMakeLists.txt once the desktop target exists.
set(RAGTIME_MACOS_PACKAGING "${PROJECT_SOURCE_DIR}/packaging/macos")
set(RAGTIME_MACOS_ICON "${CMAKE_CURRENT_BINARY_DIR}/generated/ragtime.icns")
add_custom_command(OUTPUT "${RAGTIME_MACOS_ICON}"
    COMMAND /bin/sh "${RAGTIME_MACOS_PACKAGING}/make-icon.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}/res/ragtime.png" "${RAGTIME_MACOS_ICON}"
    DEPENDS res/ragtime.png "${RAGTIME_MACOS_PACKAGING}/make-icon.sh"
    VERBATIM)
target_sources(ragtime PRIVATE "${RAGTIME_MACOS_ICON}")
set_source_files_properties("${RAGTIME_MACOS_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
set_target_properties(ragtime PROPERTIES
    MACOSX_BUNDLE TRUE
    OUTPUT_NAME Ragtime
    MACOSX_BUNDLE_INFO_PLIST "${RAGTIME_MACOS_PACKAGING}/Info.plist.in"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.ragtime.radio"
    MACOSX_BUNDLE_BUNDLE_NAME Ragtime
    MACOSX_BUNDLE_ICON_FILE ragtime.icns
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
install(TARGETS ragtime BUNDLE DESTINATION .)
