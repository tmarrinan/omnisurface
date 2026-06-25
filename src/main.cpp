#include <iostream>
#include <vk360.h>

int main()
{
    Vulkan360* app = new Vulkan360("resrc/config_plane.txt");
    if (!app->hasValidDisplayConfig())
    {
        fprintf(stderr, "Error: Vulkan360 could not read display config file\n");
        return EXIT_FAILURE;
    }
    printf("OmniSurface Vulkan360 display config read in.\n");
    app->initializeWindow("OmniSurface", "resrc/images/SampleOmni3D.png");

    while (!app->shouldClose())
    {
        app->pollEvents();
        // TODO: process events here

        int buffer_idx = app->getRenderBufferIndex();
        app->drawFrame(buffer_idx);
        app->swapBuffers(buffer_idx);
    }

    return EXIT_SUCCESS;
}
