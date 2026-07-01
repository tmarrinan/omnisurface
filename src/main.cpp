#include <iostream>
#include <fstream>
#include <string>
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(_WIN32)
#include <windows.h>
extern "C" {
    _declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;       // Forces NVIDIA Performance GPU
    _declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1; // Forces AMD Performance GPU
}
#endif

#include "vk360.h"


// Data types
#if defined(_WIN32)
typedef HANDLE ExternalHandle;
#elif defined(__linux__)
typedef int ExternalHandle;
#endif

enum DisplayBaseShape : uint8_t { BASE_SHAPE_PLANE, BASE_SHAPE_CYLINDER };

struct DisplayConfig {
    int monitor_index;
    DisplayBaseShape base_shape;
    uint32_t facets;
    double radius;
    double height;
};

struct PresentData {
    GLuint render_buffers[2];
    GLuint img_available_sem;
    GLuint img_finished_sem;
};

// Function definitions
bool readOmniSurfaceConfigFile(const char* filename, DisplayConfig* config_ptr);
GLFWwindow* createFullscreenWindow(const char* title, int monitor_idx, int* width, int* height, bool* is_stereo);
void importExternalTextureArray(vk360::ExternalImageInfo& ext_img_info, GLuint* texture);
void importExternalSemaphore(ExternalHandle sem_handle, GLuint* semaphore);

// Main program
int main()
{
    // Read in display configuration
    DisplayConfig config{};
    if (!readOmniSurfaceConfigFile("resrc/config_plane.txt", &config))
    {
        fprintf(stderr, "OmniSurface> Error: Failed to read config file\n");
        return EXIT_FAILURE;
    }

    // Create fullscreen window
    int window_w, window_h;
    bool is_stereo;
    GLFWwindow* window = createFullscreenWindow("OmniSurface", config.monitor_index, &window_w, &window_h, &is_stereo);
    if (!window)
    {
        fprintf(stderr, "OmniSurface> Error: Failed to create fullscreen window\n");
        return EXIT_FAILURE;
    }
    printf("OmniSurface> Info: Launching %s window on monitor %d with resolution %dx%d\n",
        is_stereo ? "Stereo 3D" : "Standard 2D", config.monitor_index, window_w, window_h);

    // Query the GPU name and UUID from the active OpenGL context
    const GLubyte* renderer = glGetString(GL_RENDERER);
    GLubyte gl_device_uuid[GL_UUID_SIZE_EXT];
    glGetUnsignedBytevEXT(GL_DEVICE_UUID_EXT, gl_device_uuid);
    printf("OmniSurface> Info: Using %s (UUID=%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x)\n",
        renderer,
        gl_device_uuid[0],  gl_device_uuid[1],  gl_device_uuid[2],  gl_device_uuid[3],
        gl_device_uuid[4],  gl_device_uuid[5],  gl_device_uuid[6],  gl_device_uuid[7],
        gl_device_uuid[8],  gl_device_uuid[9],  gl_device_uuid[10], gl_device_uuid[11],
        gl_device_uuid[12], gl_device_uuid[13], gl_device_uuid[14], gl_device_uuid[15]);

    // Setup Vulkan 360 renderer
    vk360::Vulkan360* app = new vk360::Vulkan360(gl_device_uuid, window_w, window_h, is_stereo);

    // Import external data to OpenGL for framebuffer presentation
    vk360::ExternalImageInfo render_buffer_info[2];
    app->getExternalRenderBufferInfo(0, &(render_buffer_info[0]));
    app->getExternalRenderBufferInfo(1, &(render_buffer_info[1]));
    ExternalHandle img_available_handle, img_finished_handle;
    app->getExternalImageAvailableSemaphoreInfo(&img_available_handle);
    app->getExternalImageFinishedSemaphoreInfo(&img_finished_handle);

    PresentData present;
    importExternalTextureArray(render_buffer_info[0], &(present.render_buffers[0]));
    importExternalTextureArray(render_buffer_info[1], &(present.render_buffers[1]));
    importExternalSemaphore(img_available_handle, &(present.img_available_sem));
    importExternalSemaphore(img_finished_handle, &(present.img_finished_sem));

    printf("gl errors: %d\n", glGetError());

    // Main render loop
    while (!glfwWindowShouldClose(window))
    {
        // Poll for user events
        glfwPollEvents();

        // Check if `esc` key has been pressed - if so, set flag to make window close
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }
    }

    return EXIT_SUCCESS;
}

