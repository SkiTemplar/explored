"""Lógica pura de ``normalize.py`` (sin ``bpy``): color, argumentos, reglas de recoloreo,
medida y pivote. Se prueba con pytest sin Blender; ``normalize.py`` la aplica a las mallas.
"""

from __future__ import annotations

import json
import math
from collections.abc import Iterable, Sequence
from pathlib import Path
from typing import Any, Protocol

RGB = tuple[float, float, float]
Lab = tuple[float, float, float]


class Point(Protocol):
    """Cualquier cosa con ``x``, ``y`` y ``z`` (``mathutils.Vector`` o un punto de test)."""

    @property
    def x(self) -> float: ...
    @property
    def y(self) -> float: ...
    @property
    def z(self) -> float: ...


# ---------------------------------------------------------------------------
# Color: sRGB <-> lineal <-> Oklab (mismas fórmulas que Tools/Textures/texgen)
# ---------------------------------------------------------------------------

def hex_to_srgb(h: str) -> RGB:
    """``#rrggbb`` (con o sin ``#``) a sRGB en [0, 1]."""
    h = h.lstrip("#")
    if len(h) != 6:
        raise ValueError(f"color hexadecimal no válido: #{h}")
    return (int(h[0:2], 16) / 255.0, int(h[2:4], 16) / 255.0, int(h[4:6], 16) / 255.0)


def srgb_to_linear(c: float) -> float:
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def linear_to_srgb(c: float) -> float:
    c = min(max(c, 0.0), 1.0)
    return 12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055


def rgb3(values: Iterable[float]) -> RGB:
    """Los tres primeros canales como tupla de 3 (descarta alfa)."""
    r, g, b = list(values)[:3]
    return (float(r), float(g), float(b))


def oklab(srgb: Sequence[float]) -> Lab:
    r, g, b = (srgb_to_linear(c) for c in srgb[:3])
    l_ = 0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b
    m_ = 0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b
    s_ = 0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b
    l_, m_, s_ = (math.copysign(abs(v) ** (1 / 3), v) for v in (l_, m_, s_))
    return (
        0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_,
        1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_,
        0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_,
    )


def delta_e(a: Sequence[float], b: Sequence[float]) -> float:
    return math.dist(a, b)


def to_hex(srgb: Sequence[float]) -> str:
    return "#" + "".join(f"{round(min(max(c, 0), 1) * 255):02x}" for c in srgb[:3])


# ---------------------------------------------------------------------------
# Datos y argumentos
# ---------------------------------------------------------------------------

def load_json(p: Path) -> dict[str, Any]:
    return json.loads(p.read_text(encoding="utf-8"))


def parse_args(argv: list[str]) -> dict[str, Any]:
    """Argumentos tras ``--`` en la línea de Blender (ver la docstring de ``normalize.py``)."""
    args: dict[str, Any] = {"lote": None, "ids": [], "isla": "Landing", "analyze": False, "tiles": False,
                            "export": True}
    it = iter(argv)
    key = None
    for a in it:
        if a in ("--lote", "--isla"):
            value = next(it, None)
            if value is None or value.startswith("--"):
                raise SystemExit(f"{a} necesita un valor")
            args[a[2:]] = value
        elif a == "--ids":
            key = "ids"
            continue
        elif a == "--analyze":
            args["analyze"] = True
        elif a == "--tiles":
            args["tiles"] = True
        elif a == "--no-export":
            args["export"] = False
        elif key == "ids" and not a.startswith("--"):
            args["ids"].append(a)
            continue
        else:
            raise SystemExit(f"argumento desconocido: {a}")
        key = None
    return args


# ---------------------------------------------------------------------------
# Recoloreo a la paleta
# ---------------------------------------------------------------------------

def compile_rules(recolor: dict[str, Any]) -> tuple[list[tuple[Lab, str]], float, str | None]:
    """Reglas ``from`` → ``to`` en Oklab, tolerancia y muestra por defecto de una entrada."""
    compiled = [(oklab(hex_to_srgb(r["from"])), r["to"]) for r in recolor.get("rules", [])]
    return compiled, float(recolor.get("tolerance", 0.12)), recolor.get("default")


