"""Fotobasheado estilizado de roca: parte de fotografías CC0 de Poly Haven (ver
`fetch_polyhaven.py`, que rellena la caché en `.cache/polyhaven/<asset>/`) y las lleva
al mismo estilo cartoon pintado a mano que el resto de `texgen/materials.py`.

Sustituye al basalto/caliza procedurales (Voronoi anisótropo): a la escala de tile de
Explored (3 m), las celdas de una rejilla —por irregular que sea su Voronoi— siempre
delatan una rejilla. Una fotografía real ya trae la variedad de forma que cuesta fingir.

Pipeline, todo con borde periódico (mantiene el tileado del original):
  1. Cargar diffuse/displacement/AO/roughness a `size` px.
  2. «Delit»: dividir por una versión muy desenfocada de la propia luminancia para quitar
     la iluminación desigual de la foto (deja solo relieve medio/fino).
  3. Filtro de Kuwahara (4 cuadrantes, óleo): quita el detalle de alta frecuencia
     conservando bordes vivos — el aspecto pintado a mano.
  4. Paleta propia por `ramp()` sobre la luminancia ya delit, con un resto de la
     crominancia original de baja opacidad para que no quede un degradado sintético
     plano; vetas oscuras en las grietas (AO real) y aristas claras (cavidad de la
     altura); un acento cálido opcional donde la foto ya era cálida (liquen/óxido).
  5. `macro_variation()` al final, igual que el resto de materiales (misma cantidad ya
     validada por `test_macro.py`, en vez de fiarse del contraste propio de la foto).
  6. Altura = displacement delit y suavizado; normal derivada de una versión de la
     altura con más desenfoque todavía («que la normal case con las formas grandes»,
     no con los poros de la foto de origen).
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image

from .noise import blur, height_to_normal, macro_variation, mix_color, ramp

CACHE = Path(__file__).resolve().parent.parent / ".cache" / "polyhaven"
LUMA = np.array([0.2126, 0.7152, 0.0722])


@dataclass(frozen=True)
class RockStyle:
    stops: list[tuple[float, str]]  # paleta objetivo, de oscuro a claro
    vein: str  # tono de grieta/óxido oscuro
    warm_accent: str | None  # acento cálido (liquen/óxido) donde la foto ya era cálida
    macro_warm: str
    macro_cool: str
    chroma: float  # cuánta crominancia original de la foto se reintroduce (0-1)
    depth: float
    rough_lo: float
    rough_hi: float


# Gris kárstico con vetas oscuras y toques ocres — referencia El Nido / Ha Long, no crema.
LIMESTONE_STYLE = RockStyle(
    stops=[(0.0, "#5f6260"), (0.35, "#868985"), (0.65, "#aeb0ab"), (1.0, "#dcded7")],
    vein="#2c2a28",
    warm_accent="#a9793c",
    macro_warm="#c7b98e",
    macro_cool="#8c959a",
    chroma=0.4,
    depth=0.028,
    rough_lo=0.5,
    rough_hi=0.92,
)

# Basalto gris violáceo oscuro.
VOLCANIC_STYLE = RockStyle(
    stops=[(0.0, "#2e2c3d"), (0.35, "#413e52"), (0.65, "#5a5569"), (1.0, "#756f85")],
    vein="#100f18",
    warm_accent=None,
    macro_warm="#7a5a52",
    macro_cool="#4f5a78",
    chroma=0.12,
    depth=0.034,
    rough_lo=0.55,
    rough_hi=0.95,
)


def _load(path: Path, size: int, color: bool) -> np.ndarray:
    img = Image.open(path)
    if color:
        img = img.convert("RGB").resize((size, size), Image.LANCZOS)
        return np.asarray(img, dtype=np.float64) / 255.0
    # Los mapas de Poly Haven (displacement/AO/roughness) vienen en gris de 16 bits
    # (modo Pillow "I;16"): `.convert("L")` los trunca mal (queda casi blanco plano),
    # así que se normaliza por el rango real del dtype y se pasa a modo "F" (float) antes
    # de redimensionar, en vez de fiarse del resize de Pillow sobre un entero de 16 bits.
    raw = np.asarray(img, dtype=np.float64)
    maxval = 65535.0 if raw.max() > 255.0 else 255.0
    img_f = Image.fromarray((raw / maxval).astype(np.float32), mode="F")
    img_f = img_f.resize((size, size), Image.LANCZOS)
    return np.asarray(img_f, dtype=np.float64)


def _cavity(height: np.ndarray, radius: float) -> np.ndarray:
    return height - blur(height, radius)


def _percentile_stretch(field: np.ndarray, lo: float = 2.0, hi: float = 98.0) -> np.ndarray:
    a, b = np.percentile(field, [lo, hi])
    if b - a < 1e-6:
        return np.clip(field, 0.0, 1.0)
    return np.clip((field - a) / (b - a), 0.0, 1.0)


def _integral(padded: np.ndarray) -> np.ndarray:
    ii = np.cumsum(np.cumsum(padded, axis=0), axis=1)
    pad = [(1, 0), (1, 0)] + [(0, 0)] * (padded.ndim - 2)
    return np.pad(ii, pad)


def _quad_sum(ii: np.ndarray, h: int, w: int, r0: int, c0: int, win: int) -> np.ndarray:
    br = ii[r0 + win:r0 + win + h, c0 + win:c0 + win + w, ...]
    tl = ii[r0:r0 + h, c0:c0 + w, ...]
    tr = ii[r0:r0 + h, c0 + win:c0 + win + w, ...]
    bl = ii[r0 + win:r0 + win + h, c0:c0 + w, ...]
    return br - tr - bl + tl


def kuwahara_periodic(rgb: np.ndarray, radius: int) -> np.ndarray:
    """Filtro de Kuwahara (4 cuadrantes de (radius+1)²) con borde periódico: en cada
    píxel, la media del cuadrante de menor varianza — el efecto óleo (bordes vivos,
    interior liso) que da el aspecto pintado a mano. `radius` en píxeles."""
    if radius < 1:
        return rgb
    h, w = rgb.shape[:2]
    win = radius + 1
    lum = rgb @ LUMA
    lum_p = np.pad(lum, [(radius, radius), (radius, radius)], mode="wrap")
    rgb_p = np.pad(rgb, [(radius, radius), (radius, radius), (0, 0)], mode="wrap")
    ii_l = _integral(lum_p)
    ii_l2 = _integral(lum_p ** 2)
    ii_c = _integral(rgb_p)
    count = float(win * win)
    variances = []
    means = []
    for r0, c0 in ((0, 0), (0, radius), (radius, 0), (radius, radius)):
        s = _quad_sum(ii_l, h, w, r0, c0, win) / count
        s2 = _quad_sum(ii_l2, h, w, r0, c0, win) / count
        variances.append(np.clip(s2 - s * s, 0.0, None))
        means.append(_quad_sum(ii_c, h, w, r0, c0, win) / count)
    best = np.argmin(np.stack(variances, axis=0), axis=0)
    mean_stack = np.stack(means, axis=0)
    return np.take_along_axis(mean_stack, best[None, ..., None], axis=0)[0]


def photobash_rock(asset: str, size: int, style: RockStyle, seed: int = 0) -> dict[str, np.ndarray]:
    """Genera BC/N/ARH a partir de la caché de `asset` (ver `fetch_polyhaven.py`)."""
    cache = CACHE / asset
    diff = _load(cache / "diff_2k.png", size, color=True)
    disp = _load(cache / "disp_2k.png", size, color=False)
    ao_src = _load(cache / "ao_2k.png", size, color=False)
    rough_src = _load(cache / "rough_2k.png", size, color=False)

    radius = max(2, round(size * 0.012))
    diff_k = kuwahara_periodic(diff, radius)

    # Delit: quita la iluminación desigual de la foto (deja el relieve, no el sol).
    lum_k = diff_k @ LUMA
    broad = blur(lum_k, 0.1)
    flat = diff_k * (0.5 / np.clip(broad, 0.08, None))[..., None]
    flat = np.clip(flat, 0.0, 1.4)
    t = _percentile_stretch(flat @ LUMA)

    albedo = ramp(t, style.stops)
    # Resto de la crominancia original (poca) para que no sea un degradado sintético.
    chroma = flat - (flat @ LUMA)[..., None]
    albedo = np.clip(albedo + chroma * style.chroma, 0.0, 1.2)

    # Grietas: oclusión real de la foto, reforzada. Aristas: cavidad de la altura (luz).
    ao_k = blur(ao_src, 0.006)
    crevice = np.clip(1.0 - ao_k, 0.0, 1.0) ** 1.4
    albedo = mix_color(albedo, style.vein, crevice * 0.35)
    height_broad = blur(disp, 0.01)
    edge = np.clip(_cavity(height_broad, 0.006) * 10.0, -1, 1)
    albedo = albedo * (1.0 + 0.22 * np.clip(edge, 0, 1) - 0.14 * np.clip(-edge, 0, 1))[..., None]

    if style.warm_accent is not None:
        warm_n = np.clip((diff_k[..., 0] - diff_k[..., 2]) * 2.2, 0.0, 1.0)  # foto ya cálida
        accent = warm_n * (1.0 - crevice) * _percentile_stretch(t, 30, 85)
        albedo = mix_color(albedo, style.warm_accent, accent * 0.35)

    albedo = macro_variation(albedo, seed + 20, warm=style.macro_warm, cool=style.macro_cool,
                              amount=0.16, value=0.06)

    rough = style.rough_lo + (style.rough_hi - style.rough_lo) * blur(rough_src, 0.006)
    rough = np.clip(rough - 0.15 * crevice, 0.04, 1.0)

    height = _percentile_stretch(blur(disp, 0.003))
    ao_out = 0.3 + 0.7 * np.clip(ao_k, 0.0, 1.0)
    normal = height_to_normal(blur(disp, 0.02), style.depth)

    return {
        "BC": np.clip(albedo, 0.035, 0.95),
        "N": normal,
        "ARH": np.stack([ao_out, np.clip(rough, 0.04, 1.0), height], axis=-1),
    }
