"""Máscara de trazos estilizados del terreno: T_TerrainStrokes (escala de grises, tileable, sin fotos).

Fondo gris medio (0,5) con `strokes` trazos cortos de punta redonda (cápsulas), orientados en
torno a `angle_deg` (hierba peinada por el viento), la mitad oscuros y la mitad claros, con borde
suavizado de un píxel. Se dibujan en coordenadas toroidales, así que la textura se repite sin
costuras; al final se recentra a media 0,5 para que el material no aclare ni oscurezca de media.

Los parámetros (y los que usa el material: escalas en metros, factores oscuro y claro, fundidos por
pendiente y distancia) están en Tools/Textures/strokes.json, que leen también
Tools/Unreal/materials_f6.py y la vista previa de Tools/Island.

Uso: uv run textures-strokes [--config strokes.json] [--out Art/Export/Textures/T_TerrainStrokes.png]
"""

from __future__ import annotations

import argparse
import json
import struct
import zlib
from pathlib import Path

import numpy as np

TOOL_DIR = Path(__file__).resolve().parents[1]
REPO_ROOT = TOOL_DIR.parents[1]
CONFIG = TOOL_DIR / 'strokes.json'
OUT_DIR = REPO_ROOT / 'Art' / 'Export' / 'Textures'


def load_config(path: Path = CONFIG) -> dict:
    config = json.loads(path.read_text(encoding='utf-8'))
    for key in ('size', 'seed', 'strokes', 'length_px', 'width_px', 'angle_deg', 'angle_spread_deg', 'dark',
                'light'):
        if key not in config:
            raise ValueError(f'{path.name}: falta «{key}»')
    if config['size'] < 16 or config['size'] & (config['size'] - 1):
        raise ValueError(f'{path.name}: «size» debe ser potencia de dos ≥ 16')
    return config


def capsule_coverage(px: np.ndarray, py: np.ndarray, a: np.ndarray, b: np.ndarray, radius: float) -> np.ndarray:
    """Cobertura 0-1 de una cápsula (segmento a-b de radio «radius») con borde suavizado de 1 px."""
    vx, vy = b[0] - a[0], b[1] - a[1]
    length_sq = max(vx * vx + vy * vy, 1e-9)
    t = np.clip(((px - a[0]) * vx + (py - a[1]) * vy) / length_sq, 0.0, 1.0)
    d = np.hypot(px - (a[0] + t * vx), py - (a[1] + t * vy))
    return np.clip(radius + 0.5 - d, 0.0, 1.0)


def draw_stroke(canvas: np.ndarray, center: tuple[float, float], angle: float, length: float, width: float,
                value: float) -> np.ndarray:
    """Copia de «canvas» con un trazo dibujado en coordenadas toroidales (sin costuras)."""
    size = canvas.shape[0]
    half = length * 0.5
    dx, dy = np.cos(angle) * half, np.sin(angle) * half
    reach = int(np.ceil(half + width)) + 2
    cx, cy = int(np.floor(center[0])), int(np.floor(center[1]))
    xs = np.arange(cx - reach, cx + reach + 1)
    ys = np.arange(cy - reach, cy + reach + 1)
    gx, gy = np.meshgrid(xs + 0.5, ys + 0.5)
    a = np.array([center[0] - dx, center[1] - dy])
    b = np.array([center[0] + dx, center[1] + dy])
    cover = capsule_coverage(gx, gy, a, b, width * 0.5)
    out = canvas.copy()
    rows, cols = np.ix_(ys % size, xs % size)
    out[rows, cols] = out[rows, cols] * (1.0 - cover) + value * cover
    return out


def generate(config: dict) -> np.ndarray:
    """Máscara (size, size) en 0-1, media 0,5."""
    size = int(config['size'])
    rng = np.random.default_rng(int(config['seed']))
    canvas = np.full((size, size), 0.5)
    base = np.radians(float(config['angle_deg']))
    spread = np.radians(float(config['angle_spread_deg']))
    for _ in range(int(config['strokes'])):
        dark = rng.random() < 0.5
        lo, hi = config['dark'] if dark else config['light']
        canvas = draw_stroke(canvas, (rng.uniform(0, size), rng.uniform(0, size)),
                             base + rng.uniform(-spread, spread), rng.uniform(*config['length_px']),
                             rng.uniform(*config['width_px']), rng.uniform(lo, hi))
    return np.clip(canvas - canvas.mean() + 0.5, 0.0, 1.0)


def encode_gray_png(image: np.ndarray) -> bytes:
    """PNG de 8 bits en escala de grises (filtro 0 por fila) a partir de valores 0-1."""
    data = np.clip(np.rint(image * 255.0), 0, 255).astype(np.uint8)
    h, w = data.shape
    raw = b''.join(b'\x00' + data[row].tobytes() for row in range(h))

    def chunk(tag: bytes, payload: bytes) -> bytes:
        return struct.pack('>I', len(payload)) + tag + payload + struct.pack('>I', zlib.crc32(tag + payload))

    header = struct.pack('>IIBBBBB', w, h, 8, 0, 0, 0, 0)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog='textures-strokes', description='Genera la máscara de trazos del terreno.')
    parser.add_argument('--config', type=Path, default=CONFIG)
    parser.add_argument('--out', type=Path, default=None, help='PNG de salida (por defecto Art/Export/Textures)')
    args = parser.parse_args(argv)
    config = load_config(args.config)
    out = args.out if args.out is not None else OUT_DIR / f"{config['texture']}.png"
    image = generate(config)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(encode_gray_png(image))
    print(f'[strokes] {out}: {image.shape[1]} x {image.shape[0]}, media {image.mean():.3f}, '
          f'desviación {image.std():.3f}')
    return 0


if __name__ == '__main__':  # pragma: no cover
    raise SystemExit(main())
