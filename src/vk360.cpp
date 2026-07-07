#include <iostream>
#include <cstdint>
#include <cstring>
#include <string>
#include <fstream>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "vk360.h"


vk360::DisplayConfig::DisplayConfig() :
    _monitor_index{ 0 }
{
    _origin[0] = 0.0;
    _origin[1] = 0.0;
    _origin[2] = 0.0;
}

vk360::DisplayConfig::~DisplayConfig()
{
    for (uint32_t i = 0; i < _surfaces.size(); i++)
    {
        delete _surfaces[i];
    }
    _surfaces.clear();
}

bool vk360::DisplayConfig::loadFromFile(const char* config_filename)
{
    std::ifstream config_file(config_filename);
    if (!config_file.is_open())
    {
        fprintf(stderr, "DisplayConfig> Error: config file '%s' not found'\n", config_filename);
        return false;
    }

    std::string line;
    bool line0 = true;
    uint32_t surface_idx = 0;
    while (std::getline(config_file, line))
    {
        if (line0 && line != "OmniSurface Config")
        {
            fprintf(stderr, "DisplayConfig> Error: config file format not recognized - 1st line should be 'OmniSurface Config'\n");
            return false;
        }
        else if (line.length() >= 10 && line.substr(0, 9) == "Monitor: ")
        {
            _monitor_index = std::stoi(line.substr(9));
        }
        else if (line.length() >= 16 && line.substr(0, 15) == "Surface Count: ")
        {
            _surfaces.resize(std::stoull(line.substr(15)));
        }
        else if (line.length() >= 1 && line.substr(0, 1) == "[")
        {
            if (surface_idx >= _surfaces.size())
            {
                fprintf(stderr, "DisplayConfig> Error: config file  read error - found more surfaces than declared count\n");
                return false;
            }
            DisplayPlaneSurface* surf_plane = nullptr;
            DisplayCylinderSurface* surf_cylinder = nullptr;
            while (std::getline(config_file, line) && (line.length() < 1 || line.substr(0, 1) != "]"))
            {
                size_t data_pos = line.find_first_not_of(" ");
                if (data_pos != std::string::npos)
                {
                    line = line.substr(data_pos);
                }
                if (line.length() >= 13 && line.substr(0, 12) == "Base Shape: ")
                {
                    std::string shape = line.substr(12);
                    if (shape == "plane")
                    {
                        surf_plane = new DisplayPlaneSurface();
                        surf_plane->base_shape = DisplayBaseShape::BASE_SHAPE_PLANE;
                        _surfaces[surface_idx] = surf_plane;
                    }
                    else if (shape == "cylinder")
                    {
                        surf_cylinder = new DisplayCylinderSurface();
                        surf_cylinder->base_shape = DisplayBaseShape::BASE_SHAPE_CYLINDER;
                        _surfaces[surface_idx] = surf_cylinder;
                    }
                    else
                    {
                        fprintf(stderr, "DisplayConfig> Error: config file format not recognized - Base Shape must be 'plane' or 'cylinder'\n");
                        return false;
                    }
                }
                else if (line.length() >= 8 && line.substr(0, 7) == "Width: " && surf_plane != nullptr)
                {
                    surf_plane->size[0] = std::stod(line.substr(7));
                }
                else if (line.length() >= 9 && line.substr(0, 8) == "Height: " && surf_plane != nullptr)
                {
                    surf_plane->size[1] = std::stod(line.substr(8));
                }
                else if (line.length() >= 9 && line.substr(0, 8) == "Center: " && surf_plane != nullptr)
                {
                    size_t comma1 = line.find(',');
                    size_t comma2 = line.find(',', comma1 + 1);
                    if (comma1 == std::string::npos || comma2 == std::string::npos)
                    {
                        fprintf(stderr, "OmniSurface> Error: config file format not recognized - Center expexts 3 comma separated numbers\n");
                        return false;
                    }
                    surf_plane->center[0] = std::stod(line.substr(8, comma1));
                    surf_plane->center[1] = std::stod(line.substr(comma1 + 1, comma2 - comma1 - 1));
                    surf_plane->center[2] = std::stod(line.substr(comma2 + 1));
                }
                else if (line.length() >= 9 && line.substr(0, 8) == "Normal: " && surf_plane != nullptr)
                {
                    size_t comma1 = line.find(',');
                    size_t comma2 = line.find(',', comma1 + 1);
                    if (comma1 == std::string::npos || comma2 == std::string::npos)
                    {
                        fprintf(stderr, "OmniSurface> Error: config file format not recognized - Normal expexts 3 comma separated numbers\n");
                        return false;
                    }
                    surf_plane->normal[0] = std::stod(line.substr(8, comma1));
                    surf_plane->normal[1] = std::stod(line.substr(comma1 + 1, comma2 - comma1 - 1));
                    surf_plane->normal[2] = std::stod(line.substr(comma2 + 1));
                }
                else if (line.length() >= 9 && line.substr(0, 8) == "Radius: " && surf_cylinder != nullptr)
                {
                    surf_cylinder->radius = std::stod(line.substr(8));
                }
                else if (line.length() >= 16 && line.substr(0, 15) == "Bottom Height: " && surf_cylinder != nullptr)
                {
                    surf_cylinder->altitude[0] = std::stod(line.substr(15));
                }
                else if (line.length() >= 13 && line.substr(0, 12) == "Top Height: " && surf_cylinder != nullptr)
                {
                    surf_cylinder->altitude[1] = std::stod(line.substr(12));
                }
                else if (line.length() >= 13 && line.substr(0, 12) == "Left Angle: " && surf_cylinder != nullptr)
                {
                    surf_cylinder->sector[0] = std::stod(line.substr(12));
                }
                else if (line.length() >= 14 && line.substr(0, 13) == "Right Angle: " && surf_cylinder != nullptr)
                {
                    surf_cylinder->sector[1] = std::stod(line.substr(13));
                }
            }
            surface_idx++;
        }
        else if (line.length() >= 9 && line.substr(0, 8) == "Origin: ")
        {
            size_t comma1 = line.find(',');
            size_t comma2 = line.find(',', comma1 + 1);
            if (comma1 == std::string::npos || comma2 == std::string::npos)
            {
                fprintf(stderr, "DisplayConfig> Error: config file format not recognized - Normal expexts 3 comma separated numbers\n");
                return false;
            }
            _origin[0] = std::stod(line.substr(8, comma1));
            _origin[1] = std::stod(line.substr(comma1 + 1, comma2 - comma1 - 1));
            _origin[2] = std::stod(line.substr(comma2 + 1));
        }

        line0 = false;
    }

    config_file.close();
    return true;
}