def pick_swatch(color: Sequence[float], compiled: list[tuple[Lab, str]], tol: float, default: str | None,
                game_id: str = "?") -> str:
    """Muestra de la regla más cercana en Oklab; ``default`` si ninguna está a ``tol`` o menos.

    A igual distancia gana la primera regla (orden del catálogo), para que el resultado no
    dependa del orden de iteración.
    """
    lab = oklab(color)
    best, dist = None, math.inf
    for flab, to in compiled:
        d = delta_e(flab, lab)
        if d < dist:
            best, dist = to, d
    if best is None or dist > tol:
        if default is None:
            raise RuntimeError(f"{game_id}: color {to_hex(color)} sin regla y sin default")
        return default
    return best


def gradient(swatch: dict[str, Any], h: float) -> tuple[tuple[float, float], list[float]]:
    """UV de imagen (v = 0 abajo, convención de Blender) y color lineal a la altura ``h``.

    ``h`` va de 0 (abajo) a 1 (arriba); fuera de ese rango se recorta. ``paleta.json`` da ``v``
    desde arriba de la imagen: arriba claro, abajo oscuro (``docs/art/paleta.md``).
    """
    h = min(max(h, 0.0), 1.0)
    u = swatch["uv"][0]
    v0, v1 = swatch["v_rango"]
    top = swatch["arriba"]["lineal"]
    mid = swatch["medio"]["lineal"]
    bot = swatch["abajo"]["lineal"]
    v_img = v0 + (1.0 - h) * (v1 - v0)
    if h >= 0.5:
        t = (h - 0.5) * 2
        col = [mid[i] + (top[i] - mid[i]) * t for i in range(3)]
    else:
        t = h * 2
        col = [bot[i] + (mid[i] - bot[i]) * t for i in range(3)]
    return (u, 1.0 - v_img), col


def share_percent(share: dict[str, float]) -> dict[str, float]:
    """Área por muestra en % (una decimal), de mayor a menor."""
    total = sum(share.values()) or 1.0
    return {k: round(100 * v / total, 1) for k, v in sorted(share.items(), key=lambda kv: -kv[1])}


# ---------------------------------------------------------------------------
# Medida y pivote
# ---------------------------------------------------------------------------

def bbox_dims(vs: Sequence[Point]) -> RGB:
    if not vs:
        raise ValueError("malla sin vértices")
    return (
        max(v.x for v in vs) - min(v.x for v in vs),
        max(v.y for v in vs) - min(v.y for v in vs),
        max(v.z for v in vs) - min(v.z for v in vs),
    )


def size_factor(vs: Sequence[Point], size: dict[str, Any]) -> float:
    """Escala uniforme para que el eje ``size.axis`` (x, y, z o max) mida ``size.m`` metros."""
    dims = bbox_dims(vs)
    axis = size.get("axis", "max")
    if axis == "max":
        cur = max(dims)
    elif axis in ("x", "y", "z"):
        cur = dims["xyz".index(axis)]
    else:
        raise ValueError(f"eje de medida no válido: {axis}")
    if cur <= 0:
        raise ValueError(f"la malla es plana en el eje {axis}: no se puede escalar a {size['m']} m")
    return size["m"] / cur


def pivot_origin(vs: Sequence[Point], pivot: dict[str, Any]) -> RGB:
    """Punto que pasa a ser el origen: ``base`` o ``agarre`` (ver la docstring de ``normalize.py``)."""
    if not vs:
        raise ValueError("malla sin vértices")
    zmin = min(v.z for v in vs)
    if pivot["kind"] == "base":
        cx = (min(v.x for v in vs) + max(v.x for v in vs)) / 2
        cy = (min(v.y for v in vs) + max(v.y for v in vs)) / 2
        return (cx, cy, zmin)
    if pivot["kind"] == "agarre":
        zmax = max(v.z for v in vs)
        height = zmax - zmin
        if pivot.get("end", "bottom") == "top":
            ring = [v for v in vs if v.z >= zmax - 0.15 * height]
            z = zmax - pivot["gripFromEndM"]
        else:
            ring = [v for v in vs if v.z <= zmin + 0.15 * height]
            z = zmin + pivot["gripFromEndM"]
        if pivot.get("centerAt", "end") == "grip":
            # Arcos y lanzas: el eje del mango se toma a la altura del agarre, no en la
            # punta (las palas de un arco se curvan hacia la cuerda).
            band = 0.04 * height
            ring = [v for v in vs if abs(v.z - z) <= band] or ring
        cx = sum(v.x for v in ring) / len(ring)
        cy = sum(v.y for v in ring) / len(ring)
        return (cx, cy, z)
    raise ValueError(f"pivote desconocido: {pivot['kind']}")
