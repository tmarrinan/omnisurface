cmake_minimum_required(VERSION 3.16)

project(omnisurface LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GLAD_DIR "" CACHE PATH "Local directory containing GLAD include and lib development files")


# Find Vulkan and GLFW
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

# Fetch libwebsockets
include(FetchContent)
FetchContent_Declare(
    libwebsockets
    GIT_REPOSITORY https://github.com/warmcat/libwebsockets.git
    GIT_TAG        v5.0.0
)
set(LWS_WITH_STATIC ON CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TESTAPPS ON CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_SERVER ON CACHE BOOL "" FORCE)
if(WIN32)
    set(LWS_WITH_SCHANNEL ON CACHE BOOL "" FORCE)
    set(LWS_WITH_SSL OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_GNUTLS OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_MBEDTLS OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_WOLFSSL OFF CACHE BOOL "" FORCE)
else()
    set(LWS_WITH_SSL ON CACHE BOOL "" FORCE)
    find_package(OpenSSL REQUIRED)
endif()
FetchContent_MakeAvailable(libwebsockets)


# Add local Glad library
add_library(GLAD STATIC IMPORTED GLOBAL)

target_include_directories(GLAD INTERFACE "${GLAD_DIR}/include")

if(WIN32)
    set_target_properties(GLAD PROPERTIES
        IMPORTED_LOCATION "${GLAD_DIR}/lib/glad.lib"
    )
elseif(UNIX)
    set_target_properties(GLAD PROPERTIES
        IMPORTED_LOCATION "${GLAD_DIR}/lib/libglad.a"
    )
endif()


# Add submodule for DTrack SDK
add_subdirectory(extern/DTrackSDK)


# Main project
add_executable(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(${PROJECT_NAME} PRIVATE
    include/stb_image.h
    include/native_render_handle.h
    include/vk360.h
    include/uiserver.h
    src/vk360.cpp
    src/uiserver.cpp
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
    glfw
    GLAD
    DTrackSDK_LIB
    websockets
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
)

# Compile GLSL shaders to SPIR-V
find_program(SPV_COMPILER NAMES glslangValidator)
if(SPV_COMPILER)
    set(SPV_FLAGS_VERT "-V" "-S" "vert")
    set(SPV_FLAGS_FRAG "-V" "-S" "frag")
else()
    find_program(SPV_COMPILER NAMES glslc)
    set(SPV_FLAGS_VERT "-fshader-stage=vertex")
    set(SPV_FLAGS_FRAG "-fshader-stage=fragment")
endif()
if(SPV_COMPILER)
    message(STATUS "Using ${SPV_COMPILER} as GLSL to SPIR-V compiler")
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${SPV_COMPILER} ${SPV_FLAGS_VERT}
            -o ${CMAKE_SOURCE_DIR}/resrc/shaders/equirect_nonplanar_vert.glsl.spv
            ${CMAKE_SOURCE_DIR}/resrc/shaders/equirect_nonplanar_vert.glsl
        COMMAND ${SPV_COMPILER} ${SPV_FLAGS_FRAG}
            -o ${CMAKE_SOURCE_DIR}/resrc/shaders/equirect_nonplanar_frag.glsl.spv
            ${CMAKE_SOURCE_DIR}/resrc/shaders/equirect_nonplanar_frag.glsl
        COMMENT "Compiling OmniSurface Vulkan360 GLSL shaders to SPIR-V"
    )
else()
    message(WARNING "Warning: no GLSL to SPIR-V compiler found. Please install glslangValidator or glslc.")
endif()
unset(SPV_COMPILER CACHE)
unset(SPV_FLAGS_VERT CACHE)
unset(SPV_FLAGS_FRAG CACHE)

# Copy 'resrc' directory to output
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resrc
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/resrc
)

# Copy 'http' directory to output
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/http
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/http
)
