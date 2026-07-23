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
    struct GLSLDisplaySurface {
        int base_shape; // PLANE = 0, CYLINDER = 1, UNKNOWN = INT_MAX
        float d1[3];    // PLANE = bottom left corner, CYLINDER = radius
        float d2[3];    // PLANE = bottom right corner, CYLINDER = altitude
        float d3[3];    // PLANE = top left corner, CYLINDER = sector
    };
    struct GLSLDisplayData {
        // virtual desktop screen layout
        int grid_dims[2];
        float resolution[2];
        // physical display surface layout
        GLSLDisplaySurface surfaces[32];
    };

    class DisplayConfig {
    private:
        int _monitor_index;
        uint32_t _display_grid[2];
        std::vector<GLSLDisplaySurface*> _surfaces;
        float _origin[3];
    public:
        DisplayConfig();
        ~DisplayConfig();

        bool loadFromFile(const char* config_filename);
        int getMonitor();
        uint32_t getNumberOfDisplayColumns();
        uint32_t getNumberOfDisplayRows();
        uint32_t getSurfaceCount();
        void getSurface(uint32_t index, GLSLDisplaySurface** surf_ptr);
        void getOrigin(float* origin);
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
    struct PushConst360 {
        float rotation[2];
        uint32_t is_stereo;
    };

    class Vulkan360 {
    private:
        struct VulkanRenderBuffer {
            VkCommandBuffer cmd;
            VulkanInteropSemaphore sem_vk_available;
            VulkanInteropSemaphore sem_vk_finished;
            VkFence in_flight_fence;
            VulkanInteropImage image;
            VkFramebuffer framebuffer;
        };
        struct VulkanData {
            VkExtent2D extent;
            VkInstance instance;
            VkPhysicalDevice physical_device;
            int q_family_index;
            VkDevice device;
            VkQueue queue;
            VkCommandPool pool;
            VulkanRenderBuffer render_buffer[2];
            int render_buffer_index;
            VkRenderPass render_pass;
            VkDescriptorSetLayout desc_layout;
            VkDescriptorPool desc_pool;
            VkPipeline pipeline;
            VkPipelineLayout pipeline_layout;
            VulkanTexture media360;
            VkDescriptorSet desc;
            VkBuffer ubo_buffer;
            VkDeviceMemory ubo_memory;
            vk360::GLSLDisplayData display_data_ubo;
        };

        VulkanData _vk;
        bool _is_stereo;
        float _view_rotation[2];
        DisplayConfig* _config;

        void createVulkanInstance(VkInstance* instance_ptr);
        void findPhysicalDevice(uint8_t* device_uuid, VkPhysicalDevice* physical_device_ptr, int* q_fam_idx_ptr);
        int findGraphicsComputeFamilyIndex(VkPhysicalDevice physical_device);
        void createVulkanDeviceAndQueue(VkDevice* device_ptr, VkQueue* queue_ptr);
        void createCommandPool(VkCommandPool* pool_ptr);
        void createCommandBuffer(VkCommandBuffer* cmd_ptr);
        void createSyncObjects(VulkanInteropSemaphore* img_available_ptr, VulkanInteropSemaphore* img_finished_ptr, VkFence* in_flight_ptr);
        void createExternalSemaphore(VulkanInteropSemaphore* ext_sem);
        void createExternalImage(uint32_t width, uint32_t height, uint32_t layers, VkFormat format, VulkanInteropImage* ext_img);
        void createFramebuffer(VulkanImageData& image, VkFramebuffer* framebuffer_ptr);
        void createRenderPass(VkFormat color_format, VkRenderPass* render_pass_ptr);
        void createDescriptorSet(VkDescriptorSetLayout* desc_layout_ptr, VkDescriptorPool* desc_pool_ptr, VkDescriptorSet* desc_ptr);
        void createUniformBufferObject(VkBuffer* ubo_ptr, VkDeviceMemory* ubo_memory_ptr);
        void createGraphicsPipeline(VkPipeline* pipeline_ptr, VkPipelineLayout* pipeline_layout_ptr);
        int findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties);
        void transitionImageLayoutToGeneral(VkImage image, VkFormat format, uint32_t layers);
        void loadShaderModule(const char* path, VkShaderModule* shader_ptr);
        void beginCommandBuffer(VulkanRenderBuffer& rb);
        void dipatchMenuCompute(VulkanRenderBuffer& rb);
        void beginRenderPass(VulkanRenderBuffer& rb);
        void drawTestScreen(VulkanRenderBuffer& rb);
        void draw360Image(VulkanRenderBuffer& rb);
        void draw3dMenu(VulkanRenderBuffer& rb);
        void endCommandBufferAndSubmit(VulkanRenderBuffer& rb);

    public:
        Vulkan360(uint8_t* device_uuid, uint32_t width, uint32_t height, bool is_stereo, DisplayConfig* config);
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
        void setViewRotation(float yaw, float pitch);
        int drawFrame();
    };
}
