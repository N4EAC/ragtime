if(NOT APPLE)
    message(FATAL_ERROR "The built-in modem is currently supported on macOS.")
endif()
if(NOT EXISTS "${RAGTIME_DATA2G_SOURCE_DIR}/native/CMakeLists.txt")
    message(FATAL_ERROR "Point RAGTIME_DATA2G_SOURCE_DIR at a Data2G checkout (tested with v0.5.3).")
endif()

# Find the imported targets in the parent scope as Ragtime also links them.
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Network WebSockets)

# Override the nested project's options locally, so a separate Data2G build
# keeps its own options. Build only the libraries Ragtime actually links.
set(DATA2G_BUILD_HOST ON)
set(DATA2G_BUILD_GUI OFF)
set(DATA2G_BUILD_TESTS OFF)
set(DATA2G_BUILD_PYMODULE OFF)
set(DATA2G_BUILD_QTAUDIO ON)
set(DATA2G_AUDIO_BACKEND miniaudio)
# Ragtime already owns Hamlib and can follow the modem's PTT events.
set(DATA2G_BUILD_RIG OFF)
add_subdirectory("${RAGTIME_DATA2G_SOURCE_DIR}/native" "${CMAKE_CURRENT_BINARY_DIR}/data2g" EXCLUDE_FROM_ALL)
