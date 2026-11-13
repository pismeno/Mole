import argparse
import json
import uuid
from pathlib import Path

import puremagic
from utils.assets import AssetType, get_meta_file_path


def append_type(processed_asset_list: list[dict], old_cooked_assets: list[dict], type: AssetType) -> None:
    for old_asset in old_cooked_assets:
        if old_asset["type"] == type.value:
            processed_asset_list.append(old_asset)
            return

    processed_asset_list.append({
        "uuid" : str(uuid.uuid4()),
        "type" : int(type)
    })

parser = argparse.ArgumentParser(
    description="Generate .meta files for given files."
)

parser.add_argument(
    "files",
    nargs="+",
    type=Path,
    help="One or more file paths to process",
)

args = parser.parse_args()

for file_path_str in args.files:
    file_path = Path(file_path_str)
    print(f"Processing resolved path: {file_path}")

    if not file_path.exists():
        continue

    if file_path.stat().st_size == 0:
        print(f"Skipping {file_path} (file is currently empty)")
        continue

    old_cooked_assets: list[dict] = []
    meta_file_path = get_meta_file_path(file_path)

    if meta_file_path.is_file():
        try:
            with open(meta_file_path, "r", encoding="utf-8") as f:
                meta_json = json.loads(f.read())
                old_cooked_assets = meta_json.get("cooked_assets", [])
        except (json.JSONDecodeError, OSError) as e:
            print(f"Warning: Failed to read existing meta file for {file_path}: {e}")

    try:
        mime_type = puremagic.from_file(str(file_path), mime=True)
    except (puremagic.PureError, ValueError) as e:
        print(f"Could not identify file type for {file_path}: {e}")
        mime_type = None

    cooked_assets: list[dict] = []
    name_lower = file_path.name.lower()

    if name_lower.endswith(".scene") or name_lower.endswith(".scene.json"):
        append_type(cooked_assets, old_cooked_assets, AssetType.SCENE)
    elif name_lower.endswith(".mat") or name_lower.endswith(".material") or name_lower.endswith(".mat.json"):
        append_type(cooked_assets, old_cooked_assets, AssetType.MATERIAL)
    elif mime_type:
        match mime_type.split("/"):
            case ["image", "png" | "jpeg" | "jpg"]:
                append_type(cooked_assets, old_cooked_assets, AssetType.IMAGE)
            case ["text", _] | ["application", "json"]:
                if file_path.name.endswith(".glsl"):
                    file_name_ns = file_path.name.removesuffix(".glsl")
                    if file_name_ns.endswith(".vert"):
                        append_type(cooked_assets, old_cooked_assets, AssetType.VERTEX_SHADER)
                    elif file_name_ns.endswith(".frag"):
                        append_type(cooked_assets, old_cooked_assets, AssetType.FRAGMENT_SHADER)
                    elif file_name_ns.endswith(".comp"):
                        append_type(cooked_assets, old_cooked_assets, AssetType.COMPUTE_SHADER)
                    elif file_name_ns.endswith(".geom"):
                        append_type(cooked_assets, old_cooked_assets, AssetType.GEOMETRY_SHADER)
                    elif file_name_ns.endswith(".tesc"):
                        append_type(cooked_assets, old_cooked_assets, AssetType.TESS_CONTROL_SHADER)
                    elif file_name_ns.endswith(".tese"):
                        append_type(cooked_assets, old_cooked_assets, AssetType.TESS_EVALUATION_SHADER)
                else:
                    append_type(cooked_assets, old_cooked_assets, AssetType.TEXT)
            case _:
                print(f"Unknown type by Mole engine for file {file_path}")
    else:
        print(f"Could not identify file type for {file_path}")

    meta_content = { "cooked_assets" : cooked_assets }

    with open(meta_file_path, "w", encoding="utf-8") as mf:
        print(f"Writing meta file for {file_path}")
        mf.write(json.dumps(meta_content))
