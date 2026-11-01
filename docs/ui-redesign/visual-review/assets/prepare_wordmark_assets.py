"""Preserve supplied wordmarks and crop review derivatives without recoloring."""

import argparse
import hashlib
import json
import shutil
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare(mode, supplied):
    original = ROOT / "originals" / f"wordmark-{mode}.png"
    source = Path(supplied) if supplied else original
    original.parent.mkdir(exist_ok=True)
    source_hash = digest(source)
    if original.exists():
        if digest(original) != source_hash:
            raise ValueError(f"Refusing to overwrite a different {mode} original")
    else:
        shutil.copy2(source, original)
    with Image.open(original) as image:
        image = image.convert("RGBA")
        alpha = image.getchannel("A")
        # Very faint isolated pixels outside the artwork should not set its size.
        # Retain all original pixels inside the padded crop, including texture.
        bounds = alpha.point(lambda value: 255 if value >= 8 else 0).getbbox()
        if bounds is None:
            raise ValueError(f"No visible artwork in {mode}")
        left, top, right, bottom = bounds
        crop = (max(0, left - 4), max(0, top - 4),
                min(image.width, right + 4), min(image.height, bottom + 4))
        derivative = image.crop(crop)
        output = ROOT / f"wordmark-{mode}.png"
        derivative.save(output, optimize=True)
        record = {
            "mode": mode, "original": str(original.relative_to(ROOT)),
            "originalSize": list(image.size), "originalMode": image.mode,
            "originalSha256": source_hash, "alphaRange": list(alpha.getextrema()),
            "visibleAlphaThreshold": 8, "padding": 4, "crop": list(crop),
            "output": output.name, "outputSize": list(derivative.size),
            "outputSha256": digest(output),
            "processing": "Crop only; original RGBA pixels inside crop preserved",
        }
    if digest(source) != source_hash or digest(original) != source_hash:
        raise ValueError("Original changed during preparation")
    return record


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--day-source")
    parser.add_argument("--night-source")
    args = parser.parse_args()
    result = [prepare("day", args.day_source), prepare("night", args.night_source)]
    (ROOT / "wordmark-assets-metadata.json").write_text(
        json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))
