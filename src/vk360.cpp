#include <iostream>
#include <cstdint>
#include <cstring>
#include <string>
#include <fstream>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "vk360.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


vk360::DisplayConfig::DisplayConfig() :
    _monitor_index{ 0 }, _tracking_type{ TrackingSystem::NONE }, _tracking_port{ 0 }, _tracking_camera_id{ 0 }, _tracking_controller_id{ 0 }
{
    _display_grid[0] = 1;
    _display_grid[1] = 1;
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
        else if (line.length() >= 18 && line.substr(0, 17) == "Camera Tracking: ")
        {
            size_t comma1 = line.find(',');
            size_t comma2 = line.find(',', comma1 + 1);
            size_t comma3 = line.find(',', comma2 + 1);
            if (comma1 == std::string::npos || comma2 == std::string::npos || comma3 == std::string::npos)
            {
                fprintf(stderr, "OmniSurface> Error: config file format not recognized - camera tracking expects 4 comma separated values\n");
                return false;
            }
            std::string tracking_type = line.substr(17, comma1 - 17);
            if (tracking_type == "DTrack") _tracking_type = TrackingSystem::DTRACK;
            else if (tracking_type == "VRPN") _tracking_type = TrackingSystem::VRPN;
            _tracking_port = std::stoul(line.substr(comma1 + 1, comma2 - comma1 - 1));
            _tracking_camera_id = std::stoul(line.substr(comma2 + 1, comma3 - comma2 - 1));
            _tracking_controller_id = std::stoul(line.substr(comma3 + 1));
        }
        else if (line.length() >= 10 && line.substr(0, 9) == "Monitor: ")
        {
            _monitor_index = std::stoi(line.substr(9));
        }
        else if (line.length() >= 16 && line.substr(0, 15) == "Surface Count: ")
        {
            size_t comma = line.find(',');
            if (comma == std::string::npos)
            {
                fprintf(stderr, "OmniSurface> Error: config file format not recognized - surface count expects 2 comma separated numbers\n");
                return false;
            }
            _display_grid[0] = std::stoul(line.substr(15, comma - 15));
            _display_grid[1] = std::stoul(line.substr(comma + 1));

            size_t surface_count = _display_grid[0] * _display_grid[1];
            if (surface_count > 32)
            {
                fprintf(stderr, "OmniSurface> Error: detected %zu display surfaces (exceeds max of 32)\n", surface_count);
                return false;
            }
            _surfaces.resize(surface_count);
        }
        else if (line.length() >= 1 && line.substr(0, 1) == "[")
        {
            if (surface_idx >= _surfaces.size())
            {
                fprintf(stderr, "DisplayConfig> Error: config file  read error - found more surfaces than declared count\n");
                return false;
            }
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
                        _surfaces[surface_idx] = new GLSLDisplaySurface();
                        _surfaces[surface_idx]->base_shape = 0;
                    }
                    else if (shape == "cylinder")
                    {
                        _surfaces[surface_idx] = new GLSLDisplaySurface();
                        _surfaces[surface_idx]->base_shape = 1;
                    }
                    else
                    {
                        fprintf(stderr, "DisplayConfig> Error: config file format not recognized - Base Shape must be 'plane' or 'cylinder'\n");
                        return false;
                    }
                }
                else if (line.length() >= 14 && line.substr(0, 13) == "Bottom Left: " && _surfaces[surface_idx]->base_shape == 0)
                {
                    size_t comma1 = line.find(',');
                    size_t comma2 = line.find(',', comma1 + 1);
                    if (comma1 == std::string::npos || comma2 == std::string::npos)
                    {
                        fprintf(stderr, "OmniSurface> Error: config file format not recognized - coordinate expects 3 comma separated numbers\n");
                        return false;
                    }
                    _surfaces[surface_idx]->d1[0] = std::stof(line.substr(13, comma1 - 13));
                    _surfaces[surface_idx]->d1[1] = std::stof(line.substr(comma1 + 1, comma2 - comma1 - 1));
                    _surfaces[surface_idx]->d1[2] = std::stof(line.substr(comma2 + 1));
                }
                else if (line.length() >= 15 && line.substr(0, 14) == "Bottom Right: " && _surfaces[surface_idx]->base_shape == 0)
                {
                    size_t comma1 = line.find(',');
                    size_t comma2 = line.find(',', comma1 + 1);
                    if (comma1 == std::string::npos || comma2 == std::string::npos)
                    {
                        fprintf(stderr, "OmniSurface> Error: config file format not recognized - coordinate expects 3 comma separated numbers\n");
                        return false;
                    }
                    _surfaces[surface_idx]->d2[0] = std::stof(line.substr(14, comma1 - 14));
                    _surfaces[surface_idx]->d2[1] = std::stof(line.substr(comma1 + 1, comma2 - comma1 - 1));
                    _surfaces[surface_idx]->d2[2] = std::stof(line.substr(comma2 + 1));
                }
                else if (line.length() >= 11 && line.substr(0, 10) == "Top Left: " && _surfaces[surface_idx]->base_shape == 0)
                {
                    size_t comma1 = line.find(',');
                    size_t comma2 = line.find(',', comma1 + 1);
                    if (comma1 == std::string::npos || comma2 == std::string::npos)
                    {
                        fprintf(stderr, "OmniSurface> Error: config file format not recognized - coordinate expects 3 comma separated numbers\n");
                        return false;
                    }
                    _surfaces[surface_idx]->d3[0] = std::stof(line.substr(10, comma1 - 10));
                    _surfaces[surface_idx]->d3[1] = std::stof(line.substr(comma1 + 1, comma2 - comma1 - 1));
                    _surfaces[surface_idx]->d3[2] = std::stof(line.substr(comma2 + 1));
                }
                else if (line.length() >= 9 && line.substr(0, 8) == "Radius: " && _surfaces[surface_idx]->base_shape == 1)
                {
                    _surfaces[surface_idx]->d1[0] = std::stof(line.substr(8));
                }
                else if (line.length() >= 16 && line.substr(0, 15) == "Bottom Height: " && _surfaces[surface_idx]->base_shape == 1)
                {
                    _surfaces[surface_idx]->d2[0] = std::stof(line.substr(15));
                }
                else if (line.length() >= 13 && line.substr(0, 12) == "Top Height: " && _surfaces[surface_idx]->base_shape == 1)
                {
                    _surfaces[surface_idx]->d2[1] = std::stof(line.substr(12));
                }
                else if (line.length() >= 13 && line.substr(0, 12) == "Left Angle: " && _surfaces[surface_idx]->base_shape == 1)
                {
                    _surfaces[surface_idx]->d3[0] = std::stof(line.substr(12)) * (M_PI / 180.0);
                }
                else if (line.length() >= 14 && line.substr(0, 13) == "Right Angle: " && _surfaces[surface_idx]->base_shape == 1)
                {
                    _surfaces[surface_idx]->d3[1] = std::stof(line.substr(13)) * (M_PI / 180.0);
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
                fprintf(stderr, "DisplayConfig> Error: config file format not recognized - coordinate expects 3 comma separated numbers\n");
                return false;
            }
            _origin[0] = std::stof(line.substr(8, comma1 - 8));
            _origin[1] = std::stof(line.substr(comma1 + 1, comma2 - comma1 - 1));
            _origin[2] = std::stof(line.substr(comma2 + 1));
        }

        line0 = false;
    }

    config_file.close();
    return true;
}

