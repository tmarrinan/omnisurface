#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
//#include <vulkan/vulkan.h>
//#include "native_render_handle.h"

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
    struct VulkanData {
        VkInstance instance;
        VkSurfaceKHR surface;
        VkPhysicalDevice physical_device;
        int q_family_index;
        VkDevice device;
        VkQueue queue;
        VkSwapchainKHR swapchain;
    };

    Config _config;
    GLFWwindow* _window;
    uint32_t _window_w;
    uint32_t _window_h;
    VulkanData _vk;
    bool _is_stereo;

    void readDisplayConfig(const char* config_filename);
    void createFullscreenWindow(const char* title, GLFWwindow** window_ptr);
    void createVulkanInstance(VkInstance* instance_ptr);
    void createVulkanSurface(VkSurfaceKHR* surface_ptr);
    void findPhysicalDevice(VkPhysicalDevice* physical_device_ptr, int* q_fam_idx_ptr);
    int findGraphicsComputeFamilyIndex(VkPhysicalDevice physical_device);
    bool hasStereo3dCapability();
    void createVulkanDeviceAndQueue(VkDevice* device_ptr, VkQueue* queue_ptr);
    void createSwapChain(VkSwapchainKHR* swapchain_ptr);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height);

public:
    Vulkan360(const char* config_filename);
    ~Vulkan360();

    bool hasValidDisplayConfig();
    int initializeWindow(const char* title);
};
