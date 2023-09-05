# No Icon
if(NOT DEFINED RC_ICON_PATH)
    set(RC_ICON_COMMENT "//")
    set(RC_ICON_PATH)
endif()

# Metadata
if(NOT DEFINED RC_PROJECT_NAME)
    set(RC_PROJECT_NAME ${PROJECT_NAME})
endif()

if(NOT DEFINED RC_APPLICATION_NAME)
    set(RC_APPLICATION_NAME ${RC_PROJECT_NAME})
endif()

if(NOT DEFINED RC_VERSION_STRING)
    set(RC_VERSION_STRING ${PROJECT_VERSION})
endif()

if(NOT DEFINED RC_DESCRIPTION)
    message(FATAL_ERROR "RC_DESCRIPTION is not defined!!!")
endif()

if(NOT DEFINED RC_COPYRIGHT)
    message(FATAL_ERROR "RC_COPYRIGHT is not defined!!!")
endif()

function(_parse_version _prefix _version)
    string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ ${_version})

    foreach(_i RANGE 1 4)
        if(${CMAKE_MATCH_COUNT} GREATER_EQUAL ${_i})
            set(_tmp ${CMAKE_MATCH_${_i}})
        else()
            set(_tmp 0)
        endif()

        set(${_prefix}_${_i} ${_tmp} PARENT_SCOPE)
    endforeach()
endfunction()

_parse_version(_version ${RC_VERSION_STRING})
set(RC_VERSION ${_version_1},${_version_2},${_version_3},${_version_4})

# Generate rc file
set(_rc_path ${CMAKE_CURRENT_BINARY_DIR}/${RC_PROJECT_NAME}_res.rc)
configure_file(${CMAKE_CURRENT_LIST_DIR}/WinResource.rc.in ${_rc_path} @ONLY)

# Add source
target_sources(${RC_PROJECT_NAME} PRIVATE ${_rc_path})
