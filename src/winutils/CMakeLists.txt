project(winutils)

file(GLOB _src *.h *.cpp)

add_library(${PROJECT_NAME} STATIC ${_src})

target_link_libraries(${PROJECT_NAME} PRIVATE shlwapi)

target_include_directories(${PROJECT_NAME} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
)