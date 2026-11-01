"""Write a hash manifest for a freshly built StreamGet onedir package."""

import argparse
import hashlib
import json
import platform
import subprocess
import sys
from importlib import metadata
from pathlib import Path
from typing import Any


MANIFEST_NAME = "build-provenance.json"
INPUT_PATHS = (
    "service/streamget_service.py",
    "service/protocol.py",
    "service/douyu_backend.py",
    "service/requirements.txt",
    "service/requirements-build.txt",
    "service/requirements-lock-win-py313.txt",
    "service/verify_dependency_lock.py",
    "service/package_provenance.py",
    "scripts/bootstrap-streamget-service.ps1",
    "scripts/build-streamget-service.ps1",
    "app/resources/hamster_agent_roster.json",
)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _git_value(repo_root: Path, *arguments: str) -> str | None:
    try:
        result = subprocess.run(
            ["git", "-C", str(repo_root), *arguments],
            check=True,
            capture_output=True,
            text=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return None
    return result.stdout.strip()


def _worktree_dirty(repo_root: Path) -> bool | None:
    checks = (
        ("diff", "--quiet", "--ignore-submodules", "--"),
        ("diff", "--cached", "--quiet", "--ignore-submodules", "--"),
    )
    for arguments in checks:
        try:
            result = subprocess.run(
                ["git", "-C", str(repo_root), *arguments],
                check=False,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
        except OSError:
            return None
        if result.returncode not in (0, 1):
            return None
        if result.returncode == 1:
            return True

    untracked = _git_value(repo_root, "ls-files", "--others", "--exclude-standard")
    return None if untracked is None else bool(untracked)


def _package_files(package_dir: Path) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    for path in sorted(package_dir.rglob("*")):
        if not path.is_file() or path.name == MANIFEST_NAME:
            continue
        relative_path = path.relative_to(package_dir).as_posix()
        result[relative_path] = {
            "bytes": path.stat().st_size,
            "sha256": sha256_file(path),
        }
    return result


def create_manifest(native_root: Path, package_dir: Path) -> dict[str, Any]:
    native_root = native_root.resolve()
    package_dir = package_dir.resolve()
    repo_root = native_root.parent

    missing_inputs = [
        relative_path
        for relative_path in INPUT_PATHS
        if not (native_root / relative_path).is_file()
    ]
    if missing_inputs:
        raise FileNotFoundError(f"Missing StreamGet provenance inputs: {missing_inputs}")
    if not (package_dir / "streamget_service.exe").is_file():
        raise FileNotFoundError("PyInstaller output is missing streamget_service.exe")

    files = _package_files(package_dir)
    if not files:
        raise ValueError("PyInstaller output package is empty")

    return {
        "schemaVersion": 1,
        "inputs": {
            relative_path: sha256_file(native_root / relative_path)
            for relative_path in INPUT_PATHS
        },
        "files": files,
        "build": {
            "gitBranch": _git_value(repo_root, "rev-parse", "--abbrev-ref", "HEAD"),
            "gitHead": _git_value(repo_root, "rev-parse", "HEAD"),
            "worktreeDirty": _worktree_dirty(repo_root),
            "pythonImplementation": platform.python_implementation(),
            "pythonVersion": platform.python_version(),
            "platform": platform.platform(),
            "streamgetVersion": metadata.version("streamget"),
            "pyinstallerVersion": metadata.version("pyinstaller"),
            "pipVersion": metadata.version("pip"),
        },
    }


def write_manifest(native_root: Path, package_dir: Path) -> Path:
    manifest = create_manifest(native_root, package_dir)
    output_path = package_dir / MANIFEST_NAME
    output_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return output_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-root", type=Path, required=True)
    parser.add_argument("--package-dir", type=Path, required=True)
    args = parser.parse_args()
    try:
        manifest_path = write_manifest(args.native_root, args.package_dir)
    except (OSError, ValueError, metadata.PackageNotFoundError) as error:
        print(f"StreamGet provenance generation failed: {error}", file=sys.stderr)
        return 1
    print(manifest_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
