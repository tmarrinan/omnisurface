#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
extern "C" {
    _declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;       // Forces NVIDIA Performance GPU
    _declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1; // Forces AMD Performance GPU
}
#endif

#include "DTrackSDK.hpp"
#include "uiserver.h"
#include "vk360.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


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

struct InteractionState {
    bool mouse_button_down[2];
    double cursor_pos[2];
    double cursor_delta[2];
    bool camera_move[6];
};

struct TrackSyncData {
    std::mutex sync_mutex;
    bool new_data;
    bool exit;
    float camera_position[3];
};

// Function definitions
GLFWwindow* createFullscreenWindow(const char* title, int monitor_idx, int* width, int* height, bool* is_stereo);
void importExternalTextureArray(vk360::ExternalImageInfo& ext_img_info, GLuint* texture);
void importExternalSemaphore(ExternalHandle sem_handle, GLuint* semaphore);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void mouseMoveCallback(GLFWwindow* window, double xpos, double ypos);
void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void dtrackTask(uint16_t local_port, int camera_id, int controller_id, TrackSyncData* tracking_data);

// Main program
int main()
{
    // Read in display configuration
    vk360::DisplayConfig *config = new vk360::DisplayConfig();
    if (!config->loadFromFile("resrc/config_3plane.txt"))
    //if (!config->loadFromFile("resrc/config_halfcylinder.txt"))
    {
        fprintf(stderr, "OmniSurface> Error: Failed to read config file\n");
        return EXIT_FAILURE;
    }
    config->printConfig();

    // UI Server (image selection from remote web browser)
    uis::UiServer* server = new uis::UiServer("http", 8080);
    server->setImageSelectionRootDirectory(config->getImageDirectory());
    server->listenAsync();

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

    // Register mouse event callbacks
    InteractionState interaction{};
    memset(&interaction, 0, sizeof(InteractionState));
    glfwSetWindowUserPointer(window, &interaction);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, mouseMoveCallback);
    glfwSetKeyCallback(window, keyboardCallback);

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
    vk360::Vulkan360* app = new vk360::Vulkan360(gl_device_uuid, window_w, window_h, is_stereo, config);
    //vk360::Vulkan360* app = new vk360::Vulkan360(gl_device_uuid, window_w, window_h, true, config);

    // Import external data to OpenGL for framebuffer presentation
    PresentData present[2];
    uint32_t num_views = is_stereo ? 2 : 1;
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

        glGenFramebuffers(num_views, present[i].fbo);
        for (uint32_t j = 0; j < num_views; j++)
        {
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

    // Connect to tracking system
    std::thread tracking_thread;
    TrackSyncData* tracking_data = new TrackSyncData();
    tracking_data->new_data = false;
    tracking_data->exit = false;
    tracking_data->camera_position[0] = 0.0;
    tracking_data->camera_position[1] = 1.8;
    tracking_data->camera_position[2] = 0.0;
    const vk360::TrackingSystem tracking_type = config->getTrackingSystemType();
    if (tracking_type == vk360::TrackingSystem::DTRACK)
    {
        tracking_thread = std::thread(dtrackTask, config->getTrackingPort(), config->getTrackingCameraId(), config->getTrackingControllerId(), tracking_data);
    }
    else if (tracking_type == vk360::TrackingSystem::VRPN)
    {
        // TODO: fill this in
    }

    // Initialize OpenGL settings
    glDisable(GL_DEPTH_TEST);
    GLenum draw_buffer_index[2] = { GL_BACK_LEFT, GL_BACK_RIGHT };
    if (!is_stereo) draw_buffer_index[0] = GL_BACK;

    app->loadImage("resrc/images/SampleOmni3D.png", true);

    // Main render loop
    GLuint buffers[1];
    GLenum layouts[1] = { GL_LAYOUT_GENERAL_EXT };
    float rotation[2] = { 0.0, 0.0 };
    float camera_position[3] = { 0.0, 1.8, 0.0 };
    float origin[3];
    config->getOrigin(origin);
    while (!glfwWindowShouldClose(window))
    {
        // Poll for user events
        glfwPollEvents();

        // Check if `esc` key has been pressed - if so, set flag to make window close
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }

        // Check if new 360 image file selected from UI
        if (server->fileSelected())
        {
            std::filesystem::path media = server->getSelectedFileName();
            app->loadImage(media.string().c_str(), false); // TODO: figure out way to tell if stereo or no
            server->markSelectedFileAsProcessed();
        }

        // Update position (TODO: modify to account for delta time)
        if (interaction.camera_move[0]) camera_position[0] -= 0.05;
        if (interaction.camera_move[1]) camera_position[0] += 0.05;
        if (interaction.camera_move[2]) camera_position[1] -= 0.05;
        if (interaction.camera_move[3]) camera_position[1] += 0.05;
        if (interaction.camera_move[4]) camera_position[2] -= 0.05;
        if (interaction.camera_move[5]) camera_position[2] += 0.05;
        app->setCameraPosition(camera_position[0], camera_position[1], camera_position[2]);

        // Update rotation
        if (interaction.mouse_button_down[0])
        {
            float delta_theta = -interaction.cursor_delta[0] / 1000.0;
            float delta_phi = interaction.cursor_delta[1] / 1000.0;

            float new_theta = std::fmod(rotation[0] + delta_theta + M_PI, 2.0 * M_PI);
            if (new_theta < 0.0) new_theta += 2.0 * M_PI;
            rotation[0] = new_theta - M_PI;

            rotation[1] = std::min(std::max(rotation[1] + delta_phi, static_cast<float>(-0.5 * M_PI)), static_cast<float>(0.5 * M_PI));
        }
        interaction.cursor_delta[0] = 0.0;
        interaction.cursor_delta[1] = 0.0;
        app->setViewRotation(rotation[0], rotation[1]);

        // Updates from camera tracking
        {
            std::lock_guard lock(tracking_data->sync_mutex);
            if (tracking_data->new_data)
            {
                camera_position[0] = tracking_data->camera_position[0] - origin[0];
                camera_position[1] = tracking_data->camera_position[1] - origin[1];
                camera_position[2] = tracking_data->camera_position[2] - origin[2];

                app->setCameraPosition(camera_position[0], camera_position[1], camera_position[2]);
            }
        }

        // Trigger render (Vulkan)
        uint32_t buffer_idx = app->drawFrame();

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

            glBlitFramebuffer(0, 0, window_w, window_h, 0, window_h, window_w, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
        glfwSwapBuffers(window);

        // Signal that OpenGL is finished with rendered image (now available for Vulkan to reuse)
        glSignalSemaphoreEXT(present[buffer_idx].sem_signal_available, 0, nullptr, 1, buffers, layouts);
        glFlush();
    }

    // Stop DTrack
    tracking_data->exit = true;
    if (tracking_thread.joinable()) tracking_thread.join();

    // Stop UI Server
    server->shutdown();

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
    int x_offset, y_offset;
    glfwGetMonitorPos(monitors[monitor_idx], &x_offset, &y_offset);
    const GLFWvidmode* mode = glfwGetVideoMode(monitors[monitor_idx]);
    *width = mode->width;
    *height = mode->height;

    // Create fullscreen window
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, title, nullptr, nullptr);
    if (window)
    {
        glfwSetWindowPos(window, x_offset, y_offset);
        glfwShowWindow(window);
    }
    else
    {
        glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
        glfwWindowHint(GLFW_STEREO, GLFW_FALSE);
        *width = 0.8 * static_cast<double>(mode->width);
        *height = 0.8 * static_cast<double>(mode->height);
        window = glfwCreateWindow(*width, *height, title, nullptr, nullptr);
        //window = glfwCreateWindow(mode->width, mode->height, title, monitors[monitor_idx], nullptr);
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

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    InteractionState* state = static_cast<InteractionState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS) state->mouse_button_down[0] = true;
        else if (action == GLFW_RELEASE) state->mouse_button_down[0] = false;
        
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS) state->mouse_button_down[1] = true;
        else if (action == GLFW_RELEASE) state->mouse_button_down[1] = false;

    }
    glfwGetCursorPos(window, &(state->cursor_pos[0]), &(state->cursor_pos[1]));
    state->cursor_delta[0] = 0.0;
    state->cursor_delta[1] = 0.0;
}

