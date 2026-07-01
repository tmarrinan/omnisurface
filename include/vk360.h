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
    enum ExternalObjectType : uint8_t { RENDER_BUFFER_0, RENDER_BUFFER_1, AVAILABLE_SEMAPHORE, FINISHED_SEMAPHORE };
    struct VulkanImageData {
        VkImage image = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
        uint64_t mem_size = 0;
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

    class Vulkan360 {
    private:
        struct VulkanData {
            VkInstance instance;
            VkPhysicalDevice physical_device;
            int q_family_index;
            VkDevice device;
            VkQueue queue;
            VkCommandPool pool;
            VkCommandBuffer cmd;
            VulkanInteropSemaphore img_available;
            VulkanInteropSemaphore img_finished;
            VkFence in_flight;
            VulkanInteropImage render_buffer[2];
        };

        VulkanData _vk;
        uint32_t _width;
        uint32_t _height;
        bool _is_stereo;

        void createVulkanInstance(VkInstance* instance_ptr);
        void findPhysicalDevice(uint8_t* device_uuid, VkPhysicalDevice* physical_device_ptr, int* q_fam_idx_ptr);
        int findGraphicsComputeFamilyIndex(VkPhysicalDevice physical_device);
        void createVulkanDeviceAndQueue(VkDevice* device_ptr, VkQueue* queue_ptr);
        void createCommandPoolAndBuffer(VkCommandPool* pool_ptr, VkCommandBuffer* cmd_ptr);
        void createSyncObjects(VulkanInteropSemaphore* img_available_ptr, VulkanInteropSemaphore* img_finished_ptr, VkFence* in_flight_ptr);
        void createExternalSemaphore(VulkanInteropSemaphore* ext_sem);
        void createExternalImage(uint32_t width, uint32_t height, uint32_t layers, VkFormat format, VulkanInteropImage* ext_img);
        int findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties);
        void transitionImageLayoutToGeneral(VkImage image, VkFormat format);
        void recordImageBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout,
            uint32_t src_queue_family, uint32_t dest_queue_family, VkAccessFlags src_access, VkAccessFlags dst_access,
            VkImageAspectFlags aspect_flags, VkPipelineStageFlags src_stage, VkPipelineStageFlags dest_stage);

    public:
        Vulkan360(uint8_t* device_uuid, uint32_t width, uint32_t height, bool is_stereo);
        ~Vulkan360();

#if defined(_WIN32)
        HANDLE getExternalHandle(ExternalObjectType obj_type, uint64_t* mem_size);
#elif defined(__linux__)
        int getExternalHandle(ExternalObjectType obj_type, uint64_t* mem_size);
#endif
        void drawFrame(uint32_t buffer_idx);
    };
}
