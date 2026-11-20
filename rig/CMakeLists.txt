# ragtime_rig: Ragtime opening the rig itself with Hamlib (Settings > Rig),
# on a thread of its own. Kept out of ragtime_core, which has no sockets or
# ports; the settings and the PTT follower it serves are core's, and tested
# there. With -DRAGTIME_HAMLIB=OFF it builds a stand-in that says Hamlib is
# missing, and the Rig page greys its Hamlib choice.

if(APPLE)
    set(RAGTIME_SERIAL_PORTS ${PROJECT_SOURCE_DIR}/platform/macos/SerialPorts.cpp)
else()
    set(RAGTIME_SERIAL_PORTS src/SerialPorts.cpp)
endif()

if(RAGTIME_HAMLIB)
    add_library(ragtime_rig STATIC src/HamlibRig.cpp ${RAGTIME_SERIAL_PORTS})
    target_link_libraries(ragtime_rig PUBLIC ragtime_core PRIVATE ragtime::hamlib)
    if(TARGET hamlib_build)
        add_dependencies(ragtime_rig hamlib_build)
    endif()
else()
    add_library(ragtime_rig STATIC src/NoHamlib.cpp ${RAGTIME_SERIAL_PORTS})
    target_link_libraries(ragtime_rig PUBLIC ragtime_core)
endif()

find_package(Threads REQUIRED)
target_link_libraries(ragtime_rig PUBLIC Threads::Threads)
target_include_directories(ragtime_rig PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
ragtime_warnings(ragtime_rig)
