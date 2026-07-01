# OmniSurface
Immersive viewer for omnidirectional media with support for non-planar and stereo 3D surfaces

### Features
- [ ] 360 images (jpg, png)
- [ ] 360 videos (h264, h265, vp9)
- [ ] Cinema science 360 data sets
- [ ] 2D map with 360 street view

### Dependencies
CMake [https://cmake.org/download/](https://cmake.org/download/)
* Download/Install 'Latest Release'

GLAD (v2) -- [https://gen.glad.sh/](https://gen.glad.sh/)
* egl: Version 1.5
* gl: Version 4.6
* Core
* Extensions:
    * `GL_EXT_memory_object`
    * `GL_EXT_memory_object_fd` (*Linux only)
    * `GL_EXT_memory_object_win32` (*Windows only)
    * `GL_EXT_semaphore`
    * `GL_EXT_semaphore_fd` (*Linux only)
    * `GL_EXT_semaphore_win32` (*Windows only)
* Options (check the following):
    * loader

Vulkan -- [https://vulkan.lunarg.com/sdk/home](https://vulkan.lunarg.com/sdk/home)

GLFW -- [https://www.glfw.org/download](https://www.glfw.org/download)
 * Download from source
 * Build/Install using CMake
