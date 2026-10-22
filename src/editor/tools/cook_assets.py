from pathlib import Path
from collections.abc import Callable
from typing import BinaryIO
import argparse
import json
import struct
import os

from utils.assets import AssetType, cooking, get_meta_file_path

HEADER_FORMAT = "<4sIII"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)
CURRENT_VERSION = 1
MAGIC = b"MOLE"

def write_asset_header(out_file: BinaryIO, asset_type: AssetType, payload_size: int) -> None:
    header = struct.pack(
        HEADER_FORMAT,
        MAGIC,
        CURRENT_VERSION,
        int(asset_type),
        payload_size
    )
    out_file.write(header)

parser = argparse.ArgumentParser(
    description="Cook assets based on .meta files of given files."
)

parser.add_argument(
    "destination",
    type=Path,
    help="Destination folder for the cooked assets"
)

parser.add_argument(
    "files",
    nargs="+",
    type=Path,
    help="One or more file paths to process",
)

args = parser.parse_args()

cook_funcs: dict[AssetType, Callable[[Path], bytes]] = {
    AssetType.IMAGE : cooking.stream_file,
    AssetType.TEXT : cooking.stream_file,
    AssetType.GENERIC_BINARY : cooking.stream_file,
    AssetType.VERTEX_SHADER : cooking.compile_glsl_to_spirv,
    AssetType.FRAGMENT_SHADER : cooking.compile_glsl_to_spirv,
    AssetType.COMPUTE_SHADER : cooking.compile_glsl_to_spirv,
    AssetType.GEOMETRY_SHADER : cooking.compile_glsl_to_spirv,
    AssetType.TESS_CONTROL_SHADER : cooking.compile_glsl_to_spirv,
    AssetType.TESS_EVALUATION_SHADER : cooking.compile_glsl_to_spirv,
}

for file_path_str in args.files:
    file_path = Path(file_path_str)
    meta_file_path = get_meta_file_path(file_path)

    if not file_path.is_file():
        print(str(file_path) + " is not a file")
        continue
    if not meta_file_path.is_file():
        print(str(file_path) + " does not have a .meta file")
        continue

    try:
        with open(meta_file_path, "r", encoding="utf-8") as mf:
            mf_json = json.load(mf)
    except (json.JSONDecodeError, OSError, UnicodeDecodeError) as e:
        print(f"Skipping {file_path}: Could not read or parse meta file: {e}")
        continue

    if not isinstance(mf_json, dict):
        print(f"Skipping {file_path}: Meta file root is not a JSON object")
        continue

    cooked_assets = mf_json.get("cooked_assets")
    if not isinstance(cooked_assets, list):
        print(f"Skipping {file_path}: 'cooked_assets' missing or not a list")
        continue

    for cooked_asset in cooked_assets:
        if not isinstance(cooked_asset, dict):
            print(f"Skipping invalid asset entry in {meta_file_path}: not an object")
            continue

        asset_uuid = cooked_asset.get("uuid")
        if not isinstance(asset_uuid, str) or not asset_uuid.strip():
            print(f"Skipping invalid asset entry in {meta_file_path}: missing or invalid 'uuid'")
            continue

        asset_type_val = cooked_asset.get("type")
        try:
            asset_type = AssetType(asset_type_val)
        except ValueError:
            print(f"Skipping invalid asset type '{asset_type_val}' in {meta_file_path}")
            continue

        if asset_type == AssetType.UNKNOWN:
            print(f"Invalid asset type: UNKNOWN in {meta_file_path}")
            continue

        cook_fn = cook_funcs.get(asset_type)
        if not cook_fn:
            print(f"No cook function for {asset_type.name}")
            continue

        dst_path = args.destination / (asset_uuid + ".asset")
        dst_tmp_path = dst_path.with_name(dst_path.name + ".tmp")

        args.destination.mkdir(parents=True, exist_ok=True)

        try:
            with open(dst_tmp_path, "wb") as f_out:
                print(f"Cooking {file_path} to {dst_path}")
                # Write dummy bytes to reserve space for the header
                f_out.write(b'\x00' * HEADER_SIZE)
                
                cook_fn(file_path, f_out)
                total_size = f_out.tell()
                payload_size = total_size - HEADER_SIZE
                
                # Go back to the very beginning of the file and write the real header
                f_out.seek(0)
                write_asset_header(f_out, asset_type, payload_size)

            os.replace(dst_tmp_path, dst_path)
        except Exception as e:
            print(f"Failed to cook {file_path}: {e}")
            if dst_tmp_path.exists():
                dst_tmp_path.unlink(missing_ok=True)