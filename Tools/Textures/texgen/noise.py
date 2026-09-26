"""Primitivas periódicas (sin costuras) para texturas procedurales.

Todas trabajan en coordenadas de tile normalizadas: u (columnas, hacia la derecha) y
v (filas, hacia abajo) en [0, 1). Cada función devuelve un campo que se repite exacto
cada tile, porque o bien se construye en el dominio de frecuencia con frecuencias
enteras (FFT), o bien usa una rejilla que envuelve (Voronoi) o fases enteras (ondas).
Así las mismas escalas valen para cualquier resolución (256 en tests, 1024/2048 en juego).
"""

from __future__ import annotations

import numpy as np


def uv_grid(size: int) -> tuple[np.ndarray, np.ndarray]:
    """Centros de píxel en [0, 1): (u, v) con forma (size, size)."""
    c = (np.arange(size, dtype=np.float64) + 0.5) / size
    return np.meshgrid(c, c)


def _freqs(size: int) -> tuple[np.ndarray, np.ndarray]:
    """Frecuencias en ciclos por tile (enteros) para fft2."""
    f = np.fft.fftfreq(size) * size
    return np.meshgrid(f, f)


def spectral_noise(
    size: int,
    seed: int,
    fmin: float = 1.0,
    fmax: float = 64.0,
    beta: float = 2.0,
    stretch: tuple[float, float] = (1.0, 1.0),
    angle: float = 0.0,
) -> np.ndarray:
    """Ruido fractal periódico por filtrado espectral de ruido blanco.

    `fmin`/`fmax`: banda en ciclos por tile. `beta`: pendiente de la potencia (1/f^beta):
    2 = fBm clásico, más alto = más suave. `stretch` (su, sv) alarga las formas en u o v
    (anisotropía para vetas, fibras o estrías); `angle` gira esa anisotropía (radianes).
    Devuelve media 0 y desviación 1.
    """
    rng = np.random.default_rng(seed)
    white = rng.standard_normal((size, size))
    fx, fy = _freqs(size)
    ca, sa = np.cos(angle), np.sin(angle)
    ru = (fx * ca + fy * sa) * stretch[0]
    rv = (-fx * sa + fy * ca) * stretch[1]
    r = np.hypot(ru, rv)
    r[0, 0] = 1.0
    amp = r ** (-beta / 2.0)
    # Bordes de banda suaves para no generar anillos.
    amp *= 1.0 / (1.0 + (fmin / r) ** 8)
    amp *= np.exp(-((r / fmax) ** 2))
    amp[0, 0] = 0.0
    field = np.fft.ifft2(np.fft.fft2(white) * amp).real
    std = field.std()
    return field / (std + 1e-12)


def unit(field: np.ndarray, spread: float = 3.0) -> np.ndarray:
    """Campo de media 0 y desviación 1 a [0, 1] estable (no depende de mín/máx de la muestra)."""
    return np.clip(0.5 + field / (2.0 * spread), 0.0, 1.0)


def normalize01(a: np.ndarray) -> np.ndarray:
    lo, hi = float(a.min()), float(a.max())
    return (a - lo) / (hi - lo + 1e-12)


def smoothstep(e0: float, e1: float, x: np.ndarray) -> np.ndarray:
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def blur(field: np.ndarray, sigma: float) -> np.ndarray:
    """Desenfoque gaussiano periódico; `sigma` en unidades de tile."""
    size = field.shape[0]
    fx, fy = _freqs(size)
    g = np.exp(-2.0 * (np.pi * sigma) ** 2 * (fx * fx + fy * fy))
    if field.ndim == 2:
        return np.fft.ifft2(np.fft.fft2(field) * g).real
    return np.stack([blur(field[..., i], sigma) for i in range(field.shape[-1])], axis=-1)