bool readOmniSurfaceConfigFile(const char* filename, DisplayConfig* config_ptr)
{
    std::ifstream config_file(filename);
    if (!config_file.is_open())
    {
        fprintf(stderr, "OmniSurface> Error: config file '%s' not found'\n", filename);
        return false;
    }

    std::string line;
    int lineno = 0;
    while (std::getline(config_file, line))
    {
        if (lineno == 0 && line != "OmniSurface Config")
        {
            fprintf(stderr, "OmniSurface> Error: config file format not recognized - 1st line should be 'OmniSurface Config'\n");
            return false;
        }
        else if (line.length() >= 10 && line.substr(0, 9) == "Monitor: ")
        {
            config_ptr->monitor_index = std::stoi(line.substr(9));
        }
        else if (line.length() >= 13 && line.substr(0, 12) == "Base Shape: ")
        {
            std::string shape = line.substr(12);
            if (shape == "plane")
            {
                config_ptr->base_shape = DisplayBaseShape::BASE_SHAPE_PLANE;
            }
            else if (shape == "cylinder")
            {
                config_ptr->base_shape = DisplayBaseShape::BASE_SHAPE_CYLINDER;
            }
            else
            {
                fprintf(stderr, "Vulkan360> Error: config file format not recognized - Base Shape must be 'plane' or 'cylinder'\n");
                return false;
            }
        }
        else if (line.length() >= 9 && line.substr(0, 8) == "Facets: ")
        {
            config_ptr->facets = std::stoul(line.substr(8));
        }
        else if (line.length() >= 9 && line.substr(0, 8) == "Radius: ")
        {
            config_ptr->radius = std::stod(line.substr(8));
        }
        else if (line.length() >= 9 && line.substr(0, 8) == "Height: ")
        {
            config_ptr->height = std::stod(line.substr(8));
        }

        lineno++;
    }

    config_file.close();
    return true;
}


GLFWwindow* createFullscreenWindow(const char* title, int monitor_idx, int* width, int* height, bool* is_stereo)
{
    // Initialize GLFW
    if (!glfwInit())
    {
        fprintf(stderr, "OmniSurface> Error: failed to initialize GLFW\n");
        return nullptr;
    }

    // Prevent GLFW from implicitly creating an OpenGL context
    //glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API
    // Set OpenGL context to 4.6 core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // Attempt to enable quad-buffer stereo 3d
    glfwWindowHint(GLFW_STEREO, GLFW_TRUE);

    // Get desired monitor
    int monitor_count;
    GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
    if (monitor_idx >= monitor_count)
    {
        fprintf(stderr, "OmniSurface> Error: failed to find monitor %d (only detected %d monitors)\n", monitor_idx, monitor_count);
        return nullptr;
    }

    // Get resolution of primary monitor
    const GLFWvidmode* mode = glfwGetVideoMode(monitors[monitor_idx]);
    *width = mode->width;
    *height = mode->height;

    // Create fullscreen window
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, title, monitors[monitor_idx], nullptr);
    if (!window)
    {
        glfwWindowHint(GLFW_STEREO, GLFW_FALSE);
        window = glfwCreateWindow(mode->width, mode->height, title, monitors[monitor_idx], nullptr);
        if (!window)
        {
            fprintf(stderr, "OmniSurface> Error: could not create `GLFWwindow`\n");
            return nullptr;
        }
    }

    // Set OpenGL context to control window
    glfwMakeContextCurrent(window);

    // Load OpenGL extensions
    if (gladLoadGL(glfwGetProcAddress) == 0)
    {
        fprintf(stderr, "OmniSurface> Error: failed to initialize Glad\n");
        return nullptr;
    }

    // Check whether window is quad-buffer stereo capable
    GLboolean stereo_support;
    glGetBooleanv(GL_STEREO, &stereo_support);
    *is_stereo = stereo_support;

    return window;
}

void importExternalTextureArray(vk360::ExternalImageInfo& ext_img_info, GLuint* texture)
{
    GLuint mem_obj;
    glCreateMemoryObjectsEXT(1, &mem_obj);
#if defined(_WIN32)
    glImportMemoryWin32HandleEXT(mem_obj, ext_img_info.memory_size, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, ext_img_info.external_handle);
#elif defined(__linux__)
    glImportMemoryFdEXT(mem_obj, ext_img_info.memory_size, GL_HANDLE_TYPE_OPAQUE_FD_EXT, ext_img_info.external_handle);
#endif

    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, *texture);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_TILING_EXT, GL_OPTIMAL_TILING_EXT);
    glTexStorageMem3DEXT(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, ext_img_info.width, ext_img_info.height, ext_img_info.layers, mem_obj, 0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void importExternalSemaphore(ExternalHandle sem_handle, GLuint* semaphore)
{
    // Import the FD for OpenGL version of the shared semaphore
    glGenSemaphoresEXT(1, semaphore);
#if defined(_WIN32)
    glImportSemaphoreWin32HandleEXT(*semaphore, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, sem_handle);
#elif defined(__linux__)
    glImportSemaphoreFdEXT(*semaphore, GL_HANDLE_TYPE_OPAQUE_FD_EXT, sem_handle);
#endif
}
