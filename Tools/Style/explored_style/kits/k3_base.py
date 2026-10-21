"""Utilidades comunes de las formas del kit K3 (objetos y fauna).

Colores por nombre de la paleta, ejes de los tornos y piezas pequeñas que se
repiten: cajas, contornos en sentido antihorario, caras de corte con anillos
y retoques de vértices (bordes partidos, cortes en bisel).
"""

from __future__ import annotations

import math
from collections.abc import Callable, Sequence

import bpy  # noqa: F401  (debe importarse antes que mathutils)

from mathutils import Matrix, Vector

from explored_style.meshkit import Coord, MeshBuilder, cross, vec
from explored_style.palette import load_palette

C: dict[str, int] = {c.name: c.index for c in load_palette().colors}

# Orientaciones del eje Z local de `lathe` (el perfil gira alrededor de él).
AX_X = Matrix.Rotation(math.pi / 2, 4, 'Y')       # Z local -> +X del mundo
AX_NEG_Y = Matrix.Rotation(math.pi / 2, 4, 'X')   # Z local -> -Y (cara hacia la cámara de la lámina)
AX_Y = Matrix.Rotation(-math.pi / 2, 4, 'X')      # Z local -> +Y


def ccw(points: Sequence[Coord], normal: Coord) -> list[Vector]:
    """Devuelve el contorno (simple, convexo o no) en sentido antihorario
    visto desde «normal», según el signo de su área (método de Newell)."""
    pts = [vec(p) for p in points]
    area = Vector((0.0, 0.0, 0.0))
    for a, b in zip(pts, pts[1:] + pts[:1], strict=True):
        area += cross(a, b)
    if area.dot(vec(normal)) < 0.0:
        pts.reverse()
    return pts


def box(b: MeshBuilder, lo: Coord, hi: Coord, color: int) -> None:
    """Caja alineada con los ejes, de quads, con las normales hacia fuera."""
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    bottom = [b.vert((x0, y0, z0)), b.vert((x1, y0, z0)), b.vert((x1, y1, z0)), b.vert((x0, y1, z0))]
    top = [b.vert((x0, y0, z1)), b.vert((x1, y0, z1)), b.vert((x1, y1, z1)), b.vert((x0, y1, z1))]
    b.bridge(bottom, top, color)
    b.face(tuple(reversed(bottom)), color)
    b.face(tuple(top), color)


def ring_center(b: MeshBuilder, ring: Sequence[int]) -> Vector:
    total = Vector((0.0, 0.0, 0.0))
    for i in ring:
        total += Vector(b.verts[i])
    return total / len(ring)


def end_face(b: MeshBuilder, ring: Sequence[int], scales: Sequence[float], colors: Sequence[int],
             cap: int, at_start: bool, inset: Coord = (0.0, 0.0, 0.0)) -> None:
    """Cara de corte con anillos concéntricos (vetas del tronco, boca de la
    caña): copias del anillo escaladas hacia su centro, unidas en el mismo
    plano y cerradas con un abanico. «at_start» indica que la cara mira hacia
    atrás del tubo (primer anillo) y «inset» desplaza el centro (hueco)."""
    center = ring_center(b, ring)
    prev = list(ring)
    for scale, color in zip(scales, colors, strict=True):
        inner = [b.vert(center.lerp(Vector(b.verts[i]), scale)) for i in ring]
        if at_start:
            b.bridge(inner, prev, color)
        else:
            b.bridge(prev, inner, color)
        prev = inner
    b.cap(prev, center + vec(inset), cap, flip=at_start)


def move_verts(b: MeshBuilder, indices: Sequence[int], offset_of: Callable[[int, Vector], Coord]) -> None:
    """Desplaza cada vértice «indices[k]» con offset_of(k, posición)."""
    for k, i in enumerate(indices):
        co = Vector(b.verts[i])
        moved = co + vec(offset_of(k, co))
        b.verts[i] = (moved.x, moved.y, moved.z)


def polar(r: float, angle: float, z: float) -> Vector:
    return Vector((r * math.cos(angle), r * math.sin(angle), z))