def sample(field: np.ndarray, u: np.ndarray, v: np.ndarray) -> np.ndarray:
    """Muestreo bilineal periódico de `field` en coordenadas de tile (u, v)."""
    size_v, size_u = field.shape[:2]
    x = u * size_u - 0.5
    y = v * size_v - 0.5
    x0 = np.floor(x).astype(np.int64)
    y0 = np.floor(y).astype(np.int64)
    tx = x - x0
    ty = y - y0
    x0 %= size_u
    y0 %= size_v
    x1 = (x0 + 1) % size_u
    y1 = (y0 + 1) % size_v
    if field.ndim == 3:
        tx = tx[..., None]
        ty = ty[..., None]
    a = field[y0, x0] * (1 - tx) + field[y0, x1] * tx
    b = field[y1, x0] * (1 - tx) + field[y1, x1] * tx
    return a * (1 - ty) + b * ty


def voronoi(size: int, nx: int, ny: int, seed: int, jitter: float = 0.85, u=None, v=None,
            isotropic: bool = True) -> dict:
    """Voronoi periódico sobre una rejilla nx × ny de puntos perturbados.

    Distancias en unidades de «celda media» (1 ≈ separación entre puntos). Devuelve:
      f1, f2: distancia al primer y segundo punto; edge = f2 - f1 (≈0 en las juntas);
      id: aleatorio [0, 1) por celda; id2: segundo aleatorio por celda;
      dx, dy: vector del píxel al punto más cercano (en unidades de celda).
    `u`, `v` opcionales permiten evaluar en coordenadas deformadas (domain warping).
    `isotropic=False` mide en unidades de celda: con nx ≠ ny las celdas salen alargadas como
    la rejilla (placas de corteza, vetas), y la búsqueda 3 × 3 sigue siendo exacta.
    """
    rng = np.random.default_rng(seed)
    offs = rng.random((ny, nx, 2))
    ids = rng.random((ny, nx))
    ids2 = rng.random((ny, nx))
    if u is None or v is None:
        u, v = uv_grid(size)
    gx = np.mod(u, 1.0) * nx
    gy = np.mod(v, 1.0) * ny
    cx = np.floor(gx).astype(np.int64)
    cy = np.floor(gy).astype(np.int64)
    # Escala para que las distancias sean isótropas aun con celdas no cuadradas.
    sx = np.sqrt(ny / nx) if isotropic else 1.0
    sy = np.sqrt(nx / ny) if isotropic else 1.0
    f1 = np.full(gx.shape, np.inf)
    f2 = np.full(gx.shape, np.inf)
    cid = np.zeros(gx.shape)
    cid2 = np.zeros(gx.shape)
    ndx = np.zeros(gx.shape)
    ndy = np.zeros(gx.shape)
    for oy in (-1, 0, 1):
        for ox in (-1, 0, 1):
            jx = cx + ox
            jy = cy + oy
            wx = jx % nx
            wy = jy % ny
            px = jx + 0.5 + (offs[wy, wx, 0] - 0.5) * jitter
            py = jy + 0.5 + (offs[wy, wx, 1] - 0.5) * jitter
            dx = (px - gx) * sx
            dy = (py - gy) * sy
            d = np.hypot(dx, dy)
            closer = d < f1
            second = (~closer) & (d < f2)
            f2 = np.where(closer, f1, np.where(second, d, f2))
            f1 = np.where(closer, d, f1)
            cid = np.where(closer, ids[wy, wx], cid)
            cid2 = np.where(closer, ids2[wy, wx], cid2)
            ndx = np.where(closer, dx, ndx)
            ndy = np.where(closer, dy, ndy)
    return {"f1": f1, "f2": f2, "edge": f2 - f1, "id": cid, "id2": cid2, "dx": ndx, "dy": ndy}


