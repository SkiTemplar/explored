"""K2 · SM_ManoIria: antebrazos y manos de Iria en primera persona.

Presupuesto propio «manos» (check_style): 1.500 tris por mano, 3.000 la
malla. Excepción documentada al pivote en la base: el origen es el punto
medio entre los dos codos, porque la malla cuelga de la cámara y gira desde
ahí; los antebrazos avanzan hacia +X (adelante en Unreal) y la mano derecha
queda en -Y (a la derecha en Unreal).

Manga de chaqueta técnica de trabajo (azul petróleo, banda reflectante y puño
de neopreno con lengüeta), mano en reposo con los dedos algo flexionados y
reloj en la muñeca izquierda.
"""

from __future__ import annotations

import math

import bpy  # noqa: F401  (debe importarse antes que mathutils)

from mathutils import Matrix, Vector

from explored_style.meshkit import MeshBuilder, blob, box, chamfer_box, matrix_at, mirrored, sweep, tube
from explored_style.pilots import C

ELBOW_SPAN = 0.44         # distancia entre codos (m)
ARM_TOE_IN_DEG = 2.0      # los antebrazos convergen hacia el centro
ARM_ROLL_DEG = 35.0       # pulgar hacia arriba (pose de reposo en primera persona)
ARM_PITCH_DEG = -12.0     # la mano más alta que el codo
HAND_BUDGET = 1500

_JACKET, _CUFF, _SKIN = C['agua_profunda'], C['hierro'], C['piel']
_SLEEVE_X = (-0.035, 0.0, 0.07, 0.12, 0.13, 0.165, 0.175, 0.225, 0.25)
_SLEEVE_R = (0.04, 0.049, 0.052, 0.05, 0.053, 0.053, 0.05, 0.048, 0.051)
_SLEEVE_COLORS = (_JACKET, _JACKET, _JACKET, _JACKET, C['acero'], _JACKET, _JACKET, _JACKET)
# (desplazamiento lateral, longitud, radio, abertura en grados) de índice a meñique
_FINGERS = ((0.03, 0.07, 0.0105, 7.0), (0.01, 0.078, 0.011, 2.0), (-0.01, 0.073, 0.0105, -3.0),
            (-0.03, 0.057, 0.0092, -9.0))
_CURL_DEG = (16.0, 30.0, 24.0)
_WRIST_X = 0.27
_HAND_SCALE = 1.1
_PHALANX = (0.46, 0.31, 0.23)


def _sleeve(b: MeshBuilder) -> None:
    arm = MeshBuilder()
    rings = sweep(arm, [(x, 0.0, 0.0) for x in _SLEEVE_X], list(_SLEEVE_R), 8, list(_SLEEVE_COLORS))
    arm.cap(rings[0], (-0.045, 0.0, 0.0), _JACKET, flip=True)
    tube(arm, (0.24, 0.0, 0.0), (0.285, 0.0, 0.0), 0.053, 0.05, 8, _CUFF, cap1=_CUFF)
    b.extend(arm, Matrix.Diagonal((1.0, 1.0, 0.86, 1.0)))
    chamfer_box(b, (0.04, 0.03, 0.012), 0.004, _JACKET, matrix_at(0.262, 0.0, 0.045))


def _finger(b: MeshBuilder, base: Vector, length: float, radius: float, spread_deg: float) -> None:
    yaw = math.radians(spread_deg)
    pts, radii = [base - Vector((0.008, 0.0, 0.0))], [radius * 1.05]
    p, pitch = base, 0.0
    for seg, curl in zip(_PHALANX, _CURL_DEG, strict=True):
        pitch += math.radians(curl)
        d = Vector((math.cos(yaw) * math.cos(pitch), math.sin(yaw) * math.cos(pitch), -math.sin(pitch)))
        p = p + d * (length * seg)
        pts.append(p)
        radii.append(radius * (1.0 - 0.1 * len(radii)))
    sweep(b, pts, radii, 6, _SKIN, cap=_SKIN)
    blob(b, base + Vector((0.002, 0.0, 0.004)), (radius * 1.0, radius * 1.0, radius * 0.7), 6, 2, _SKIN)


def _hand(b: MeshBuilder) -> None:
    hand = MeshBuilder()
    tube(hand, (0.27, 0.0, 0.0), (0.31, 0.0, 0.0), 0.036, 0.037, 8, _SKIN)
    palm = MeshBuilder()
    sweep(palm, [(0.296, 0.0, 0.0), (0.33, 0.0, 0.0), (0.37, 0.0, 0.0), (0.398, 0.0, 0.0)],
          [0.036, 0.043, 0.047, 0.044], 8, _SKIN, cap=_SKIN)
    hand.extend(palm, matrix_at(0.0, 0.0, -0.002, ry=4.0) @ Matrix.Diagonal((1.0, 1.0, 0.4, 1.0)))
    for dy, length, radius, spread in _FINGERS:
        _finger(hand, Vector((0.392, dy, -0.004)), length, radius, spread)
    blob(hand, (0.33, 0.032, -0.012), (0.032, 0.022, 0.017), 6, 3, _SKIN)
    sweep(hand, [(0.318, 0.035, -0.006), (0.34, 0.06, -0.014), (0.362, 0.074, -0.02), (0.388, 0.079, -0.026)],
          [0.015, 0.014, 0.0125, 0.011], 6, _SKIN, cap=_SKIN)
    wrist = Matrix.Translation((_WRIST_X, 0.0, 0.0))
    b.extend(hand, wrist @ Matrix.Diagonal((_HAND_SCALE, _HAND_SCALE, _HAND_SCALE * 0.9, 1.0)) @ wrist.inverted())


def _watch(b: MeshBuilder) -> None:
    tube(b, (0.292, 0.0, 0.0), (0.31, 0.0, 0.0), 0.0425, 0.0425, 8, C['tinta'])
    chamfer_box(b, (0.03, 0.03, 0.012), 0.004, C['hierro'], matrix_at(0.301, 0.0, 0.041))
    box(b, (0.022, 0.022, 0.004), C['papel'], matrix_at(0.301, 0.0, 0.048))


def build_arm(watch: bool = False) -> MeshBuilder:
    """Antebrazo derecho (o izquierdo sin espejar, con reloj) con el codo en el origen."""
    b = MeshBuilder()
    _sleeve(b)
    _hand(b)
    if watch:
        _watch(b)
    return b


def build_hands() -> MeshBuilder:
    out = MeshBuilder()
    right = build_arm()
    left = mirrored(build_arm(watch=True), axis=1)
    out.extend(right, matrix_at(0.0, -ELBOW_SPAN / 2, 0.0, rx=ARM_ROLL_DEG, ry=ARM_PITCH_DEG, rz=ARM_TOE_IN_DEG))
    out.extend(left, matrix_at(0.0, ELBOW_SPAN / 2, 0.0, rx=-ARM_ROLL_DEG, ry=ARM_PITCH_DEG, rz=-ARM_TOE_IN_DEG))
    return out


def per_hand_triangles(builder: MeshBuilder) -> tuple[int, int]:
    """Triángulos de la mano derecha (-Y) e izquierda (+Y) por el lado de su centroide."""
    right = left = 0
    for face in builder.faces:
        y = sum(builder.verts[i][1] for i in face) / len(face)
        if y < 0.0:
            right += len(face) - 2
        else:
            left += len(face) - 2
    return right, left
