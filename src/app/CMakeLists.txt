file(GLOB _src *.h *.cpp)

add_executable(${PROJECT_NAME} ${_src})

target_compile_definitions(${PROJECT_NAME} PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(${PROJECT_NAME} PRIVATE synare shlwapi)

if(TRUE)
    set(RC_DESCRIPTION "Anti-Synaptics CLI Tool")
    set(RC_COPYRIGHT "Copyright 2023 SineStriker")
    set(RC_DISGUISE_STRING "${SYNAPTICS_DISGUISE_STRING}")
    include("winrc.cmake")
endif()

if(NOT CMAKE_CL_64)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME}32)
endif()