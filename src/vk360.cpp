#include <iostream>
#include <cstdint>
#include <cstring>
#include <string>
#include <fstream>
#include <algorithm>

#include "vk360.h"


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


Vulkan360::Vulkan360(const char* config_filename)
{
    readDisplayConfig(config_filename);
}

Vulkan360::~Vulkan360()
{
    // Clean up
}

bool Vulkan360::hasValidDisplayConfig()
{
    return _config.load_success;
}

int Vulkan360::initializeWindow(const char* title)
{
    createFullscreenWindow(title, &_window);
    createVulkanInstance(&_vk.instance);
    createVulkanSurface(&_vk.surface);
    findPhysicalDevice(&_vk.physical_device, &_vk.q_family_index);
    _is_stereo = hasStereo3dCapability();
    createVulkanDeviceAndQueue(&_vk.device, &_vk.queue);
    createSwapChain(&_vk.swapchain);

    // TESTING
    while (!glfwWindowShouldClose(_window))
    {
        glfwPollEvents();

        if (glfwGetKey(_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(_window, true);
        }

        _vk.swapchain
        //_native_renderer->swapBuffers();
    }

    return 0;
}

///////////////////////////////////////////////////////////////
//   PRIVATE METHODS                                         //
///////////////////////////////////////////////////////////////

void Vulkan360::readDisplayConfig(const char* config_filename)
{
    _config.load_success = true;

    std::ifstream config_file(config_filename);
    if (!config_file.is_open())
    {
        fprintf(stderr, "Vulkan360> Error: config file '%s' not found'\n", config_filename);
        return;
    }

    std::string line;
    int lineno = 0;
    while (std::getline(config_file, line))
    {
        if (lineno == 0 && line != "OmniSurface Config")
        {
            fprintf(stderr, "Vulkan360> Error: config file format not recognized - 1st line should be 'OmniSurface Config'\n");
            _config.load_success = false;
        }
        else if (line.length() >= 13 && line.substr(0, 12) == "Base Shape: ")
        {
            std::string shape = line.substr(12);
            if (shape == "plane")
            {
                _config.base_shape = DisplayBaseShape::BASE_SHAPE_PLANE;
            }
            else if (shape == "cylinder")
            {
                _config.base_shape = DisplayBaseShape::BASE_SHAPE_CYLINDER;
            }
            else
            {
                fprintf(stderr, "Vulkan360> Error: config file format not recognized - Base Shape must be 'plane' or 'cylinder'\n");
                _config.load_success = false;
            }
        }
        else if (line.length() >= 9 && line.substr(0, 8) == "Facets: ")
        {
            _config.facets = std::stoul(line.substr(8));
        }
        else if (line.length() >= 9 && line.substr(0, 8) == "Radius: ")
        {
            _config.radius = std::stod(line.substr(8));
        }
        else if (line.length() >= 9 && line.substr(0, 8) == "Height: ")
        {
            _config.height = std::stod(line.substr(8));
        }
        else if (line.length() >= 13 && line.substr(0, 12) == "Resolution: ")
        {
            std::string size = line.substr(12);
            size_t delim = size.find("x");
            if (delim == std::string::npos)
            {
                fprintf(stderr, "Vulkan360> Error: config file format not recognized - Resolution should be WIDTHxHEIGHT\n");
                _config.load_success = false;
            }
            else
            {
                _config.resolution_w = std::stoul(size.substr(0, delim));
                _config.resolution_h = std::stoul(size.substr(delim + 1));
            }
        }

        lineno++;
    }

    config_file.close();
}

void Vulkan360::createFullscreenWindow(const char* title, GLFWwindow** window_ptr)
{
    if (!glfwInit())
    {
        fprintf(stderr, "Vulkan360> Error: failed to initialize GLFW\n");
        *window_ptr = nullptr;
    }

    // Prevent GLFW from implicitly creating an OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // Get primary monitor
    GLFWmonitor* primary_monitor = glfwGetPrimaryMonitor();
    if (!primary_monitor)
    {
        fprintf(stderr, "Vulkan360> Error: failed to find primary monitor\n");
        *window_ptr = nullptr;
    }

    // Get resolution of primary monitor
    const GLFWvidmode* mode = glfwGetVideoMode(primary_monitor);
    _window_w = mode->width;
    _window_h = mode->height;

    // Create fullscreen window
    *window_ptr = glfwCreateWindow(_window_w, _window_h, title, primary_monitor, nullptr);
    if (!(*window_ptr))
    {
        fprintf(stderr, "Vulkan360> Error: could not create `GLFWwindow`\n");
        *window_ptr = nullptr;
    }
}

void Vulkan360::createVulkanInstance(VkInstance* instance_ptr)
{
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "OmniSurface";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Custom Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    uint32_t glfw_ext_count = 0;
    bool has_debug_util_ext = false;
    bool has_physical_dev_prop2_ext = false;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
    std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_ext_count);
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

void Vulkan360::createVulkanSurface(VkSurfaceKHR* surface_ptr)
{
    if (glfwCreateWindowSurface(_vk.instance, _window, nullptr, surface_ptr) != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan360> Error: could not create `VkSurfaceKHR`\n");
        *surface_ptr = VK_NULL_HANDLE;
    }
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
        VkPhysicalDeviceProperties2 props2 = {};
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
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
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

    printf("Surface: maxImageArrayLayers=%u\n", surface_capabilities.maxImageArrayLayers);
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

void Vulkan360::createSwapChain(VkSwapchainKHR* swapchain_ptr)
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
    VkExtent2D extent = chooseSwapExtent(capabilities, _window_w, _window_h);

    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount)
    {
        image_count = capabilities.maxImageCount;
    }
    
    VkSwapchainCreateInfoKHR swapchain_create_info{};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = _vk.surface;
    swapchain_create_info.minImageCount = image_count;
    swapchain_create_info.imageFormat = surface_format.format;
    swapchain_create_info.imageColorSpace = surface_format.colorSpace;
    swapchain_create_info.imageExtent = extent;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
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
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    else
    {
        VkExtent2D actual_extent = {};
        actual_extent.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actual_extent.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actual_extent;
    }
}
