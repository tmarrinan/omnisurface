#pragma once
#include <cstdint>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <vulkan/vulkan.h>


namespace vk360 {
    enum DisplayBaseShape : uint8_t { BASE_SHAPE_UNKNOWN, BASE_SHAPE_PLANE, BASE_SHAPE_CYLINDER };

    struct DisplaySurface {
        DisplayBaseShape base_shape;
    };

    struct DisplayPlaneSurface : DisplaySurface {
        double size[2];
        double center[3];
        double normal[3];
    };

    struct DisplayCylinderSurface : DisplaySurface {
        double radius;
        double altitude[2];
        double sector[2];
    };

    class DisplayConfig {
    private:
        int _monitor_index;
        std::vector<DisplaySurface*> _surfaces;
        double _origin[3];
    public:
        DisplayConfig();
        ~DisplayConfig();

        bool loadFromFile(const char* config_filename);
        int getMonitor();
        uint32_t getSurfaceCount();
        void getSurface(uint32_t index, DisplaySurface** surf_ptr);
        void getOrigin(double* origin);
        void printConfig();
    };


    struct ExternalImageInfo {
#if defined(_WIN32)
        HANDLE external_handle = nullptr;
#elif defined(__linux__)
        int external_handle = 0;
#endif
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t layers = 0;
        uint64_t memory_size = 0;
    };
    struct VulkanImageData {
        VkImage image = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
        uint64_t mem_size = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t layers = 0;
    };
    struct VulkanInteropImage {
        VulkanImageData vk_img_data;
#if defined(_WIN32)
        HANDLE external_handle = nullptr;
#elif defined(__linux__)
        int external_handle = 0;
#endif
    };
    struct VulkanInteropSemaphore {
        VkSemaphore semaphore;
#if defined(_WIN32)
        HANDLE external_handle = nullptr;
#elif defined(__linux__)
        int external_handle = 0;
#endif
    };
    struct VulkanTexture {
        VkImage image = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        bool stereo = false;
    };

    class Vulkan360 {
    private:
        struct VulkanRenderBuffer {
            VkCommandBuffer cmd;
            VulkanInteropSemaphore sem_vk_available;
            VulkanInteropSemaphore sem_vk_finished;
            VkFence in_flight_fence;
            VulkanInteropImage image;
        };
        struct VulkanData {
            VkInstance instance;
            VkPhysicalDevice physical_device;
            int q_family_index;
            VkDevice device;
            VkQueue queue;
            VkCommandPool pool;
            VulkanRenderBuffer render_buffer[2];
            int render_buffer_index;
            VulkanTexture media360;
        };

        VulkanData _vk;
        uint32_t _width;
        uint32_t _height;
        bool _is_stereo;

        void createVulkanInstance(VkInstance* instance_ptr);
        void findPhysicalDevice(uint8_t* device_uuid, VkPhysicalDevice* physical_device_ptr, int* q_fam_idx_ptr);
        int findGraphicsComputeFamilyIndex(VkPhysicalDevice physical_device);
        void createVulkanDeviceAndQueue(VkDevice* device_ptr, VkQueue* queue_ptr);
        void createCommandPool(VkCommandPool* pool_ptr);
        void createCommandBuffer(VkCommandBuffer* cmd_ptr);
        void createSyncObjects(VulkanInteropSemaphore* img_available_ptr, VulkanInteropSemaphore* img_finished_ptr, VkFence* in_flight_ptr);
        void createExternalSemaphore(VulkanInteropSemaphore* ext_sem);
        void createExternalImage(uint32_t width, uint32_t height, uint32_t layers, VkFormat format, VulkanInteropImage* ext_img);
        int findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties);
        void transitionImageLayoutToGeneral(VkImage image, VkFormat format, uint32_t layers);

    public:
        Vulkan360(uint8_t* device_uuid, uint32_t width, uint32_t height, bool is_stereo);
        ~Vulkan360();

        void getExternalRenderBufferInfo(uint32_t index, ExternalImageInfo* ext_img);
#if defined(_WIN32)
        void getExternalSignalAvailableSemaphoreHandle(uint32_t index, HANDLE* ext_handle);
        void getExternalWaitFinishedSemaphoreHandle(uint32_t index, HANDLE* ext_handle);
#elif defined(__linux__)
        void getExternalSignalAvailableSemaphoreHandle(uint32_t index, int* ext_handle);
        void getExternalWaitFinishedSemaphoreHandle(uint32_t index, int* ext_handle);
#endif
        void loadImage(const char* path, bool is_stereo);
        int drawTestScreen();
        int drawMonoImage(float rotation);
        int drawStereoImage(float rotation);
    };
}
