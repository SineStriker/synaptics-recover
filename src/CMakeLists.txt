if(VC_LTL_Root)
    include("VC-LTL.cmake")
    add_compile_options("/MT")
endif()

add_executable(${PROJECT_NAME} main.cpp)

target_compile_definitions(${PROJECT_NAME} PRIVATE _UNICODE UNICODE)