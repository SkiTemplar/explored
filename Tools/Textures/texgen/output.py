"""Escritura de PNG, manifiesto de importación y hoja de contacto."""

from __future__ import annotations

import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageOps

# Sufijo -> (clase de textura para Unreal, sRGB)
KINDS = {
    "BC": ("color", True),
    "N": ("normal", False),
    "ARH": ("masks", False),
    "M": ("masks", False),
}


def to_u8(a: np.ndarray) -> np.ndarray:
    return np.clip(a * 255.0 + 0.5, 0, 255).astype(np.uint8)


def encode(suffix: str, arr: np.ndarray) -> Image.Image:
    if suffix == "N":
        return Image.fromarray(to_u8(arr * 0.5 + 0.5), "RGB")
    if arr.ndim == 2:
        return Image.fromarray(to_u8(arr), "L")
    return Image.fromarray(to_u8(arr), "RGBA" if arr.shape[-1] == 4 else "RGB")


def texture_name(material: str, suffix: str) -> str:
    return f"T_{material}_{suffix}"


def write_maps(out: Path, material: str, maps: dict[str, np.ndarray]) -> list[str]:
    out.mkdir(parents=True, exist_ok=True)
    names = []
    for suffix, arr in maps.items():
        name = texture_name(material, suffix)
        encode(suffix, arr).save(out / f"{name}.png", optimize=False, compress_level=6)
        names.append(name)
    return names


def write_manifest(out: Path, entries: dict[str, dict]) -> Path:
    """textures.json: nombre -> {srgb, kind}. Lo lee Tools/Unreal/import_textures.py."""
    path = out / "textures.json"
    path.write_text(json.dumps(entries, indent=2, ensure_ascii=False, sort_keys=True) + "\n", encoding="utf-8")
    return path


# ---------------------------------------------------------------------------
# Hoja de contacto
# ---------------------------------------------------------------------------

def srgb_to_linear(c: np.ndarray) -> np.ndarray:
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def linear_to_srgb(c: np.ndarray) -> np.ndarray:
    c = np.clip(c, 0.0, 1.0)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)


LIGHT = np.array([-0.5, -0.55, 0.67])
LIGHT = LIGHT / np.linalg.norm(LIGHT)


def lit_preview(albedo: np.ndarray, normal: np.ndarray, ao: np.ndarray | None, rough: np.ndarray | None) -> np.ndarray:
    """Vista iluminada simple (sol arriba-izquierda + cielo) para juzgar relieve y color juntos.
    Normal en convención DirectX: y hacia abajo en la imagen."""
    lin = srgb_to_linear(albedo)
    ndl = np.clip((normal * LIGHT).sum(-1), 0.0, 1.0)
    ao = np.ones(ndl.shape) if ao is None else ao
    rough = np.full(ndl.shape, 0.8) if rough is None else rough
    h = LIGHT + np.array([0.0, 0.0, 1.0])
    h /= np.linalg.norm(h)
    ndh = np.clip((normal * h).sum(-1), 0.0, 1.0)
    shin = 2.0 / np.maximum(rough ** 4, 1e-3)
    spec = (1.0 - rough) ** 2 * ndh ** np.minimum(shin, 400.0) * 0.6
    sun = np.array([1.0, 0.95, 0.85]) * 1.05
    sky = np.array([0.55, 0.65, 0.8]) * 0.5
    color = lin * (sun * ndl[..., None] * (0.5 + 0.5 * ao[..., None]) + sky * ao[..., None]) + spec[..., None] * sun
    return linear_to_srgb(color)


def _font(size: int):
    for path in ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
                 "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf",
                 "C:/Windows/Fonts/arialbd.ttf"):
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            continue
    return ImageFont.load_default()


def _thumb(arr: np.ndarray, px: int, tiles: int = 1) -> Image.Image:
    if arr.ndim == 3 and arr.shape[-1] == 4:
        arr = arr[..., :3]
    img = Image.fromarray(to_u8(np.tile(arr, (tiles, tiles, 1) if arr.ndim == 3 else (tiles, tiles))))
    return img.convert("RGB").resize((px, px), Image.LANCZOS)


def contact_sheet(cards: list[dict], path: Path, title: str, cols: int = 5, big: int = 300) -> Path:
    """cards: [{name, info, lit, thumbs: [array, ...]}]; lit se muestra en 2×2 para ver el tileado
    y, debajo, en 4×4 reducido (vista lejana: delata la repetición) junto a los mapas."""
    small = big // 4
    pad = 14
    label_h = 44
    card_w = big
    card_h = label_h + big + small
    rows = (len(cards) + cols - 1) // cols
    head = 56
    W = pad + cols * (card_w + pad)
    H = head + rows * (card_h + pad) + pad
    sheet = Image.new("RGB", (W, H), (32, 34, 40))
    draw = ImageDraw.Draw(sheet)
    draw.text((pad, 14), title, fill=(240, 236, 226), font=_font(24))
    f_name, f_info = _font(17), _font(12)
    for i, card in enumerate(cards):
        cx = pad + (i % cols) * (card_w + pad)
        cy = head + (i // cols) * (card_h + pad)
        draw.text((cx, cy + 2), card["name"], fill=(250, 244, 230), font=f_name)
        draw.text((cx, cy + 24), card["info"], fill=(170, 172, 180), font=f_info)
        sheet.paste(_thumb(card["lit"], big, tiles=2), (cx, cy + label_h))
        sheet.paste(_thumb(card["lit"], small, tiles=4), (cx, cy + label_h + big))
        for j, th in enumerate(card["thumbs"][:3]):
            sheet.paste(_thumb(th, small), (cx + (j + 1) * small, cy + label_h + big))
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path, optimize=True)
    # Límite de 2 MB para versionarla: se recorta precisión por canal (6 y luego 5 bits). Una
    # paleta global de 256 colores falseaba tonos (manchas verdosas en la arena mojada).
    for bits in (6, 5):
        if path.stat().st_size <= 1_900_000:
            break
        ImageOps.posterize(sheet, bits).save(path, optimize=True)
    return path
