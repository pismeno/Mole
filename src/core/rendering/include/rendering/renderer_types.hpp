#pragma once

#include <cstdint>

namespace mole {

enum class ERendererType { 
  Metal, 
  Vulkan, 
  DirectX 
};

enum class ERendererNativeHandle {
  // Core Context / Device
  Instance,             // VkInstance
  PhysicalAdapter,      // VkPhysicalDevice / IDXGIAdapter / id<MTLDevice>
  Device,               // VkDevice / ID3D12Device / ID3D11Device / id<MTLDevice> / WGPUDevice
  ImmediateContext,     // ID3D11DeviceContext / HGLRC / EGLContext

  // Execution & Presentation
  GraphicsQueue,        // VkQueue / ID3D12CommandQueue / id<MTLCommandQueue> / WGPUQueue
  ComputeQueue,         // VkQueue / ID3D12CommandQueue
  TransferQueue,        // VkQueue / ID3D12CommandQueue
  Swapchain,            // VkSwapchainKHR / IDXGISwapChain / WGPUSwapChain
  ActiveCommandBuffer,  // VkCommandBuffer / ID3D12GraphicsCommandList / id<MTLCommandBuffer>
};

struct RendererBackendInfo {
  uint32_t graphics_queue_family = 0;
  uint32_t compute_queue_family  = 0;
  uint32_t transfer_queue_family = 0;
  uint32_t swapchain_image_count = 0;
  uint32_t native_swapchain_format = 0; // Cast to VkFormat / DXGI_FORMAT / MTLPixelFormat
};
  
} // mole