vk360::TrackingSystem vk360::DisplayConfig::getTrackingSystemType()
{
    return _tracking_type;
}

uint16_t vk360::DisplayConfig::getTrackingPort()
{
    return _tracking_port;
}

int vk360::DisplayConfig::getTrackingCameraId()
{
    return _tracking_camera_id;
}

int vk360::DisplayConfig::getTrackingControllerId()
{
    return _tracking_controller_id;
}

int vk360::DisplayConfig::getMonitor()
{
    return _monitor_index;
}

uint32_t vk360::DisplayConfig::getNumberOfDisplayColumns()
{
    return _display_grid[0];
}

uint32_t vk360::DisplayConfig::getNumberOfDisplayRows()
{
    return _display_grid[1];
}

uint32_t vk360::DisplayConfig::getSurfaceCount()
{
    return static_cast<uint32_t>(_surfaces.size());
}

void vk360::DisplayConfig::getSurface(uint32_t index, vk360::GLSLDisplaySurface** surf_ptr)
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

void vk360::DisplayConfig::getOrigin(float* origin)
{
    origin[0] = _origin[0];
    origin[1] = _origin[1];
    origin[2] = _origin[2];
}

void vk360::DisplayConfig::printConfig()
{
    // TODO: maybe store 4 corners of planar surface instead of center and normal and size
    printf("********* OmniSurface Config *********\n");
    if (_tracking_type == TrackingSystem::NONE)
    {
        printf("Camera Tracking: None\n");
    }
    else
    {
        std::string tracking_type = "unknown";
        if (_tracking_type == TrackingSystem::DTRACK) tracking_type = "DTrack";
        else if (_tracking_type == TrackingSystem::VRPN) tracking_type = "VRPN";
        printf("Camera Tracking: %s\n", tracking_type.c_str());
        printf("  Port: %hu\n", _tracking_port);
        printf("  Camera ID: %d\n", _tracking_camera_id);
        printf("  Controller ID: %d\n", _tracking_controller_id);
    }
    printf("Monitor: %u\n", _monitor_index);
    printf("Display Grid: %ux%u\n", _display_grid[0], _display_grid[1]);
    printf("Origin: (%.4f, %.4f, %.4f)\n", _origin[0], _origin[1], _origin[2]);
    printf("Display Surfaces:\n");
    for (uint32_t i = 0; i < _surfaces.size(); i++)
    {
        if (_surfaces[i] == nullptr)
        {
            printf("WARNING: surface is NULL\n");
        }
        else if (_surfaces[i]->base_shape == 0)
        {
            printf("  Planar\n");
            printf("    bottom left:  (%.4f, %.4f, %.4f)\n", _surfaces[i]->d1[0], _surfaces[i]->d1[1], _surfaces[i]->d1[2]);
            printf("    bottom right: (%.4f, %.4f, %.4f)\n", _surfaces[i]->d2[0], _surfaces[i]->d2[1], _surfaces[i]->d2[2]);
            printf("    top left:     (%.4f, %.4f, %.4f)\n", _surfaces[i]->d3[0], _surfaces[i]->d3[1], _surfaces[i]->d3[2]);
        }
        else if (_surfaces[i]->base_shape == 1)
        {
            printf("  Cylindrical\n");
            printf("    radius:   %.4f\n", _surfaces[i]->d1[0]);
            printf("    altitude: %.4f - %.4f\n", _surfaces[i]->d2[0], _surfaces[i]->d2[1]);
            printf("    sector:   %.4f - %.4f\n", _surfaces[i]->d3[0] * (180.0 / M_PI), _surfaces[i]->d3[1] * (180.0 / M_PI));
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


vk360::Vulkan360::Vulkan360(uint8_t* device_uuid, uint32_t width, uint32_t height, bool is_stereo, DisplayConfig* config) :
    _is_stereo{ is_stereo }, _config{ config }
{
    
    _camera_position[0] = 0.0f;
    _camera_position[0] = 1.8f;
    _camera_position[0] = 0.0f;
    _view_rotation[0] = 0.0f;
    _view_rotation[1] = 0.0f;
    _vk.extent.width = width;
    _vk.extent.height = height;
    VkFormat fbo_color_format = VK_FORMAT_R8G8B8A8_SRGB;
    createVulkanInstance(&_vk.instance);
    findPhysicalDevice(device_uuid, &_vk.physical_device, &_vk.q_family_index);
    createVulkanDeviceAndQueue(&_vk.device, &_vk.queue);
    createCommandPool(&_vk.pool);
    createRenderPass(fbo_color_format, &_vk.render_pass);
    createDescriptorSet(&_vk.desc_layout, &_vk.desc_pool, &_vk.desc);
    createUniformBufferObject(&_vk.ubo_buffer, &_vk.ubo_memory);
    for (uint32_t i = 0; i < 2; i++)
    {
        VulkanRenderBuffer& buf = _vk.render_buffer[i];
        createCommandBuffer(&buf.cmd);
        createSyncObjects(&buf.sem_vk_available, &buf.sem_vk_finished, &buf.in_flight_fence);
        createExternalImage(_vk.extent.width, _vk.extent.height, _is_stereo ? 2 : 1, fbo_color_format, &buf.image);
        createFramebuffer(buf.image.vk_img_data, &buf.framebuffer);

        VkSubmitInfo prime_submit{};
        prime_submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        prime_submit.signalSemaphoreCount = 1;
        prime_submit.pSignalSemaphores = &_vk.render_buffer[i].sem_vk_available.semaphore;
        if (vkQueueSubmit(_vk.queue, 1, &prime_submit, VK_NULL_HANDLE) != VK_SUCCESS)
        {
            fprintf(stderr, "Vulkan360> Error: failed to signal interop semaphore\n");
        }
    }
    vkQueueWaitIdle(_vk.queue);
    createGraphicsPipeline(&_vk.pipeline, &_vk.pipeline_layout);

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

void vk360::Vulkan360::loadImage(const char* path, bool is_stereo)
{
    // Free old texture (if exists)
    if (_vk.media360.image != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(_vk.device);

        vkDestroySampler(_vk.device, _vk.media360.sampler, nullptr);
        vkDestroyImageView(_vk.device, _vk.media360.view, nullptr);
        vkDestroyImage(_vk.device, _vk.media360.image, nullptr);
        vkFreeMemory(_vk.device, _vk.media360.memory, nullptr);
    }

    // Read image
    int w, h, ch;
    uint8_t* pixels = stbi_load(path, &w, &h, &ch, 4);

    // Create CPU buffer to store image pixels
    size_t image_size = w * h * 4;

    VkBufferCreateInfo buffer_create_info{};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = image_size;
    buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    if (vkCreateBuffer(_vk.device, &buffer_create_info, nullptr, &buffer) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to create pixel buffer\n");
        return;
    }

    VkMemoryRequirements buffer_mem_reqs;
    vkGetBufferMemoryRequirements(_vk.device, buffer, &buffer_mem_reqs);

    VkMemoryAllocateInfo buffer_alloc_info{};
    buffer_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    buffer_alloc_info.allocationSize = buffer_mem_reqs.size;
    buffer_alloc_info.memoryTypeIndex = findMemoryType(buffer_mem_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceMemory buffer_mem;
    vkAllocateMemory(_vk.device, &buffer_alloc_info, nullptr, &buffer_mem);
    vkBindBufferMemory(_vk.device, buffer, buffer_mem, 0);

    // Copy pixel data to CPU buffer
    void* data;
    vkMapMemory(_vk.device, buffer_mem, 0, image_size, 0, &data);
    std::memcpy(data, pixels, image_size);
    vkUnmapMemory(_vk.device, buffer_mem);

    // Free pixels from CPU memory
    stbi_image_free(pixels);

    // Create image
    VkImageCreateInfo image_create_info{};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.extent.width = w;
    image_create_info.extent.height = h;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.format = VK_FORMAT_R8G8B8A8_SRGB;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(_vk.device, &image_create_info, nullptr, &(_vk.media360.image)) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to create image\n");
        return;
    }

    // Allocate memory
    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(_vk.device, _vk.media360.image, &mem_reqs);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = findMemoryType(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vkAllocateMemory(_vk.device, &alloc_info, nullptr, &(_vk.media360.memory));
    vkBindImageMemory(_vk.device, _vk.media360.image, _vk.media360.memory, 0);

    // Transfer pixel buffer to GPU image
    VkCommandBufferAllocateInfo cmd_alloc_info{};
    cmd_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_alloc_info.commandPool = _vk.pool;
    cmd_alloc_info.commandBufferCount = 1;

    VkCommandBuffer upload_cmd;
    vkAllocateCommandBuffers(_vk.device, &cmd_alloc_info, &upload_cmd);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(upload_cmd, &begin_info);

    VkImageMemoryBarrier barrier_to_upload{};
    barrier_to_upload.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier_to_upload.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier_to_upload.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier_to_upload.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier_to_upload.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier_to_upload.srcAccessMask = 0;
    barrier_to_upload.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier_to_upload.image = _vk.media360.image;
    barrier_to_upload.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier_to_upload.subresourceRange.baseMipLevel = 0;
    barrier_to_upload.subresourceRange.levelCount = 1;
    barrier_to_upload.subresourceRange.baseArrayLayer = 0;
    barrier_to_upload.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(upload_cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier_to_upload);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1 };

    vkCmdCopyBufferToImage(upload_cmd, buffer, _vk.media360.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region);

    VkImageMemoryBarrier barrier_to_sample{};
    barrier_to_sample.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier_to_sample.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier_to_sample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier_to_sample.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier_to_sample.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier_to_sample.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier_to_sample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier_to_sample.image = _vk.media360.image;
    barrier_to_sample.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier_to_sample.subresourceRange.baseMipLevel = 0;
    barrier_to_sample.subresourceRange.levelCount = 1;
    barrier_to_sample.subresourceRange.baseArrayLayer = 0;
    barrier_to_sample.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(upload_cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier_to_sample);

    vkEndCommandBuffer(upload_cmd);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &upload_cmd;

    // Submit to your graphics or transfer queue and wait for it to finish
    vkQueueSubmit(_vk.queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(_vk.queue);

    // Free temp structures
    vkFreeCommandBuffers(_vk.device, _vk.pool, 1, &upload_cmd);
    vkDestroyBuffer(_vk.device, buffer, nullptr);
    vkFreeMemory(_vk.device, buffer_mem, nullptr);

    // Create image view
    VkImageViewCreateInfo view_create_info{};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.image = _vk.media360.image;
    view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_create_info.format = VK_FORMAT_R8G8B8A8_SRGB;
    view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = 1;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = 1;

    if (vkCreateImageView(_vk.device, &view_create_info, nullptr, &(_vk.media360.view)) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to create image view\n");
        return;
    }

    // Create texture sampler
    VkSamplerCreateInfo sampler_create_info{};
    sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_create_info.magFilter = VK_FILTER_LINEAR;
    sampler_create_info.minFilter = VK_FILTER_LINEAR;
    sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_create_info.anisotropyEnable = VK_FALSE;
    sampler_create_info.maxAnisotropy = 1.0f;
    sampler_create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_create_info.unnormalizedCoordinates = VK_FALSE;
    sampler_create_info.compareEnable = VK_FALSE;
    sampler_create_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler_create_info.mipLodBias = 0.0f;
    sampler_create_info.minLod = 0.0f;
    sampler_create_info.maxLod = 1.0f;

    if (vkCreateSampler(_vk.device, &sampler_create_info, nullptr, &(_vk.media360.sampler)) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to create texture sampler\n");
        return;
    }

    VkDescriptorImageInfo desc_image_info{};
    desc_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    desc_image_info.imageView = _vk.media360.view;
    desc_image_info.sampler = _vk.media360.sampler;

    VkDescriptorBufferInfo desc_buffer_info{};
    desc_buffer_info.buffer = _vk.ubo_buffer;
    desc_buffer_info.offset = 0;
    desc_buffer_info.range = sizeof(GLSLDisplayData);

    VkWriteDescriptorSet desc_writes[2]{};
    desc_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    desc_writes[0].dstSet = _vk.desc;
    desc_writes[0].dstBinding = 0; // layout(binding = 0)
    desc_writes[0].dstArrayElement = 0;
    desc_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_writes[0].descriptorCount = 1;
    desc_writes[0].pImageInfo = &desc_image_info;
    desc_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    desc_writes[1].dstSet = _vk.desc;
    desc_writes[1].dstBinding = 1; // layout(binding = 1)
    desc_writes[1].dstArrayElement = 0;
    desc_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_writes[1].descriptorCount = 1;
    desc_writes[1].pBufferInfo = &desc_buffer_info;

    vkUpdateDescriptorSets(_vk.device, 2, desc_writes, 0, nullptr);

    _vk.media360.stereo = is_stereo;
}

void vk360::Vulkan360::setCameraPosition(float x, float y, float z)
{
    _camera_position[0] = x;
    _camera_position[1] = y;
    _camera_position[2] = z;
}

void vk360::Vulkan360::setViewRotation(float yaw, float pitch)
{
    _view_rotation[0] = yaw;
    _view_rotation[1] = pitch;
}

int vk360::Vulkan360::drawFrame()
{
    // Get back buffer - resources to render image into
    VulkanRenderBuffer& buf = _vk.render_buffer[_vk.render_buffer_index];

    // Prepare command buffer
    beginCommandBuffer(buf);

    // Draw test screen (if no image loaded)
    if (_vk.media360.image == VK_NULL_HANDLE)
    {
        drawTestScreen(buf);
    }

    // Render scene
    else
    {
        // Run compute pass for indirect menu draw
        dipatchMenuCompute(buf);

        // Begin render pass
        beginRenderPass(buf);

        // Draw 360 image
        draw360Image(buf);

        // Draw 3D menu
        draw3dMenu(buf);

        // End render pass
        vkCmdEndRenderPass(buf.cmd);
    }

    // End command buffer and submit
    endCommandBufferAndSubmit(buf);

    // Swap buffers (toggle between 0 and 1)
    int render_buf_idx = _vk.render_buffer_index;
    _vk.render_buffer_index = 1 - _vk.render_buffer_index;

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

    VkPhysicalDeviceVulkan12Features vulkan12_features{};
    vulkan12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12_features.uniformBufferStandardLayout = VK_TRUE;
    vulkan12_features.scalarBlockLayout = VK_TRUE;
    vulkan12_features.pNext = &multiview_features;

    VkPhysicalDeviceFeatures2 device_features2{};
    device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    device_features2.pNext = &vulkan12_features;

    vkGetPhysicalDeviceFeatures2(_vk.physical_device, &device_features2);
    if (multiview_features.multiview == VK_FALSE)
    {
        fprintf(stderr, "Vulkan360> Error: Vulkan multiview not supported\n");
        *device_ptr = VK_NULL_HANDLE;
        *queue_ptr = VK_NULL_HANDLE;
        return;
    }

    if (vulkan12_features.uniformBufferStandardLayout == VK_FALSE || vulkan12_features.scalarBlockLayout == VK_FALSE)
    {
        fprintf(stderr, "Vulkan360> Error: Vulkan uniform std430/scalar layout not supported\n");
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
    device_create_info.pNext = &vulkan12_features;
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
    VkSemaphoreGetWin32HandleInfoKHR get_handle_info{};
    get_handle_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR;
    get_handle_info.semaphore = ext_sem->semaphore;
    get_handle_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;

    PFN_vkGetSemaphoreWin32HandleKHR vkGetSemaphoreWin32HandleKHR =
        (PFN_vkGetSemaphoreWin32HandleKHR)vkGetDeviceProcAddr(_vk.device, "vkGetSemaphoreWin32HandleKHR");
    vkGetSemaphoreWin32HandleKHR(_vk.device, &get_handle_info, &(ext_sem->external_handle));
#elif defined(__linux__)
    VkSemaphoreGetFdInfoKHR get_fd_info{};
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
    VkImageCreateInfo image_info{};
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
    image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    vkCreateImage(_vk.device, &image_info, nullptr, &(ext_img->vk_img_data.image));

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(_vk.device, ext_img->vk_img_data.image, &mem_reqs);

    // Define the Export info
    VkExportMemoryAllocateInfo export_info{};
    export_info.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
#if defined(_WIN32)
    export_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#elif defined(__linux__)
    export_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#else
    #error "Unsupported operating system"
#endif
    export_info.pNext = nullptr;

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = findMemoryType(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    alloc_info.pNext = &export_info;

    vkAllocateMemory(_vk.device, &alloc_info, nullptr, &(ext_img->vk_img_data.memory));
    vkBindImageMemory(_vk.device, ext_img->vk_img_data.image, ext_img->vk_img_data.memory, 0);

    // Extract the external handle
#if defined (_WIN32)
    ext_img->external_handle = nullptr;
    VkMemoryGetWin32HandleInfoKHR get_handle_info{};
    get_handle_info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
    get_handle_info.memory = ext_img->vk_img_data.memory;
    get_handle_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    PFN_vkGetMemoryWin32HandleKHR vkGetMemoryWin32HandleKHR =
        (PFN_vkGetMemoryWin32HandleKHR)vkGetDeviceProcAddr(_vk.device, "vkGetMemoryWin32HandleKHR");
    vkGetMemoryWin32HandleKHR(_vk.device, &get_handle_info, &(ext_img->external_handle));
#elif defined(__linux__)
    image->external_handle = -1;
    VkMemoryGetFdInfoKHR get_fd_info{};
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

    // Create image view
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = ext_img->vk_img_data.image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = layers;
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    if (vkCreateImageView(_vk.device, &view_info, nullptr, &(ext_img->vk_img_data.view)) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to create `VkImageView`\n");
    }
}

void vk360::Vulkan360::createFramebuffer(VulkanImageData& image, VkFramebuffer* framebuffer_ptr)
{
    VkFramebufferCreateInfo framebuffer_info{};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = _vk.render_pass;
    framebuffer_info.attachmentCount = 1;
    framebuffer_info.pAttachments = &image.view;
    framebuffer_info.width = _vk.extent.width;
    framebuffer_info.height = _vk.extent.height;
    framebuffer_info.layers = 1;

    if (vkCreateFramebuffer(_vk.device, &framebuffer_info, nullptr, framebuffer_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to create `VkFramebuffer`\n");
    }
}

void vk360::Vulkan360::createRenderPass(VkFormat color_format, VkRenderPass* render_pass_ptr)
{
    VkAttachmentDescription color_attachment{};
    color_attachment.format = color_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    void* multiview_ptr = nullptr;
    uint32_t view_mask = 0b11;
    uint32_t correlation_mask = 0b11;
    VkRenderPassMultiviewCreateInfo multiview_info{};
    if (_is_stereo)
    {
        multiview_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
        multiview_info.subpassCount = 1;
        multiview_info.pViewMasks = &view_mask;
        multiview_info.correlationMaskCount = 1;
        multiview_info.pCorrelationMasks = &correlation_mask;
        multiview_ptr = &multiview_info;
    }

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;
    render_pass_info.pNext = multiview_ptr;

    if (vkCreateRenderPass(_vk.device, &render_pass_info, nullptr, render_pass_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to create `VkRenderPass`\n");
        render_pass_ptr = nullptr;
    }
}

void vk360::Vulkan360::createDescriptorSet(VkDescriptorSetLayout* desc_layout_ptr, VkDescriptorPool* desc_pool_ptr, VkDescriptorSet* desc_ptr)
{
    // Create descriptor set layout
    VkDescriptorSetLayoutBinding sampler_layout_binding{};
    sampler_layout_binding.binding = 0;
    sampler_layout_binding.descriptorCount = 1;
    sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler_layout_binding.pImmutableSamplers = nullptr;
    sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding ubo_layout_binding{};
    ubo_layout_binding.binding = 1;
    ubo_layout_binding.descriptorCount = 1;
    ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_layout_binding.pImmutableSamplers = nullptr;
    ubo_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding bindings[] = { sampler_layout_binding, ubo_layout_binding };

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 2;
    layout_info.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(_vk.device, &layout_info, nullptr, desc_layout_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to create `VkDescriptorSetLayout`\n");
        desc_layout_ptr = nullptr;
        desc_pool_ptr = nullptr;
        return;
    }

    // Create descriptor set pool
    VkDescriptorPoolSize pool_sizes[2]{};
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[0].descriptorCount = 2;
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[1].descriptorCount = 2;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.poolSizeCount = 2;
    pool_info.pPoolSizes = pool_sizes;
    pool_info.maxSets = 2;

    if (vkCreateDescriptorPool(_vk.device, &pool_info, nullptr, desc_pool_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to create `VkDescriptorPool`\n");
        desc_layout_ptr = nullptr;
        desc_pool_ptr = nullptr;
        return;
    }

    // Create descriptor set
    VkDescriptorSetAllocateInfo desc_alloc_info{};
    desc_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    desc_alloc_info.descriptorPool = _vk.desc_pool;
    desc_alloc_info.descriptorSetCount = 1;
    desc_alloc_info.pSetLayouts = &_vk.desc_layout;

    if (vkAllocateDescriptorSets(_vk.device, &desc_alloc_info, desc_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to allocate descriptor set\n");
        return;
    }
}

void vk360::Vulkan360::createUniformBufferObject(VkBuffer* ubo_ptr, VkDeviceMemory* ubo_memory_ptr)
{
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = sizeof(GLSLDisplayData);
    buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; 
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(_vk.device, &buffer_info, nullptr, ubo_ptr) != VK_SUCCESS) {
        fprintf(stderr, "Vulkan360> Error: Failed to create `VkBuffer`\n");
        ubo_ptr = nullptr;
        ubo_memory_ptr = nullptr;
        return;
    }

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(_vk.device, *ubo_ptr, &mem_reqs);

    VkMemoryAllocateInfo buffer_alloc_info{};
    buffer_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    buffer_alloc_info.allocationSize = mem_reqs.size;
    buffer_alloc_info.memoryTypeIndex = findMemoryType(mem_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkAllocateMemory(_vk.device, &buffer_alloc_info, nullptr, ubo_memory_ptr);
    vkBindBufferMemory(_vk.device, *ubo_ptr, *ubo_memory_ptr, 0);

    // Populate UBO based on config file data
    _vk.display_data_ubo.grid_dims[0] = _config->getNumberOfDisplayColumns();
    _vk.display_data_ubo.grid_dims[1] = _config->getNumberOfDisplayRows();
    _vk.display_data_ubo.resolution[0] = static_cast<float>(_vk.extent.width);
    _vk.display_data_ubo.resolution[1] = static_cast<float>(_vk.extent.height);
    GLSLDisplaySurface* surf_ptr;
    for (uint32_t i = 0; i < _config->getSurfaceCount(); i++)
    {
        _config->getSurface(i, &surf_ptr);
        memcpy(&(_vk.display_data_ubo.surfaces[i]), surf_ptr, sizeof(GLSLDisplaySurface));
    }

    void* mapped_data;
    vkMapMemory(_vk.device, *ubo_memory_ptr, 0, sizeof(GLSLDisplayData), 0, &mapped_data);
    memcpy(mapped_data, &_vk.display_data_ubo, sizeof(GLSLDisplayData));
    vkUnmapMemory(_vk.device, *ubo_memory_ptr);
}

void vk360::Vulkan360::createGraphicsPipeline(VkPipeline* pipeline_ptr, VkPipelineLayout* pipeline_layout_ptr)
{
    // Create Pipeline Layout
    VkPushConstantRange push_constant_range{};
    push_constant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(PushConst360);

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &_vk.desc_layout;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;

    if (vkCreatePipelineLayout(_vk.device, &pipeline_layout_info, nullptr, pipeline_layout_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to create `VkPipelineLayout`\n");
        pipeline_layout_ptr = nullptr;
        pipeline_ptr = nullptr;
        return;
    }

    // Load vertex and fragment shaders
    VkShaderModule vertex_shader, fragment_shader;
    loadShaderModule("resrc/shaders/equirect_nonplanar_vert.glsl.spv", &vertex_shader);
    loadShaderModule("resrc/shaders/equirect_nonplanar_frag.glsl.spv", &fragment_shader);

    // Create Pipeline
    VkPipelineShaderStageCreateInfo shader_stage_info[2]{};
    shader_stage_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stage_info[0].module = vertex_shader;
    shader_stage_info[0].pName = "main";

    shader_stage_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stage_info[1].module = fragment_shader;
    shader_stage_info[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(_vk.extent.width), static_cast<float>(_vk.extent.height), 0.0f, 1.0f };
    VkRect2D scissor{ {0, 0}, _vk.extent };

    VkPipelineViewportStateCreateInfo viewport_state_info{};
    viewport_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_info.viewportCount = 1;
    viewport_state_info.pViewports = &viewport;
    viewport_state_info.scissorCount = 1;
    viewport_state_info.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer_info{};
    rasterizer_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer_info.depthClampEnable = VK_FALSE;
    rasterizer_info.rasterizerDiscardEnable = VK_FALSE;
    rasterizer_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer_info.lineWidth = 1.0f;
    rasterizer_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling_info{};
    multisampling_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling_info.sampleShadingEnable = VK_FALSE;
    multisampling_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blend_info{};
    color_blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_info.attachmentCount = 1;
    color_blend_info.pAttachments = &color_blend_attachment;

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stage_info;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state_info;
    pipeline_info.pRasterizationState = &rasterizer_info;
    pipeline_info.pMultisampleState = &multisampling_info;
    pipeline_info.pColorBlendState = &color_blend_info;
    pipeline_info.layout = *pipeline_layout_ptr;
    pipeline_info.renderPass = _vk.render_pass;
    pipeline_info.subpass = 0;

    if (vkCreateGraphicsPipelines(_vk.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, pipeline_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to create `VkPipeline`\n");
        pipeline_layout_ptr = nullptr;
        pipeline_ptr = nullptr;
        return;
    }

    vkDestroyShaderModule(_vk.device, vertex_shader, nullptr);
    vkDestroyShaderModule(_vk.device, fragment_shader, nullptr);
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

void vk360::Vulkan360::loadShaderModule(const char* path, VkShaderModule* shader_ptr)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        fprintf(stderr, "Vulkan360> Error: Failed to open shader file '%s'\n", path);
        *shader_ptr = VK_NULL_HANDLE;
        return;
    }

    size_t file_size = (size_t)file.tellg();
    std::vector<char> buffer(file_size);
    file.seekg(0);
    file.read(buffer.data(), file_size);
    file.close();

    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = buffer.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    if (vkCreateShaderModule(_vk.device, &create_info, nullptr, shader_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: Failed to create `VkShaderModule`\n");
        *shader_ptr = VK_NULL_HANDLE;
        return;
    }
}

void vk360::Vulkan360::beginCommandBuffer(VulkanRenderBuffer& rb)
{
    // Wait for prior frame resources to be available to overwrite
    vkWaitForFences(_vk.device, 1, &rb.in_flight_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(_vk.device, 1, &rb.in_flight_fence);

    // Reset command buffer
    vkResetCommandBuffer(rb.cmd, 0);

    // Record draw commands
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(rb.cmd, &begin_info) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to begin recording command buffer\n");
    }
}

void vk360::Vulkan360::dipatchMenuCompute(VulkanRenderBuffer& rb)
{

}

void vk360::Vulkan360::beginRenderPass(VulkanRenderBuffer& rb)
{
    // Configure the render pass execution
    VkClearValue clear_color = { {{0.0f, 0.0f, 0.0f, 1.0f}} };

    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = _vk.render_pass;
    render_pass_info.framebuffer = rb.framebuffer;
    render_pass_info.renderArea.offset = { 0, 0 };
    render_pass_info.renderArea.extent = _vk.extent;
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;

    // Start the render pass
    vkCmdBeginRenderPass(rb.cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
}

void vk360::Vulkan360::drawTestScreen(VulkanRenderBuffer& rb)
{
    // Render (clear each eye a different color)
    VkImageMemoryBarrier barrier_to_clear{};
    barrier_to_clear.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier_to_clear.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier_to_clear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier_to_clear.srcAccessMask = 0;
    barrier_to_clear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier_to_clear.image = rb.image.vk_img_data.image;
    barrier_to_clear.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier_to_clear.subresourceRange.baseMipLevel = 0;
    barrier_to_clear.subresourceRange.levelCount = 1;
    barrier_to_clear.subresourceRange.baseArrayLayer = 0;
    barrier_to_clear.subresourceRange.layerCount = rb.image.vk_img_data.layers;

    vkCmdPipelineBarrier(rb.cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier_to_clear);

    VkClearColorValue clear_color_left = { {0.2f, 0.3f, 0.6f, 1.0f} };
    VkClearColorValue clear_color_right = { {0.8f, 0.2f, 0.2f, 1.0f} };

    VkImageSubresourceRange range_left{};
    range_left.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range_left.baseMipLevel = 0;
    range_left.levelCount = 1;
    range_left.baseArrayLayer = 0;
    range_left.layerCount = 1;

    vkCmdClearColorImage(rb.cmd, rb.image.vk_img_data.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color_left, 1, &range_left);

    if (rb.image.vk_img_data.layers > 1)
    {
        VkImageSubresourceRange range_right{};
        range_right.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range_right.baseMipLevel = 0;
        range_right.levelCount = 1;
        range_right.baseArrayLayer = 1;
        range_right.layerCount = 1;

        vkCmdClearColorImage(rb.cmd, rb.image.vk_img_data.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color_right, 1, &range_right);
    }

    VkImageMemoryBarrier barrier_to_present = barrier_to_clear;
    barrier_to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier_to_present.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier_to_present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier_to_present.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    vkCmdPipelineBarrier(rb.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier_to_present);
}

void vk360::Vulkan360::draw360Image(VulkanRenderBuffer& rb)
{
    // Bind graphics pipeline
    vkCmdBindPipeline(rb.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _vk.pipeline);

    // Draw fullscreen quad
    PushConst360 pc{
        {_camera_position[0], _camera_position[1], _camera_position[2]},
        {_view_rotation[0], _view_rotation[1]},
        static_cast<uint32_t>(_vk.media360.stereo)
    };
    vkCmdBindDescriptorSets(rb.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _vk.pipeline_layout,
        0, 1, &_vk.desc, 0, nullptr);
    vkCmdPushConstants(rb.cmd, _vk.pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConst360), &pc);

    vkCmdDraw(rb.cmd, 6, 1, 0, 0);
}

void vk360::Vulkan360::draw3dMenu(VulkanRenderBuffer& rb)
{

}

void vk360::Vulkan360::endCommandBufferAndSubmit(VulkanRenderBuffer& rb)
{
    // End command buffer
    if (vkEndCommandBuffer(rb.cmd) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to record command buffer\n");
    }

    // Submit recorded commands
    VkSemaphore wait_semaphores[] = { rb.sem_vk_available.semaphore };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
    VkSemaphore signal_semaphores[] = { rb.sem_vk_finished.semaphore };

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &rb.cmd;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    if (vkQueueSubmit(_vk.queue, 1, &submit_info, rb.in_flight_fence) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to submit draw command buffer\n");
    }
}
