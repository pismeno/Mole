# Plan: MaterialPool Implementation & Refactoring

## Overview
Implement `src/core/resources/material_pool.cpp` to match the finalized contract in `src/core/resources/include/resources/material_pool.hpp`. This refactors `MaterialPool` from exposing raw references (`get()` / `view()`) to:
1. **Scoped Callbacks (`with_material`)**: Safe bulk inspection for serialization and GPU uniform buffer packing without reference leaking.
2. **Value-Based Accessors (`get_vertex_shader`, `get_fragment_shader`, `get_property`)**: Safe against vector reallocations and generational slot recycling.
3. **Generational Arena Safety**: Tracking `alive` flags and generational counter bumps to prevent stale handle access.
4. **Unified Error Handling**: Returning `mole::Result<T>` and `mole::Result<void>` instead of raw pointers/booleans.
5. **Asset Synchronization**: Supporting push-based hot-reloading from `AssetHandle<MaterialAsset>`.

---

## 1. Prerequisites / Header Fix
- [x] **1.1 Fix line 226 in `src/core/resources/include/resources/material_pool.hpp`**:
  - In `load_material()`, change:
    ```cpp
    return add(std::move(mat), material_uuid);
    ```
    to:
    ```cpp
    return add(std::move(mat));
    ```
    *(Since `add(Material, UUID)` was removed in favor of `add(Material)` and `add(const AssetHandle<MaterialAsset>&)`).*

---

## 2. Implementation Steps (`src/core/resources/material_pool.cpp`)

### 2.1 Arena Slot Validation & UUID Lookup
- [x] **`is_valid(MaterialID material_id) const noexcept`**:
  - Check `material_id != MaterialID::INVALID`.
  - Check `material_id.index() < entries.size()`.
  - Check `entries[index].alive == true`.
  - Check `entries[index].generation == material_id.generation()`.
- [x] **`is_valid(UUID uuid) const noexcept`**:
  - If `uuid.is_nil()`, return `false`.
  - Look up `uuid` in `uuid_to_id`. If found, check `is_valid(it->second)`.
- [x] **`get_id(UUID uuid) const`**:
  - If `uuid.is_nil()`, return `mole::fail("UUID is nil")`.
  - Look up `uuid` in `uuid_to_id`. If found and `is_valid(it->second)`, return `it->second`.
  - Return `mole::fail("Material not found for UUID")`.

### 2.2 Material Creation & Acquisition (`add`)
- [x] **`MaterialID add(Material material)`**:
  - If `recycled_indices` is not empty: pop `index = recycled_indices.back()`.
  - Else: `index = next_unused_index++` and `entries.emplace_back()`.
  - Set `entry.alive = true`.
  - Set `entry.material = std::move(material)`.
  - Increment `entry.version++`.
  - Reset `entry.uuid = UUID{}`, `entry.source_asset = {}`, `entry.synced_asset_version = 0`.
  - Return `MaterialID::from(index, entry.generation)`.
