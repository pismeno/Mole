import argparse
import subprocess
import sys
import json
from pathlib import Path

from utils.assets import is_meta_file, get_meta_file_path

SCRIPT_DIR = Path(__file__).resolve().parent
GEN_META_SCRIPT = SCRIPT_DIR / "gen_asset_meta_files.py"
COOK_ASSETS_SCRIPT = SCRIPT_DIR / "cook_assets.py"

parser = argparse.ArgumentParser(
    description="Inspect given directory, cleanup lone .meta files, generate missing .meta files and cook them."
)
parser.add_argument("inspect_destination", type=Path, help="Destination folder for inspection")
parser.add_argument("cook_destination", type=Path, help="Destination folder for the cooked assets")
args = parser.parse_args()

to_gen_meta: list[str] = []

# Clean up orphaned .meta files
for meta_path in args.inspect_destination.rglob("*.meta"):
    if not meta_path.is_file():
        continue
        
    source_asset_path = meta_path.with_name(meta_path.name.removesuffix(".meta"))
    
    if not source_asset_path.is_file():
        print(f"Deleting lone .meta file '{meta_path}'")
        meta_path.unlink()

# Find existing assets that are missing .meta files
for path in args.inspect_destination.rglob("*"):
    if not path.is_file() or is_meta_file(path):
        continue

    if not get_meta_file_path(path).is_file():
        to_gen_meta.append(str(path))

# Run gen_asset_meta_files.py only if there are assets missing metadata
if to_gen_meta:
    print(f"Calling gen_asset_meta_files.py for {len(to_gen_meta)} assets without .meta files")
    try:
        result = subprocess.run(
            [sys.executable, str(GEN_META_SCRIPT), *to_gen_meta],
            capture_output=True,
            text=True,
            check=True
        )
        print("gen_asset_meta_files.py Output:\n", result.stdout)
    except subprocess.CalledProcessError as e:
        print(f"gen_asset_meta_files.py failed (exit {e.returncode}):\n{e.stderr}")
        sys.exit(e.returncode)

    cook_args = [str(args.cook_destination)] + to_gen_meta
    print("Calling cook_assets.py for newly generated assets")
    try:
        result = subprocess.run(
            [sys.executable, str(COOK_ASSETS_SCRIPT), *cook_args],
            capture_output=True,
            text=True,
            check=True
        )
        print("cook_assets.py Output:\n", result.stdout)
    except subprocess.CalledProcessError as e:
        print(f"cook_assets.py failed (exit {e.returncode}):\n{e.stderr}")
        sys.exit(e.returncode)

# Collect all currently referenced cooked UUIDs across metadata files
needed_assets: list[str] = []

for path in args.inspect_destination.rglob("*"):
    if is_meta_file(path):
        continue

    meta_path = get_meta_file_path(path)
    if not meta_path.is_file():
        continue

    try:
        with open(meta_path, "r", encoding="utf-8") as mf:
            mf_json = json.load(mf)
    except (json.JSONDecodeError, OSError) as e:
        print(f"Could not parse metadata file {meta_path}: {e}")
        continue

    if not isinstance(mf_json, dict):
        continue

    cooked_assets = mf_json.get("cooked_assets") or []
    for cooked_asset in cooked_assets:
        if isinstance(cooked_asset, dict):
            asset_uuid = cooked_asset.get("uuid")
            if asset_uuid is not None:
                needed_assets.append(asset_uuid)

needed_assets_set = set(needed_assets)
for path in args.cook_destination.rglob("*.asset"):
    if not path.is_file():
        continue

    if path.stem not in needed_assets_set:
        print(f"Deleting no longer needed {path}")
        path.unlink(missing_ok=True)