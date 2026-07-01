#include <iostream>
#include <cstdint>
#include <cstring>
#include <string>
#include <fstream>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "vk360.h"


static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data)
{
    // Filter out unwanted messages (only show warnings and errors)
    if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        fprintf(stderr, "[VULKAN VALIDATION] %s\n", callback_data->pMessage);
    }

    // Return VK_FALSE - returning VK_TRUE aborts the API call with an error
    return VK_FALSE;
}


vk360::Vulkan360::Vulkan360(uint8_t* device_uuid, uint32_t width, uint32_t height, bool is_stereo) :
    _width{ width }, _height{ height }, _is_stereo{ is_stereo }
    
{
    createVulkanInstance(&_vk.instance);
    findPhysicalDevice(device_uuid, &_vk.physical_device, &_vk.q_family_index);
    createVulkanDeviceAndQueue(&_vk.device, &_vk.queue);
    createCommandPoolAndBuffer(&_vk.pool, &_vk.cmd);
    createSyncObjects(&_vk.img_available, &_vk.img_finished, &_vk.in_flight);
    createExternalImage(_width, _height, _is_stereo ? 2 : 1, VK_FORMAT_R8G8B8A8_UNORM, &(_vk.render_buffer[0]));
    createExternalImage(_width, _height, _is_stereo ? 2 : 1, VK_FORMAT_R8G8B8A8_UNORM, &(_vk.render_buffer[1]));
}

vk360::Vulkan360::~Vulkan360()
{
    // Clean up
}

void vk360::Vulkan360::getExternalRenderBufferInfo(uint32_t index, ExternalImageInfo* ext_img)
{
    ext_img->external_handle = _vk.render_buffer[index].external_handle;
    ext_img->memory_size = _vk.render_buffer[index].vk_img_data.mem_size;
    ext_img->width = _vk.render_buffer[index].vk_img_data.width;
    ext_img->height = _vk.render_buffer[index].vk_img_data.height;
    ext_img->layers = _vk.render_buffer[index].vk_img_data.layers;
}

#if defined(_WIN32)
void vk360::Vulkan360::getExternalImageAvailableSemaphoreInfo(HANDLE* ext_handle)
#elif defined(__linux__)
void vk360::Vulkan360::getExternalImageAvailableSemaphoreInfo(int* ext_handle)
#endif
{
    *ext_handle = _vk.img_available.external_handle;
}

#if defined(_WIN32)
void vk360::Vulkan360::getExternalImageFinishedSemaphoreInfo(HANDLE* ext_handle)
#elif defined(__linux__)
void vk360::Vulkan360::getExternalImageFinishedSemaphoreInfo(int* ext_handle)
#endif
{
    *ext_handle = _vk.img_finished.external_handle;
}

void vk360::Vulkan360::drawFrame(uint32_t buffer_idx)
{
    // Record draw commands
    //VkImage current_image = _vk.swapchain_images[buffer_idx];

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(_vk.cmd, &begin_info) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to begin recording command buffer\n");
    }

    /////
    /////

    VkImageMemoryBarrier barrierToClear{};
    barrierToClear.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierToClear.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // likely ok, but could do transition to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR at creation time, then set to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR here
    barrierToClear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrierToClear.srcAccessMask = 0;
    barrierToClear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    //barrierToClear.image = current_image;
    barrierToClear.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrierToClear.subresourceRange.baseMipLevel = 0;
    barrierToClear.subresourceRange.levelCount = 1;
    barrierToClear.subresourceRange.baseArrayLayer = 0;
    barrierToClear.subresourceRange.layerCount = _is_stereo ? 2 : 1; // Clear both stereo layers if true

    vkCmdPipelineBarrier(_vk.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrierToClear);

    // =================================================================
    // THE CLEAR COMMAND (Choose your background color here!)
    // =================================================================
    // Float values represent Normalized Red, Green, Blue, Alpha (0.0 to 1.0)
    VkClearColorValue clearColor = { {0.2f, 0.3f, 0.4f, 1.0f} }; // A nice slate steel blue

    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = _is_stereo ? 2 : 1; // Erase both stereo layers in tandem!

    //vkCmdClearColorImage(_vk.cmd, current_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

    // --- BARRIER 2: Transition Image Layout back to Present Source ---
    VkImageMemoryBarrier barrierToPresent = barrierToClear;
    barrierToPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrierToPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrierToPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrierToPresent.dstAccessMask = 0;

    vkCmdPipelineBarrier(_vk.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrierToPresent);

    // =================================================================
    // CLOSE THE COMMAND BUFFER
    // =================================================================
    if (vkEndCommandBuffer(_vk.cmd) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to record command buffer!");
    }

    /////
    /////


    // Submit your recorded drawing commands
    VkSemaphore wait_semaphores[] = { _vk.img_available.semaphore };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signal_semaphores[] = { _vk.img_finished.semaphore };

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &_vk.cmd;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    if (vkQueueSubmit(_vk.queue, 1, &submit_info, _vk.in_flight) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to submit draw command buffer\n");
    }
}

