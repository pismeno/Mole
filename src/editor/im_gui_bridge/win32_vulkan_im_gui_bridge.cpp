#include "win32_vulkan_im_gui_bridge.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include <algorithm>
#include <utility>

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_win32.h>

#include "editor/editor_context.hpp"
#include "rendering/irenderer.hpp"
#include "rendering/renderer_types.hpp"
#include "rendering/rendering_context.hpp"
#include "utils/result.hpp"

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

Win32VulkanImGuiBridge::Win32VulkanImGuiBridge(const EditorContext& context, void* window_handle) : 
  renderer(context.rendering_context->renderer), 
  window_handle(window_handle) {}

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

Result<std::unique_ptr<Win32VulkanImGuiBridge>> Win32VulkanImGuiBridge::create(const EditorContext& context, void* window_handle) {
  auto bridge = std::unique_ptr<Win32VulkanImGuiBridge>(new Win32VulkanImGuiBridge(context, window_handle));
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

  this->window_handle = window_handle;
  g_original_wndproc =
      reinterpret_cast<WNDPROC>(SetWindowLongPtr((HWND)window_handle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(imgui_wndproc_hook)));

  ImGui::GetPlatformIO().Platform_CreateVkSurface = create_vk_surface;

  const RendererBackendInfo backend_info = renderer->get_backend_info();

  VkFormat color_format = static_cast<VkFormat>(backend_info.native_swapchain_format);

  VkPipelineRenderingCreateInfoKHR pipeline_rendering_create_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
      .pNext = nullptr,
      .viewMask = 0,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &color_format,
      .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
      .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
  };

  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.ApiVersion = VK_API_VERSION_1_3;
  init_info.Instance = renderer->get_native_handle<VkInstance>(ERendererNativeHandle::Instance);
  init_info.PhysicalDevice = renderer->get_native_handle<VkPhysicalDevice>(ERendererNativeHandle::PhysicalAdapter);
  init_info.Device = renderer->get_native_handle<VkDevice>(ERendererNativeHandle::Device);
  init_info.QueueFamily = backend_info.graphics_queue_family;
  init_info.Queue = renderer->get_native_handle<VkQueue>(ERendererNativeHandle::GraphicsQueue);
  init_info.DescriptorPoolSize = 64;
  init_info.MinImageCount = 2;
  init_info.ImageCount = (std::max)(2u, backend_info.swapchain_image_count);
  init_info.UseDynamicRendering = true;
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo = pipeline_rendering_create_info;

  if (!ImGui_ImplVulkan_Init(&init_info)) {
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    return mole::fail("Failed to initialize ImGui Vulkan backend");
  }

  return mole::ok();
}

void Win32VulkanImGuiBridge::begin_frame() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
}

void Win32VulkanImGuiBridge::end_frame() {
  ImGui::Render();

  ImDrawData* draw_data = ImGui::GetDrawData();
  if (draw_data) {
    auto command_buffer = renderer->get_native_handle<VkCommandBuffer>(ERendererNativeHandle::ActiveCommandBuffer);
    auto image_view = renderer->get_native_handle<VkImageView>(ERendererNativeHandle::ActiveRenderTargetView);

    if (command_buffer != VK_NULL_HANDLE && image_view != VK_NULL_HANDLE) {
      VkRenderingAttachmentInfo attachment_info{
          .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
          .pNext = nullptr,
          .imageView = image_view,
          .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          .resolveMode = VK_RESOLVE_MODE_NONE,
          .resolveImageView = VK_NULL_HANDLE,
          .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
          .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
          .clearValue = {},
      };

      VkRenderingInfo rendering_info{
          .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
          .pNext = nullptr,
          .flags = 0,
          .renderArea = {{0, 0}, {renderer->get_framebuffer_width(), renderer->get_framebuffer_height()}},
          .layerCount = 1,
          .viewMask = 0,
          .colorAttachmentCount = 1,
          .pColorAttachments = &attachment_info,
          .pDepthAttachment = nullptr,
          .pStencilAttachment = nullptr,
      };

      vkCmdBeginRendering(command_buffer, &rendering_info);
      ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer);
      vkCmdEndRendering(command_buffer);
    }
  }

  ImGuiIO& io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
  }
}

} // namespace mole
