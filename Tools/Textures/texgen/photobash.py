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
  3. Filtro de Kuwahara generalizado (8 ventanas gaussianas, peso suave por varianza,
     óleo): quita el detalle de alta frecuencia conservando bordes vivos — el aspecto
     pintado a mano, sin los brochazos cuadrados del Kuwahara clásico.
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
from .polyhaven import fetch

from .polyhaven import CACHE
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
    height_tone: float = 0.0  # cuánto manda la altura real (formas grandes) sobre la luminancia
    bands: int = 0  # >0: escalonado suave del tono en tantas bandas (caras planas pintadas)


# Gris kárstico con vetas oscuras y toques ocres — referencia El Nido / Ha Long, no crema.
LIMESTONE_STYLE = RockStyle(
    stops=[(0.0, "#6b6e6b"), (0.35, "#8c8f8a"), (0.65, "#aeb0ab"), (1.0, "#dcded7")],
    vein="#2c2a28",
    warm_accent="#a9793c",
    macro_warm="#c7b98e",
    macro_cool="#8c959a",
    chroma=0.4,
    depth=0.028,
    rough_lo=0.5,
    rough_hi=0.92,
    height_tone=0.3,
)

# Basalto gris violáceo oscuro.
VOLCANIC_STYLE = RockStyle(
    stops=[(0.0, "#2e2c3d"), (0.35, "#413e52"), (0.65, "#5a5569"), (1.0, "#756f85")],
    vein="#100f18",
    warm_accent=None,
    macro_warm="#7a5a52",
    macro_cool="#4f5a78",
    chroma=0.12,
    depth=0.045,
    rough_lo=0.55,
    rough_hi=0.95,
    height_tone=0.55,
    bands=5,
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


def soft_bands(t: np.ndarray, n: int, softness: float = 0.3) -> np.ndarray:
    """Escalonado suave de `t` (0-1) en `n` bandas: planos de tono casi constante unidos
    por transiciones cortas (el «cel shading» pintado de las caras de roca low-poly)."""
    x = np.clip(t, 0.0, 1.0) * n
    k = np.floor(x)
    f = x - k
    edge = 0.5 * softness
    step = np.clip((f - (0.5 - edge)) / (2.0 * edge), 0.0, 1.0)
    step = step * step * (3.0 - 2.0 * step)
    return np.clip((k + step) / n, 0.0, 1.0)


def _percentile_stretch(field: np.ndarray, lo: float = 2.0, hi: float = 98.0) -> np.ndarray:
    a, b = np.percentile(field, [lo, hi])
    if b - a < 1e-6:
        return np.clip(field, 0.0, 1.0)
    return np.clip((field - a) / (b - a), 0.0, 1.0)


def _shift(field: np.ndarray, dy: int, dx: int) -> np.ndarray:
    """Desplazamiento periódico: el valor en (y, x) pasa a ser el de (y + dy, x + dx)."""
    return np.roll(field, (-dy, -dx), axis=(0, 1))


def kuwahara_periodic(rgb: np.ndarray, radius: int, sectors: int = 8, q: float = 6.0) -> np.ndarray:
    """Kuwahara generalizado y suave (Papari et al.), con borde periódico: en cada píxel,
    media de `sectors` ventanas gaussianas desplazadas `radius` px en direcciones
    repartidas en círculo, ponderadas por var^(-q/2). Da el efecto óleo (bordes vivos,
    interior liso) sin los «brochazos» rectangulares del Kuwahara clásico de 4 cuadrados
    con `argmin` (que se leían como una rejilla de manchas cuadradas en la roca)."""
    if radius < 1:
        return rgb
    size = rgb.shape[0]
    sigma = 0.7 * radius / size  # blur() trabaja en unidades de tile
    lum = rgb @ LUMA
    m_rgb = blur(rgb, sigma)
    m_l = blur(lum, sigma)
    m_l2 = blur(lum * lum, sigma)
    means, logw = [], []
    for k in range(sectors):
        a = 2.0 * np.pi * (k + 0.5) / sectors
        dy, dx = round(radius * np.sin(a)), round(radius * np.cos(a))
        mu = _shift(m_l, dy, dx)
        var = np.clip(_shift(m_l2, dy, dx) - mu * mu, 0.0, None)
        means.append(_shift(m_rgb, dy, dx))
        logw.append(-0.5 * q * np.log(var + 1e-5))
    logw = np.stack(logw, axis=0)
    w = np.exp(logw - logw.max(axis=0, keepdims=True))
    w /= w.sum(axis=0, keepdims=True)
    return np.einsum("khw,khwc->hwc", w, np.stack(means, axis=0))


def photobash_rock(asset: str, size: int, style: RockStyle, seed: int = 0) -> dict[str, np.ndarray]:
    """Genera BC/N/ARH a partir de la caché de `asset` (ver `fetch_polyhaven.py`)."""
    cache = fetch(asset, CACHE)
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
    if style.height_tone > 0.0:
        # Volumen legible: las caras altas de la roca más claras y los huecos más oscuros,
        # para que se lean las formas grandes y no solo el moteado de la foto.
        form = _percentile_stretch(blur(disp, 0.004))
        t = _percentile_stretch((1.0 - style.height_tone) * t + style.height_tone * form)
    if style.bands > 0:
        t = 0.45 * t + 0.55 * soft_bands(t, style.bands)

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
