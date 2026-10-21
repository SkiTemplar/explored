"""K2 · Faro: torre, linterna y casa del farero (presupuesto «faro», 3.000 tris).

Convención de todo el kit: metros, Z arriba, frente hacia -Y (la puerta mira
a la cámara de la lámina), pivote en el centro de la base.

- SM_FaroTorre: 12 m. Zócalo octogonal de piedra, fuste troncocónico con
  falda acampanada, blanco con una banda roja, cornisa roja que vuela hasta la
  galería, barandilla de hierro, ventanucos y anexo de entrada con puerta.
  La galería (suelo de la linterna) queda a `DECK_Z`.
- SM_FaroLinterna: octógono acristalado con cúpula roja; el lado frontal es la
  puerta (abierto para ver el mecanismo). Dentro, sobre el pedestal, las 3
  ranuras de juego: aro de la lente (vertical, de cara a la puerta), dos huecos
  de tarro (izquierda y derecha) y el portamecha (detrás del aro, a la altura
  de su centro). Posiciones locales en `LANTERN_SLOTS`.
- SM_FaroCasa: casa blanca de 6 × 4,4 m con tejado rojo a dos aguas,
  contraventanas, chimenea y escalón.
"""

from __future__ import annotations

import math

import bpy  # noqa: F401  (debe importarse antes que mathutils)

from mathutils import Matrix, Vector

from explored_style.meshkit import MeshBuilder, blob, box, chamfer_box, face_out, lathe, matrix_at, tube
from explored_style.pilots import C

DECK_Z = 11.0
RAIL_R = 2.16
_TOWER_SIDES = 24
# (radio, altura) del fuste y la cornisa; un color por tramo.
_SHAFT = ((2.25, 0.6), (2.08, 1.2), (1.96, 2.4), (1.87, 4.0), (1.83, 5.2), (1.77, 7.4), (1.67, 9.0),
          (1.6, 10.1), (1.74, 10.3), (2.02, 10.6), (2.3, 10.78), (2.32, DECK_Z))
_W, _R = C['papel'], C['rojo_faro']
_SHAFT_COLORS = (_W, _W, _W, _W, _R, _W, _W, _R, _R, _R, C['roca_media'])

LANTERN_R = 1.4          # circunradio del octógono
_LANTERN_TOP = 2.35      # altura del acristalado
LANTERN_SLOTS = {        # posición local (m) de cada ranura de juego
    'aro_lente': (0.0, -0.22, 1.47),
    'tarro_izq': (-0.5, 0.08, 1.05),
    'tarro_der': (0.5, 0.08, 1.05),
    'portamecha': (0.0, 0.18, 1.44),
}


def shaft_radius(z: float) -> float:
    """Radio exterior del fuste a la altura «z» (interpolado en el perfil)."""
    for (r0, z0), (r1, z1) in zip(_SHAFT, _SHAFT[1:], strict=False):
        if z0 <= z <= z1:
            return r0 + (r1 - r0) * (z - z0) / (z1 - z0)
    raise ValueError(f'altura fuera del fuste: {z}')


def _facing(az_deg: float, radius: float, z: float) -> Matrix:
    """Marco local con -Y mirando hacia fuera en el acimut «az_deg»."""
    a = math.radians(az_deg)
    return matrix_at(radius * math.cos(a), radius * math.sin(a), z, rz=az_deg + 90.0)


# ---------------------------------------------------------------------------
# SM_FaroTorre
# ---------------------------------------------------------------------------
def _tower_window(b: MeshBuilder, az: float, z: float) -> None:
    r = shaft_radius(z)
    piece = MeshBuilder()
    chamfer_box(piece, (0.62, 0.12, 0.9), 0.03, C['roca_clara'], matrix_at(0.0, 0.0, 0.0))
    box(piece, (0.42, 0.12, 0.66), C['cielo_noche'], matrix_at(0.0, -0.03, 0.03))
    box(piece, (0.72, 0.2, 0.08), C['roca_media'], matrix_at(0.0, -0.04, -0.47))
    b.extend(piece, _facing(az, r - 0.02, z))