///////////////////////////////////////////////////////////////
//   PRIVATE METHODS                                         //
///////////////////////////////////////////////////////////////
void vk360::Vulkan360::createVulkanInstance(VkInstance* instance_ptr)
{
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "OmniSurface";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Custom Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> extensions = {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
        //VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        //VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
        //VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
    };
    std::vector<const char*> validation_layers = { "VK_LAYER_KHRONOS_validation" };

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = extensions.size();
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledLayerCount = validation_layers.size();
    create_info.ppEnabledLayerNames = validation_layers.data();
    create_info.pNext = nullptr;

    if (vkCreateInstance(&create_info, nullptr, instance_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: could not create `VkInstance`\n");
        *instance_ptr = VK_NULL_HANDLE;
    }

    VkDebugUtilsMessengerEXT debug_messenger;
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
    debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_create_info.pfnUserCallback = debugCallback;
    debug_create_info.pUserData = nullptr;

    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(*instance_ptr, "vkCreateDebugUtilsMessengerEXT");
    if (vkCreateDebugUtilsMessengerEXT(*instance_ptr, &debug_create_info, nullptr, &debug_messenger) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Warning: failed to set up debug messenger\n");
    }
}

void vk360::Vulkan360::findPhysicalDevice(uint8_t* device_uuid, VkPhysicalDevice* physical_device_ptr, int* q_fam_idx_ptr)
{
    *physical_device_ptr = VK_NULL_HANDLE;
    *q_fam_idx_ptr = -1;

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(_vk.instance, &device_count, nullptr);
    if (device_count == 0)
    {
        fprintf(stderr, "Vulkan360> Error: failed to find any GPU with Vulkan support!\n");
        *physical_device_ptr = VK_NULL_HANDLE;
    }
    std::vector<VkPhysicalDevice> physical_devices(device_count);
    vkEnumeratePhysicalDevices(_vk.instance, &device_count, physical_devices.data());
    for (uint32_t i = 0; i < physical_devices.size(); i++)
    {
        VkPhysicalDeviceIDProperties device_id_props{};
        device_id_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;

        VkPhysicalDeviceProperties2 props2{};
        props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props2.pNext = &device_id_props;
        vkGetPhysicalDeviceProperties2(physical_devices[i], &props2);

        int q_fam_idx = findGraphicsComputeFamilyIndex(physical_devices[i]);
        if (memcmp(device_uuid, device_id_props.deviceUUID, VK_UUID_SIZE) == 0 && q_fam_idx >= 0)
        {
            *physical_device_ptr = physical_devices[i];
            *q_fam_idx_ptr = q_fam_idx;
            break;
        }
    }

    if (*physical_device_ptr == VK_NULL_HANDLE)
    {
        fprintf(stderr, "Vulkan360> Error: could not find target GPU device\n");
    }
    if (*q_fam_idx_ptr < 0)
    {
        fprintf(stderr, "Vulkan360> Error: GPU device does not support graphics and compute shaders\n");
    }
}

int vk360::Vulkan360::findGraphicsComputeFamilyIndex(VkPhysicalDevice physical_device)
{
    int index = -1;
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());
    for (uint32_t i = 0; i < queue_family_count; i++)
    {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            index = i;
            break;
        }
    }
    return index;
}

