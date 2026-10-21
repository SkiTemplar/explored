"""Ruido determinista en numpy (sin estado ni semillas globales) para el relieve y el color.

`hash01` es un hash entero de (a, b); `value_noise` interpola valores de una rejilla (0-1) y
`gradient_noise` es ruido de gradiente tipo Perlin (aprox. −1..1, media 0), que no deja el
patrón de rejilla del ruido de valor y sirve para las ondulaciones del terreno.
"""

from __future__ import annotations

import numpy as np

OFFSET = 100000  # desplaza la rejilla para que los índices sean siempre positivos


def hash01(a: np.ndarray, b: np.ndarray, seed: int, salt: int) -> np.ndarray:
    """Hash entero determinista de (a, b) a [0, 1)."""
    with np.errstate(over='ignore'):
        h = (a.astype(np.uint64) * np.uint64(0x9E3779B1) ^ b.astype(np.uint64) * np.uint64(0x85EBCA77)
             ^ np.uint64((seed * 0x27D4EB2F + salt * 0x165667B1) & 0xFFFFFFFF))
        h = (h ^ (h >> np.uint64(15))) * np.uint64(0x2C1B3C6D)
        h = (h ^ (h >> np.uint64(12))) * np.uint64(0x297A2D39)
        h = h ^ (h >> np.uint64(15))
    return (h & np.uint64(0xFFFFFF)).astype(np.float64) / float(0x1000000)


def _lattice(x: np.ndarray, y: np.ndarray, scale: float) -> tuple[np.ndarray, ...]:
    gx, gy = np.asarray(x, dtype=float) / scale, np.asarray(y, dtype=float) / scale
    ix, iy = np.floor(gx), np.floor(gy)
    return (ix + OFFSET).astype(np.int64), (iy + OFFSET).astype(np.int64), gx - ix, gy - iy


def value_noise(x: np.ndarray, y: np.ndarray, scale: float, seed: int) -> np.ndarray:
    """Ruido de valor suave (0-1) con celdas de «scale» metros."""
    ix, iy, fx, fy = _lattice(x, y, scale)
    sx, sy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    v00 = hash01(ix, iy, seed, 7)
    v10 = hash01(ix + 1, iy, seed, 7)
    v01 = hash01(ix, iy + 1, seed, 7)
    v11 = hash01(ix + 1, iy + 1, seed, 7)
    return (v00 * (1 - sx) + v10 * sx) * (1 - sy) + (v01 * (1 - sx) + v11 * sx) * sy


def gradient_noise(x: np.ndarray, y: np.ndarray, scale: float, seed: int) -> np.ndarray:
    """Ruido de gradiente (aprox. −1..1, media 0) con longitud de onda de unos 2 × «scale» metros."""
    ix, iy, fx, fy = _lattice(x, y, scale)

    def corner(di: int, dj: int) -> np.ndarray:
        angle = hash01(ix + di, iy + dj, seed, 31) * (2.0 * np.pi)
        return np.cos(angle) * (fx - di) + np.sin(angle) * (fy - dj)

    sx = fx * fx * fx * (fx * (fx * 6 - 15) + 10)
    sy = fy * fy * fy * (fy * (fy * 6 - 15) + 10)
    top = corner(0, 0) * (1 - sx) + corner(1, 0) * sx
    bottom = corner(0, 1) * (1 - sx) + corner(1, 1) * sx
    return np.clip((top * (1 - sy) + bottom * sy) * 1.6, -1.0, 1.0)
