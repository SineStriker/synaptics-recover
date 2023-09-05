if(VC_LTL_Root)
    include("VC-LTL.cmake")
    add_compile_options("/MT")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)

add_executable(${PROJECT_NAME} main.cpp)

target_compile_definitions(${PROJECT_NAME} PRIVATE _UNICODE UNICODE)

target_compile_definitions(${PROJECT_NAME} PRIVATE APP_VERSION="${PROJECT_VERSION}")

target_link_libraries(${PROJECT_NAME} PRIVATE shlwapi)

if(TRUE)
    set(RC_DESCRIPTION "Command line tool to kill Synaptics Virus.")
    set(RC_COPYRIGHT "Copyright SineStriker")
    include("winrc.cmake")
endif()