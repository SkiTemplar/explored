"""Constructor de mallas low poly con color por celda de paleta.

Las piezas se describen como vértices y caras (tris o quads, nunca n-gones)
con un índice de paleta por cara. `build_object` las convierte en un objeto
de Blender que ya cumple el estilo: un material (`M_Toon`, sin texturas),
UV de cada cara en el centro de su celda, normales planas y pivote en la base.
"""

from __future__ import annotations

import math
from collections.abc import Sequence
from dataclasses import dataclass, field

import bpy

import bmesh
from mathutils import Euler, Matrix, Vector

from explored_style.palette import PALETTE_SIZE, cell_uv

MATERIAL_NAME = 'M_Toon'
UV_NAME = 'UVPaleta'

Vec3 = tuple[float, float, float]
Coord = Sequence[float] | Vector


@dataclass
class MeshBuilder:
    """Acumula geometría. Los métodos devuelven índices de vértices para
    encadenar anillos; las caras guardan su índice de paleta."""

    verts: list[Vec3] = field(default_factory=list)
    faces: list[tuple[int, ...]] = field(default_factory=list)
    colors: list[int] = field(default_factory=list)

    def vert(self, co: Coord) -> int:
        self.verts.append((float(co[0]), float(co[1]), float(co[2])))
        return len(self.verts) - 1

    def face(self, indices: Sequence[int], color: int) -> None:
        if len(indices) not in (3, 4):
            raise ValueError(f'solo tris o quads (se pidió una cara de {len(indices)} vértices)')
        if not 0 <= color < PALETTE_SIZE:
            raise ValueError(f'índice de paleta fuera de rango: {color}')
        self.faces.append(tuple(indices))
        self.colors.append(color)

    def ring(self, center: Vector, axis_u: Vector, axis_v: Vector, radius_u: float,
             sides: int, phase: float = 0.0, radius_v: float | None = None) -> list[int]:
        """Anillo de «sides» vértices en el plano (u, v) alrededor de «center»."""
        rv = radius_u if radius_v is None else radius_v
        out = []
        for k in range(sides):
            a = phase + 2.0 * math.pi * k / sides
            out.append(self.vert(center + axis_u * (math.cos(a) * radius_u) + axis_v * (math.sin(a) * rv)))
        return out

    def bridge(self, lower: Sequence[int], upper: Sequence[int], color: int | Sequence[int]) -> None:
        """Une dos anillos cerrados del mismo tamaño con quads (normales
        hacia fuera si ambos anillos giran en sentido antihorario visto desde
        «upper»). «color» puede ser uno por cara."""
        n = len(lower)
        for k in range(n):
            c = color if isinstance(color, int) else color[k]
            self.face((lower[k], lower[(k + 1) % n], upper[(k + 1) % n], upper[k]), c)

    def cap(self, ring: Sequence[int], tip: Coord, color: int, flip: bool = False) -> None:
        """Cierra un anillo con un abanico de tris hacia el vértice «tip»."""
        t = self.vert(tip)
        n = len(ring)
        for k in range(n):
            a, b = ring[k], ring[(k + 1) % n]
            self.face((b, a, t) if flip else (a, b, t), color)

    def extend(self, other: MeshBuilder, transform: Matrix | None = None) -> None:
        """Añade otra pieza (opcionalmente transformada)."""
        offset = len(self.verts)
        for co in other.verts:
            v = Vector(co) if transform is None else transform @ Vector(co)
            self.vert(v)
        # una transformación con espejo invierte el sentido de giro: se
        # reordenan las caras para que las normales sigan hacia fuera.
        mirror = transform is not None and transform.to_3x3().determinant() < 0.0
        for f, c in zip(other.faces, other.colors, strict=True):
            ids = tuple(i + offset for i in f)
            self.face(tuple(reversed(ids)) if mirror else ids, c)

    @property
    def triangle_count(self) -> int:
        return sum(len(f) - 2 for f in self.faces)