int vk360::DisplayConfig::getMonitor()
{
    return _monitor_index;
}

uint32_t vk360::DisplayConfig::getSurfaceCount()
{
    return static_cast<uint32_t>(_surfaces.size());
}

void vk360::DisplayConfig::getSurface(uint32_t index, vk360::DisplaySurface** surf_ptr)
{
    if (index >= _surfaces.size())
    {
        *surf_ptr = nullptr;
    }
    else
    {
        *surf_ptr = _surfaces[index];
    }
}

void vk360::DisplayConfig::getOrigin(double* origin)
{
    origin[0] = _origin[0];
    origin[1] = _origin[1];
    origin[2] = _origin[2];
}

void vk360::DisplayConfig::printConfig()
{
    // TODO: maybe store 4 corners of planar surface instead of center and normal and size
    printf("********* OmniSurface Config *********\n");
    printf("Monitor: %u\n", _monitor_index);
    printf("Origin: (%.4f, %.4f, %.4f)\n", _origin[0], _origin[1], _origin[2]);
    printf("Display Surfaces:\n");
    for (uint32_t i = 0; i < _surfaces.size(); i++)
    {
        if (_surfaces[i] == nullptr) printf("WARNING: surface is NULL\n");
        else if (_surfaces[i]->base_shape == vk360::DisplayBaseShape::BASE_SHAPE_PLANE)
        {
            DisplayPlaneSurface* surf = reinterpret_cast<DisplayPlaneSurface*>(_surfaces[i]);
            printf("  Planar\n");
            printf("    size: %.4f x %.4f\n", surf->size[0], surf->size[1]);
            printf("    center: (%.4f, %.4f, %.4f)\n", surf->center[0], surf->center[1], surf->center[2]);
            printf("    normal: (%.4f, %.4f, %.4f)\n", surf->normal[0], surf->normal[1], surf->normal[2]);
        }
        else if (_surfaces[i]->base_shape == vk360::DisplayBaseShape::BASE_SHAPE_CYLINDER)
        {
            DisplayCylinderSurface* surf = reinterpret_cast<DisplayCylinderSurface*>(_surfaces[i]);
            printf("  Cylindrical\n");
            printf("    radius: %.4f\n", surf->radius);
            printf("    sector: %.4f - %.4f\n", surf->sector[0], surf->sector[1]);
            printf("    altitude: %.4f - %.4f\n", surf->altitude[0], surf->altitude[1]);
        }
    }
    printf("**************************************\n");
}


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
    createCommandPool(&_vk.pool);
    for (uint32_t i = 0; i < 2; i++)
    {
        VulkanRenderBuffer& buf = _vk.render_buffer[i];
        createCommandBuffer(&buf.cmd);
        createSyncObjects(&buf.sem_vk_available, &buf.sem_vk_finished, &buf.in_flight_fence);
        createExternalImage(_width, _height, _is_stereo ? 2 : 1, VK_FORMAT_R8G8B8A8_UNORM, &buf.image);

        VkSubmitInfo prime_submit{};
        prime_submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        prime_submit.signalSemaphoreCount = 1;
        prime_submit.pSignalSemaphores = &_vk.render_buffer[i].sem_vk_available.semaphore;
        VkResult r = vkQueueSubmit(_vk.queue, 1, &prime_submit, VK_NULL_HANDLE);
        if (r != VK_SUCCESS)
        {
            fprintf(stderr, "ERROR: %d\n", r);
        }
    }
    vkQueueWaitIdle(_vk.queue);

    _vk.render_buffer_index = 0;
}

