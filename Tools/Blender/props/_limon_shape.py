"""
_limon_shape.py — geometría compartida del kit «Limón» (limon.py).

Forma del casco por estación: manga, arrufo, quilla, línea de flotación y
lanzamiento de roda y espejo. El barco terminado (SM_Limon) y el armazón en
grada (SM_Limon_Frame) muestrean LA MISMA forma, así que cuadernas y tablas
del armazón caen exactamente sobre la superficie del casco terminado.
Coordenadas: metros, proa hacia +X, Z = 0 en la línea de flotación.

Las primitivas con caras etiquetadas (loft, sweep, slab, cloth) y el pintor
por cara viven en _prims.py y se reexportan al final de este módulo.
"""

import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))

# ---------------------------------------------------------------------------
# Forma del casco
# ---------------------------------------------------------------------------
X_STERN = -3.10          # x del espejo en la quilla
X_BOW = 3.05             # x de la roda en la quilla
B_MAX = 1.10             # media manga máxima (manga 2,2 m)
BULWARK_H = 0.22         # altura de la regala sobre la cubierta
DECK_CAMBER = 0.04       # bombeo de la cubierta en crujía
STEM_RAKE = 0.32         # lanzamiento de la roda (avance de la cabeza)
TRANSOM_RAKE = -0.22     # lanzamiento del espejo (retroceso de la cabeza)


def _clamp01(x):
    return max(0.0, min(1.0, x))


def half_beam(t):
    """Media manga en la regala; t = 0 espejo, t = 1 roda."""
    if t < 0.45:
        f = 0.70 + 0.30 * math.sin(0.5 * math.pi * t / 0.45)
    else:
        u = (t - 0.45) / 0.55
        f = max(0.0, 1.0 - u ** 1.9) ** 0.8
    return max(0.035, B_MAX * f)


def sheer(t):
    """Altura de la regala: arrufo marcado, sube más a proa que a popa."""
    return (0.58 + 0.30 * max(0.0, (t - 0.5) / 0.5) ** 2
            + 0.12 * max(0.0, (0.35 - t) / 0.35) ** 2)


def keel(t):
    """Fondo del casco en crujía (sin la orza, que se añade aparte)."""
    return (-0.38 + 0.36 * max(0.0, (t - 0.55) / 0.45) ** 1.6
            + 0.18 * max(0.0, (0.25 - t) / 0.25) ** 1.5)


def waterline_z(t):
    return max(0.04, keel(t) + 0.10)


def bulwark_thickness(t):
    return min(0.07, 0.4 * half_beam(t))


def inner_half_beam(t):
    return half_beam(t) - bulwark_thickness(t)


def deck_z(t, y=0.0):
    r = min(1.0, abs(y) / max(1e-4, inner_half_beam(t)))
    return sheer(t) - BULWARK_H + DECK_CAMBER * (1.0 - r * r)


def rake(t):
    return (STEM_RAKE * max(0.0, (t - 0.7) / 0.3) ** 2
            + TRANSOM_RAKE * max(0.0, (0.12 - t) / 0.12) ** 2)


def x_station(t):
    return X_STERN + (X_BOW - X_STERN) * t


def t_of_x(x):
    return _clamp01((x - X_STERN) / (X_BOW - X_STERN))


def x_at(t, z):
    """x real de un punto de la sección t a la altura z (roda y espejo
    lanzados: la sección se inclina con la altura)."""
    k, s = keel(t), sheer(t)
    return x_station(t) + rake(t) * (z - k) / (s - k)


def hull_point(t, u, side):
    """Punto de la piel exterior. u = 0 quilla, 0.5 flotación, 1 regala;
    side = +1 estribor (+Y), -1 babor (-Y)."""
    b, s, k = half_beam(t), sheer(t), keel(t)
    b_wl = 0.88 * b
    zw = waterline_z(t)
    if u <= 0.5:
        phi = (u / 0.5) * 0.5 * math.pi
        y = b_wl * math.sin(phi) ** 0.75
        z = zw - (zw - k) * math.cos(phi)
    else:
        w = (u - 0.5) / 0.5
        y = b_wl + (b - b_wl) * w ** 0.7 + 0.025 * b * math.sin(math.pi * w)
        z = zw + (s - zw) * w
    return Vector((x_at(t, z), side * y, z))


def u_below_sheer(t, dz):
    """u del costado a `dz` metros por debajo de la regala."""
    s, zw = sheer(t), waterline_z(t)
    return 0.5 + 0.5 * (1.0 - dz / (s - zw))


def hull_normal(t, u, side):
    """Normal exterior de la sección (plano YZ) en el punto (t, u)."""
    e = 0.01
    a = hull_point(t, max(0.0, u - e), side)
    b = hull_point(t, min(1.0, u + e), side)
    n = Vector((0.0, b.z - a.z, -(b.y - a.y)))
    if n.length < 1e-9:
        return Vector((0.0, 0.0, -1.0))
    n.normalize()
    p = hull_point(t, u, side)
    centre = Vector((p.x, 0.0, 0.5 * (sheer(t) + keel(t))))
    if n.dot(p - centre) < 0.0:
        n.negate()
    return n


# Primitivas genéricas: viven en _prims.py y se reexportan aquí para que
# limon.py las use con el mismo prefijo H.
from _prims import cloth, loft, paint, slab, smooth, solid, sweep, translate_mesh, xz  # noqa: E402,F401
