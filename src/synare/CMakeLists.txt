project(synare)

find_package(OpenXLSX REQUIRED)

file(GLOB _src *.h *.cpp)

add_library(${PROJECT_NAME} STATIC ${_src})

target_compile_definitions(${PROJECT_NAME} PRIVATE
    APP_DISGUISE_STRING="${SYNAPTICS_DISGUISE_STRING}"
)

target_link_libraries(${PROJECT_NAME} PUBLIC winutils)

target_link_libraries(${PROJECT_NAME} PRIVATE version OpenXLSX::OpenXLSX)

target_include_directories(${PROJECT_NAME} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
)