void mouseMoveCallback(GLFWwindow* window, double xpos, double ypos)
{
    InteractionState* state = static_cast<InteractionState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr) return;

    state->cursor_delta[0] = xpos - state->cursor_pos[0];
    state->cursor_delta[1] = ypos - state->cursor_pos[1];
    state->cursor_pos[0] = xpos;
    state->cursor_pos[1] = ypos;
}

void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    InteractionState* state = static_cast<InteractionState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr) return;

    switch (key)
    {
    case GLFW_KEY_A:
        if (action == GLFW_PRESS) state->camera_move[0] = true;
        if (action == GLFW_RELEASE) state->camera_move[0] = false;
        break;
    case GLFW_KEY_D:
        if (action == GLFW_PRESS) state->camera_move[1] = true;
        if (action == GLFW_RELEASE) state->camera_move[1] = false;
        break;
    case GLFW_KEY_S:
        if (action == GLFW_PRESS) state->camera_move[2] = true;
        if (action == GLFW_RELEASE) state->camera_move[2] = false;
        break;
    case GLFW_KEY_W:
        if (action == GLFW_PRESS) state->camera_move[3] = true;
        if (action == GLFW_RELEASE) state->camera_move[3] = false;
        break;
    case GLFW_KEY_R:
        if (action == GLFW_PRESS) state->camera_move[4] = true;
        if (action == GLFW_RELEASE) state->camera_move[4] = false;
        break;
    case GLFW_KEY_F:
        if (action == GLFW_PRESS) state->camera_move[5] = true;
        if (action == GLFW_RELEASE) state->camera_move[5] = false;
        break;
    }
}

