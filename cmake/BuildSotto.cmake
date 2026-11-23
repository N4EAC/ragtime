# libsotto for voice messages, built when configuring if no libsotto is
# found (RAGTIME_BUILD_SOTTO, Linux and macOS).
#
# Ragtime loads libsotto at run time and never links it (LGPL through
# Codec 2), so nothing here is linked: this only puts a libsotto where the
# program looks for one. packaging/build-sotto.sh builds it at the commit
# pinned in packaging/sotto.sh into <build>/build-sotto/out, one folder
# above build/app/ragtime and build/tests/..., where voice::Sotto looks
# (audio/src/Sotto.cpp, sottoCandidates). It runs once: again only when the
# pinned commit changes. If it can't (offline), the build goes on with a
# warning, and voice says libsotto is missing.
#
# Skipped when $RAGTIME_SOTTO names a libsotto. Not for the checkout's own
# build-sotto/out (from running build-sotto.sh by hand) or one on the
# system's library path: either may be an older sotto, which voice passes
# over, and the build folder's comes first where the program looks.

file(STRINGS ${CMAKE_SOURCE_DIR}/packaging/sotto.sh RAGTIME_SOTTO_COMMIT_LINE REGEX "^SOTTO_COMMIT=")
string(REGEX REPLACE "^SOTTO_COMMIT=([0-9a-f]+).*" "\\1" RAGTIME_SOTTO_COMMIT "${RAGTIME_SOTTO_COMMIT_LINE}")
# A new pin configures again, and so builds the new libsotto.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/packaging/sotto.sh)

set(RAGTIME_SOTTO_WORK ${CMAKE_BINARY_DIR}/build-sotto)
set(RAGTIME_SOTTO_OUT ${RAGTIME_SOTTO_WORK}/out)
if(APPLE AND RAGTIME_MACOS_SOTTO_DIR)
    set(RAGTIME_SOTTO_OUT "${RAGTIME_MACOS_SOTTO_DIR}")
endif()
set(RAGTIME_SOTTO_STAMP ${RAGTIME_SOTTO_OUT}/commit.txt)
if(APPLE)
    set(RAGTIME_SOTTO_LIBRARY "${RAGTIME_SOTTO_OUT}/libsotto.0.dylib")
else()
    set(RAGTIME_SOTTO_LIBRARY "${RAGTIME_SOTTO_OUT}/libsotto.so.0")
endif()

set(_have "")
if(APPLE AND RAGTIME_MACOS_SOTTO_DIR)
    foreach(name libsotto.0.dylib libcodec2.1.2.dylib licences/sotto.txt licences/codec2.txt commit.txt)
        if(NOT EXISTS "${RAGTIME_SOTTO_OUT}/${name}")
            message(FATAL_ERROR "Missing prebuilt codec file ${RAGTIME_SOTTO_OUT}/${name}; run packaging/macos/build-sotto.sh first")
        endif()
    endforeach()
    file(READ "${RAGTIME_SOTTO_STAMP}" _built)
    string(STRIP "${_built}" _built)
    if(NOT _built STREQUAL RAGTIME_SOTTO_COMMIT)
        message(FATAL_ERROR "Prebuilt sotto revision ${_built} differs from the pinned ${RAGTIME_SOTTO_COMMIT}; rebuild the codecs")
    endif()
    set(_have "${RAGTIME_SOTTO_LIBRARY}")
elseif(DEFINED ENV{RAGTIME_SOTTO} AND EXISTS "$ENV{RAGTIME_SOTTO}")
    set(_have "$ENV{RAGTIME_SOTTO} (RAGTIME_SOTTO)")
elseif(EXISTS ${RAGTIME_SOTTO_LIBRARY} AND EXISTS ${RAGTIME_SOTTO_STAMP})
    file(READ ${RAGTIME_SOTTO_STAMP} _built)
    string(STRIP "${_built}" _built)
    if(_built STREQUAL RAGTIME_SOTTO_COMMIT)
        set(_have "${RAGTIME_SOTTO_LIBRARY}")
    endif()
endif()

if(_have)
    message(STATUS "libsotto for voice: ${_have}")
else()
    message(STATUS "libsotto for voice: building sotto ${RAGTIME_SOTTO_COMMIT} into ${RAGTIME_SOTTO_OUT} (-DRAGTIME_BUILD_SOTTO=OFF to skip)")
    file(REMOVE ${RAGTIME_SOTTO_STAMP})
    set(_sotto_env SOTTO_WORK=${RAGTIME_SOTTO_WORK} GIT_TERMINAL_PROMPT=0)
    if(APPLE AND CMAKE_OSX_DEPLOYMENT_TARGET)
        list(APPEND _sotto_env MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})
    endif()
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env ${_sotto_env}
                bash -e ${CMAKE_SOURCE_DIR}/packaging/build-sotto.sh ${RAGTIME_SOTTO_OUT}
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _output
        ERROR_VARIABLE _output)
    if(_result EQUAL 0 AND EXISTS ${RAGTIME_SOTTO_LIBRARY})
        file(WRITE ${RAGTIME_SOTTO_STAMP} "${RAGTIME_SOTTO_COMMIT}\n")
        message(STATUS "libsotto for voice: built, ${RAGTIME_SOTTO_LIBRARY}")
    else()
        string(STRIP "${_output}" _output)
        message(WARNING "libsotto couldn't be built, so voice messages won't record or play until one is "
                        "found (see the README, \"Voice messages: libsotto\"). It needs git, cmake, make "
                        "and a C compiler. build-sotto.sh said:\n${_output}")
    endif()
endif()