vk360::Vulkan360::~Vulkan360()
{
    // Clean up
}

void vk360::Vulkan360::getExternalRenderBufferInfo(uint32_t index, ExternalImageInfo* ext_img)
{
    ext_img->external_handle = _vk.render_buffer[index].image.external_handle;
    ext_img->memory_size = _vk.render_buffer[index].image.vk_img_data.mem_size;
    ext_img->width = _vk.render_buffer[index].image.vk_img_data.width;
    ext_img->height = _vk.render_buffer[index].image.vk_img_data.height;
    ext_img->layers = _vk.render_buffer[index].image.vk_img_data.layers;
}

#if defined(_WIN32)
void vk360::Vulkan360::getExternalSignalAvailableSemaphoreHandle(uint32_t index, HANDLE* ext_handle)
#elif defined(__linux__)
void vk360::Vulkan360::getExternalSignalAvailableSemaphoreHandle(uint32_t index, int* ext_handle)
#endif
{
    *ext_handle = _vk.render_buffer[index].sem_vk_available.external_handle;
}

#if defined(_WIN32)
void vk360::Vulkan360::getExternalWaitFinishedSemaphoreHandle(uint32_t index, HANDLE* ext_handle)
#elif defined(__linux__)
void vk360::Vulkan360::getExternalWaitFinishedSemaphoreHandle(uint32_t index, int* ext_handle)
#endif
{
    *ext_handle = _vk.render_buffer[index].sem_vk_finished.external_handle;
}