void dtrackTask(uint16_t local_port, int camera_id, int controller_id, TrackSyncData* tracking_data)
{
    DTrackSDK* dt = new DTrackSDK(local_port);
    if (!dt->isDataInterfaceValid())
    {
        fprintf(stderr, "OmniSurface> Warning: failed to connect to DTrack\n");
        return;
    }
    
    printf("OmniSurface> Info: listening for DTrack data on port %u\n", dt->getDataPort());
    dt->setDataTimeoutUS(50000); // 50 ms
    while (!tracking_data->exit)
    {
        if (dt->receive())
        {
            printf("DTrack: received data for %d tracked bodies\n", dt->getNumBody());
            for (int i = 0; i < dt->getNumBody(); i++)
            {
                const DTrackBody* body = dt->getBody(i);
                if (body && body->isTracked())
                {
                    if (body->id == camera_id)
                    {
                        std::lock_guard lock(tracking_data->sync_mutex);
                        //DTrackQuaternion quat = body->getQuaternion();
                        tracking_data->camera_position[0] = 0.001 * body->loc[0];
                        tracking_data->camera_position[1] = 0.001 * body->loc[1];
                        tracking_data->camera_position[2] = 0.001 * body->loc[2];
                        tracking_data->new_data = true;
                    }
                    //else
                    //{
                    //    DTrackQuaternion quat = body->getQuaternion();
                    //    printf("[BODY] %3d: pos = (%.3f, %.3f, %.3f); rot = (%.3f, %.3f, %.3f, %.3f)\n", body->id,
                    //        body->loc[0], body->loc[1], body->loc[2], quat.w, quat.x, quat.y, quat.z);
                    //}
                }
            }
        }
    }
}
