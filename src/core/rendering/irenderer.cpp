#include "rendering/irenderer.hpp"

#include <memory>
#include <utility>

#include "rendering/rendering_context.hpp"
#include "utils/result.hpp"

#ifdef BIRD_PLATFORM_WINDOWS
#include "rendering/vulkan/vulkan_renderer.hpp"
#elif defined(BIRD_PLATFORM_APPLE)
#include "rendering/metal/metal_renderer.hpp"
#endif

namespace mole {

Result<std::unique_ptr<IRenderer>> IRenderer::create(RendererOptions options, RenderingContext& context) {
  switch (options.type) {
    case RendererType::Metal: {
#ifdef BIRD_PLATFORM_APPLE
      return std::make_unique<MetalRenderer>();
#else
      return mole::fail("Metal renderer is only supported on macOS");
#endif
    }
    case RendererType::Vulkan: {
#ifdef BIRD_PLATFORM_WINDOWS
      auto r_vulkan = VulkanRenderer::create(context, options.native_window_handle, options.window_width, options.window_height);
      if (!r_vulkan) return mole::fail(r_vulkan.error());

      return std::unique_ptr<IRenderer>(std::move(r_vulkan).value());
#else
      return mole::fail("Vulkan renderer is only supported on Windows");
#endif
    }
    case RendererType::DirectX: {
      return mole::fail("DirectX renderer is not yet implemented");
    }
    default: {
      return mole::fail("Unsupported renderer type");
    }
  }
}

} // mole