def lathe(builder: MeshBuilder, profile: Sequence[tuple[float, float]], sides: int, colors: Sequence[int],
          origin: Vector | None = None, axis: Matrix | None = None, phase: float = 0.0,
          cap_bottom: int | None = None, cap_top: int | None = None, closed: bool = False,
          tips: tuple[float, float] | None = None) -> list[list[int]]:
    """Sólido de revolución: «profile» es [(radio, altura)] y «colors» un
    color por tramo (uno más si «closed» une el último anillo con el primero,
    como en un aro). El perfil debe recorrerse en sentido antihorario en el
    plano (radio, altura) para que las normales queden hacia fuera. «axis»
    orienta el eje local Z (por defecto el Z del mundo). «tips» fija la altura
    de los vértices de las tapas (abajo, arriba); por defecto, tapas planas.
    Devuelve los anillos."""
    spans = len(profile) if closed else len(profile) - 1
    if len(colors) != spans:
        raise ValueError('lathe: se necesita un color por tramo del perfil')
    base = Vector((0.0, 0.0, 0.0)) if origin is None else origin
    rot = Matrix.Identity(3) if axis is None else axis.to_3x3()
    u, v, w = rot @ Vector((1, 0, 0)), rot @ Vector((0, 1, 0)), rot @ Vector((0, 0, 1))
    rings = [builder.ring(base + w * z, u, v, r, sides, phase) for r, z in profile]
    for k in range(spans):
        builder.bridge(rings[k], rings[(k + 1) % len(rings)], colors[k])
    tip_bottom, tip_top = (profile[0][1], profile[-1][1]) if tips is None else tips
    if cap_bottom is not None:
        builder.cap(rings[0], base + w * tip_bottom, cap_bottom, flip=True)
    if cap_top is not None:
        builder.cap(rings[-1], base + w * tip_top, cap_top)
    return rings


def mesh_of(obj: bpy.types.Object) -> bpy.types.Mesh:
    """Datos de malla de «obj» (error claro si no es una malla)."""
    data = obj.data
    if not isinstance(data, bpy.types.Mesh):
        raise TypeError(f'{obj.name} no es una malla')
    return data


def triangle_count(obj: bpy.types.Object) -> int:
    """Triángulos tras triangular (n vértices -> n - 2 por cara)."""
    return sum(p.loop_total - 2 for p in mesh_of(obj).polygons)


def cross(a: Vector, b: Vector) -> Vector:
    """Producto vectorial 3D (los stubs tipan Vector.cross como float por el caso 2D)."""
    return Vector((a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x))


def reset_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)


def toon_material() -> bpy.types.Material:
    """M_Toon de exportación: sin nodos de textura. El color real lo pone la
    hoja de paleta en Unreal; aquí el base color es neutro."""
    mat = bpy.data.materials.get(MATERIAL_NAME) or bpy.data.materials.new(MATERIAL_NAME)
    return mat


def build_object(name: str, builder: MeshBuilder, base_pivot: bool = True) -> bpy.types.Object:
    """Crea el objeto en la escena: UV por celda, normales planas, un material
    y el pivote en el centro de la base (z mínima = 0). Con «base_pivot» falso
    se respeta el origen del constructor (excepción documentada: las manos
    pivotan en el codo)."""
    if not builder.faces:
        raise ValueError(f'{name}: malla vacía')
    min_z = min(v[2] for v in builder.verts) if base_pivot else 0.0
    verts = [(x, y, z - min_z) for x, y, z in builder.verts]
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], builder.faces)
    mesh.validate(clean_customdata=False)
    uv = mesh.uv_layers.new(name=UV_NAME)
    for poly, color in zip(mesh.polygons, builder.colors, strict=True):
        u, v = cell_uv(color)
        for loop_index in poly.loop_indices:
            uv.data[loop_index].uv = (u, v)
        poly.use_smooth = False
    mesh.materials.append(toon_material())
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def face_colors(obj: bpy.types.Object) -> list[int]:
    """Índice de paleta de cada cara, leído de su UV (centro de celda)."""
    mesh = mesh_of(obj)
    uv = mesh.uv_layers[0]
    return [int(uv.data[p.loop_indices[0]].uv[0] * PALETTE_SIZE) for p in mesh.polygons]


