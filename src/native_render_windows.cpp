#include <iostream>
#include <cstring>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "native_render_handle.h"

struct NativeRenderHandle::NativeData {
    Microsoft::WRL::ComPtr<IDXGIFactory6> dxgi_factory;
    Microsoft::WRL::ComPtr<ID3D12Device> d3d12_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain;
};

NativeRenderHandle::NativeRenderHandle()
{
    _native_data = new NativeRenderHandle::NativeData();
    memset(_luid, 0, 8);
}

NativeRenderHandle::~NativeRenderHandle()
{
    _native_data->swap_chain.Reset();
    _native_data->command_queue.Reset();
    _native_data->d3d12_device.Reset();
    _native_data->dxgi_factory.Reset();
    delete _native_data;
}

GLFWwindow* NativeRenderHandle::createFullscreenWindow(const char* title)
{
    // Prevent GLFW from implicitly creating an OpenGL/Vulkan context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // Get primary monitor
    GLFWmonitor* primary_monitor = glfwGetPrimaryMonitor();
    if (!primary_monitor)
    {
        fprintf(stderr, "NativeRenderHandle> Failed to find primary monitor\n");
        return nullptr;
    }

    // Get resolution of primary monitor
    const GLFWvidmode* mode = glfwGetVideoMode(primary_monitor);

    // Create fullscreen window
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, title, primary_monitor, nullptr);
    if (!window)
    {
        fprintf(stderr, "NativeRenderHandle> Failed to create GLFW window\n");
        return nullptr;
    }

    // Get native Windows handle
    HWND hwnd = glfwGetWin32Window(window);
    if (!hwnd)
    {
        fprintf(stderr, "NativeRenderHandle> Failed to retrieve native Win32 window handle\n");
        glfwDestroyWindow(window);
        return nullptr;
    }

    // Create DXGI Factory
    UINT dxgi_factory_flags = 0;
    if (FAILED(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&(_native_data->dxgi_factory)))))
    {
        fprintf(stderr, "NativeRenderHandle> Failed to create DXGI Factory\n");
        glfwDestroyWindow(window);
        return nullptr;
    }

    // Enumerate GPUs and select dedicate GPU
    Microsoft::WRL::ComPtr<IDXGIAdapter1> gpu_adapter;
    IDXGIAdapter1* selected_adapter = nullptr;
    for (UINT i = 0; SUCCEEDED(_native_data->dxgi_factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&gpu_adapter))); i++)
    {
        DXGI_ADAPTER_DESC1 desc;
        gpu_adapter->GetDesc1(&desc);

        // Skip software rendering emulators (like Microsoft Basic Render Driver)
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

        // Ensure adapter actually supports DX12
        if (SUCCEEDED(D3D12CreateDevice(gpu_adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
        {
            selected_adapter = gpu_adapter.Get();
            memcpy(_luid, &(desc.AdapterLuid), 8);
            break;
        }
    }

    // Create the D3D12 Device (targeting selected high-performance GPU)
    if (FAILED(D3D12CreateDevice(selected_adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&(_native_data->d3d12_device)))))
    {
        fprintf(stderr, "NativeRenderHandle> Failed to create D3D12 Device\n");
        glfwDestroyWindow(window);
        return nullptr;
    }

    // Create the Command Queue
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(_native_data->d3d12_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&(_native_data->command_queue)))))
    {
        fprintf(stderr, "NativeRenderHandle> Failed to create Command Queue\n");
        glfwDestroyWindow(window);
        return nullptr;
    }

    // Initialize Swap Chain
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
    swap_chain_desc.BufferCount = 2;
    swap_chain_desc.Width = mode->width;
    swap_chain_desc.Height = mode->height;
    swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> temp_swap_chain;
    if (FAILED(_native_data->dxgi_factory->CreateSwapChainForHwnd(_native_data->command_queue.Get(), hwnd, &swap_chain_desc, nullptr, nullptr, &temp_swap_chain)))
    {
        fprintf(stderr, "NativeRenderHandle> Failed to create Swap Chain\n");
        glfwDestroyWindow(window);
        return nullptr;
    }

    // Upgrade the swap chain interface version to access modern features
    temp_swap_chain.As(&(_native_data->swap_chain));

    // Block DXGI from automatically responding to standard Alt+Enter window style changes
    _native_data->dxgi_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    return window;
}

void NativeRenderHandle::swapBuffers()
{
    _native_data->swap_chain->Present(1, 0);
}