def _tower_entrance(b: MeshBuilder) -> None:
    stone, wood = C['roca_clara'], C['madera_oscura']
    chamfer_box(b, (1.9, 1.5, 3.0), 0.08, _W, matrix_at(0.0, -2.2, 1.5))
    chamfer_box(b, (2.2, 1.85, 0.2), 0.06, _R, matrix_at(0.0, -2.2, 3.1))
    chamfer_box(b, (1.4, 0.55, 0.16), 0.04, C['roca_media'], matrix_at(0.0, -3.15, 0.08))
    box(b, (0.95, 0.08, 2.0), wood, matrix_at(0.0, -2.96, 1.16))
    for zb in (0.55, 1.75):
        box(b, (0.95, 0.04, 0.14), C['madera_media'], matrix_at(0.0, -3.01, zb))
    box(b, (0.06, 0.06, 0.12), C['laton'], matrix_at(0.33, -3.03, 1.15))
    for x in (-0.55, 0.55):
        chamfer_box(b, (0.16, 0.14, 2.2), 0.03, stone, matrix_at(x, -2.98, 1.1))
    chamfer_box(b, (1.3, 0.16, 0.2), 0.03, stone, matrix_at(0.0, -2.98, 2.28))


def _tower_railing(b: MeshBuilder) -> None:
    iron = C['hierro']
    posts = 16
    for k in range(posts):
        a = 360.0 * k / posts + 11.25
        rad = math.radians(a)
        box(b, (0.07, 0.07, 1.0), iron, matrix_at(RAIL_R * math.cos(rad), RAIL_R * math.sin(rad), DECK_Z + 0.5,
                                                    rz=a))
    for z0, z1, w in ((DECK_Z + 0.94, DECK_Z + 1.02, 0.06), (DECK_Z + 0.48, DECK_Z + 0.52, 0.03)):
        lathe(b, [(RAIL_R - w, z0), (RAIL_R + w, z0), (RAIL_R + w, z1), (RAIL_R - w, z1)], posts, [iron] * 4,
              closed=True)


def build_tower() -> MeshBuilder:
    b = MeshBuilder()
    lathe(b, [(2.78, 0.0), (2.78, 0.42), (2.56, 0.6)], 8, [C['roca_media'], C['roca_clara']],
          phase=math.pi / 8, cap_bottom=C['roca_media'], cap_top=C['roca_clara'])
    lathe(b, list(_SHAFT), _TOWER_SIDES, list(_SHAFT_COLORS), cap_top=C['roca_media'])
    _tower_entrance(b)
    for az, z in ((-62.0, 4.4), (-118.0, 8.3), (40.0, 6.3), (150.0, 3.2)):
        _tower_window(b, az, z)
    _tower_railing(b)
    return b


# ---------------------------------------------------------------------------
# SM_FaroLinterna
# ---------------------------------------------------------------------------
def _side_frame(k: int, radius: float, z: float) -> Matrix:
    """Marco del lado k del octógono (k = 0 es la puerta, hacia -Y)."""
    return _facing(-90.0 + 45.0 * k, radius, z)


def _lantern_shell(b: MeshBuilder) -> None:
    iron, glass = C['hierro'], C['cristal']
    apothem = LANTERN_R * math.cos(math.pi / 8)
    side = 2 * LANTERN_R * math.sin(math.pi / 8)
    lathe(b, [(1.52, 0.0), (1.52, 0.08)], 8, [C['roca_media']], phase=math.pi / 8,
          cap_bottom=C['roca_media'], cap_top=C['roca_media'])
    for k in range(8):
        a = math.radians(-67.5 + 45.0 * k)
        box(b, (0.1, 0.1, _LANTERN_TOP - 0.08), iron,
            matrix_at(LANTERN_R * math.cos(a), LANTERN_R * math.sin(a), (_LANTERN_TOP + 0.08) / 2,
                      rz=-67.5 + 45.0 * k))
    for k in range(1, 8):
        chamfer_box(b, (side + 0.02, 0.12, 0.88), 0.03, _R, _side_frame(k, apothem, 0.52))
        box(b, (side - 0.06, 0.035, 1.38), glass, _side_frame(k, apothem, 1.65))
        box(b, (side, 0.06, 0.05), iron, _side_frame(k, apothem + 0.01, 1.65))
    box(b, (side - 0.06, 0.035, 0.28), glass, _side_frame(0, apothem, 2.2))
    box(b, (side, 0.08, 0.07), iron, _side_frame(0, apothem, 2.03))
    profile = [(1.38, _LANTERN_TOP), (1.52, 2.40), (1.62, 2.50), (1.62, 2.60), (1.5, 2.66), (1.4, 2.95),
               (1.12, 3.28), (0.7, 3.52), (0.25, 3.64)]
    lathe(b, profile, 8, [iron, iron, _R, _R, _R, _R, _R, _R], phase=math.pi / 8, cap_bottom=iron,
          cap_top=_R, tips=(_LANTERN_TOP, 3.67))
    blob(b, (0.0, 0.0, 3.82), (0.2, 0.2, 0.18), 8, 3, C['laton'])
    tube(b, (0.0, 0.0, 3.95), (0.0, 0.0, 4.6), 0.03, 0.025, 4, iron, cap1=iron)
    box(b, (0.42, 0.02, 0.14), C['laton'], matrix_at(0.12, 0.0, 4.42))


