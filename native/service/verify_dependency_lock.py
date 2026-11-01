"""Check the pinned Windows StreamGet build environment before packaging."""

import argparse
import platform
import sys
from collections import deque
from importlib import metadata
from pathlib import Path

from packaging.requirements import Requirement
from packaging.utils import canonicalize_name


ROOT = Path(__file__).resolve().parent
PYTHON_VERSION = (3, 13, 15)


def pinned_requirements(path: Path) -> dict[str, str]:
    pins: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        text = line.split("#", 1)[0].strip()
        if not text:
            continue
        requirement = Requirement(text)
        specs = list(requirement.specifier)
        if (requirement.url or requirement.marker or requirement.extras or len(specs) != 1
                or specs[0].operator != "==" or "*" in specs[0].version):
            raise ValueError(f"Not an exact version pin in {path.name}: {text}")
        name = canonicalize_name(requirement.name)
        if name in pins:
            raise ValueError(f"Duplicate pin in {path.name}: {name}")
        pins[name] = specs[0].version
    return pins


def verify(lock_path: Path | None = None) -> list[str]:
    errors: list[str] = []
    if platform.system() != "Windows" or sys.version_info[:3] != PYTHON_VERSION:
        errors.append("Requires Windows and CPython 3.13.15")
        return errors

    lock = pinned_requirements(lock_path or ROOT / "requirements-lock-win-py313.txt")
    runtime = pinned_requirements(ROOT / "requirements.txt")
    build = pinned_requirements(ROOT / "requirements-build.txt")
    for name, version in {**runtime, **build}.items():
        if lock.get(name) != version:
            errors.append(f"Direct requirement differs from lock: {name}=={version}")

    # Follow environment markers and requested extras (streamget needs httpx[http2]).
    pending = deque((name, ()) for name in runtime | build)
    visited: set[tuple[str, tuple[str, ...]]] = set()
    required: set[str] = {"pip"}
    while pending:
        name, extras = pending.popleft()
        key = (name, tuple(sorted(extras)))
        if key in visited:
            continue
        visited.add(key)
        required.add(name)
        try:
            distribution = metadata.distribution(name)
        except metadata.PackageNotFoundError:
            errors.append(f"Missing installed dependency: {name}")
            continue
        version = distribution.version
        if lock.get(name) != version:
            errors.append(f"Installed version differs from lock: {name}=={version}")
        for raw in distribution.requires or ():
            requirement = Requirement(raw)
            if requirement.marker and not any(
                requirement.marker.evaluate({"extra": extra}) for extra in ("", *extras)
            ):
                continue
            child = canonicalize_name(requirement.name)
            if version_not_compatible(child, requirement):
                errors.append(f"Incompatible dependency requirement: {name} needs {requirement}")
            pending.append((child, tuple(requirement.extras)))

    if set(lock) != required:
        errors.append(f"Lock closure differs: missing={sorted(required - set(lock))}, "
                      f"unused={sorted(set(lock) - required)}")
    try:
        pip_version = metadata.version("pip")
    except metadata.PackageNotFoundError:
        pip_version = "missing"
    if lock.get("pip") != pip_version:
        errors.append(f"Installed pip differs from lock: {pip_version}")
    return errors


def version_not_compatible(name: str, requirement: Requirement) -> bool:
    try:
        return metadata.version(name) not in requirement.specifier
    except metadata.PackageNotFoundError:
        return False  # The missing dependency is reported by the traversal.


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lock-path", type=Path, default=None,
                        help="override lock path for isolated verifier tests")
    args = parser.parse_args()
    failures = verify(args.lock_path)
    for failure in failures:
        print(failure, file=sys.stderr)
    if not failures:
        print("StreamGet dependency lock verified")
    sys.exit(bool(failures))
