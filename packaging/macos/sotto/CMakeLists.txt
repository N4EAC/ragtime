cmake_minimum_required(VERSION 3.16)
project(RagtimeSotto LANGUAGES C)
if(NOT APPLE)
    message(FATAL_ERROR "This build produces macOS voice codecs")
endif()
if(NOT EXISTS "${SOTTO_SOURCE}/c/sotto.c")
    message(FATAL_ERROR "Set SOTTO_SOURCE to the pinned sotto checkout")
endif()

set(BUILD_SHARED_LIBS ON CACHE BOOL "Shared codecs" FORCE)
set(UNITTEST OFF CACHE BOOL "Build Codec 2 tests" FORCE)
add_subdirectory("${SOTTO_SOURCE}/codec/codec2" codec2 EXCLUDE_FROM_ALL)

# Match the pinned Makefile's v3.8 floating-point variant and tables.
# Only these codecs are packaged; Ragtime continues to load them at run time.
set(sources sotto sotto_lr sotto_vlr sotto_vlr_dec sotto_vlr_tables
    sotto_codebook sotto_hiss sotto_hiss_tables)
list(TRANSFORM sources PREPEND "${SOTTO_SOURCE}/c/")
list(TRANSFORM sources APPEND ".c")
add_library(sotto SHARED ${sources})
set_target_properties(sotto PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES
    C_VISIBILITY_PRESET hidden VERSION 0.1.0 SOVERSION 0)
target_compile_definitions(sotto PRIVATE SOTTO_V36 SOTTO_PITCH_THIRD SOTTO_V38)
target_compile_options(sotto PRIVATE -ffp-contract=off)
target_include_directories(sotto PRIVATE "${SOTTO_SOURCE}/c"
    "${SOTTO_SOURCE}/codec/codec2/src" "${CMAKE_CURRENT_BINARY_DIR}/codec2")
target_link_libraries(sotto PRIVATE codec2 m)
foreach(codec sotto codec2)
    set_target_properties(${codec} PROPERTIES MACOSX_RPATH YES
        INSTALL_NAME_DIR "@rpath" BUILD_WITH_INSTALL_NAME_DIR YES
        INSTALL_RPATH "@loader_path" BUILD_WITH_INSTALL_RPATH YES)
endforeach()
install(TARGETS sotto codec2 LIBRARY DESTINATION .)
install(FILES "${SOTTO_SOURCE}/LICENSE" DESTINATION licences RENAME sotto.txt)
install(FILES "${SOTTO_SOURCE}/codec/codec2/COPYING" DESTINATION licences RENAME codec2.txt)
