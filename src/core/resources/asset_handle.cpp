#include "resources/asset_handle.hpp"

#include "resources/asset_pool.hpp"
#include "resources/asset_types.hpp"

namespace mole {

template<typename T>
AssetHandle<T>::AssetHandle(AssetPool<T>* pool, uint32_t index, uint32_t generation) noexcept
    : pool(pool), index(index), generation(generation) {}

template<typename T>
AssetHandle<T>::~AssetHandle() {
  release_ref();
}

template<typename T>
AssetHandle<T>::AssetHandle(const AssetHandle& other)
    : pool(other.pool), index(other.index), generation(other.generation) {
  add_ref();
}

template<typename T>
AssetHandle<T>& AssetHandle<T>::operator=(const AssetHandle& other) {
  if (this != &other) {
    release_ref();
    pool = other.pool;
    index = other.index;
    generation = other.generation;
    add_ref();
  }
  return *this;
}

template<typename T>
AssetHandle<T>::AssetHandle(AssetHandle&& other) noexcept
    : pool(other.pool), index(other.index), generation(other.generation) {
  other.pool = nullptr;
  other.index = std::numeric_limits<uint32_t>::max();
}

template<typename T>
AssetHandle<T>& AssetHandle<T>::operator=(AssetHandle&& other) noexcept {
  if (this != &other) {
    release_ref();
    pool = other.pool;
    index = other.index;
    generation = other.generation;

    other.pool = nullptr;
    other.index = std::numeric_limits<uint32_t>::max();
  }
  return *this;
}

template<typename T>
bool AssetHandle<T>::is_valid() const noexcept {
  if (!pool || index == std::numeric_limits<uint32_t>::max()) return false;
  return pool->is_loaded(index, generation);
}

template<typename T>
const T& AssetHandle<T>::get() const noexcept {
  return pool->resolve(index, generation);
}

template<typename T>
void AssetHandle<T>::add_ref() {
  if (pool && index != std::numeric_limits<uint32_t>::max()) {
    pool->add_ref(index, generation);
  }
}

template<typename T>
void AssetHandle<T>::release_ref() {
  if (pool && index != std::numeric_limits<uint32_t>::max()) {
    pool->release(index, generation);
    pool = nullptr;
    index = std::numeric_limits<uint32_t>::max();
  }
}

template<typename T>
UUID AssetHandle<T>::get_uuid() const noexcept {
  return pool->get_uuid(index);
}

template<typename T>
uint32_t AssetHandle<T>::get_version() const noexcept {
  return pool->get_version(index);
}

template class AssetHandle<ImageAsset>;
template class AssetHandle<VertexShaderAsset>;
template class AssetHandle<FragmentShaderAsset>;
template class mole::AssetHandle<mole::TextAsset>;

} // mole