void vk360::Vulkan360::createVulkanDeviceAndQueue(VkDevice* device_ptr, VkQueue* queue_ptr)
{
    // Verify multiview is supported
    VkPhysicalDeviceMultiviewFeatures multiview_features{};
    multiview_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
    multiview_features.pNext = nullptr;

    VkPhysicalDeviceFeatures2 device_features2{};
    device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    device_features2.pNext = &multiview_features;

    vkGetPhysicalDeviceFeatures2(_vk.physical_device, &device_features2);
    if (multiview_features.multiview == VK_FALSE)
    {
        fprintf(stderr, "Vulkan360> Error: Vulkan multiview not supported\n");
        *device_ptr = VK_NULL_HANDLE;
        *queue_ptr = VK_NULL_HANDLE;
        return;
    }

    // Create device and queue
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = _vk.q_family_index;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;

    const std::vector<const char*> device_extensions = {
#if defined(_WIN32)
        VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME
#elif defined(__linux__)
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME
#endif
    };

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;
    device_create_info.enabledExtensionCount = device_extensions.size();
    device_create_info.ppEnabledExtensionNames = device_extensions.data();
    device_create_info.pNext = &multiview_features;
    if (vkCreateDevice(_vk.physical_device, &device_create_info, nullptr, device_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: could not create `vkDevice`\n");
        *device_ptr = VK_NULL_HANDLE;
        *queue_ptr = VK_NULL_HANDLE;
        return;
    }

    vkGetDeviceQueue(*device_ptr, _vk.q_family_index, 0, queue_ptr);
}

void vk360::Vulkan360::createCommandPoolAndBuffer(VkCommandPool* pool_ptr, VkCommandBuffer* cmd_ptr)
{
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = _vk.q_family_index;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(_vk.device, &pool_info, nullptr, pool_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: could not create `vkCommandPool`\n");
        *pool_ptr = VK_NULL_HANDLE;
        *cmd_ptr = VK_NULL_HANDLE;
        return;
    }

    VkCommandBufferAllocateInfo cmd_alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cmd_alloc_info.commandPool = *pool_ptr;
    cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_alloc_info.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(_vk.device, &cmd_alloc_info, cmd_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: could not allocate `vkCommandBuffer`\n");
        *pool_ptr = VK_NULL_HANDLE;
        *cmd_ptr = VK_NULL_HANDLE;
        return;
    }
}

void vk360::Vulkan360::createSyncObjects(VulkanInteropSemaphore* img_available_ptr, VulkanInteropSemaphore* img_finished_ptr, VkFence* in_flight_ptr)
{
    // Create external semaphores
    createExternalSemaphore(img_available_ptr);
    createExternalSemaphore(img_finished_ptr);

    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateFence(_vk.device, &fence_create_info, nullptr, in_flight_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to create `VkFence`\n");
        *in_flight_ptr = VK_NULL_HANDLE;
        return;
    }
}

void vk360::Vulkan360::createExternalSemaphore(VulkanInteropSemaphore* ext_sem)
{
    // Specify the handle type
    VkExportSemaphoreCreateInfo export_info{};
    export_info.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
#if defined(_WIN32)
    export_info.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#elif defined(__linux__)
    export_info.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
#else
    #error "Unsupported operating system"
#endif

    // Create Vulkan semaphores
    VkSemaphoreCreateInfo sem_create_info{};
    sem_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    sem_create_info.pNext = &export_info;
    if (vkCreateSemaphore(_vk.device, &sem_create_info, nullptr, &(ext_sem->semaphore)) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to create `VkSemaphore`\n");
        ext_sem = nullptr;
        return;
    }

    // Extract the external handle
#if defined(_WIN32)
    VkSemaphoreGetWin32HandleInfoKHR get_handle_info = {};
    get_handle_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR;
    get_handle_info.semaphore = ext_sem->semaphore;
    get_handle_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;

    PFN_vkGetSemaphoreWin32HandleKHR vkGetSemaphoreWin32HandleKHR =
        (PFN_vkGetSemaphoreWin32HandleKHR)vkGetDeviceProcAddr(_vk.device, "vkGetSemaphoreWin32HandleKHR");
    vkGetSemaphoreWin32HandleKHR(_vk.device, &get_handle_info, &(ext_sem->external_handle));
#elif defined(__linux__)
    VkSemaphoreGetFdInfoKHR get_fd_info = {};
    get_fd_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
    get_fd_info.semaphore = ext_sem->semaphore;
    get_fd_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

    PFN_vkGetSemaphoreFdKHR vkGetSemaphoreFdKHR =
        (PFN_vkGetSemaphoreFdKHR)vkGetDeviceProcAddr(device, "vkGetSemaphoreFdKHR");
    vkGetSemaphoreFdKHR(device, &get_fd_info, &(ext_sem->external_handle));
#else
    #error "Unsupported operating system"
#endif
}

void vk360::Vulkan360::createExternalImage(uint32_t width, uint32_t height, uint32_t layers, VkFormat format, VulkanInteropImage* ext_img)
{
    // Specify the handle type
    VkExternalMemoryImageCreateInfo ext_image_info = {};
    ext_image_info.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
#if defined (_WIN32)
    ext_image_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#elif defined (__linux__)
    ext_image_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#else
    #error "Unsupported operating system"
#endif

    // Setup the Image
    VkImageCreateInfo image_info = {};
    VkImageUsageFlags usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (format == VK_FORMAT_R8G8B8A8_UNORM)
    {
        usage_flags |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    else if (format == VK_FORMAT_D32_SFLOAT)
    {
        usage_flags |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    else if (format == VK_FORMAT_D24_UNORM_S8_UINT)
    {
        usage_flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.pNext = &ext_image_info;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = format;
    image_info.extent = { width, height, 1 };
    image_info.mipLevels = 1;
    image_info.arrayLayers = layers;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.usage = usage_flags;

    vkCreateImage(_vk.device, &image_info, nullptr, &(ext_img->vk_img_data.image));

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(_vk.device, ext_img->vk_img_data.image, &mem_reqs);

    // Define the Export info
    VkExportMemoryAllocateInfo export_info = {};
    export_info.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
#if defined(_WIN32)
    export_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#elif defined(__linux__)
    export_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#else
    #error "Unsupported operating system"
#endif
    export_info.pNext = nullptr;

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = findMemoryType(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    alloc_info.pNext = &export_info;

    vkAllocateMemory(_vk.device, &alloc_info, nullptr, &(ext_img->vk_img_data.memory));
    vkBindImageMemory(_vk.device, ext_img->vk_img_data.image, ext_img->vk_img_data.memory, 0);

    // Extract the external handle
#if defined (_WIN32)
    ext_img->external_handle = nullptr;
    VkMemoryGetWin32HandleInfoKHR get_handle_info = {};
    get_handle_info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
    get_handle_info.memory = ext_img->vk_img_data.memory;
    get_handle_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    PFN_vkGetMemoryWin32HandleKHR vkGetMemoryWin32HandleKHR =
        (PFN_vkGetMemoryWin32HandleKHR)vkGetDeviceProcAddr(_vk.device, "vkGetMemoryWin32HandleKHR");
    vkGetMemoryWin32HandleKHR(_vk.device, &get_handle_info, &(ext_img->external_handle));
#elif defined(__linux__)
    image->external_handle = -1;
    VkMemoryGetFdInfoKHR get_fd_info = {};
    get_fd_info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    get_fd_info.memory = image->vk_img_data.memory;
    get_fd_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    PFN_vkGetMemoryFdKHR vkGetMemoryFdKHR =
        (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(device, "vkGetMemoryFdKHR");
    vkGetMemoryFdKHR(device, &get_fd_info, &(image->external_handle));
#else
    #error "Unsupported operating system"
#endif

    ext_img->vk_img_data.mem_size = mem_reqs.size;
    ext_img->vk_img_data.format = format;
    ext_img->vk_img_data.width = width;
    ext_img->vk_img_data.height = height;
    ext_img->vk_img_data.layers = layers;

    transitionImageLayoutToGeneral(ext_img->vk_img_data.image, ext_img->vk_img_data.format);
}

int vk360::Vulkan360::findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties)
{
    int index = -1;
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(_vk.physical_device, &mem_properties);
    for (int i = 0; i < mem_properties.memoryTypeCount; i++)
    {
        if ((type_filter & (1u << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            index = i;
            break;
        }
    }
    return index;
}

void vk360::Vulkan360::transitionImageLayoutToGeneral(VkImage image, VkFormat format)
{
    // Allocate a temporary command buffer
    VkCommandBufferAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = _vk.pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(_vk.device, &alloc_info, &cmd);

    // Start recording
    VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);

    VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT)
    {
        aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    else if (format == VK_FORMAT_D24_UNORM_S8_UINT)
    {
        aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    recordImageBarrier(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, 0,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        aspect_flags, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    vkEndCommandBuffer(cmd);

    // Submit and wait (blocking call for setup only)
    VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    vkQueueSubmit(_vk.queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(_vk.queue);

    vkFreeCommandBuffers(_vk.device, _vk.pool, 1, &cmd);
}

void vk360::Vulkan360::recordImageBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout,
    uint32_t src_queue_family, uint32_t dest_queue_family, VkAccessFlags src_access, VkAccessFlags dst_access,
    VkImageAspectFlags aspect_flags, VkPipelineStageFlags src_stage, VkPipelineStageFlags dest_stage)
{
    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = src_queue_family;
    barrier.dstQueueFamilyIndex = dest_queue_family;
    barrier.image = image;
    barrier.subresourceRange = { aspect_flags, 0, 1, 0, 1 };
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;

    vkCmdPipelineBarrier(cmd, src_stage, dest_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}
