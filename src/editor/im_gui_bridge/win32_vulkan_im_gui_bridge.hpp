#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "editor/iim_gui_bridge.hpp"
#include "osal/ifile_system.hpp"
#include "rendering/renderer_types.hpp"
#include "utils/result.hpp"

struct VkImageView_T;
struct VkDescriptorSet_T;
using VkImageView = VkImageView_T*;
using VkDescriptorSet = VkDescriptorSet_T*;

namespace mole {

struct EditorContext;
class IRenderer;

class Win32VulkanImGuiBridge : public IImGuiBridge {
public:
  ~Win32VulkanImGuiBridge() override;

  void begin_frame() override;
  Result<void> end_frame() override;

  [[nodiscard]] Result<uint64_t> get_texture_id(RenderTargetID target) override;
  void release_texture(RenderTargetID target) override;

  static Result<std::unique_ptr<Win32VulkanImGuiBridge>> create(const EditorContext& context, void* window_handle, PresenterID presenter);
private:
  explicit Win32VulkanImGuiBridge(const EditorContext& context, void* window_handle, PresenterID presenter);

  struct Texture {
    VkImageView image_view;
    VkDescriptorSet descriptor_set;
  };

  struct RetiredTexture {
    uint64_t retire_after_frame;
    VkDescriptorSet descriptor_set;
  };

  Result<void> init(IFileSystem* file_system);
  void free_retired_textures();

    void* window_handle;
  PresenterID presenter;
  uint32_t frames_in_flight;

  std::unordered_map<RenderTargetID, Texture> textures;
  std::vector<RetiredTexture> retired_textures;
  uint64_t frame_count = 0;
  uint32_t dockspace_id = 0;

  IRenderer* renderer;
};

} // mole