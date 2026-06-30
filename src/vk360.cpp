#include "vk360.h"

#include <iostream>
#include <cstdint>
#include <cstring>
#include <string>
#include <fstream>
#include <algorithm>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data)
{
    // Filter out unwanted messages (e.g., only show warnings and errors)
    if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        fprintf(stderr, "[VULKAN VALIDATION] %s\n", callback_data->pMessage);
    }

    // Return VK_FALSE - returning VK_TRUE aborts the API call with an error
    return VK_FALSE;
}


Vulkan360::Vulkan360(const char** exts, uint32_t ext_count, void* w_handle, void* m_handle) :
    _is_stereo(false)
{
    createVulkanInstance(exts, ext_count, &_vk.instance);
    createVulkanSurface(w_handle, m_handle, &_vk.surface);
    findPhysicalDevice(&_vk.physical_device, &_vk.q_family_index);
    // TODO: check for stereo capability???
    createVulkanDeviceAndQueue(&_vk.device, &_vk.queue);
    //createSwapChain(&_vk.swapchain, &_vk.swapchain_images);
    createCommandPoolAndBuffer(&_vk.pool, &_vk.cmd);
    //createSyncObjects(&_vk.img_available, &_vk.img_finished, &_vk.in_flight);
}

Vulkan360::~Vulkan360()
{
    // Clean up
}

int Vulkan360::initializeWindow(const char* title, const char* default_image)
{
    ////createFullscreenWindow(title, &_window);
    //createVulkanInstance(&_vk.instance);
    //createVulkanSurface(&_vk.surface);
    //findPhysicalDevice(&_vk.physical_device, &_vk.q_family_index);

    //_is_stereo = hasStereo3dCapability();
    //printf("Vulkan360> Info: creating %s window\n", _is_stereo ? "Stereo 3D" : "standard");

    //createVulkanDeviceAndQueue(&_vk.device, &_vk.queue);
    //createSwapChain(&_vk.swapchain, &_vk.swapchain_images);
    //createCommandPoolAndBuffer(&_vk.pool, &_vk.cmd);
    //createSyncObjects(&_vk.img_available, &_vk.img_finished, &_vk.in_flight);

    //int w, h, ch;
    //uint8_t* pixels = stbi_load(default_image, &w, &h, &ch, 4);
    //printf("Read in image: %dx%d, %p\n", w, h, pixels);

    //// TODO: return -1 if any of the above fails

    return 0;
}

uint32_t Vulkan360::getRenderBufferIndex()
{
    // Wait for the previous frame to finish on the GPU
    vkWaitForFences(_vk.device, 1, &_vk.in_flight, VK_TRUE, UINT64_MAX);
    vkResetFences(_vk.device, 1, &_vk.in_flight);

    // Acquire the next available image from the swapchain
    uint32_t img_idx;
    vkAcquireNextImageKHR(_vk.device, _vk.swapchain, UINT64_MAX, _vk.img_available, VK_NULL_HANDLE, &img_idx);

    return img_idx;
}