def _lantern_mechanism(b: MeshBuilder) -> None:
    iron, brass, dark = C['hierro'], C['laton'], C['tinta']
    lathe(b, [(0.32, 0.08), (0.32, 0.16), (0.15, 0.24), (0.12, 0.78), (0.25, 0.86), (0.7, 0.92), (0.7, 1.0)], 10,
          [iron, iron, iron, iron, brass, brass], cap_top=C['acero'])
    x, y, z = LANTERN_SLOTS['aro_lente']
    lathe(b, [(0.34, -0.06), (0.45, -0.06), (0.45, 0.06), (0.34, 0.06)], 16, [brass] * 4,
          origin=Vector((x, y, z)), axis=Matrix.Rotation(math.pi / 2, 4, 'X'), closed=True)
    chamfer_box(b, (0.24, 0.14, 0.08), 0.02, brass, matrix_at(x, y, 1.03))
    wx, wy, _ = LANTERN_SLOTS['portamecha']
    lathe(b, [(0.1, 0.0), (0.1, 0.03), (0.035, 0.06), (0.03, 0.36), (0.08, 0.42), (0.08, 0.46)], 8,
          [brass] * 5, origin=Vector((wx, wy, 1.0)), cap_top=dark, tips=(0.0, 0.43))
    for key in ('tarro_izq', 'tarro_der'):
        jx, jy, _ = LANTERN_SLOTS[key]
        lathe(b, [(0.12, 0.0), (0.14, 0.03), (0.14, 0.2), (0.115, 0.2), (0.115, 0.05)], 10,
              [iron, iron, brass, dark], origin=Vector((jx, jy, 1.0)), cap_bottom=iron, cap_top=dark)


def build_lantern() -> MeshBuilder:
    b = MeshBuilder()
    _lantern_shell(b)
    _lantern_mechanism(b)
    return b


# ---------------------------------------------------------------------------
# SM_FaroCasa
# ---------------------------------------------------------------------------
_HX, _HY = 3.0, 2.2
_BASE_Z, _EAVE_Z, _RIDGE_Z = 0.3, 2.95, 4.5


