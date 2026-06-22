#include <vulkan/vulkan.h>
// TODO: Backend WINDOWS (DX12) / LINUX (OpenGL) for Stereo 3D

namespace vk360
{
    struct VulkanData {
        VkDevice device;
        VkQueue queue;
    };
    
    int init(const char* config_filename, VulkanData *vk);
}