void Vulkan360::drawFrame(uint32_t buffer_idx)
{
    // Record draw commands
    VkImage current_image = _vk.swapchain_images[buffer_idx];

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
    barrierToClear.image = current_image;
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

    vkCmdClearColorImage(_vk.cmd, current_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

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
    VkSemaphore wait_semaphores[] = { _vk.img_available };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signal_semaphores[] = { _vk.img_finished };

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

void Vulkan360::swapBuffers(uint32_t buffer_idx)
{
    // Present rendered image on swap chain
    VkSwapchainKHR swapchains[] = { _vk.swapchain };
    VkSemaphore signal_semaphores[] = { _vk.img_finished };

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &buffer_idx;
    present_info.pResults = nullptr;

    vkQueuePresentKHR(_vk.queue, &present_info);
}

//bool Vulkan360::shouldClose()
//{
//    return glfwWindowShouldClose(_window);
//}
//
//void Vulkan360::pollEvents()
//{
//    glfwPollEvents();
//
//    if (glfwGetKey(_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
//    {
//        glfwSetWindowShouldClose(_window, true);
//    }
//}

///////////////////////////////////////////////////////////////
//   PRIVATE METHODS                                         //
///////////////////////////////////////////////////////////////

//void Vulkan360::readDisplayConfig(const char* config_filename)
//{
//    _config.load_success = true;
//
//    std::ifstream config_file(config_filename);
//    if (!config_file.is_open())
//    {
//        fprintf(stderr, "Vulkan360> Error: config file '%s' not found'\n", config_filename);
//        return;
//    }
//
//    std::string line;
//    int lineno = 0;
//    while (std::getline(config_file, line))
//    {
//        if (lineno == 0 && line != "OmniSurface Config")
//        {
//            fprintf(stderr, "Vulkan360> Error: config file format not recognized - 1st line should be 'OmniSurface Config'\n");
//            _config.load_success = false;
//        }
//        else if (line.length() >= 13 && line.substr(0, 12) == "Base Shape: ")
//        {
//            std::string shape = line.substr(12);
//            if (shape == "plane")
//            {
//                _config.base_shape = DisplayBaseShape::BASE_SHAPE_PLANE;
//            }
//            else if (shape == "cylinder")
//            {
//                _config.base_shape = DisplayBaseShape::BASE_SHAPE_CYLINDER;
//            }
//            else
//            {
//                fprintf(stderr, "Vulkan360> Error: config file format not recognized - Base Shape must be 'plane' or 'cylinder'\n");
//                _config.load_success = false;
//            }
//        }
//        else if (line.length() >= 9 && line.substr(0, 8) == "Facets: ")
//        {
//            _config.facets = std::stoul(line.substr(8));
//        }
//        else if (line.length() >= 9 && line.substr(0, 8) == "Radius: ")
//        {
//            _config.radius = std::stod(line.substr(8));
//        }
//        else if (line.length() >= 9 && line.substr(0, 8) == "Height: ")
//        {
//            _config.height = std::stod(line.substr(8));
//        }
//        else if (line.length() >= 13 && line.substr(0, 12) == "Resolution: ")
//        {
//            std::string size = line.substr(12);
//            size_t delim = size.find("x");
//            if (delim == std::string::npos)
//            {
//                fprintf(stderr, "Vulkan360> Error: config file format not recognized - Resolution should be WIDTHxHEIGHT\n");
//                _config.load_success = false;
//            }
//            else
//            {
//                _config.resolution_w = std::stoul(size.substr(0, delim));
//                _config.resolution_h = std::stoul(size.substr(delim + 1));
//            }
//        }
//
//        lineno++;
//    }
//
//    config_file.close();
//}
//
//void Vulkan360::createFullscreenWindow(const char* title, GLFWwindow** window_ptr)
//{
//    if (!glfwInit())
//    {
//        fprintf(stderr, "Vulkan360> Error: failed to initialize GLFW\n");
//        *window_ptr = nullptr;
//    }
//
//    // Prevent GLFW from implicitly creating an OpenGL context
//    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
//
//    // Get primary monitor
//    GLFWmonitor* primary_monitor = glfwGetPrimaryMonitor();
//    if (!primary_monitor)
//    {
//        fprintf(stderr, "Vulkan360> Error: failed to find primary monitor\n");
//        *window_ptr = nullptr;
//    }
//
//    // Get resolution of primary monitor
//    const GLFWvidmode* mode = glfwGetVideoMode(primary_monitor);
//    _window_w = mode->width;
//    _window_h = mode->height;
//
//    // Create fullscreen window
//    *window_ptr = glfwCreateWindow(_window_w, _window_h, title, primary_monitor, nullptr);
//    if (!(*window_ptr))
//    {
//        fprintf(stderr, "Vulkan360> Error: could not create `GLFWwindow`\n");
//        *window_ptr = nullptr;
//    }
//}

void Vulkan360::createVulkanInstance(const char** exts, uint32_t ext_count, VkInstance* instance_ptr)
{
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "OmniSurface";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Custom Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    bool has_debug_util_ext = false;
    bool has_physical_dev_prop2_ext = false;
    std::vector<const char*> extensions(exts, exts + ext_count);
    for (int i = 0; i < extensions.size(); i++)
    {
        if (strcmp(extensions[i], VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) has_debug_util_ext = true;
        if (strcmp(extensions[i], VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0) has_physical_dev_prop2_ext = true;
    }
    if (!has_debug_util_ext) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (!has_physical_dev_prop2_ext) extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

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

void Vulkan360::createVulkanSurface(void* w_handle, void* m_handle, VkSurfaceKHR* surface_ptr)
{
#if defined(_WIN32)
    HWND hwnd = (HWND)w_handle;
    HMODULE hmod = (HMODULE)m_handle;

    VkWin32SurfaceCreateInfoKHR create_surface_info{};
    create_surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    create_surface_info.hwnd = hwnd;
    create_surface_info.hinstance = hmod;

    if (vkCreateWin32SurfaceKHR(_vk.instance, &create_surface_info, nullptr, surface_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: could not create `VkSurfaceKHR`\n");
        *surface_ptr = VK_NULL_HANDLE;
    }

#elif defined(__linux__)
    Window x11_window = (Window)w_handle;
    Display* x11_display = (Display*)m_handle;

    VkXlibSurfaceCreateInfoKHR create_surface_info{};
    create_surface_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    create_surface_info.window = x11_window;
    create_surface_info.dpy = x11_display;

    if (vkCreateXlibSurfaceKHR(instance, &create_surface_info, nullptr, surface_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: could not create `VkSurfaceKHR`\n");
        *surface_ptr = VK_NULL_HANDLE;
    }
#else
    #error "Unsupported operating system"
#endif
}

void Vulkan360::findPhysicalDevice(VkPhysicalDevice* physical_device_ptr, int* q_fam_idx_ptr)
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
        VkPhysicalDeviceProperties2 props2{};
        props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        vkGetPhysicalDeviceProperties2(physical_devices[i], &props2);

        int q_fam_idx = findGraphicsComputeFamilyIndex(physical_devices[i]);
        if (props2.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && q_fam_idx >= 0)
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
    else
    {
        VkPhysicalDeviceProperties device_props;
        vkGetPhysicalDeviceProperties(*physical_device_ptr, &device_props);

        printf("Vulkan360> Info: Using GPU Device %s\n", device_props.deviceName);
    }
    if (*q_fam_idx_ptr < 0)
    {
        fprintf(stderr, "Vulkan360> Error: GPU device does not support graphics and compute shaders\n");
    }
}

int Vulkan360::findGraphicsComputeFamilyIndex(VkPhysicalDevice physical_device)
{
    int index = -1;
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());
    for (uint32_t i = 0; i < queue_family_count; i++)
    {
        VkBool32 present_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, _vk.surface, &present_support);
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT && present_support)
        {
            index = i;
            break;
        }
    }
    return index;
}

bool Vulkan360::hasStereo3dCapability()
{
    VkSurfaceCapabilitiesKHR surface_capabilities{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_vk.physical_device, _vk.surface, &surface_capabilities) != VK_SUCCESS)
    {
        return false;
    }

    if (surface_capabilities.maxImageArrayLayers < 2)
    {
        return false;
    }

    VkPhysicalDeviceMultiviewFeatures multiview_features{};
    multiview_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
    multiview_features.pNext = nullptr;

    VkPhysicalDeviceFeatures2 device_features2{};
    device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    device_features2.pNext = &multiview_features;

    vkGetPhysicalDeviceFeatures2(_vk.physical_device, &device_features2);

    printf("Physical Device: multiview=%d\n", multiview_features.multiview);
    if (multiview_features.multiview == VK_FALSE)
    {
        return false;
    }

    return true;
}

void Vulkan360::createVulkanDeviceAndQueue(VkDevice* device_ptr, VkQueue* queue_ptr)
{
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = _vk.q_family_index;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;

    VkPhysicalDeviceMultiviewFeatures multiview_features{};
    multiview_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
    multiview_features.multiview = VK_TRUE;

    const std::vector<const char*> device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;
    device_create_info.enabledExtensionCount = device_extensions.size();
    device_create_info.ppEnabledExtensionNames = device_extensions.data();
    device_create_info.pNext = _is_stereo ? &multiview_features : nullptr;
    if (vkCreateDevice(_vk.physical_device, &device_create_info, nullptr, device_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: could not create `vkDevice`\n");
        *device_ptr = VK_NULL_HANDLE;
        *queue_ptr = VK_NULL_HANDLE;
    }

    vkGetDeviceQueue(*device_ptr, _vk.q_family_index, 0, queue_ptr);
}

void Vulkan360::createSwapChain(VkSwapchainKHR* swapchain_ptr, std::vector<VkImage>* swapchain_images_ptr)
{
    VkSurfaceCapabilitiesKHR capabilities;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_vk.physical_device, _vk.surface, &capabilities) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to get physical device surface capabilities\n");
        *swapchain_ptr = VK_NULL_HANDLE;
    }

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(_vk.physical_device, _vk.surface, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(_vk.physical_device, _vk.surface, &format_count, formats.data());

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(_vk.physical_device, _vk.surface, &present_mode_count, nullptr);
    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(_vk.physical_device, _vk.surface, &present_mode_count, present_modes.data());

    VkSurfaceFormatKHR surface_format = chooseSwapSurfaceFormat(formats);
    //VkExtent2D extent = chooseSwapExtent(capabilities, _window_w, _window_h);
    VkExtent2D extent = chooseSwapExtent(capabilities, 1024, 768); // TODO: fix!

    uint32_t img_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && img_count > capabilities.maxImageCount)
    {
        img_count = capabilities.maxImageCount;
    }
    
    VkSwapchainCreateInfoKHR swapchain_create_info{};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = _vk.surface;
    swapchain_create_info.minImageCount = img_count;
    swapchain_create_info.imageFormat = surface_format.format;
    swapchain_create_info.imageColorSpace = surface_format.colorSpace;
    swapchain_create_info.imageExtent = extent;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.preTransform = capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;
    if (_is_stereo)
    {
        swapchain_create_info.imageArrayLayers = 2; // layer 0 = left eye, layer 1 = right eye
        swapchain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    }
    else
    {
        swapchain_create_info.imageArrayLayers = 1;
        VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;
        for (uint32_t i = 0; i < present_modes.size(); i++)
        {
            if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                best_mode = present_modes[i];
                break;
            }
        }
        swapchain_create_info.presentMode = best_mode;
    }

    if (vkCreateSwapchainKHR(_vk.device, &swapchain_create_info, nullptr, swapchain_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: could not create `VkSwapchainKHR`\n");
        *swapchain_ptr = VK_NULL_HANDLE;
    }

    uint32_t swapchain_img_count = 0;
    vkGetSwapchainImagesKHR(_vk.device, *swapchain_ptr, &swapchain_img_count, nullptr);
    swapchain_images_ptr->resize(swapchain_img_count);
    if (vkGetSwapchainImagesKHR(_vk.device, _vk.swapchain, &swapchain_img_count, swapchain_images_ptr->data()) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to get swapchain images\n");
    }
}

VkSurfaceFormatKHR Vulkan360::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats)
{
    for (uint32_t i = 0; i < available_formats.size(); i++)
    {
        if (available_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return available_formats[i];
        }
    }
    return available_formats[0];
}

VkExtent2D Vulkan360::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height)
{
    if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)())
    {
        return capabilities.currentExtent;
    }
    else
    {
        VkExtent2D actual_extent{};
        actual_extent.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actual_extent.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actual_extent;
    }
}

void Vulkan360::createCommandPoolAndBuffer(VkCommandPool* pool_ptr, VkCommandBuffer* cmd_ptr)
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
    }
}

void Vulkan360::createSyncObjects(VkSemaphore* img_available_ptr, VkSemaphore* img_finished_ptr, VkFence* in_flight_ptr)
{
    VkSemaphoreCreateInfo sem_create_info{};
    sem_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(_vk.device, &sem_create_info, nullptr, img_available_ptr) != VK_SUCCESS ||
        vkCreateSemaphore(_vk.device, &sem_create_info, nullptr, img_finished_ptr) != VK_SUCCESS ||
        vkCreateFence(_vk.device, &fence_create_info, nullptr, in_flight_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: failed to create synchronization objects\n");
        *img_available_ptr = VK_NULL_HANDLE;
        *img_finished_ptr = VK_NULL_HANDLE;
        *in_flight_ptr = VK_NULL_HANDLE;
    }
}