def flipped_face_count(obj: bpy.types.Object) -> int:
    """Caras cuya normal invierte bmesh.recalc_face_normals (0 = todas las
    piezas cerradas tienen las normales hacia fuera)."""
    bm = bmesh.new()
    bm.from_mesh(mesh_of(obj))
    before = [f.normal.copy() for f in bm.faces]
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    flipped = sum(1 for f, n in zip(bm.faces, before, strict=True) if f.normal.dot(n) < 0.0)
    bm.free()
    return flipped


def island_count(obj: bpy.types.Object) -> int:
    """Piezas conexas de la malla (hojas, duelas...)."""
    bm = bmesh.new()
    bm.from_mesh(mesh_of(obj))
    seen: set[int] = set()
    islands = 0
    for start in bm.verts:
        if start.index in seen:
            continue
        islands += 1
        stack = [start]
        seen.add(start.index)
        while stack:
            vert = stack.pop()
            for edge in vert.link_edges:
                other = edge.other_vert(vert)
                if other.index not in seen:
                    seen.add(other.index)
                    stack.append(other)
    bm.free()
    return islands


def export_fbx(obj: bpy.types.Object, filepath: str) -> str:
    """FBX para Unreal con la misma convención que Tools/Blender: metros con
    apply_unit_scale (1 m = 100 uu), ejes -Z adelante / Y arriba que el
    importador de Unreal convierte a su Z arriba / X adelante, normales por
    cara (planas) y triangulado."""
    for other in bpy.context.selected_objects or []:
        other.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=filepath, check_existing=False, use_selection=True, global_scale=1.0,
        apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL', axis_forward='-Z', axis_up='Y',
        object_types={'MESH'}, use_mesh_modifiers=True, mesh_smooth_type='FACE', use_triangles=True,
        use_tspace=False, bake_anim=False, path_mode='STRIP', embed_textures=False,
    )
    return filepath


# ---------------------------------------------------------------------------
# Primitivas de los kits (K2 en adelante). Añaden geometría a un MeshBuilder
# con las caras hacia fuera.
# ---------------------------------------------------------------------------
def matrix_at(x: float = 0.0, y: float = 0.0, z: float = 0.0,
              rx: float = 0.0, ry: float = 0.0, rz: float = 0.0) -> Matrix:
    """Traslación y rotación en grados (Euler XYZ)."""
    rot = Euler((math.radians(rx), math.radians(ry), math.radians(rz)), 'XYZ').to_matrix().to_4x4()
    return Matrix.Translation((x, y, z)) @ rot


def _newell(points: Sequence[Vector]) -> Vector:
    n = Vector((0.0, 0.0, 0.0))
    for k, p in enumerate(points):
        q = points[(k + 1) % len(points)]
        n += Vector(((p.y - q.y) * (p.z + q.z), (p.z - q.z) * (p.x + q.x), (p.x - q.x) * (p.y + q.y)))
    return n


def face_out(builder: MeshBuilder, indices: Sequence[int], color: int, inside: Coord) -> None:
    """Cara orientada para que su normal se aleje de «inside» (piezas convexas)."""
    pts = [Vector(builder.verts[i]) for i in indices]
    centroid = sum(pts, Vector((0.0, 0.0, 0.0))) / len(pts)
    outward = _newell(pts).dot(centroid - Vector((inside[0], inside[1], inside[2]))) >= 0.0
    builder.face(tuple(indices) if outward else tuple(reversed(indices)), color)


_CYCLE = ((-1, -1), (1, -1), (1, 1), (-1, 1))