- [x] **`Result<MaterialID> add(const AssetHandle<MaterialAsset>& asset)`**:
  - Check `asset.is_valid()`. If false, return `mole::fail("Invalid MaterialAsset handle")`.
  - Check if `asset.get_uuid()` is already mapped in `uuid_to_id` and valid:
    - If found, return the existing `MaterialID`.
  - Otherwise, construct `Material mat` from asset (`vertex_shader`, `fragment_shader`, `properties`).
  - Allocate slot with `MaterialID id = add(std::move(mat))``.
  - Link asset details directly on `entries[id.index()]`:
    - `entry.uuid = asset.get_uuid()`.
    - `entry.source_asset = asset`.
    - `entry.synced_asset_version = asset.get_version()`.
    - If `!entry.uuid.is_nil()`, record `uuid_to_id[entry.uuid] = id`.
  - Return `id`.

### 2.3 Value Accessors (Pure `const`)
- [x] Implement `MaterialID` overloads:
  - **`get_vertex_shader(MaterialID)`**: Validate ID -> return `entry.material.vertex_shader` or `mole::fail`.
  - **`get_fragment_shader(MaterialID)`**: Validate ID -> return `entry.material.fragment_shader` or `mole::fail`.
  - **`get_property(MaterialID, string_view)`**: Validate ID -> find in `entry.material.properties` -> return copy of `MaterialProperty` or `mole::fail("Property not found")`.
  - **`get_uuid(MaterialID)`**: Validate ID -> return `entry.uuid` or `mole::fail`.
  - **`get_version(MaterialID)`**: Validate ID -> return `entry.version` or `mole::fail`.
- [x] Implement `UUID` overloads:
  - Forward by calling `auto r_id = get_id(uuid); if (!r_id) return mole::fail(r_id.error());` and delegating to the `MaterialID` overload.

### 2.4 Mutators (`set_property`, `set_vertex_shader`, `set_fragment_shader`)
- [x] Implement `MaterialID` overloads:
  - Validate `is_valid(material_id)`.
  - Update property / shader handle in `entry.material`.
  - Increment `entry.version++` so renderers detect the update.
  - Return `mole::ok()`.
- [x] Implement `UUID` overloads:
  - Forward to `get_id(uuid)` and delegate to the `MaterialID` overload.

### 2.5 Source Asset Binding (`set_source_asset`, `get_source_asset`)
- [x] **`set_source_asset(MaterialID id, AssetHandle<MaterialAsset> asset)`**:
  - Validate `is_valid(id)`.
  - If `!asset.is_valid()` (unlinking):
    - If `!entry.uuid.is_nil()`, erase from `uuid_to_id`.
    - Clear `entry.uuid`, `entry.source_asset`, `entry.synced_asset_version`.
    - Return `mole::ok()`.
  - If linking:
    - Validate `UUID uuid = asset.get_uuid()` is not nil.
    - Conflict check: ensure `uuid` is not already mapped to another valid `MaterialID`.
    - If changing from an existing UUID, erase old mapping from `uuid_to_id`.
    - Map `uuid_to_id[uuid] = id`.
    - Set `entry.uuid = uuid`, `entry.source_asset = asset`, `entry.synced_asset_version = asset.get_version()`.
    - Re-copy shaders & properties from `asset` into `entry.material`.
    - Increment `entry.version++`.
    - Return `mole::ok()`.
- [x] **`get_source_asset(MaterialID id) const`**:
  - Validate `is_valid(id)`. Return `entry.source_asset` or `mole::fail("Invalid MaterialID")`.

### 2.6 Reload & Synchronization (`reload_from_source`, `reload_all_from_source`)
- [x] **`reload_from_source(MaterialID id)`**:
  - Validate `is_valid(id)`.
  - Check `entry.source_asset.is_valid()`. If not, return `mole::fail("Material does not have a source asset")`.
  - Copy `vertex_shader`, `fragment_shader`, `properties` from `entry.source_asset`.
  - Update `entry.synced_asset_version = entry.source_asset.get_version()`.
  - Increment `entry.version++`.
  - Return `mole::ok()`.
- [x] **`reload_from_source(UUID uuid)`**:
  - Forward via `get_id(uuid)` and call `reload_from_source(MaterialID)`.
- [x] **`reload_all_from_source()`**:
  - Loop through `entries`: for any `entry.alive && entry.source_asset.is_valid()`:
    - If `entry.source_asset.get_version() != entry.synced_asset_version`:
      - Re-copy shaders & properties, update `synced_asset_version`, increment `version++`.

### 2.7 Destruction & Pool Reset (`destroy`, `clear`)
- [x] **`destroy(MaterialID id)`**:
  - Validate `is_valid(id)`. If not, return `mole::fail("Invalid MaterialID")`.
  - Set `entry.alive = false`.
  - Increment `entry.generation++` (invalidates all external handles).
  - If `!entry.uuid.is_nil()`, erase from `uuid_to_id` and reset `entry.uuid`.
  - Reset `entry.source_asset`, `entry.synced_asset_version`, `entry.material`.
  - Push `id.index()` onto `recycled_indices`.
  - Return `mole::ok()`.
- [x] **`destroy(UUID uuid)`**:
  - Forward via `get_id(uuid)` and call `destroy(MaterialID)`.
- [x] **`clear()`**:
  - For each entry: set `entry.alive = false`, `entry.generation++`, `entry.uuid = {}`, `entry.source_asset = {}`, `entry.material = {}`, `entry.synced_asset_version = 0`.
  - Clear `uuid_to_id`.
  - Rebuild `recycled_indices` with all indices from `next_unused_index - 1` down to 0.

---

## 3. Call Site & Consumer Updates
Since `MaterialPool::get()` was removed, update callers across the codebase:
- [x] **`src/core/rendering/packed_material_cache.cpp`**:
  - Replace `const Material& material = material_pool->get(material_id);` with:
    ```cpp
    auto r = material_pool->with_material(material_id, [&](const Material& material) {
      entry.packed_data = material_packer->pack(material, layout);
    });
    ```
- [x] **`src/core/rendering/vulkan/vulkan_renderer.cpp`**:
  - In `end_pass`: use `material_pool->get_vertex_shader(batch_mat_id)` and `material_pool->get_fragment_shader(batch_mat_id)` instead of `material_pool->get(...)`.
- [x] **`src/core/rendering/vulkan/vulkan_material_handler.cpp`**:
  - Use `with_material` when collecting texture references in `upload_material`.
- [x] **`tests/material_pool_test.cpp` and `tests/scene_material_asset_test.cpp`**:
  - Update tests to use the new `add()`, `with_material()`, `get_property()`, and `Result` error checks.

---

## 4. Verification
- [x] Build: `ninja -C build/debug`.
- [x] Tests: Run `tests/MoleTests.exe` and confirm all tests pass.
