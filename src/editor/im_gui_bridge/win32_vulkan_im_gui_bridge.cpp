#include "win32_vulkan_im_gui_bridge.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include <algorithm>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_win32.h>

#include "editor/editor_context.hpp"
#include "rendering/irenderer.hpp"
#include "rendering/renderer_types.hpp"
#include "rendering/rendering_context.hpp"
#include "utils/result.hpp"

static_assert(sizeof(ImTextureID) == sizeof(uint64_t));
static_assert(sizeof(ImGuiID) == sizeof(uint32_t));

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

WNDPROC g_original_wndproc = nullptr;

LRESULT CALLBACK imgui_wndproc_hook(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
    return true;
  }
  return CallWindowProc(g_original_wndproc, hwnd, msg, wparam, lparam);
}

int create_vk_surface(ImGuiViewport* viewport, ImU64 vk_instance, const void* vk_allocator, ImU64* out_vk_surface) {
  VkWin32SurfaceCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  create_info.hwnd = static_cast<HWND>(viewport->PlatformHandleRaw);
  create_info.hinstance = ::GetModuleHandle(nullptr);
  return static_cast<int>(vkCreateWin32SurfaceKHR(reinterpret_cast<VkInstance>(vk_instance), &create_info,
                                                  static_cast<const VkAllocationCallbacks*>(vk_allocator),
                                                  reinterpret_cast<VkSurfaceKHR*>(out_vk_surface)));
}

} // namespace

namespace mole {

Win32VulkanImGuiBridge::Win32VulkanImGuiBridge(const EditorContext& context, void* window_handle, PresenterID presenter) : 
  renderer(context.rendering_context->renderer),
  window_handle(window_handle),
  presenter(presenter),
  frames_in_flight(context.rendering_context->renderer->get_backend_info().frames_in_flight) {}

Win32VulkanImGuiBridge::~Win32VulkanImGuiBridge() {
  if (g_original_wndproc && window_handle) {
    SetWindowLongPtr(static_cast<HWND>(window_handle), GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_original_wndproc));
    g_original_wndproc = nullptr;
  }
  auto device = renderer->get_native_handle<VkDevice>(ERendererNativeHandle::Device);
  if (device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device);
  }
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
}

Result<std::unique_ptr<Win32VulkanImGuiBridge>> Win32VulkanImGuiBridge::create(const EditorContext& context, void* window_handle, PresenterID presenter) {
  auto bridge = std::unique_ptr<Win32VulkanImGuiBridge>(new Win32VulkanImGuiBridge(context, window_handle, presenter));
  MOLE_TRY(bridge->init());
  return bridge;
}

Result<void> Win32VulkanImGuiBridge::init() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui::StyleColorsDark();

  if (!ImGui_ImplWin32_Init(window_handle)) {
    ImGui::DestroyContext();
    return mole::fail("Failed to initialize ImGui Win32 backend");
  }

  g_original_wndproc =
      reinterpret_cast<WNDPROC>(SetWindowLongPtr((HWND)window_handle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(imgui_wndproc_hook)));

  ImGui::GetPlatformIO().Platform_CreateVkSurface = create_vk_surface;

  const RendererBackendInfo backend_info = renderer->get_backend_info();
  MOLE_TRY_ASSIGN(const PresenterInfo presenter_info, renderer->get_info(presenter));

  VkFormat color_format = static_cast<VkFormat>(presenter_info.native_format);

  VkPipelineRenderingCreateInfoKHR pipeline_rendering_create_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
      .pNext = nullptr,
      .viewMask = 0,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &color_format,
      .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
      .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
  };

  ImGui_ImplVulkan_InitInfo init_info{
    .ApiVersion = VK_API_VERSION_1_3,
    .Instance = renderer->get_native_handle<VkInstance>(ERendererNativeHandle::Instance),
    .PhysicalDevice = renderer->get_native_handle<VkPhysicalDevice>(ERendererNativeHandle::PhysicalAdapter),
    .Device = renderer->get_native_handle<VkDevice>(ERendererNativeHandle::Device),
    .QueueFamily = backend_info.graphics_queue_family,
    .Queue = renderer->get_native_handle<VkQueue>(ERendererNativeHandle::GraphicsQueue),
    .DescriptorPoolSize = 64,
    .MinImageCount = 2,
    .ImageCount = (std::max)(2u, presenter_info.image_count),
    .UseDynamicRendering = true,
  };
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo = pipeline_rendering_create_info;
  

  if (!ImGui_ImplVulkan_Init(&init_info)) {
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    return mole::fail("Failed to initialize ImGui Vulkan backend");
  }

  return mole::ok();
}

void Win32VulkanImGuiBridge::begin_frame() {
  ++frame_count;
  free_retired_textures();

  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
}

Result<uint64_t> Win32VulkanImGuiBridge::get_texture_id(RenderTargetID target) {
  auto image_view = renderer->get_native_handle<VkImageView>(target, ERenderTargetNativeHandle::ImageView);
  if (image_view == VK_NULL_HANDLE) return mole::fail("Render target has no image view (stale or presenter-owned)");

  auto it = textures.find(target);
  if (it != textures.end()) {
    if (it->second.image_view == image_view) return reinterpret_cast<uint64_t>(it->second.descriptor_set);
    retired_textures.push_back({
      .retire_after_frame = frame_count + frames_in_flight,
      .descriptor_set = it->second.descriptor_set
    });
    textures.erase(it);
  }

  VkDescriptorSet descriptor_set = ImGui_ImplVulkan_AddTexture(image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  if (descriptor_set == VK_NULL_HANDLE) return mole::fail("Failed to allocate ImGui texture descriptor set");

  textures.emplace(target, Texture{
    .image_view = image_view,
    .descriptor_set = descriptor_set
  });
  return reinterpret_cast<uint64_t>(descriptor_set);
}

void Win32VulkanImGuiBridge::release_texture(RenderTargetID target) {
  auto it = textures.find(target);
  if (it == textures.end()) return;
  retired_textures.push_back({
    .retire_after_frame = frame_count + frames_in_flight,
    .descriptor_set = it->second.descriptor_set
  });
  textures.erase(it);
}

void Win32VulkanImGuiBridge::free_retired_textures() {
  std::erase_if(retired_textures, [this](const RetiredTexture& retired) {
    if (retired.retire_after_frame > frame_count) return false;
    ImGui_ImplVulkan_RemoveTexture(retired.descriptor_set);
    return true;
  });
}

Result<void> Win32VulkanImGuiBridge::end_frame() {
  ImGui::Render();

  ImDrawData* draw_data = ImGui::GetDrawData();
  if (draw_data) {
    MOLE_TRY_ASSIGN(bool pass_open, renderer->begin_pass(renderer->get_render_target(presenter), PassOptions{.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)}));
    if (pass_open) {
      auto command_buffer = renderer->get_native_handle<VkCommandBuffer>(ERendererNativeHandle::ActiveCommandBuffer);
      ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer);
      MOLE_TRY(renderer->end_pass());
    }
  }

  ImGuiIO& io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
  }

  return mole::ok();
}

} // mole