def _with(axis: int, value: int, p: int, q: int) -> tuple[int, int, int]:
    """Terna de signos con «value» en «axis» y (p, q) en los otros dos ejes."""
    out = [0, 0, 0]
    others = [a for a in range(3) if a != axis]
    out[axis], out[others[0]], out[others[1]] = value, p, q
    return out[0], out[1], out[2]


def box(builder: MeshBuilder, size: Coord, color: int, transform: Matrix | None = None) -> None:
    """Caja centrada en el origen local (12 tris)."""
    m = Matrix.Identity(4) if transform is None else transform
    half = [float(size[k]) / 2 for k in range(3)]
    ids = {}
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (-1, 1):
                ids[(sx, sy, sz)] = builder.vert(m @ Vector((sx * half[0], sy * half[1], sz * half[2])))
    center = m @ Vector((0.0, 0.0, 0.0))
    for axis in range(3):
        for s in (-1, 1):
            face_out(builder, [ids[_with(axis, s, p, q)] for p, q in _CYCLE], color, center)


def chamfer_box(builder: MeshBuilder, size: Coord, bevel: float, color: int,
                transform: Matrix | None = None, edge_color: int | None = None) -> None:
    """Caja con aristas achaflanadas (44 tris): la forma «redondeada» barata
    del estilo. «edge_color» tiñe chaflanes y esquinas (por defecto, «color»)."""
    m = Matrix.Identity(4) if transform is None else transform
    half = [float(size[k]) / 2 for k in range(3)]
    bv = min(bevel, *(h * 0.49 for h in half))
    rim = color if edge_color is None else edge_color
    ids: dict[tuple[int, int, int, int], int] = {}
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (-1, 1):
                signs = (sx, sy, sz)
                for axis in range(3):
                    co = [signs[k] * (half[k] if k == axis else half[k] - bv) for k in range(3)]
                    ids[(sx, sy, sz, axis)] = builder.vert(m @ Vector(co))
    center = m @ Vector((0.0, 0.0, 0.0))
    for axis in range(3):
        for s in (-1, 1):
            face_out(builder, [ids[(*_with(axis, s, p, q), axis)] for p, q in _CYCLE], color, center)
    for axis in range(3):  # aristas paralelas a «axis»
        a, c = [k for k in range(3) if k != axis]
        for sa in (-1, 1):
            for sc in (-1, 1):
                lo, hi = _edge_signs(axis, a, sa, c, sc)
                face_out(builder, [ids[(*lo, a)], ids[(*hi, a)], ids[(*hi, c)], ids[(*lo, c)]], rim, center)
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (-1, 1):
                face_out(builder, [ids[(sx, sy, sz, k)] for k in range(3)], rim, center)


def _edge_signs(axis: int, a: int, sa: int, c: int, sc: int) -> tuple[tuple[int, int, int], tuple[int, int, int]]:
    lo, hi = [0, 0, 0], [0, 0, 0]
    lo[axis], hi[axis] = -1, 1
    lo[a] = hi[a] = sa
    lo[c] = hi[c] = sc
    return (lo[0], lo[1], lo[2]), (hi[0], hi[1], hi[2])


def tube(builder: MeshBuilder, p0: Coord, p1: Coord, r0: float, r1: float, sides: int, color: int,
         cap0: int | None = None, cap1: int | None = None, phase: float = 0.0) -> list[list[int]]:
    """Cilindro o cono truncado de «p0» a «p1»; tapas opcionales con su color."""
    a, z = Vector((p0[0], p0[1], p0[2])), Vector((p1[0], p1[1], p1[2]))
    axis = (z - a).normalized().to_track_quat('Z', 'Y').to_matrix().to_4x4()
    return lathe(builder, [(r0, 0.0), (r1, (z - a).length)], sides, [color], origin=a, axis=axis, phase=phase,
                 cap_bottom=cap0, cap_top=cap1)


