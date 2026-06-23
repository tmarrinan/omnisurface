#pragma once
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "native_render_handle.h"

class Vulkan360 {
private:
    enum DisplayBaseShape : uint8_t { BASE_SHAPE_PLANE, BASE_SHAPE_CYLINDER };
    struct Config {
        bool load_success;
        DisplayBaseShape base_shape;
        uint32_t facets;
        double radius;
        double height;
        uint32_t resolution_w;
        uint32_t resolution_h;
    };

    Config _config;
    GLFWwindow* _window;
    NativeRenderHandle* _native_renderer;

    void readDisplayConfig(const char* config_filename);

public:
    Vulkan360(const char* config_filename);
    ~Vulkan360();

    bool hasValidDisplayConfig();
    int initializeWindow(const char* title);
};
