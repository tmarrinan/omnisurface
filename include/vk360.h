#pragma once
#include <cstdint>
#include <vector>
#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__linux__)
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#include <vulkan/vulkan.h>
//#include "native_render_handle.h"

class Vulkan360 {
private:
    struct VulkanData {
        VkInstance instance;
        VkSurfaceKHR surface;
        VkPhysicalDevice physical_device;
        int q_family_index;
        VkDevice device;
        VkQueue queue;
        VkSwapchainKHR swapchain;
        VkCommandPool pool;
        VkCommandBuffer cmd;
        VkSemaphore img_available;
        VkSemaphore img_finished;
        VkFence in_flight;
        std::vector<VkImage> swapchain_images;
    };

    VulkanData _vk;
    bool _is_stereo;

    //void readDisplayConfig(const char* config_filename);
    //void createFullscreenWindow(const char* title, GLFWwindow** window_ptr);
    void createVulkanInstance(const char** exts, uint32_t ext_count, VkInstance* instance_ptr);
    void createVulkanSurface(void* w_handle, void* m_handle, VkSurfaceKHR* surface_ptr);
    void findPhysicalDevice(VkPhysicalDevice* physical_device_ptr, int* q_fam_idx_ptr);
    int findGraphicsComputeFamilyIndex(VkPhysicalDevice physical_device);
    bool hasStereo3dCapability();
    void createVulkanDeviceAndQueue(VkDevice* device_ptr, VkQueue* queue_ptr);
    void createSwapChain(VkSwapchainKHR* swapchain_ptr, std::vector<VkImage>* swapchain_images_ptr);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height);
    void createCommandPoolAndBuffer(VkCommandPool* pool_ptr, VkCommandBuffer* cmd_ptr);
    void createSyncObjects(VkSemaphore* img_available_ptr, VkSemaphore* img_finished_ptr, VkFence* in_flight_ptr);

public:
    Vulkan360(const char** exts, uint32_t ext_count, void* w_handle, void* m_handle);
    ~Vulkan360();

    int initializeWindow(const char* title, const char* default_image);
    uint32_t getRenderBufferIndex();
    void drawFrame(uint32_t buffer_idx);
    void swapBuffers(uint32_t buffer_idx);
    //bool shouldClose();
    //void pollEvents();
};
