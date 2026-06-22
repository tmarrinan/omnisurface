cmake_minimum_required(VERSION 3.16)

project(omnisurface LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

add_executable(${PROJECT_NAME}
    src/vk360.cpp
    src/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resrc
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/resrc
)
