if(NOT DEFINED BUNDLE_EXECUTABLE)
    message(FATAL_ERROR "BUNDLE_EXECUTABLE must name the app executable")
endif()

get_filename_component(_macos_dir "${BUNDLE_EXECUTABLE}" DIRECTORY)
get_filename_component(_contents_dir "${_macos_dir}" DIRECTORY)
set(_frameworks_dir "${_contents_dir}/Frameworks")

if(NOT EXISTS "${_frameworks_dir}")
    return()
endif()

file(GLOB _framework_entries LIST_DIRECTORIES false "${_frameworks_dir}/*.dylib")
set(_renamed_from)
set(_renamed_to)

foreach(_entry IN LISTS _framework_entries)
    if(IS_SYMLINK "${_entry}")
        file(READ_SYMLINK "${_entry}" _target_name)

        if(NOT IS_ABSOLUTE "${_target_name}")
            set(_target_path "${_frameworks_dir}/${_target_name}")
        else()
            set(_target_path "${_target_name}")
        endif()

        if(NOT EXISTS "${_target_path}")
            message(FATAL_ERROR "Bundle dylib symlink ${_entry} points at missing ${_target_name}")
        endif()

        get_filename_component(_entry_name "${_entry}" NAME)
        get_filename_component(_target_real "${_target_path}" REALPATH)
        set(_tmp_path "${_entry}.tmp")

        file(COPY_FILE "${_target_real}" "${_tmp_path}")
        file(REMOVE "${_entry}")
        file(RENAME "${_tmp_path}" "${_entry}")

        list(APPEND _renamed_from "${_target_name}")
        list(APPEND _renamed_to "${_entry_name}")
    endif()
endforeach()

set(_sdl2_dylib "${_frameworks_dir}/libSDL2-2.0.0.dylib")
set(_sdl3_dylib "${_frameworks_dir}/libSDL3.dylib")

if(EXISTS "${_sdl2_dylib}" AND NOT EXISTS "${_sdl3_dylib}")
    file(STRINGS "${_sdl2_dylib}" _sdl2_sdl3_refs REGEX "libSDL3\\.dylib|Failed loading SDL3 library" LIMIT_COUNT 1)

    if(_sdl2_sdl3_refs)
        find_library(_host_sdl3_dylib NAMES SDL3 libSDL3
            PATHS /opt/homebrew/lib /usr/local/lib
            NO_DEFAULT_PATH
        )

        if(NOT _host_sdl3_dylib)
            find_library(_host_sdl3_dylib NAMES SDL3 libSDL3)
        endif()

        if(NOT _host_sdl3_dylib)
            message(FATAL_ERROR "Bundled SDL2 is sdl2-compat and requires libSDL3.dylib, but SDL3 was not found")
        endif()

        file(COPY_FILE "${_host_sdl3_dylib}" "${_sdl3_dylib}")

        execute_process(
            COMMAND install_name_tool -id "@executable_path/../Frameworks/libSDL3.dylib" "${_sdl3_dylib}"
            RESULT_VARIABLE _sdl3_install_name_result
        )

        if(NOT _sdl3_install_name_result EQUAL 0)
            message(FATAL_ERROR "Failed to set install name for ${_sdl3_dylib}")
        endif()
    endif()
endif()

file(GLOB _bundle_dylibs LIST_DIRECTORIES false "${_frameworks_dir}/*.dylib")

foreach(_dylib IN LISTS _bundle_dylibs)
    get_filename_component(_dylib_name "${_dylib}" NAME)

    execute_process(
        COMMAND install_name_tool -id "@executable_path/../Frameworks/${_dylib_name}" "${_dylib}"
        RESULT_VARIABLE _install_name_result
    )

    if(NOT _install_name_result EQUAL 0)
        message(FATAL_ERROR "Failed to set install name for ${_dylib}")
    endif()
endforeach()

list(LENGTH _renamed_from _rename_count)
math(EXPR _last_rename_index "${_rename_count} - 1")

if(_rename_count GREATER 0)
    foreach(_dependent IN LISTS _bundle_dylibs BUNDLE_EXECUTABLE)
        foreach(_index RANGE 0 ${_last_rename_index})
            list(GET _renamed_from ${_index} _from_name)
            list(GET _renamed_to ${_index} _to_name)

            execute_process(
                COMMAND install_name_tool
                    -change "@executable_path/../Frameworks/${_from_name}"
                            "@executable_path/../Frameworks/${_to_name}"
                            "${_dependent}"
                RESULT_VARIABLE _change_result
                ERROR_QUIET
            )

            if(NOT _change_result EQUAL 0)
                execute_process(
                    COMMAND install_name_tool
                        -change "@loader_path/${_from_name}"
                                "@executable_path/../Frameworks/${_to_name}"
                                "${_dependent}"
                    RESULT_VARIABLE _loader_change_result
                    ERROR_QUIET
                )
            endif()
        endforeach()
    endforeach()

    foreach(_from_name IN LISTS _renamed_from)
        set(_from_path "${_frameworks_dir}/${_from_name}")
        if(EXISTS "${_from_path}" AND NOT IS_SYMLINK "${_from_path}")
            file(REMOVE "${_from_path}")
        endif()
    endforeach()
endif()

file(GLOB _final_dylibs LIST_DIRECTORIES false "${_frameworks_dir}/*.dylib")

foreach(_dylib IN LISTS _final_dylibs)
    file(CHMOD "${_dylib}" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
endforeach()
