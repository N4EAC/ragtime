# The Ragtime desktop app: wxWidgets on top of ragtime_core. Keep logic in
# core where it can be tested without a display; the app draws and wires.

if(RAGTIME_BUILD_WX)
    include(BuildWxWidgets)
else()
    find_package(wxWidgets 3.2 REQUIRED COMPONENTS html net core base)
    set(RAGTIME_WX_LIBS ${wxWidgets_LIBRARIES})
endif()

# The window icon, built in: ragtime.ico from a resource on Windows, the PNG
# elsewhere. tools/icon/make_icons.sh renders both from res/ragtime.svg.
if(WIN32)
    set(RAGTIME_RESOURCES res/ragtime.rc)
    # windres looks for ragtime.ico on the include path.
    set(RAGTIME_RESOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/res)
else()
    include(EmbedFile)
    ragtime_embed_file(${CMAKE_CURRENT_SOURCE_DIR}/res/ragtime.png
        ${CMAKE_CURRENT_BINARY_DIR}/generated/RagtimeIconPng.h RAGTIME_ICON_PNG)
endif()

add_executable(ragtime WIN32 MACOSX_BUNDLE
    src/RagtimeApp.cpp
    src/AppIcon.cpp
    src/MainFrame.cpp
    src/BroadcastPage.cpp
    src/ChatList.cpp
    src/ConnectionBar.cpp
    src/FilesPage.cpp
    src/GearshiftDialog.cpp
    src/DrawnIcon.cpp
    src/DeckParts.cpp
    src/VoiceDeck.cpp
    src/EarControls.cpp
    src/EarFilterBar.cpp
    src/EarFrame.cpp
    src/EarScope.cpp
    src/GearshiftIcon.cpp
    src/HostConnection.cpp
    src/PathStripView.cpp
    src/PathsDialog.cpp
    src/RigConnection.cpp
    src/RigControl.cpp
    src/Html.cpp
    src/PhraseHighlight.cpp
    src/PictureDialog.cpp
    src/PictureFrame.cpp
    src/PictureView.cpp
    src/Pictures.cpp
    src/ReactionChips.cpp
    src/SealBadge.cpp
    src/Prefs.cpp
    src/QuitSignal.cpp
    src/Report.cpp
    src/SessionPage.cpp
    src/SettingsDialog.cpp
    src/RecentWorkPane.cpp
    src/StationList.cpp
    src/Thresholds.cpp
    src/ToggleSwitch.cpp
    src/Theme.cpp
    src/VoiceCards.cpp
    src/SwrBar.cpp
    src/UnpickClick.cpp
    src/ArrivalBar.cpp
    src/TxLamp.cpp
    src/WatchDialog.cpp
    src/globus/GlobusPanel.cpp
    src/globus/GlobusToggle.cpp
    src/globus/GlobusView.cpp
    ${RAGTIME_RESOURCES}
)
target_include_directories(ragtime PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated ${RAGTIME_RESOURCE_DIR})

