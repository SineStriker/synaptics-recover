project(synare)

file(GLOB_RECURSE _src *.h *.cpp)

add_library(${PROJECT_NAME} STATIC ${_src})

target_compile_definitions(${PROJECT_NAME} PRIVATE
    APP_DISGUISE_STRING="${SYNAPTICS_DISGUISE_STRING}"
)

target_link_libraries(${PROJECT_NAME} PUBLIC winutils)

target_link_libraries(${PROJECT_NAME} PRIVATE version)

target_include_directories(${PROJECT_NAME} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    external
    external/nowide
    external/pugixml
    external/zippy
)