def scatter_dots(size: int, n: int, seed: int, radius: float, jitter: float = 1.0,
                 keep: float = 1.0, vary: float = 0.0, u=None, v=None) -> dict:
    """Puntos dispersos (una celda n × n por punto, con probabilidad `keep`): disco suave de
    radio `radius` (unidades de celda), ± `vary` (fracción) según el punto.
    Devuelve máscara [0, 1] y el id por punto."""
    vo = voronoi(size, n, n, seed, jitter=jitter, u=u, v=v)
    r = radius * (1.0 + vary * (2.0 * vo["id"] - 1.0))
    disc = 1.0 - smoothstep(r * 0.55, r, vo["f1"])
    alive = (vo["id2"] < keep).astype(np.float64)
    return {"mask": disc * alive, "alive": alive, "id": vo["id"], "f1": vo["f1"], "dx": vo["dx"], "dy": vo["dy"]}


def height_to_normal(height: np.ndarray, depth: float) -> np.ndarray:
    """Normal en espacio tangente, convención DirectX (la de Unreal: verde = +V, hacia abajo).

    `height` en [0, 1]; `depth` = relieve máximo en unidades de tile (0.01 = 1 % del ancho),
    así la intensidad no cambia con la resolución. Devuelve vectores en [-1, 1].
    """
    size = height.shape[0]
    dhdu = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5 * size
    dhdv = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5 * size
    n = np.stack([-dhdu * depth, -dhdv * depth, np.ones_like(height)], axis=-1)
    return n / np.linalg.norm(n, axis=-1, keepdims=True)


def ambient_occlusion(height: np.ndarray, radii=(0.004, 0.016, 0.05), strength: float = 1.0) -> np.ndarray:
    """Oclusión aproximada: cuánto está un punto por debajo de su entorno (varios radios)."""
    occ = np.zeros_like(height)
    for i, r in enumerate(radii):
        occ += np.clip(blur(height, r) - height, 0.0, None) * (2.2 / (i + 1))
    return np.clip(1.0 - occ * strength, 0.0, 1.0)


def hex_rgb(h: str) -> np.ndarray:
    h = h.lstrip("#")
    return np.array([int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)])


def ramp(t: np.ndarray, stops: list[tuple[float, str]]) -> np.ndarray:
    """Gradiente de color (sRGB) por tramos: stops = [(posición, '#rrggbb'), ...]."""
    pos = np.array([p for p, _ in stops])
    cols = np.stack([hex_rgb(c) for _, c in stops])
    t = np.clip(t, pos[0], pos[-1])
    return np.stack([np.interp(t, pos, cols[:, i]) for i in range(3)], axis=-1)


def lerp(a, b, t):
    """Interpolación; si a/b son colores (…, 3) y t es escalar por píxel, t se amplía."""
    t = np.asarray(t, dtype=np.float64)
    color = np.shape(a)[-1:] == (3,) or np.shape(b)[-1:] == (3,)
    if color and t.ndim and t.shape[-1:] != (3,):
        t = t[..., None]
    return a + (b - a) * t


def mix_color(base: np.ndarray, color: str | np.ndarray, mask: np.ndarray) -> np.ndarray:
    c = hex_rgb(color) if isinstance(color, str) else color
    return base + (c - base) * mask[..., None]


def macro_variation(albedo: np.ndarray, seed: int, warm: str, cool: str, amount: float = 0.12,
                    value: float = 0.08) -> np.ndarray:
    """Variación de color de gran escala (1–3 ciclos por tile) para romper la repetición:
    zonas algo más cálidas/frías y más claras/oscuras, sin llegar a manchar."""
    size = albedo.shape[0]
    hue = unit(spectral_noise(size, seed, fmin=1.0, fmax=3.0, beta=2.0), 2.2)
    val = spectral_noise(size, seed + 1, fmin=1.0, fmax=4.0, beta=2.0)
    tint = lerp(hex_rgb(cool), hex_rgb(warm), hue)
    tinted = albedo * (1.0 - amount) + albedo * tint / tint.mean(axis=-1, keepdims=True) * amount
    return tinted * (1.0 + value * np.clip(val, -2.0, 2.0) * 0.5)[..., None]
