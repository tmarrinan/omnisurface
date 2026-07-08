#include <iostream>
#include <fstream>
#include <string>
#include <vector>
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

struct PresentData {
    GLuint render_image;
    GLuint sem_signal_available;
    GLuint sem_wait_finished;
    GLuint fbo[2];
};

// Function definitions
GLFWwindow* createFullscreenWindow(const char* title, int monitor_idx, int* width, int* height, bool* is_stereo);
void importExternalTextureArray(vk360::ExternalImageInfo& ext_img_info, GLuint* texture);
void importExternalSemaphore(ExternalHandle sem_handle, GLuint* semaphore);

// Main program
int main()
{
    // Read in display configuration
    vk360::DisplayConfig *config = new vk360::DisplayConfig();
    if (!config->loadFromFile("resrc/config_3plane.txt"))
    {
        fprintf(stderr, "OmniSurface> Error: Failed to read config file\n");
        return EXIT_FAILURE;
    }
    config->printConfig();

    // Create fullscreen window
    int window_w, window_h;
    bool is_stereo;
    GLFWwindow* window = createFullscreenWindow("OmniSurface", config->getMonitor(), &window_w, &window_h, &is_stereo);
    if (!window)
    {
        fprintf(stderr, "OmniSurface> Error: Failed to create fullscreen window\n");
        return EXIT_FAILURE;
    }
    printf("OmniSurface> Info: Launching %s window on monitor %d with resolution %dx%d\n",
        is_stereo ? "Stereo 3D" : "Standard 2D", config->getMonitor(), window_w, window_h);

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
    PresentData present[2];
    for (uint32_t i = 0; i < 2; i++)
    {
        vk360::ExternalImageInfo render_buffer_info;
        app->getExternalRenderBufferInfo(i, &render_buffer_info);

        ExternalHandle sem_available_handle, sem_finished_handle;
        app->getExternalSignalAvailableSemaphoreHandle(i, &sem_available_handle);
        app->getExternalWaitFinishedSemaphoreHandle(i, &sem_finished_handle);

        importExternalTextureArray(render_buffer_info, &(present[i].render_image));
        importExternalSemaphore(sem_available_handle, &(present[i].sem_signal_available));
        importExternalSemaphore(sem_finished_handle, &(present[i].sem_wait_finished));

        for (uint32_t j = 0; j < (is_stereo ? 2 : 1); j++)
        {
            glGenFramebuffers(1, &(present[i].fbo[j]));
            glBindFramebuffer(GL_FRAMEBUFFER, present[i].fbo[j]);
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, present[i].render_image, 0, j);

            GLenum draw_buffers[1] = { GL_COLOR_ATTACHMENT0 };
            glDrawBuffers(1, draw_buffers);

            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                fprintf(stderr, "OmniSurface> Error: failed to create framebuffer object\n");
            }
        }
    }

    // Initialize OpenGL settings
    glDisable(GL_DEPTH_TEST);
    uint32_t num_views = is_stereo ? 2 : 1;
    GLenum draw_buffer_index[2] = { GL_BACK_LEFT, GL_BACK_RIGHT };
    if (!is_stereo) draw_buffer_index[0] = GL_BACK;

    app->loadImage("resrc/images/SampleOmni3D.png", true);

    // Main render loop
    GLuint buffers[1];
    GLenum layouts[1] = { GL_LAYOUT_GENERAL_EXT };
    while (!glfwWindowShouldClose(window))
    {
        // Poll for user events
        glfwPollEvents();

        // Check if `esc` key has been pressed - if so, set flag to make window close
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }

        // Trigger render (Vulkan)
        uint32_t buffer_idx = app->drawTestScreen();

        // Wait for Vulkan to signal render is complete
        buffers[0] = present[buffer_idx].render_image;
        glWaitSemaphoreEXT(present[buffer_idx].sem_wait_finished, 0, nullptr, 1, buffers, layouts);
        
        // Blit rendered image to screen
        for (uint32_t i = 0; i < num_views; i++)
        {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, present[buffer_idx].fbo[i]);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glDrawBuffer(draw_buffer_index[i]);

            glBlitFramebuffer(0, 0, window_w, window_h, 0, 0, window_w, window_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
        glfwSwapBuffers(window);

        // Signal that OpenGL is finished with rendered image (now available for Vulkan to reuse)
        glSignalSemaphoreEXT(present[buffer_idx].sem_signal_available, 0, nullptr, 1, buffers, layouts);
        glFlush();
    }

    return EXIT_SUCCESS;
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
    // Create shared semaphore from provided handle
    glGenSemaphoresEXT(1, semaphore);
#if defined(_WIN32)
    glImportSemaphoreWin32HandleEXT(*semaphore, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, sem_handle);
#elif defined(__linux__)
    glImportSemaphoreFdEXT(*semaphore, GL_HANDLE_TYPE_OPAQUE_FD_EXT, sem_handle);
#endif
}
