#include "rendering/vulkan/vulkan_context.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifdef BIRD_PLATFORM_WINDOWS
#include <windows.h>
#endif
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "utils/result.hpp"

namespace mole {

Result<std::unique_ptr<VulkanContext>> VulkanContext::create(void* native_window_handle) {
  auto context = std::unique_ptr<VulkanContext>(new VulkanContext());

  auto r_create_instance = context->create_instance();
  if (!r_create_instance) return mole::fail(r_create_instance.error());

  auto r_create_surface = context->create_surface(native_window_handle);
  if (!r_create_surface) return mole::fail(r_create_surface.error());

  auto r_pick_physical_device = context->pick_physical_device();
  if (!r_pick_physical_device) return mole::fail(r_pick_physical_device.error());

  auto r_create_logical_device = context->create_logical_device();
  if (!r_create_logical_device) return mole::fail(r_create_logical_device.error());

  return std::move(context);
}

Result<void> VulkanContext::create_instance() {
  vk::ApplicationInfo app_info{
      .pApplicationName = "Hello Triangle",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "MoleEngine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_3
  };

  std::vector<const char*> extensions = { "VK_KHR_surface" };
#ifdef BIRD_PLATFORM_WINDOWS
  extensions.push_back("VK_KHR_win32_surface");
#endif
#ifdef BIRD_PLATFORM_LINUX
  extensions.push_back("VK_KHR_xcb_surface");
#endif

  std::vector<const char*> layers;
#ifndef NDEBUG
  layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

  vk::InstanceCreateInfo create_info{
      .pApplicationInfo = &app_info,
      .enabledLayerCount = static_cast<uint32_t>(layers.size()),
      .ppEnabledLayerNames = layers.data(),
      .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
      .ppEnabledExtensionNames = extensions.data()
  };

  auto vkr_create = vk::createInstance(create_info);
  if (vkr_create.result != vk::Result::eSuccess) {
    return mole::fail("Failed to create Vulkan Instance");
  }

  vk_instance = vk::raii::Instance(context, vkr_create.value);

  return mole::ok();
}

Result<void> VulkanContext::create_surface(void* native_window_handle) {
  vk::Win32SurfaceCreateInfoKHR createInfo{
      .hinstance = GetModuleHandle(nullptr),
      .hwnd      = static_cast<HWND>(native_window_handle)
  };

  // Dereference RAII object to get the raw vk::Instance wrapper to call creation
  vk::Instance raw_instance = *vk_instance;
  auto surf_res = raw_instance.createWin32SurfaceKHR(createInfo);
  if (surf_res.result != vk::Result::eSuccess) {
    return mole::fail("Failed to create Win32 Surface");
  }

  surface = vk::raii::SurfaceKHR(vk_instance, surf_res.value);
  return mole::ok();
}

inline uint32_t calculate_texture_capacity(const vk::PhysicalDeviceLimits& limits) {
  return std::min({
                      limits.maxPerStageDescriptorSampledImages,
                      limits.maxDescriptorSetSampledImages,
                      limits.maxPerStageResources
                  });
}

Result<void> VulkanContext::pick_physical_device() {
  auto phys_devs_res = vk_instance.enumeratePhysicalDevices();
  if (phys_devs_res.result != vk::Result::eSuccess) {
    return mole::fail("Failed to enumerate physical devices");
  }

  std::vector<vk::raii::PhysicalDevice> physicalDevices = phys_devs_res.value;
  auto const devIter = std::ranges::find_if(physicalDevices, [&]( auto const & physicalDevice) {
    return is_physical_device_suitable(physicalDevice);
  });

  if (devIter == physicalDevices.end()) {
    return mole::fail("No suitable physical device found");
  }

  physical_device = *devIter;
  texture_capacity = std::min(calculate_texture_capacity(physical_device.getProperties().limits), PREFERRED_MAX_TEXTURES);
  return mole::ok();
}

bool VulkanContext::is_physical_device_suitable(const vk::PhysicalDevice &physical_device) const {
  const auto properties = physical_device.getProperties();

  // Minimum Vulkan version
  if (properties.apiVersion < vk::ApiVersion13) {
    return false;
  }

  // Queue family: find one that supports both graphics and surface presentation
  const auto queue_families = physical_device.getQueueFamilyProperties();
  bool has_suitable_queue = false;

  for (uint32_t i = 0; i < queue_families.size(); ++i) {
    const bool supports_graphics = bool(queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics);
    const auto present_res = physical_device.getSurfaceSupportKHR(i, *surface);
    const bool supports_present = (present_res.result == vk::Result::eSuccess && present_res.value);

    if (supports_graphics && supports_present) {
      has_suitable_queue = true;
      break;
    }
  }

  if (!has_suitable_queue) {
    return false;
  }

  // Required extensions
  const std::vector<const char*> required_extensions = {
      vk::KHRSwapchainExtensionName
  };

  const auto ext_res = physical_device.enumerateDeviceExtensionProperties();
  if (ext_res.result != vk::Result::eSuccess) {
    return false;
  }

  const auto& available_extensions = ext_res.value;
  for (const char* required : required_extensions) {
    bool found = false;
    for (const auto& available : available_extensions) {
      if (std::strcmp(available.extensionName, required) == 0) {
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
  }

  // Feature checks
  const auto features = physical_device.getFeatures2<
      vk::PhysicalDeviceFeatures2,
      vk::PhysicalDeviceVulkan11Features,
      vk::PhysicalDeviceVulkan13Features,
      vk::PhysicalDeviceDescriptorIndexingFeatures,
      vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

  const auto& v11 = features.template get<vk::PhysicalDeviceVulkan11Features>();
  const auto& v13 = features.template get<vk::PhysicalDeviceVulkan13Features>();
  const auto& desc_idx = features.template get<vk::PhysicalDeviceDescriptorIndexingFeatures>();
  const auto& dyn_state = features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

  const bool supports_features =
      v11.shaderDrawParameters &&
      v13.synchronization2 &&
      v13.dynamicRendering &&
      desc_idx.descriptorBindingPartiallyBound &&
      desc_idx.descriptorBindingSampledImageUpdateAfterBind &&
      dyn_state.extendedDynamicState;

  if (!supports_features) {
    return false;
  }

  // Hardware limits
  return calculate_texture_capacity(properties.limits) >= MIN_REQUIRED_TEXTURES;
}

Result<void> VulkanContext::create_logical_device() {
  std::vector<vk::QueueFamilyProperties> queue_family_properties = physical_device.getQueueFamilyProperties();

  // get the first index into queue_family_properties which supports both graphics and present
  for (uint32_t qfp_index = 0; qfp_index < queue_family_properties.size(); qfp_index++) {

    // FIX 1: Extract the ResultValue properly instead of treating it as a raw bool
    auto support_res = physical_device.getSurfaceSupportKHR(qfp_index, *surface);
    bool supports_present = (support_res.result == vk::Result::eSuccess) && support_res.value;

    if ((queue_family_properties[qfp_index].queueFlags & vk::QueueFlagBits::eGraphics) && supports_present) {
      // found a queue family that supports both graphics and present
      queue_index = qfp_index;
      break;
    }
  }

  if (queue_index == ~0u) {
    return mole::fail("Could not find a queue for graphics and present");
  }

  // query for Vulkan 1.3 features
  auto supported_features = physical_device.getFeatures2<
      vk::PhysicalDeviceFeatures2,
      vk::PhysicalDeviceVulkan11Features,
      vk::PhysicalDeviceVulkan13Features,
      vk::PhysicalDeviceDescriptorIndexingFeatures,
      vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

  const auto & supported_vulkan11 =
      supported_features.get<vk::PhysicalDeviceVulkan11Features>();

  const auto & supported_vulkan13 =
      supported_features.get<vk::PhysicalDeviceVulkan13Features>();

  const auto & supported_descriptor_indexing =
      supported_features.get<vk::PhysicalDeviceDescriptorIndexingFeatures>();

  const auto & supported_extended_dynamic_state =
      supported_features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

  if (!supported_vulkan11.shaderDrawParameters) {
    return mole::fail("Required Vulkan 1.1 feature shaderDrawParameters is not supported");
  }

  if (!supported_vulkan13.synchronization2) {
    return mole::fail("Required Vulkan 1.3 feature synchronization2 is not supported");
  }

  if (!supported_vulkan13.dynamicRendering) {
    return mole::fail("Required Vulkan 1.3 feature dynamicRendering is not supported");
  }

  if (!supported_descriptor_indexing.descriptorBindingPartiallyBound) {
    return mole::fail("Required descriptor indexing feature descriptorBindingPartiallyBound is not supported");
  }

  if (!supported_descriptor_indexing.descriptorBindingSampledImageUpdateAfterBind) {
    return mole::fail("Required descriptor indexing feature descriptorBindingSampledImageUpdateAfterBind is not supported");
  }

  if (!supported_extended_dynamic_state.extendedDynamicState) {
    return mole::fail("Required extended dynamic state feature is not supported");
  }

  vk::PhysicalDeviceVulkan11Features vulkan11_features{
      .shaderDrawParameters = true
  };

  vk::PhysicalDeviceVulkan12Features vulkan12_features{
    .shaderSampledImageArrayNonUniformIndexing = true,
    .descriptorBindingSampledImageUpdateAfterBind = true,
    .descriptorBindingPartiallyBound = true,
    .runtimeDescriptorArray = true,
  };

  vk::PhysicalDeviceVulkan13Features vulkan13_features{
      .synchronization2 = true,
      .dynamicRendering = true
  };

  vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT extended_dynamic_state_features{
      .extendedDynamicState = true
  };

  vk::StructureChain<
      vk::PhysicalDeviceFeatures2,
      vk::PhysicalDeviceVulkan11Features,
      vk::PhysicalDeviceVulkan12Features,
      vk::PhysicalDeviceVulkan13Features,
      vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
  > feature_chain = {
      {},
      vulkan11_features,
      vulkan12_features,
      vulkan13_features,
      extended_dynamic_state_features
  };

  // create a Device
  float queue_priority = 0.5f;

  vk::DeviceQueueCreateInfo device_queue_create_info{
      .queueFamilyIndex = queue_index,
      .queueCount       = 1,
      .pQueuePriorities = &queue_priority
  };

  // FIX 2: Declare the required extensions locally before using them
  std::vector<const char*> required_device_extensions = {vk::KHRSwapchainExtensionName};

  vk::DeviceCreateInfo device_create_info{
      .pNext                   = &feature_chain.get<vk::PhysicalDeviceFeatures2>(),
      .queueCreateInfoCount    = 1,
      .pQueueCreateInfos       = &device_queue_create_info,
      .enabledExtensionCount   = static_cast<uint32_t>(required_device_extensions.size()),
      .ppEnabledExtensionNames = required_device_extensions.data()
  };

  auto vkr_create_logical_device = physical_device.createDevice(device_create_info);
  if (vkr_create_logical_device.result != vk::Result::eSuccess) {
    return mole::fail("Failed to create logical device");
  }
  logical_device = std::move(vkr_create_logical_device.value);

  graphics_queue = logical_device.getQueue(queue_index, 0);

  return mole::ok();
}

} // mole