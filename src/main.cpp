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

    vk360::init("resrc/config_plane.txt", nullptr);

    return 0;
}
