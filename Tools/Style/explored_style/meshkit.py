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
from mathutils import Matrix, Vector

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
        for f, c in zip(other.faces, other.colors, strict=True):
            self.face(tuple(i + offset for i in f), c)

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


def build_object(name: str, builder: MeshBuilder) -> bpy.types.Object:
    """Crea el objeto en la escena: UV por celda, normales planas, un material
    y el pivote en el centro de la base (z mínima = 0)."""
    if not builder.faces:
        raise ValueError(f'{name}: malla vacía')
    min_z = min(v[2] for v in builder.verts)
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