int vk360::Vulkan360::drawFrame()
{
    // Get back buffer - resources to render image into
    VulkanRenderBuffer& buf = _vk.render_buffer[_vk.render_buffer_index];

    // Wait for prior frame resources to be available to overwrite
    vkWaitForFences(_vk.device, 1, &buf.in_flight_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(_vk.device, 1, &buf.in_flight_fence);

    // Reset command buffer
    vkResetCommandBuffer(buf.cmd, 0);

    // Record draw commands
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(buf.cmd, &begin_info) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to begin recording command buffer\n");
    }

    //
    // TEST - clear each eye a different color
    //
    VkImageMemoryBarrier barrier_to_clear{};
    barrier_to_clear.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier_to_clear.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier_to_clear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier_to_clear.srcAccessMask = 0;
    barrier_to_clear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier_to_clear.image = buf.image.vk_img_data.image;
    barrier_to_clear.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier_to_clear.subresourceRange.baseMipLevel = 0;
    barrier_to_clear.subresourceRange.levelCount = 1;
    barrier_to_clear.subresourceRange.baseArrayLayer = 0;
    barrier_to_clear.subresourceRange.layerCount = buf.image.vk_img_data.layers;

    vkCmdPipelineBarrier(buf.cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier_to_clear);

    VkClearColorValue clear_color_left = { {0.2f, 0.3f, 0.5f, 1.0f} };
    VkClearColorValue clear_color_right = { {0.6f, 0.2f, 0.2f, 1.0f} };

    VkImageSubresourceRange range_left{};
    range_left.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range_left.baseMipLevel = 0;
    range_left.levelCount = 1;
    range_left.baseArrayLayer = 0;
    range_left.layerCount = 1;

    vkCmdClearColorImage(buf.cmd, buf.image.vk_img_data.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color_left, 1, &range_left);

    if (buf.image.vk_img_data.layers > 1)
    {
        VkImageSubresourceRange range_right{};
        range_right.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range_right.baseMipLevel = 0;
        range_right.levelCount = 1;
        range_right.baseArrayLayer = 1;
        range_right.layerCount = 1;

        vkCmdClearColorImage(buf.cmd, buf.image.vk_img_data.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color_right, 1, &range_right);
    }

    VkImageMemoryBarrier barrier_to_present = barrier_to_clear;
    barrier_to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier_to_present.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier_to_present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier_to_present.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    vkCmdPipelineBarrier(buf.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier_to_present);

    if (vkEndCommandBuffer(buf.cmd) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to record command buffer!");
    }

    //
    // End: TEST
    //


    // Submit your recorded drawing commands
    VkSemaphore wait_semaphores[] = { buf.sem_vk_available.semaphore };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
    VkSemaphore signal_semaphores[] = { buf.sem_vk_finished.semaphore };

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &buf.cmd;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    if (vkQueueSubmit(_vk.queue, 1, &submit_info, buf.in_flight_fence)  != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to submit draw command buffer\n");
    }

    int render_buf_idx = _vk.render_buffer_index;
    _vk.render_buffer_index = 1 - _vk.render_buffer_index; // toggle between 0 and 1

    return render_buf_idx;
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

void vk360::Vulkan360::createCommandPool(VkCommandPool* pool_ptr)
{
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = _vk.q_family_index;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(_vk.device, &pool_info, nullptr, pool_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: could not create `vkCommandPool`\n");
        *pool_ptr = VK_NULL_HANDLE;
        return;
    }
}

void vk360::Vulkan360::createCommandBuffer(VkCommandBuffer* cmd_ptr)
{
    VkCommandBufferAllocateInfo cmd_alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cmd_alloc_info.commandPool = _vk.pool;
    cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_alloc_info.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(_vk.device, &cmd_alloc_info, cmd_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: could not allocate `vkCommandBuffer`\n");
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

    transitionImageLayoutToGeneral(ext_img->vk_img_data.image, ext_img->vk_img_data.format, ext_img->vk_img_data.layers);
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

void vk360::Vulkan360::transitionImageLayoutToGeneral(VkImage image, VkFormat format, uint32_t layers)
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

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect_flags;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layers;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);

    // Submit and wait (blocking call for setup only)
    VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    vkQueueSubmit(_vk.queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(_vk.queue);

    vkFreeCommandBuffers(_vk.device, _vk.pool, 1, &cmd);
}