# The ham phrase highlight in the entry lines talks to GTK itself
# (PhraseHighlight.cpp): a GtkEntry takes no wx text styles.
if(UNIX AND NOT APPLE AND NOT RAGTIME_BUILD_WX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    target_include_directories(ragtime SYSTEM PRIVATE ${GTK3_INCLUDE_DIRS})
    target_link_libraries(ragtime PRIVATE ${GTK3_LIBRARIES})
endif()

# wx headers go in as system headers, so a warning in wxWidgets itself never
# fails a -Werror build of our code.
if(RAGTIME_BUILD_WX)
    foreach(lib ${RAGTIME_WX_LIBS})
        get_target_property(dirs ${lib} INTERFACE_INCLUDE_DIRECTORIES)
        target_include_directories(ragtime SYSTEM PRIVATE ${dirs})
    endforeach()
else()
    target_include_directories(ragtime SYSTEM PRIVATE ${wxWidgets_INCLUDE_DIRS})
    target_compile_definitions(ragtime PRIVATE ${wxWidgets_DEFINITIONS})
    if(wxWidgets_CXX_FLAGS)
        separate_arguments(RAGTIME_WX_CXX_FLAGS UNIX_COMMAND "${wxWidgets_CXX_FLAGS}")
        target_compile_options(ragtime PRIVATE ${RAGTIME_WX_CXX_FLAGS})
    endif()
endif()
target_link_libraries(ragtime PRIVATE ragtime_core ragtime_rig ragtime_audio ragtime_image ragtime_denoise ${RAGTIME_WX_LIBS})
if(APPLE)
    enable_language(OBJCXX)
    target_sources(ragtime PRIVATE src/AppIconMac.mm)

    set_source_files_properties(res/ragtime.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(ragtime PRIVATE res/ragtime.icns)
    set_target_properties(ragtime PROPERTIES
        MACOSX_BUNDLE_ICON_FILE ragtime.icns
        OUTPUT_NAME Ragtime
        MACOSX_BUNDLE_BUNDLE_NAME Ragtime
        MACOSX_BUNDLE_GUI_IDENTIFIER org.baitisj.ragtime
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/res/RagtimeInfo.plist.in)
    target_link_options(ragtime PRIVATE "LINKER:-headerpad_max_install_names")
    if(RAGTIME_BUILD_SOTTO AND EXISTS "${RAGTIME_SOTTO_LIBRARY}")
        set(sottoLibraries "${RAGTIME_SOTTO_LIBRARY}" "${RAGTIME_SOTTO_OUT}/libcodec2.1.2.dylib")
        set(sottoNotices "${RAGTIME_SOTTO_OUT}/licences/sotto.txt" "${RAGTIME_SOTTO_OUT}/licences/codec2.txt")
        set_source_files_properties(${sottoLibraries} PROPERTIES MACOSX_PACKAGE_LOCATION Frameworks)
        set_source_files_properties(${sottoNotices} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/Licenses)
        target_sources(ragtime PRIVATE ${sottoLibraries} ${sottoNotices})
    endif()
endif()

if(RAGTIME_EMBED_DATA2G)
    target_sources(ragtime PRIVATE src/ConstellationPane.cpp src/ModemMonitor.cpp)
    add_library(ragtime_modem STATIC src/EmbeddedModem.cpp)
    target_compile_definitions(ragtime_modem PUBLIC RAGTIME_EMBED_DATA2G)
    target_compile_features(ragtime_modem PRIVATE cxx_std_20)
    if(RAGTIME_BUILD_WX)
        foreach(lib ${RAGTIME_WX_LIBS})
            get_target_property(dirs ${lib} INTERFACE_INCLUDE_DIRECTORIES)
            target_include_directories(ragtime_modem SYSTEM PRIVATE ${dirs})
        endforeach()
    else()
        target_include_directories(ragtime_modem SYSTEM PRIVATE ${wxWidgets_INCLUDE_DIRS})
        target_compile_definitions(ragtime_modem PRIVATE ${wxWidgets_DEFINITIONS})
        target_compile_options(ragtime_modem PRIVATE ${RAGTIME_WX_CXX_FLAGS})
    endif()
    target_link_libraries(ragtime_modem PRIVATE ragtime_audio data2g_station ${RAGTIME_WX_LIBS})
    ragtime_warnings(ragtime_modem)
    target_link_libraries(ragtime PRIVATE ragtime_modem)

    # Data2G can keep the same VERSION reply after a post-release fix.
    if(EXISTS "${RAGTIME_DATA2G_SOURCE_DIR}/.git")
        find_package(Git QUIET)
        if(Git_FOUND)
            execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
                WORKING_DIRECTORY "${RAGTIME_DATA2G_SOURCE_DIR}"
                RESULT_VARIABLE data2gRevisionResult
                OUTPUT_VARIABLE data2gRevision
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
            if(data2gRevisionResult STREQUAL "0")
                set(data2gRevisionFile "${CMAKE_CURRENT_BINARY_DIR}/generated/Data2G-revision.txt")
                file(CONFIGURE OUTPUT "${data2gRevisionFile}" CONTENT "${data2gRevision}\n")
                set_source_files_properties("${data2gRevisionFile}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
                target_sources(ragtime PRIVATE "${data2gRevisionFile}")
            endif()
        endif()
    endif()

    foreach(component IN ITEMS Data2G miniaudio pocketfft)
        if(component STREQUAL "Data2G")
            set(license "${RAGTIME_DATA2G_SOURCE_DIR}/LICENSE")
        else()
            set(license "${RAGTIME_DATA2G_SOURCE_DIR}/native/third_party/${component}/LICENSE")
        endif()
        set(notice "${CMAKE_CURRENT_BINARY_DIR}/generated/${component}-LICENSE.txt")
        configure_file("${license}" "${notice}" COPYONLY)
        set_source_files_properties("${notice}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources/Licenses)
        target_sources(ragtime PRIVATE "${notice}")
    endforeach()
endif()

ragtime_warnings(ragtime)

if(RAGTIME_TESTS AND NOT WIN32)
    # Runs without a display: --version answers before the toolkit starts.
    add_test(NAME ragtime_app_version COMMAND ragtime --version)
    set_tests_properties(ragtime_app_version PROPERTIES
        PASS_REGULAR_EXPRESSION "^Ragtime ${PROJECT_VERSION}")
endif()

# The chat list's scrolling, on a real window: skipped (77) with no display.
if(RAGTIME_TESTS AND NOT WIN32)
    add_executable(ragtime_chat_list_test ${PROJECT_SOURCE_DIR}/tests/ChatListTest.cpp src/ChatList.cpp)
    target_include_directories(ragtime_chat_list_test PRIVATE src ${PROJECT_SOURCE_DIR}/tests)
    if(RAGTIME_BUILD_WX)
        foreach(lib ${RAGTIME_WX_LIBS})
            get_target_property(dirs ${lib} INTERFACE_INCLUDE_DIRECTORIES)
            target_include_directories(ragtime_chat_list_test SYSTEM PRIVATE ${dirs})
        endforeach()
    else()
        target_include_directories(ragtime_chat_list_test SYSTEM PRIVATE ${wxWidgets_INCLUDE_DIRS})
        target_compile_definitions(ragtime_chat_list_test PRIVATE ${wxWidgets_DEFINITIONS})
        if(wxWidgets_CXX_FLAGS)
            target_compile_options(ragtime_chat_list_test PRIVATE ${RAGTIME_WX_CXX_FLAGS})
        endif()
    endif()
    target_link_libraries(ragtime_chat_list_test PRIVATE ragtime_core ${RAGTIME_WX_LIBS})
    if(GTK3_FOUND)
        target_include_directories(ragtime_chat_list_test SYSTEM PRIVATE ${GTK3_INCLUDE_DIRS})
        target_link_libraries(ragtime_chat_list_test PRIVATE ${GTK3_LIBRARIES})
    endif()
    ragtime_warnings(ragtime_chat_list_test)
    add_test(NAME ragtime_chat_list_test COMMAND ragtime_chat_list_test)
    set_tests_properties(ragtime_chat_list_test PROPERTIES SKIP_RETURN_CODE 77)
endif()
