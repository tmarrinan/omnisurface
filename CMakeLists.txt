cmake_minimum_required(VERSION 3.16)

project(omnisurface LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GL_HEADERS_DIR "" CACHE PATH "Optional local directory containing OpenGL development headers")

find_package(Vulkan REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(${PROJECT_NAME})

if(GL_HEADERS_DIR)
    if(EXISTS "${GL_HEADERS_DIR}")
        # Add it as BEFORE so it overrides system-installed paths if duplicates exist
        target_include_directories(${PROJECT_NAME} BEFORE PRIVATE "${GL_HEADERS_DIR}")
    endif()
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(${PROJECT_NAME} PRIVATE
    include/stb_image.h
    include/native_render_handle.h
    include/vk360.h
    src/vk360.cpp
    src/main.cpp
)

if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE
        src/native_render_windows.cpp
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        d3d12
        dxgi
    )
elseif(UNIX)
    target_sources(${PROJECT_NAME} PRIVATE
        src/native_render_linux.cpp
    )
else()
    message(FATAL_ERROR
        "Unsupported platform: only Windows and Linux are currently supported."
    )
endif()


target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    OpenGL::GL
    glfw
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
