"""Perfil longitudinal de los caminos: altura de su eje para nivelarlos sin pendiente transversal.

Cada camino de island.json se muestrea cada `STEP` m a lo largo de su eje con la altura base,
se suaviza con una media móvil de ±`SMOOTH` m y se limita su pendiente longitudinal a `MAX_ALONG`
grados (pasadas hacia delante y hacia atrás). La altura del eje en un punto cualquiera es la del
perfil en su proyección, fundida entre tramos con un mínimo suave de la distancia (`BLEND`) para que
no salte en el lado interior de las curvas.
"""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass

import numpy as np

from explored_island.layout import Layout

STEP = 0.25        # m entre muestras del perfil
SMOOTH = 2.5       # m: media móvil del perfil a cada lado
MAX_ALONG = 22.0   # grados: pendiente longitudinal máxima del eje
BLEND = 0.6        # m: suavidad del mínimo entre tramos

HeightFn = Callable[[np.ndarray, np.ndarray], np.ndarray]


@dataclass(frozen=True)
class Profile:
    points: tuple[tuple[float, float], ...]
    half_width: float
    starts: np.ndarray    # (S,) longitud acumulada al inicio de cada tramo
    heights: np.ndarray   # (K,) altura del eje cada STEP m


def limit_slope(h: np.ndarray, step: float, max_deg: float) -> np.ndarray:
    """Copia de «h» con |Δh| ≤ step · tan(max_deg) entre muestras vecinas (rebaja picos y rellena valles
    por igual: media de la pasada hacia delante y la de hacia atrás, repetida hasta converger)."""
    dmax = step * np.tan(np.radians(max_deg))
    out = h.astype(np.float64, copy=True)
    for _ in range(200):
        if np.abs(np.diff(out)).max(initial=0.0) <= dmax + 1e-9:
            return out
        out = 0.5 * (_clamp_forward(out, dmax) + _clamp_forward(out[::-1], dmax)[::-1])
    return _clamp_forward(out, dmax)  # pragma: no cover - la pasada hacia delante cumple siempre la cota


def _clamp_forward(h: np.ndarray, dmax: float) -> np.ndarray:
    out = h.copy()
    for i in range(1, len(out)):
        out[i] = np.clip(out[i], out[i - 1] - dmax, out[i - 1] + dmax)
    return out


def build_profiles(layout: Layout, height_fn: HeightFn) -> list[Profile]:
    profiles = []
    for path in layout.paths:
        pts = np.array(path.points, dtype=np.float64)
        lengths = np.hypot(*np.diff(pts, axis=0).T)
        starts = np.concatenate([[0.0], np.cumsum(lengths)])
        s = np.arange(0.0, starts[-1] + STEP, STEP)
        x = np.interp(s, starts, pts[:, 0])
        y = np.interp(s, starts, pts[:, 1])
        h = height_fn(x, y)
        k = max(int(SMOOTH / STEP), 1)
        padded = np.pad(h, k, mode='edge')
        smooth = np.convolve(padded, np.ones(2 * k + 1) / (2 * k + 1), mode='valid')
        profiles.append(Profile(path.points, path.width * 0.5, starts, limit_slope(smooth, STEP, MAX_ALONG)))
    return profiles


def axis_height(profiles: list[Profile], x: np.ndarray, y: np.ndarray) -> np.ndarray:
    """Altura del perfil en la proyección de cada punto, fundida entre tramos y caminos."""
    candidates: list[tuple[np.ndarray, np.ndarray]] = []
    for prof in profiles:
        s_axis = np.arange(len(prof.heights)) * STEP
        for i, ((ax, ay), (bx, by)) in enumerate(zip(prof.points[:-1], prof.points[1:], strict=True)):
            vx, vy = bx - ax, by - ay
            length_sq = max(vx * vx + vy * vy, 1e-9)
            t = np.clip(((x - ax) * vx + (y - ay) * vy) / length_sq, 0.0, 1.0)
            d = np.hypot(x - (ax + t * vx), y - (ay + t * vy)) - prof.half_width
            s = prof.starts[i] + t * np.sqrt(length_sq)
            candidates.append((d, np.interp(s, s_axis, prof.heights)))
    nearest = np.min([d for d, _ in candidates], axis=0)
    acc = np.zeros(np.shape(x))
    total = np.zeros(np.shape(x))
    for d, h in candidates:
        w = np.exp(-np.minimum(d - nearest, 50.0) / BLEND)
        acc += w * h
        total += w
    return acc / total
