from pathlib import Path

def get_meta_file_path(file_path: Path) -> Path:
    return file_path.with_name(file_path.name + ".meta")

def is_meta_file(file_path: Path) -> bool:
    return file_path.name.lower().endswith(".meta")