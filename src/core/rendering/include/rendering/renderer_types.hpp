#pragma once

#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "resources/view_data.hpp"
#include "utils/id.hpp"

namespace mole {

/** @brief Something to draw into: an offscreen image, or a presenter's backbuffer. */
using RenderTargetID = ID<struct RenderTargetIDTag>;
/** @brief A swapchain bound to an `IWindow`. */
using PresenterID = ID<struct PresenterIDTag>;

enum class ERendererType {
  Metal,
  Vulkan,
  DirectX
};

enum class ERenderTargetFormat : uint8_t {
  RGBA8_UNORM,
  RGBA8_SRGB,
  BGRA8_SRGB,
  RGBA16_FLOAT
};

enum class ERendererNativeHandle {
  // Core Context / Device
  Instance,         // VkInstance
  PhysicalAdapter,  // VkPhysicalDevice / IDXGIAdapter / id<MTLDevice>
  Device,           // VkDevice / ID3D12Device / ID3D11Device / id<MTLDevice> / WGPUDevice
  ImmediateContext, // ID3D11DeviceContext / HGLRC / EGLContext

  // Execution
  GraphicsQueue,       // VkQueue / ID3D12CommandQueue / id<MTLCommandQueue> / WGPUQueue
  ComputeQueue,        // VkQueue / ID3D12CommandQueue
  TransferQueue,       // VkQueue / ID3D12CommandQueue
  ActiveCommandBuffer  // VkCommandBuffer / ID3D12GraphicsCommandList / id<MTLCommandBuffer>
};

enum class ERenderTargetNativeHandle {
  Image,    // VkImage / ID3D12Resource / id<MTLTexture>
  ImageView // VkImageView / D3D12 RTV CPU handle / -
};

/** @brief Parameters for creating an offscreen render target. */
struct RenderTargetOptions {
  uint32_t width;
  uint32_t height;
  ERenderTargetFormat format = ERenderTargetFormat::RGBA8_SRGB;
};

/** @brief Current properties of a render target. */
struct RenderTargetInfo {
  uint32_t width;
  uint32_t height;
  uint32_t native_format; // VkFormat / DXGI_FORMAT / MTLPixelFormat
};

/** @brief Current properties of a presenter's swapchain. */
struct PresenterInfo {
  uint32_t width;
  uint32_t height;
  uint32_t image_count;
  uint32_t native_format; // VkFormat / DXGI_FORMAT / MTLPixelFormat
};

/** @brief Per-pass parameters for `IRenderer::begin_pass`. */
struct PassOptions {
  ViewData view_data;
  std::optional<glm::vec4> clear_color; // RGBA clear value; nullopt = load (keep) existing contents
};

/** @brief Backend-wide information for external renderers (e.g. ImGui). */
struct RendererBackendInfo {
  uint32_t graphics_queue_family = 0;
  uint32_t compute_queue_family = 0;
  uint32_t transfer_queue_family = 0;
  uint32_t frames_in_flight = 0;
};

} // mole
