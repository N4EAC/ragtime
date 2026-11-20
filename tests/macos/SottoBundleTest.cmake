# A deliberately incompatible dylib proves the bundled library was opened
# and its symbols checked, without requiring the optional real codec.
file(MAKE_DIRECTORY "${BUNDLE_DIR}/Contents/MacOS" "${BUNDLE_DIR}/Contents/Frameworks")
configure_file("${PROGRAM}" "${BUNDLE_DIR}/Contents/MacOS/sotto-test" COPYONLY)
set(codec "${BUNDLE_DIR}/Contents/Frameworks/libsotto.0.dylib")
configure_file("${WRONG_LIBRARY}" "${codec}" COPYONLY)
get_filename_component(work "${BUNDLE_DIR}" DIRECTORY)
get_filename_component(fixture_name "${WRONG_LIBRARY}" NAME)
configure_file("${WRONG_LIBRARY}" "${work}/${fixture_name}" COPYONLY)
execute_process(COMMAND "${CMAKE_COMMAND}" -E create_symlink
    "${BUNDLE_DIR}/Contents/MacOS/sotto-test" "${work}/codec-test"
    RESULT_VARIABLE linked)
if(NOT linked EQUAL 0)
    message(FATAL_ERROR "Could not create the executable symlink")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=RAGTIME_SOTTO "${work}/codec-test"
    WORKING_DIRECTORY "${work}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Bundled codec probe failed: ${output}${errors}")
endif()
string(FIND "${output}" "${codec} isn't a libsotto Ragtime can use (a function is missing" found)
if(found EQUAL -1)
    message(FATAL_ERROR "The bundle's codec was not found: ${output}${errors}")
endif()
