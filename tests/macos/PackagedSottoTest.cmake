# The real codecs must load after relocation without developer dyld paths.
file(REMOVE_RECURSE "${STAGE}")
file(MAKE_DIRECTORY "${STAGE}")
file(COPY "${BUNDLE}/" DESTINATION "${STAGE}")
foreach(name libsotto.0.dylib libcodec2.1.2.dylib)
    execute_process(COMMAND /usr/bin/otool -L "${STAGE}/Contents/Frameworks/${name}"
        RESULT_VARIABLE result OUTPUT_VARIABLE dependencies)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Could not inspect ${name}")
    endif()
    string(REPLACE "\n" ";" lines "${dependencies}")
    list(REMOVE_AT lines 0)
    foreach(line IN LISTS lines)
        string(STRIP "${line}" line)
        if(line AND NOT line MATCHES "^(@rpath/|@loader_path/|/usr/lib/|/System/Library/)")
            message(FATAL_ERROR "${name} depends on an unpackaged library: ${line}")
        endif()
    endforeach()
endforeach()
foreach(licence sotto.txt codec2.txt)
    if(NOT EXISTS "${STAGE}/Contents/Resources/licences/${licence}")
        message(FATAL_ERROR "Missing packaged licence ${licence}")
    endif()
endforeach()
get_filename_component(fixture_dir "${WRONG_LIBRARY}" DIRECTORY)
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    --unset=RAGTIME_SOTTO --unset=DYLD_LIBRARY_PATH --unset=DYLD_FALLBACK_LIBRARY_PATH
    RAGTIME_REQUIRE_SOTTO=1
    "RAGTIME_EXPECT_SOTTO=${STAGE}/Contents/Frameworks/libsotto.0.dylib"
    "${STAGE}/Contents/MacOS/ragtime_sotto_bundle_roundtrip"
    WORKING_DIRECTORY "${fixture_dir}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Relocated codec round trip failed: ${output}${errors}")
endif()
message(STATUS "${output}")