def _house_walls(b: MeshBuilder) -> None:
    corners = [b.vert((sx * _HX, sy * _HY, z)) for z in (_BASE_Z, _EAVE_Z) for sx, sy in
               ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    ridge = [b.vert((-_HX, 0.0, _RIDGE_Z)), b.vert((_HX, 0.0, _RIDGE_Z))]
    lo, hi = corners[:4], corners[4:]
    inside = (0.0, 0.0, 1.8)
    face_out(b, [lo[3], lo[2], lo[1], lo[0]], C['roca_media'], inside)
    for k in range(4):
        face_out(b, [lo[k], lo[(k + 1) % 4], hi[(k + 1) % 4], hi[k]], _W, inside)
    face_out(b, [hi[0], hi[1], ridge[1], ridge[0]], _W, inside)
    face_out(b, [hi[2], hi[3], ridge[0], ridge[1]], _W, inside)
    face_out(b, [hi[1], hi[2], ridge[1]], _W, inside)
    face_out(b, [hi[3], hi[0], ridge[0]], _W, inside)


def _house_roof(b: MeshBuilder) -> None:
    theta = math.atan2(_RIDGE_Z - _EAVE_Z, _HY)
    over, t, length_x = 0.42, 0.16, 2 * _HX + 0.7
    run = _HY + over
    slope = run / math.cos(theta)
    for side in (-1, 1):
        rx = math.degrees(theta) * -side
        normal = Vector((0.0, side * math.sin(theta), math.cos(theta)))
        eave = Vector((0.0, side * run, _RIDGE_Z - run * math.tan(theta)))
        up = Vector((0.0, -side * math.cos(theta), math.sin(theta)))
        center = eave + up * (slope / 2) + normal * (t / 2)
        chamfer_box(b, (length_x, slope, t), 0.04, _R, matrix_at(*center, rx=rx), edge_color=C['oxido'])
        for s in (0.22, 0.47, 0.72):
            p = eave + up * (slope * s) + normal * (t + 0.02)
            box(b, (length_x - 0.1, 0.1, 0.06), C['oxido'], matrix_at(*p, rx=rx))
    tube(b, (-length_x / 2, 0.0, _RIDGE_Z + 0.16), (length_x / 2, 0.0, _RIDGE_Z + 0.16), 0.17, 0.17, 4,
         C['oxido'], cap0=C['oxido'], cap1=C['oxido'])


def _window(shutters: bool) -> MeshBuilder:
    """Ventana local mirando a -Y, centrada en el hueco."""
    w = MeshBuilder()
    frame, glass = C['madera_clara'], C['cielo_noche']
    box(w, (0.8, 0.06, 0.9), glass, matrix_at(0.0, -0.02, 0.0))
    for x in (-0.45, 0.45):
        box(w, (0.1, 0.12, 1.0), frame, matrix_at(x, -0.04, 0.0))
    for z in (-0.5, 0.5):
        box(w, (1.0, 0.12, 0.1), frame, matrix_at(0.0, -0.04, z))
    box(w, (0.06, 0.08, 0.9), frame, matrix_at(0.0, -0.05, 0.0))
    box(w, (0.8, 0.08, 0.06), frame, matrix_at(0.0, -0.05, 0.08))
    chamfer_box(w, (1.15, 0.22, 0.09), 0.025, C['roca_clara'], matrix_at(0.0, -0.08, -0.58))
    if shutters:
        for x in (-0.78, 0.78):
            chamfer_box(w, (0.48, 0.06, 1.0), 0.02, C['agua_media'], matrix_at(x, -0.05, 0.0))
            box(w, (0.4, 0.03, 0.06), C['agua_profunda'], matrix_at(x, -0.09, 0.0))
    return w


def _house_details(b: MeshBuilder) -> None:
    for x in (-1.85, 1.85):
        b.extend(_window(True), matrix_at(x, -_HY, 1.75))
    b.extend(_window(True), matrix_at(0.6, _HY, 1.75, rz=180.0))
    b.extend(_window(False), matrix_at(-_HX, -0.2, 1.75, rz=-90.0))
    door, frame = C['agua_media'], C['madera_oscura']
    box(b, (1.0, 0.1, 2.05), door, matrix_at(0.0, -_HY - 0.02, _BASE_Z + 1.02))
    for zb in (0.75, 1.55):
        box(b, (0.8, 0.04, 0.5), C['agua_profunda'], matrix_at(0.0, -_HY - 0.08, _BASE_Z + zb))
    box(b, (0.07, 0.07, 0.07), C['laton'], matrix_at(0.36, -_HY - 0.1, _BASE_Z + 1.0))
    for x in (-0.58, 0.58):
        box(b, (0.14, 0.16, 2.2), frame, matrix_at(x, -_HY - 0.03, _BASE_Z + 1.1))
    box(b, (1.3, 0.16, 0.16), frame, matrix_at(0.0, -_HY - 0.03, _BASE_Z + 2.24))
    chamfer_box(b, (1.6, 0.7, 0.3), 0.05, C['roca_clara'], matrix_at(0.0, -_HY - 0.45, 0.15))
    blob(b, (0.0, -_HY - 0.18, _BASE_Z + 2.6), (0.1, 0.1, 0.12), 6, 2, C['laton'])
    chamfer_box(b, (0.7, 0.7, 2.3), 0.05, C['roca_clara'], matrix_at(2.2, 1.0, 4.45))
    chamfer_box(b, (0.88, 0.88, 0.16), 0.04, C['roca_media'], matrix_at(2.2, 1.0, 5.6))
    box(b, (0.42, 0.42, 0.04), C['tinta'], matrix_at(2.2, 1.0, 5.68))


def build_house() -> MeshBuilder:
    b = MeshBuilder()
    chamfer_box(b, (2 * _HX + 0.3, 2 * _HY + 0.3, 0.36), 0.06, C['roca_media'], matrix_at(0.0, 0.0, 0.18))
    _house_walls(b)
    _house_roof(b)
    _house_details(b)
    return b
