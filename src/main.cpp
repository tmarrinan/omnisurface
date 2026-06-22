#include <iostream>
#include <vk360.h>

int main()
{
    uint32_t version = VK_API_VERSION_1_0;

    if (vkEnumerateInstanceVersion)
        vkEnumerateInstanceVersion(&version);

    std::cout
        << VK_API_VERSION_MAJOR(version) << "."
        << VK_API_VERSION_MINOR(version) << "."
        << VK_API_VERSION_PATCH(version)
        << std::endl;

    Vulkan360 *render360 = new Vulkan360("resrc/config_plane.txt");
    if (!render360->hasValidDisplayConfig())
    {
        fprintf(stderr, "Error: Vulkan360 could not read display config file\n");
        return EXIT_FAILURE;
    }
    printf("OmniSurface Vulkan360 display config read in.\n");
    renderer360->initializeWindow();

    return EXIT_SUCCESS;
}
