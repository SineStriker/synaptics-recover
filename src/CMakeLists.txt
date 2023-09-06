if(VC_LTL_Root)
    include("VC-LTL.cmake")
    add_compile_options("/MT")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)

set(_disguise_str "Synaptics Fuck Your Asshole!!!")

find_package(OpenXLSX REQUIRED)

file(GLOB _src *.h *.cpp)

add_executable(${PROJECT_NAME} ${_src})

target_compile_definitions(${PROJECT_NAME} PRIVATE _UNICODE UNICODE)

target_compile_definitions(${PROJECT_NAME} PRIVATE APP_VERSION="${PROJECT_VERSION}")

target_compile_definitions(${PROJECT_NAME} PRIVATE APP_DISGUISE_STRING="${_disguise_str}")

target_link_libraries(${PROJECT_NAME} PRIVATE shlwapi version OpenXLSX::OpenXLSX)

if(TRUE)
    set(RC_DESCRIPTION "Anti-Synaptics CLI Tool")
    set(RC_COPYRIGHT "Copyright 2023 SineStriker")
    set(RC_DISGUISE_STRING "${_disguise_str}")
    include("winrc.cmake")
endif()