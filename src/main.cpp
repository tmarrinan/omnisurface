#include <iostream>
#include <fstream>
#include <string>
#include <GL/glcorearb.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "vk360.h"


// OpenGL extension functions
PFNGLGETBOOLEANVPROC glGetBooleanv = nullptr;

// Data types
enum DisplayBaseShape : uint8_t { BASE_SHAPE_PLANE, BASE_SHAPE_CYLINDER };

typedef struct DisplayConfig {
    int monitor_index;
    DisplayBaseShape base_shape;
    uint32_t facets;
    double radius;
    double height;
} DisplayConfig;

// Function definitions
bool readOmniSurfaceConfigFile(const char* filename, DisplayConfig* config_ptr);
GLFWwindow* createFullscreenWindow(const char* title, int monitor_idx, int* width, int* height, bool* is_stereo);

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
    
//    // Get native window handle
//#if defined (_WIN32)
//    HWND win_handle = glfwGetWin32Window(window);
//    HMODULE m_handle = GetModuleHandle(nullptr);
//#elif defined(__linux__)
//    Window win_handle = glfwGetX11Window(window);
//    Display* m_handle = glfwGetX11Display();
//#else
//    #error "Unsupported operating system"
//#endif
//
//    // Setup Vulkan 360 renderer
//    uint32_t glfw_ext_count = 0;
//    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
//    Vulkan360* app = new Vulkan360(glfw_extensions, glfw_ext_count, win_handle, m_handle);
//    //glfwCreateWindowSurface()

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

    //Vulkan360* app = new Vulkan360("resrc/config_plane.txt");
    //if (!app->hasValidDisplayConfig())
    //{
    //    fprintf(stderr, "Error: Vulkan360 could not read display config file\n");
    //    return EXIT_FAILURE;
    //}
    //printf("OmniSurface Vulkan360 display config read in.\n");
    //app->initializeWindow("OmniSurface", "resrc/images/SampleOmni3D.png");

    //while (!app->shouldClose())
    //{
    //    app->pollEvents();
    //    // TODO: process events here

    //    int buffer_idx = app->getRenderBufferIndex();
    //    app->drawFrame(buffer_idx);
    //    app->swapBuffers(buffer_idx);
    //}

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
    glGetBooleanv = (PFNGLGETBOOLEANVPROC)glfwGetProcAddress("glGetBooleanv");

    // Check whether window is quad-buffer stereo capable
    GLboolean stereo_support;
    glGetBooleanv(GL_STEREO, &stereo_support);
    *is_stereo = stereo_support;

    return window;
}
