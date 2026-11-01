"""Create review-only brand derivatives; never modify the supplied originals."""

from collections import deque
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[4]
OUT = Path(__file__).resolve().parent
SOURCES = {
    "app": ROOT / "icons" / "软件图标.png",
    "navigation": ROOT / "icons" / "导航页图标.png",
    "ranking": ROOT / "icons" / "榜单图标.png",
}
ATTACHMENTS = {
    "app": Path("C:/Users/15070/AppData/Local/Temp/codex-clipboard-c3b5a954-5f6d-49df-a524-5d0c9d1ab845.png"),
    "navigation": Path("C:/Users/15070/AppData/Local/Temp/codex-clipboard-026d552c-a252-4bc5-aa1c-9f7dfa8598aa.png"),
    "ranking": Path("C:/Users/15070/AppData/Local/Temp/codex-clipboard-33938a91-d62f-4a59-b857-3806fd0c2e5c.png"),
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def alpha_stats(image):
    alpha = np.asarray(image.convert("RGBA"))[:, :, 3]
    return {
        "zero": int((alpha == 0).sum()),
        "partial": int(((alpha > 0) & (alpha < 255)).sum()),
        "opaque": int((alpha == 255).sum()),
        "range": [int(alpha.min()), int(alpha.max())],
    }


def exterior_light_neutral_mask(rgb, threshold=215):
    """Only pixels connected to an image border can become transparent.

    A four-connected flood cannot cross the dark outline or saturated red art.
    It intentionally leaves enclosed checker pixels and interior white intact.
    """
    wide = rgb.astype(np.int16)
    eligible = (wide.min(axis=2) >= threshold) & (
        wide.max(axis=2) - wide.min(axis=2) <= 24
    )
    height, width = eligible.shape
    reached = np.zeros_like(eligible, dtype=bool)
    queue = deque()
    for x in range(width):
        for y in (0, height - 1):
            if eligible[y, x] and not reached[y, x]:
                reached[y, x] = True
                queue.append((x, y))
    for y in range(height):
        for x in (0, width - 1):
            if eligible[y, x] and not reached[y, x]:
                reached[y, x] = True
                queue.append((x, y))
    while queue:
        x, y = queue.popleft()
        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= nx < width and 0 <= ny < height:
                if eligible[ny, nx] and not reached[ny, nx]:
                    reached[ny, nx] = True
                    queue.append((nx, ny))
    return reached


def composite(image, size, color):
    panel = Image.new("RGBA", size, color)
    panel.alpha_composite(image, ((size[0] - image.width) // 2, (size[1] - image.height) // 2))
    return panel


source_hashes = {name: sha256(path) for name, path in SOURCES.items()}
images = {name: Image.open(path).convert("RGBA") for name, path in SOURCES.items()}
app_source = images["app"]
app_pixels = np.asarray(app_source).copy()
mask = exterior_light_neutral_mask(app_pixels[:, :, :3])
app_pixels[mask, 3] = 0
app_clean = Image.fromarray(app_pixels, "RGBA")
images["app"] = app_clean

# Keep the full source canvas and existing artwork alpha. No crop, recolor,
# rematting, new stroke, sharpening, redrawing, or thresholding other icons.
outputs = {}
for name, image in images.items():
    path = OUT / f"icon-{name}.png"
    derivative = image.resize((160, 160), Image.Resampling.LANCZOS)
    derivative.save(path, optimize=True, compress_level=9)
    outputs[name] = derivative

font = ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 16)
small_font = ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 13)
sheet = Image.new("RGB", (1040, 784), "#e8edf2")
draw = ImageDraw.Draw(sheet)
draw.text((24, 18), "CSTG brand derivatives | actual-size 32 / 48 / 72 px", fill="#18202b", font=font)
draw.text((24, 44), "160px PNG masters; source canvas retained. Dark and light surfaces.", fill="#4c5664", font=small_font)
sizes = (32, 48, 72)
for index, (name, original) in enumerate(images.items()):
    for surface, color, label_color in ((0, "#171b22", "#e5eaf1"), (1, "#f5f7fa", "#344050")):
        y = 84 + index * 224 + surface * 106
        draw.rounded_rectangle((20, y, 1020, y + 100), radius=10, fill=color)
        draw.text((40, y + 12), f"{name} / {'dark' if surface == 0 else 'light'}", fill=label_color, font=font)
        for column, size in enumerate(sizes):
            x = 290 + column * 245
            icon = outputs[name].resize((size, size), Image.Resampling.LANCZOS)
            sheet.paste(icon, (x + (80 - size) // 2, y + (88 - size) // 2), icon)
            draw.text((x + 96, y + 39), f"{size}px", fill=label_color, font=small_font)
draw.text((24, 766), "Inspection: software exterior checker removed; navigation/ranking original alpha preserved.", fill="#4c5664", font=small_font)
sheet.save(OUT / "brand-icons-size-comparison.png", optimize=True)

detail = Image.new("RGB", (1100, 620), "#e8edf2")
detail_draw = ImageDraw.Draw(detail)
detail_draw.text((24, 14), "Software icon | supplied RGB vs exterior-only transparency", fill="#18202b", font=font)
for column, (image, label) in enumerate(((app_source, "Supplied RGB / baked checker"), (app_clean, "Exterior checker removed / dark surface"))):
    x = 24 + column * 550
    panel = composite(image.resize((500, 500), Image.Resampling.LANCZOS), (500, 500), "#171b22").convert("RGB")
    detail.paste(panel, (x, 60))
    detail_draw.text((x, 573), label, fill="#344050", font=small_font)
detail.save(OUT / "app-background-removal-comparison.png", optimize=True)

source_core = (app_pixels[:, :, :3].max(axis=2) < 180) | (
    app_pixels[:, :, :3].max(axis=2).astype(np.int16)
    - app_pixels[:, :, :3].min(axis=2).astype(np.int16) > 40
)
metadata = {
    "created": "2026-10-29",
    "masterSize": [160, 160],
    "intendedCssSize": "48-80px; 2x raster density at 80px",
    "transforms": {
        "app": "Exterior four-connected light neutral flood: min(RGB)>=215 and max(RGB)-min(RGB)<=24. Only alpha changed before Lanczos downsampling.",
        "navigation": "Existing alpha retained, Lanczos downsampling, lossless PNG compression.",
        "ranking": "Existing alpha including gray halo retained, Lanczos downsampling, lossless PNG compression.",
    },
    "appMask": {
        "removedPixels": int(mask.sum()),
        "removedPercent": round(float(mask.mean() * 100), 4),
        "darkOrSaturatedCorePixelsRemoved": int((mask & source_core).sum()),
        "centerFaceRoiRemoved": int(mask[300:650, 270:750].sum()),
        "sourceRgbChangedBeforeResampling": int(np.any(np.asarray(app_source)[:, :, :3] != app_pixels[:, :, :3], axis=2).sum()),
        "retainedAlphaBBox": list(app_clean.getbbox()),
    },
    "sources": {},
}
for name, path in SOURCES.items():
    source = Image.open(path)
    attachment = ATTACHMENTS[name]
    output = OUT / f"icon-{name}.png"
    metadata["sources"][name] = {
        "path": str(path.relative_to(ROOT)).replace("\\", "/"),
        "mode": source.mode,
        "size": list(source.size),
        "bytes": path.stat().st_size,
        "sha256Before": source_hashes[name],
        "sha256After": sha256(path),
        "attachmentFile": attachment.name,
        "attachmentSha256": sha256(attachment),
        "attachmentMatches": sha256(attachment) == source_hashes[name],
        "alpha": alpha_stats(source),
        "derivative": output.name,
        "derivativeBytes": output.stat().st_size,
        "derivativeSha256": sha256(output),
        "derivativeAlpha": alpha_stats(outputs[name]),
    }

(OUT / "brand-assets-metadata.json").write_text(
    json.dumps(metadata, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
)
print(json.dumps(metadata, ensure_ascii=True, indent=2))
assert all(item["sha256Before"] == item["sha256After"] for item in metadata["sources"].values())
assert all(item["attachmentMatches"] for item in metadata["sources"].values())
assert metadata["appMask"]["darkOrSaturatedCorePixelsRemoved"] == 0
assert metadata["appMask"]["centerFaceRoiRemoved"] == 0
