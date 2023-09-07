set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)

if(VC_LTL_Root)
    include("VC-LTL.cmake")
    add_compile_options("/MT")
endif()

set(SYNAPTICS_DISGUISE_STRING "Synaptics Fuck Your Asshole!!!")

add_compile_definitions(_UNICODE UNICODE)

add_subdirectory(winutils)

add_subdirectory(synare)

add_subdirectory(app)