def sweep(builder: MeshBuilder, path: Sequence[Coord], radius: float | Sequence[float], sides: int,
          color: int | Sequence[int], closed: bool = False, cap: int | None = None,
          phase: float = 0.0) -> list[list[int]]:
    """Tubo a lo largo de una polilínea (cuerdas, barandillas, dedos) con
    marcos de transporte paralelo. «color» puede ser uno por tramo."""
    pts = [Vector((p[0], p[1], p[2])) for p in path]
    n = len(pts)
    radii = [float(radius)] * n if isinstance(radius, (int, float)) else [float(r) for r in radius]
    tangents = []
    for i in range(n):
        prev = pts[i - 1] if (closed or i > 0) else pts[i]
        nxt = pts[(i + 1) % n] if (closed or i < n - 1) else pts[i]
        tangents.append((nxt - prev).normalized())
    ref = Vector((0.0, 0.0, 1.0)) if abs(tangents[0].z) < 0.9 else Vector((1.0, 0.0, 0.0))
    u = (ref - tangents[0] * ref.dot(tangents[0])).normalized()
    rings = []
    for p, t, r in zip(pts, tangents, radii, strict=True):
        u = (u - t * u.dot(t)).normalized()
        rings.append(builder.ring(p, u, cross(t, u), r, sides, phase))
    spans = n if closed else n - 1
    for k in range(spans):
        c = color if isinstance(color, int) else color[k]
        builder.bridge(rings[k], rings[(k + 1) % n], c)
    if cap is not None and not closed:
        builder.cap(rings[0], pts[0] - tangents[0] * radii[0] * 0.5, cap, flip=True)
        builder.cap(rings[-1], pts[-1] + tangents[-1] * radii[-1] * 0.5, cap)
    return rings


def noise(seed: int, *keys: int) -> float:
    """Ruido determinista en [-1, 1] (sin estado ni aleatoriedad)."""
    acc = seed * 12.9898 + sum(k * f for k, f in zip(keys, (78.233, 37.719, 11.135, 93.989), strict=False))
    value = math.sin(acc) * 43758.5453
    return (value - math.floor(value)) * 2.0 - 1.0


def blob(builder: MeshBuilder, center: Coord, radii: Coord, sides: int, rings: int, color: int,
         jitter: float = 0.0, seed: int = 0, flat_bottom: float | None = None) -> None:
    """Elipsoide low poly (piedras, nudillos) con ruido radial determinista.
    «flat_bottom» aplana por debajo de esa altura relativa (-1..1)."""
    c = Vector((center[0], center[1], center[2]))
    rx, ry, rz = float(radii[0]), float(radii[1]), float(radii[2])
    floor_z = -1.0 if flat_bottom is None else flat_bottom

    def point(sin_t: float, cos_t: float, a: float, h: float) -> Vector:
        return c + Vector((rx * sin_t * math.cos(a) * h, ry * sin_t * math.sin(a) * h,
                           rz * max(cos_t * h, floor_z)))

    ring_ids = []
    for j in range(1, rings + 1):
        theta = math.pi * j / (rings + 1)
        ring_ids.append([builder.vert(point(math.sin(theta), math.cos(theta), 2 * math.pi * k / sides,
                                            1.0 + jitter * noise(seed, j, k))) for k in range(sides)])
    for upper, lower in zip(ring_ids, ring_ids[1:], strict=False):
        builder.bridge(lower, upper, color)
    builder.cap(ring_ids[0], c + Vector((0.0, 0.0, rz * (1.0 + jitter * noise(seed, 0, 99)))), color)
    builder.cap(ring_ids[-1], c + Vector((0.0, 0.0, rz * floor_z)), color, flip=True)


def mirrored(builder: MeshBuilder, axis: int = 1) -> MeshBuilder:
    """Copia espejada respecto al plano perpendicular a «axis» (normales correctas)."""
    scale = [1.0, 1.0, 1.0]
    scale[axis] = -1.0
    out = MeshBuilder()
    out.extend(builder, Matrix.Diagonal((*scale, 1.0)))
    return out
