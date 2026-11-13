# Plan: SceneAsset & MaterialAsset Implementation

## Overview
Implement strictly typed `SceneAsset` and `MaterialAsset` in Mole engine while maintaining clean separation between immutable cached asset data (`Asset` / `AssetHandle<T>`) and mutable runtime state (`Scene`, `MaterialPool`). Reflection logic via `CReflector` is placed on manager/factory classes (`SceneFactory`, `MaterialPool`). Payload handling includes format detection for JSON vs. binary to allow straightforward addition of binary reflectors later.

---

## 1. Payload Format Detection Strategy
To distinguish between JSON and upcoming binary serialization formats safely:
- Check the first non-whitespace byte in the payload (ignoring optional UTF-8 BOM `0xEF, 0xBB, 0xBF`, spaces, tabs, `\r`, `\n`).
- If it starts with `{` or `[`, treat as **JSON**.
- Otherwise, treat as **Binary**. (Until `BinaryReflector` is implemented, return an error: `"Binary payload format not yet supported"`).

---

## 2. Phase Breakdown

### Phase 1: Asset Types & Pipeline Scripts
- [x] **1.1 C++ Asset Types**:
  - In `src/core/resources/include/resources/asset_types.hpp`:
    - Add `Scene = 10` and `Material = 11` to `AssetType` (or `EAssetType`).
    - Define `struct SceneAsset : Asset` with `ASSET_TYPE = AssetType::Scene`, `std::vector<uint8_t> payload`, and `static const SceneAsset FALLBACK`.
    - Define `struct MaterialAsset : Asset` with `ASSET_TYPE = AssetType::Material`, shader handles, properties map, and `static const MaterialAsset FALLBACK`.
  - In `src/core/resources/asset_types.cpp`:
    - Define `SceneAsset::FALLBACK` and `MaterialAsset::FALLBACK`.
- [x] **1.2 Python Cooker & Metadata Scripts**:
  - In `src/editor/tools/utils/assets/types.py`:
    - Add `SCENE = 10` and `MATERIAL = 11` to `AssetType(enum.IntEnum)`.
  - In `src/editor/tools/gen_asset_meta_files.py`:
    - Map extensions `.scene`, `.scene.json` -> `AssetType.SCENE`.
    - Map extensions `.mat`, `.material`, `.mat.json` -> `AssetType.MATERIAL`.
  - In `src/editor/tools/cook_assets.py`:
    - Register `AssetType.SCENE: cooking.stream_file` and `AssetType.MATERIAL: cooking.stream_file` in `cook_funcs`.

---

### Phase 2: `SceneAsset` & `SceneFactory`
- [x] **2.1 `AssetPool<SceneAsset>` parsing**:
  - In `src/core/resources/asset_pool.hpp` / `src/core/resources/asset_pool.cpp`:
    - Specialize `AssetPool<SceneAsset>::parse_asset(...)`.
    - Check for JSON payload. If binary, return error.
    - Store raw bytes into `SceneAsset::payload`.
- [x] **2.2 `SceneFactory::instantiate_scene`**:
  - In `src/core/node_system/include/node_system/scene_factory.hpp` and `scene_factory.cpp`:
    - Declare and implement:
      ```cpp
      Result<std::unique_ptr<Scene>> instantiate_scene(AssetHandle<SceneAsset> scene_asset) const;
      ```
    - Check `is_json_payload(scene_asset->payload)`:
      - If JSON: parse with `nlohmann::json`, wrap with `JsonReadReflector`, call `load_scene(reader)`.
      - If Binary: return unsupported error (future: `BinaryReadReflector`).
    - Set the source handle in `Scene`: `scene->set_source_asset(scene_asset)`.
- [x] **2.3 Anchor Asset Handle in `Scene`**:
  - In `src/core/node_system/include/node_system/scene.hpp`:
    - Add `AssetHandle<SceneAsset> source_scene_asset;`.
    - Add setter/getter `set_source_asset(...)` / `get_source_asset()`.

---

### Phase 3: `MaterialAsset` & `MaterialPool` Serialization
- [x] **3.1 Material Serialization Structures**:
  - In `src/core/resources/include/resources/material.hpp`:
    - Define `enum class MaterialPropertyType : uint32_t { Float, Int, Vec2, Vec3, Vec4, Texture };`.
    - Define property serialization helper struct (capturing `name`, `type`, value variant / UUID).
- [x] **3.2 `MaterialPool` UUID tracking & Reflector methods**:
  - In `src/core/resources/include/resources/material_pool.hpp` and `material_pool.cpp`:
    - Add bidirectional lookup between `MaterialID` and `UUID` (e.g. `uuid_to_id`, `material_uuids`).
    - Add `MaterialID acquire_material(const AssetHandle<MaterialAsset>& asset);` to look up or register runtime materials from cached assets.
    - Add `UUID get_uuid(MaterialID material_id) const;`.
    - Implement `CReflector` methods on `MaterialPool`:
      ```cpp
      template <CReflector TReflector>
      requires (TReflector::is_writing())
      void save_material(TReflector& re, MaterialID material_id) const;

      template <CReflector TReflector>
      requires (!TReflector::is_writing())
      Result<MaterialID> load_material(TReflector& re, UUID material_uuid = {});
      ```
- [x] **3.3 `AssetPool<MaterialAsset>` parsing**:
  - In `src/core/resources/asset_pool.hpp` / `src/core/resources/asset_pool.cpp`:
    - Provide access to `AssetManager` in `AssetPool` (for nested asset acquisitions).
    - Specialize `AssetPool<MaterialAsset>::parse_asset(...)`:
      - Verify JSON payload.
      - Parse JSON, acquire vertex & fragment shaders (`VertexShaderAsset`, `FragmentShaderAsset`).
      - Parse properties: for texture properties, acquire `AssetHandle<ImageAsset>`.
      - Return initialized `MaterialAsset`.

---

### Phase 4: `DrawableManager` & Editor Migration
- [x] **4.1 `DrawableManager` material UUID serialization**:
  - In `src/core/node_system/include/node_system/managers/drawable_manager.hpp`:
    - Update `reflect(...)`:
      - Writing: get `material_pool->get_uuid(drawable.material_id)` and serialize `UUID`.
      - Reading: read `UUID material_uuid`; if valid, acquire `MaterialAsset` -> register with `material_pool->acquire_material` -> assign `drawable.material_id`.
- [x] **4.2 Editor Migration**:
  - In `src/editor/editor.cpp`:
    - Replace the manual `TextAsset` loading of `scene.json` with:
      ```cpp
      auto r_uuid = workspace_state->get_project_assets().resolve("scene.json", AssetType::Scene);
      auto r_scene_asset = asset_manager->acquire<SceneAsset>(r_uuid.value());
      auto r_scene = scene_factory->instantiate_scene(r_scene_asset.value());
      ```

---

## 3. Verification & Testing
- [x] Cook project assets via `python src/editor/tools/sync_assets.py`. Verify `.scene` and `.mat` files generate `.meta` files with types `10` and `11` and cook without error.
- [x] Run test suite (`ctest` or running `tests/asset_manager_test`).
- [x] Run Editor and verify `scene.json` loads cleanly through `SceneAsset` and `SceneFactory::instantiate_scene`.
- [x] Verify materials are assigned to `DrawableManager` via UUID and rendered properly.
