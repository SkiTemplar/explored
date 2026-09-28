"""
normalize.py — normaliza mallas de packs CC0 según Content/Data/packs_catalogo.json.

Se ejecuta dentro de Blender (5.2, sin interfaz):

    blender -b --factory-startup --python Tools/Packs/normalize.py -- \
        --lote lote1-herramientas [--ids hacha cuchillo] [--isla Landing] \
        [--analyze] [--tiles] [--no-export]

Por cada entrada del lote:

1. Importa el fichero del pack desde la caché (``Art/Packs/<pack>/<file>``, la deja
   ``fetch_packs.py``; ``$EXPLORED_PACKS_CACHE`` la cambia).
2. Une las mallas, aplica ``rotateDeg`` (Euler XYZ, grados) para dejar el mango en +Z y
   el filo en +X (convención de ``Tools/Blender/props/_items.py``).
3. **Recolorea**: toma el color de cada cara en la textura o el material del pack (en el
   centro UV de la cara) y le asigna la muestra de ``Tools/Textures/paleta.json`` de la
   regla ``recolor`` más cercana en Oklab (``from`` en sRGB; ``default`` si ninguna está a
   menos de ``tolerance``). Escribe **UV de la paleta** (u en el centro de la columna, v
   repartida por la altura del objeto dentro de ``v_rango``: arriba claro, abajo oscuro,
   como pide ``docs/art/paleta.md``) y el mismo degradado como color de vértice lineal
   «Col» (vista previa y respaldo). En Unreal basta con ``M_LowPoly`` y el atlas de la
   isla: la UV es la misma en las cuatro islas.
4. ``stretch`` opcional (factores x, y, z, p. ej. adelgazar un tronco rechoncho) y escala
   uniforme para que la medida del eje ``size.axis`` sea ``size.m`` metros.
5. Pivote: ``base`` (centro de la huella, z = 0 en el punto más bajo) o ``agarre``
   (socket de mano ``hand_r``: a ``gripFromEndM`` del extremo ``end`` del mango, ``bottom``
   por defecto o ``top`` para la mano alta de la pala, en el eje del mango). El eje se
   mide en el extremo (``centerAt: end``, por defecto) o a la altura del agarre
   (``centerAt: grip``, para arcos y lanzas).
6. Exporta ``Art/Export/Packs/<lote>/<mesh>.fbx`` (ignorado en git) con un único
   material ``M_LowPoly``, triangulado, mismos ajustes FBX que ``Tools/Blender``.

Fauna con esqueleto (``rig`` en la entrada, ``kind: fauna``): el fichero suele ser el
``.blend`` del pack, porque el FBX de Quaternius Farm Animals solo trae Idle y Jump. Se
conservan armadura, pesos y todas las acciones: el giro, la escala y el pivote se aplican a
la armadura y a la malla juntas, y las claves de ``location`` de los huesos se multiplican
por la misma escala (``transform_apply`` no las toca y el animal se desplazaría a la escala
original). Exporta ``SK_Pack_*.fbx`` con malla, armadura y una toma por acción.

``--analyze`` no exporta: lista, por fichero, los colores del pack agrupados (ΔE < 0.05)
con su parte del área y la altura media (0 abajo, 1 arriba), para escribir las reglas.
``--tiles`` renderiza (Workbench) una viñeta por entrada con el original a la izquierda
y el normalizado a la derecha en ``Art/Export/Packs/<lote>/_tiles/``; la hoja de contacto
la compone ``contact_sheet.py``.
"""

from __future__ import annotations

import json
import math
import os
import sys
from collections.abc import Sequence
from pathlib import Path
from typing import Any, cast

import bpy
from mathutils import Euler, Matrix, Vector

HERE = Path(__file__).resolve().parent
# Blender no añade la carpeta del script a sys.path (el intérprete estándar sí).
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from normalize_core import (  # noqa: E402
    RGB,
    compile_rules,
    delta_e,
    gradient,
    linear_to_srgb,
    load_json,
    oklab,
    parse_args,
    pick_swatch,
    pivot_origin,
    rgb3,
    share_percent,
    size_factor,
    srgb_to_linear,
    to_hex,
)

REPO = HERE.parent.parent
CATALOG = REPO / "Content" / "Data" / "packs_catalogo.json"
PALETTE = REPO / "Tools" / "Textures" / "paleta.json"
EXPORT = REPO / "Art" / "Export" / "Packs"
PACKS_JSON = HERE / "packs.json"

GREY: RGB = (0.8, 0.8, 0.8)


def cache_dir() -> Path:
    env = os.environ.get("EXPLORED_PACKS_CACHE")
    return Path(env) if env else REPO / "Art" / "Packs"


def mesh_of(obj: bpy.types.Object) -> bpy.types.Mesh:
    """Datos de malla de ``obj``; falla con un mensaje claro si el objeto no es una malla."""
    me = obj.data
    if not isinstance(me, bpy.types.Mesh):
        raise TypeError(f"{obj.name}: se esperaba una malla y es {type(me).__name__}")
    return me


def armature_of(obj: bpy.types.Object) -> bpy.types.Armature:
    arm = obj.data
    if not isinstance(arm, bpy.types.Armature):
        raise TypeError(f"{obj.name}: se esperaba una armadura y es {type(arm).__name__}")
    return arm


def euler_matrix(rot_deg: list[float] | None) -> Matrix:
    """Matriz 4×4 del giro Euler XYZ en grados (``rotateDeg`` del catálogo)."""
    return Euler([math.radians(a) for a in (rot_deg or [0, 0, 0])], "XYZ").to_matrix().to_4x4()


def triangle_count(me: bpy.types.Mesh) -> int:
    return sum(p.loop_total - 2 for p in me.polygons)


# ---------------------------------------------------------------------------
# Importación
# ---------------------------------------------------------------------------

def reset_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)


def import_file(path: Path) -> bpy.types.Object:
    """Importa y une todas las mallas en un objeto con las transformaciones aplicadas."""
    before = set(bpy.data.objects)
    ext = path.suffix.lower()
    if ext in (".gltf", ".glb"):
        bpy.ops.import_scene.gltf(filepath=str(path))
    elif ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=str(path))
    elif ext == ".obj":
        bpy.ops.wm.obj_import(filepath=str(path))
    else:
        raise ValueError(f"formato no admitido: {path}")
    new = [o for o in bpy.data.objects if o not in before]
    # Nombres, no referencias: join() borra las mallas unidas y sus referencias caducan.
    new_names = [o.name for o in new]
    meshes = [o for o in new if o.type == "MESH"]
    if not meshes:
        raise RuntimeError(f"{path.name}: sin mallas")
    # Quita esqueletos/animación: aquí solo se normaliza la malla estática de vista previa;
    # los animales con rig se importan en Unreal desde el FBX original del pack.
    for o in meshes:
        for m in list(o.modifiers):
            o.modifiers.remove(m)
        # Las claves de forma (cuerda de arco de KayKit) guardan su propia copia de los
        # vértices: si quedan, ni el render ni el FBX ven la escala ni el giro aplicados.
        if mesh_of(o).shape_keys:
            o.shape_key_clear()
        o.parent_type = "OBJECT"
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        mw = o.matrix_world.copy()
        o.parent = None
        o.matrix_world = mw
        o.select_set(True)
    view_layer = bpy.context.view_layer
    view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = view_layer.objects.active
    if obj is None:
        raise RuntimeError(f"{path.name}: la unión no dejó objeto activo")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for name in new_names:
        o = bpy.data.objects.get(name)
        if o is not None and o != obj:
            bpy.data.objects.remove(o, do_unlink=True)
    obj.name = path.stem
    return obj


def import_rigged(path: Path) -> tuple[bpy.types.Object, bpy.types.Object]:
    """Importa un animal con esqueleto sin quitarle nada: devuelve (malla, armadura).

    Un ``.blend`` se anexa entero (objetos y acciones, que en Quaternius tienen usuario
    falso y no cuelgan de ningún objeto); un FBX o glTF se importa tal cual.
    """
    before = set(bpy.data.objects)
    ext = path.suffix.lower()
    if ext == ".blend":
        # Los stubs declaran que load() devuelve None; en Blender es un gestor de contexto.
        loader = cast(Any, bpy.data.libraries.load(str(path), link=False))
        with loader as (src, dst):
            dst.objects = list(src.objects)
            dst.actions = list(src.actions)
        for o in dst.objects:
            if o is not None and o.type in {"MESH", "ARMATURE"}:
                bpy.context.scene.collection.objects.link(o)
    elif ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=str(path))
    elif ext in (".gltf", ".glb"):
        bpy.ops.import_scene.gltf(filepath=str(path))
    else:
        raise ValueError(f"formato no admitido para rig: {path}")
    new = [o for o in bpy.data.objects if o not in before and o.users_scene]
    arms = [o for o in new if o.type == "ARMATURE"]
    meshes = [o for o in new if o.type == "MESH"]
    if len(arms) != 1 or len(meshes) != 1:
        raise RuntimeError(f"{path.name}: se espera una armadura y una malla, hay {len(arms)} y {len(meshes)}")
    mesh, arm = meshes[0], arms[0]
    if not any(isinstance(m, bpy.types.ArmatureModifier) and m.object == arm for m in mesh.modifiers):
        raise RuntimeError(f"{path.name}: la malla no está deformada por la armadura")
    if mesh_of(mesh).shape_keys:
        raise RuntimeError(f"{path.name}: claves de forma en una malla con rig (no soportado)")
    for a in bpy.data.actions:
        for fc in action_fcurves(a):
            if not fc.data_path.startswith("pose.bones["):
                raise RuntimeError(f"{path.name}: la acción {a.name} anima el objeto ({fc.data_path}), no solo huesos")
    drop_orphan_channels(arm)
    mesh.name = path.stem
    return mesh, arm


def floats(prop: object) -> list[float]:
    """Copia de un ``bpy_prop_array`` (los stubs no lo declaran indexable)."""
    return [float(x) for x in cast(Sequence[float], prop)]


def keyframe_strips(action: bpy.types.Action) -> list[bpy.types.ActionKeyframeStrip]:
    """Tiras con fotogramas clave de una acción por capas (Blender 4.4+)."""
    return [strip for layer in action.layers for strip in layer.strips
            if isinstance(strip, bpy.types.ActionKeyframeStrip)]


def bone_of_path(data_path: str) -> str | None:
    """Nombre del hueso de ``pose.bones["X"].location`` (None si la curva no es de un hueso)."""
    return data_path.split('"')[1] if '"' in data_path else None


def drop_orphan_channels(arm: bpy.types.Object) -> None:
    """Quita curvas de huesos que la armadura no tiene.

    Quaternius Farm Animals comparte acciones entre especies: las del cerdo animan
    ``Tail1``..``Tail4``, que su esqueleto no trae. El exportador FBX descarta entera
    cualquier acción con una curva que no resuelve (por eso el FBX del pack solo trae Idle
    y Jump); sin esas curvas salen las seis.
    """
    bones = {b.name for b in armature_of(arm).bones}
    for a in bpy.data.actions:
        dropped: set[str] = set()
        for strip in keyframe_strips(a):
            for bag in strip.channelbags:
                for fc in list(bag.fcurves):
                    name = bone_of_path(fc.data_path)
                    if name is not None and name not in bones:
                        dropped.add(name)
                        bag.fcurves.remove(fc)
        if dropped:
            print(f"   {a.name}: sin curvas de huesos ausentes {sorted(dropped)}")


def action_fcurves(action: bpy.types.Action) -> list[bpy.types.FCurve]:
    """Curvas de una acción (acciones por capas de Blender 4.4+)."""
    return [fc for strip in keyframe_strips(action) for bag in strip.channelbags for fc in bag.fcurves]


# ---------------------------------------------------------------------------
# Muestreo del color original de cada cara
# ---------------------------------------------------------------------------

class ImageSampler:
    def __init__(self, image: bpy.types.Image):
        w, h = floats(image.size)
        self.w, self.h = int(w), int(h)
        if self.w <= 0 or self.h <= 0:
            raise ValueError(f"imagen vacía: {image.name}")
        self.px = floats(image.pixels)  # RGBA, valores tal como se guardan (sRGB en PNG de 8 bits)

    def sample(self, u: float, v: float) -> RGB:
        x = int((u % 1.0) * self.w) % self.w
        y = int((v % 1.0) * self.h) % self.h
        i = (y * self.w + x) * 4
        return rgb3(self.px[i:i + 3])


def material_source(mat: bpy.types.Material | None) -> tuple[ImageSampler | None, RGB]:
    """Textura de color base del material (si la hay) o su color base en sRGB."""
    if mat is None:
        return None, GREY
    # Blender 5: todo material tiene árbol de nodos (``use_nodes`` está obsoleto); sin BSDF
    # de Principled se usa el color de la vista.
    if mat.node_tree is not None:
        for node in mat.node_tree.nodes:
            if node.type == "BSDF_PRINCIPLED":
                inp = node.inputs["Base Color"]
                if inp.is_linked and inp.links:
                    src = inp.links[0].from_node
                    if isinstance(src, bpy.types.ShaderNodeTexImage) and src.image is not None:
                        return ImageSampler(src.image), GREY
                col = inp.default_value  # pyright: ignore[reportAttributeAccessIssue] -- socket de color
                return None, rgb3(linear_to_srgb(c) for c in col[:3])
    return None, rgb3(linear_to_srgb(c) for c in floats(mat.diffuse_color)[:3])


def face_colors(obj: bpy.types.Object) -> list[RGB]:
    """Color sRGB original de cada polígono (centro UV sobre la textura, o color del material)."""
    me = mesh_of(obj)
    sources = [material_source(m) for m in me.materials] or [(None, GREY)]
    uv = me.uv_layers.active.data if me.uv_layers.active else None
    out: list[RGB] = []
    for p in me.polygons:
        sampler, flat = sources[min(p.material_index, len(sources) - 1)]
        if sampler is not None and uv is not None:
            us = [uv[li].uv for li in p.loop_indices]
            u = sum(c.x for c in us) / len(us)
            v = sum(c.y for c in us) / len(us)
            out.append(sampler.sample(u, v))
        else:
            out.append(flat)
    return out


def write_color_attr(obj: bpy.types.Object, name: str, loop_colors: list[list[float]]) -> None:
    me = mesh_of(obj)
    if len(loop_colors) != len(me.loops):
        raise ValueError(f"{obj.name}: {len(loop_colors)} colores para {len(me.loops)} esquinas")
    if name in me.color_attributes:
        me.color_attributes.remove(me.color_attributes[name])
    attr = me.color_attributes.new(name=name, type="FLOAT_COLOR", domain="CORNER")
    if not isinstance(attr, bpy.types.FloatColorAttribute):
        raise RuntimeError(f"{obj.name}: no se pudo crear el atributo de color {name}")
    flat: list[float] = []
    for c in loop_colors:
        flat.extend((c[0], c[1], c[2], 1.0))
    attr.data.foreach_set("color", flat)
    me.color_attributes.active_color = attr
    me.color_attributes.render_color_index = me.color_attributes.find(name)


def z_range(me: bpy.types.Mesh) -> tuple[float, float]:
    """(z mínima, altura) de la malla; la altura nunca es 0 para poder dividir."""
    if not me.vertices:
        raise ValueError(f"{me.name}: malla sin vértices")
    zs = [v.co.z for v in me.vertices]
    zmin = min(zs)
    return zmin, max(max(zs) - zmin, 1e-6)


# ---------------------------------------------------------------------------
# Análisis de colores del pack
# ---------------------------------------------------------------------------

def analyze(obj: bpy.types.Object, label: str) -> list[dict[str, Any]]:
    """Imprime y devuelve los colores del pack agrupados (ΔE < 0.05), de más a menos área."""
    me = mesh_of(obj)
    cols = face_colors(obj)
    zmin, span = z_range(me)
    clusters: list[dict[str, Any]] = []
    total = 0.0
    for p, c in zip(me.polygons, cols, strict=True):
        lab = oklab(c)
        area = p.area
        total += area
        h = (p.center.z - zmin) / span
        for cl in clusters:
            if delta_e(cl["lab"], lab) < 0.05:
                cl["area"] += area
                cl["h"] += h * area
                break
        else:
            clusters.append({"lab": lab, "srgb": c, "area": area, "h": h * area})
    clusters.sort(key=lambda c: -c["area"])
    dims = tuple(round(d, 3) for d in obj.dimensions)
    print(f"ANALYZE {label}  dims={dims}  tris={triangle_count(me)}")
    total = total or 1.0
    for cl in clusters[:10]:
        h_avg = cl["h"] / cl["area"] if cl["area"] else 0.0
        print(f"   {to_hex(cl['srgb'])}  {100 * cl['area'] / total:5.1f}%  h={h_avg:.2f}")
    return clusters


# ---------------------------------------------------------------------------
# Normalización
# ---------------------------------------------------------------------------

def recolor(obj: bpy.types.Object, entry: dict[str, Any], palette: dict[str, Any]) -> dict[str, float]:
    """Asigna UV de paleta y color de vértice «Col». Devuelve {muestra: % de área}."""
    compiled, tol, default = compile_rules(entry["recolor"])
    me = mesh_of(obj)
    cols = face_colors(obj)
    zmin, span = z_range(me)

    targets: list[str] = []
    share: dict[str, float] = {}
    for p, c in zip(me.polygons, cols, strict=True):
        best = pick_swatch(c, compiled, tol, default, entry.get("gameId", "?"))
        if best not in palette:
            raise KeyError(f"{entry.get('gameId', '?')}: la muestra {best} no está en la paleta de la isla")
        targets.append(best)
        share[best] = share.get(best, 0.0) + p.area

    # UV de paleta en una capa nueva, única.
    while me.uv_layers:
        me.uv_layers.remove(me.uv_layers[0])
    uvl = me.uv_layers.new(name="UVMap")
    if uvl is None:
        raise RuntimeError(f"{obj.name}: no se pudo crear la capa UV")
    loop_cols: list[list[float]] = [[0.0, 0.0, 0.0] for _ in range(len(me.loops))]
    for p, t in zip(me.polygons, targets, strict=True):
        for li in p.loop_indices:
            z = me.vertices[me.loops[li].vertex_index].co.z
            uv, col = gradient(palette[t], (z - zmin) / span)  # 1 arriba, 0 abajo
            uvl.data[li].uv = uv
            loop_cols[li] = col
    write_color_attr(obj, "Col", loop_cols)

    mat = bpy.data.materials.get("M_LowPoly") or bpy.data.materials.new("M_LowPoly")
    if mat.node_tree is None:
        mat.use_nodes = True  # Blender < 5
    nt = mat.node_tree
    if nt is None:
        raise RuntimeError("M_LowPoly sin árbol de nodos")
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    attr = nt.nodes.new("ShaderNodeVertexColor")
    attr.layer_name = "Col"  # pyright: ignore[reportAttributeAccessIssue] -- ShaderNodeVertexColor
    nt.links.new(attr.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.85  # pyright: ignore[reportAttributeAccessIssue] -- socket float
    me.materials.clear()
    me.materials.append(mat)
    for p in me.polygons:
        p.material_index = 0
    return share_percent(share)


def transform_mesh(obj: bpy.types.Object, m: Matrix) -> None:
    me = mesh_of(obj)
    me.transform(m)
    me.update()


def orient_scale_pivot(obj: bpy.types.Object, entry: dict[str, Any]) -> None:
    transform_mesh(obj, euler_matrix(entry.get("rotateDeg")))

    stretch = entry.get("stretch")
    if stretch:
        transform_mesh(obj, Matrix.Diagonal((*stretch, 1.0)))

    vs = [v.co.copy() for v in mesh_of(obj).vertices]
    s = size_factor(vs, entry["size"])
    transform_mesh(obj, Matrix.Scale(s, 4))
    origin = pivot_origin([v * s for v in vs], entry["pivot"])
    transform_mesh(obj, Matrix.Translation([-c for c in origin]))


def orient_scale_pivot_rig(mesh: bpy.types.Object, arm: bpy.types.Object, entry: dict[str, Any]) -> float:
    """Gira, escala y centra armadura y malla juntas; devuelve la escala aplicada.

    La medida y el pivote salen de la malla en pose de reposo. Las claves de ``location``
    de los huesos están en el espacio local de cada hueso: el giro no les afecta, pero la
    escala sí, y ``transform_apply`` no la lleva a las acciones.
    """
    if entry.get("stretch"):
        raise ValueError(f"{entry['gameId']}: stretch no se admite con rig (deformaría los huesos)")
    rot = euler_matrix(entry.get("rotateDeg"))
    bpy.context.view_layer.update()
    vs = [rot @ (mesh.matrix_world @ v.co) for v in mesh_of(mesh).vertices]
    s = size_factor(vs, entry["size"])
    origin = pivot_origin([v * s for v in vs], entry["pivot"])
    m = Matrix.Translation([-c for c in origin]) @ Matrix.Scale(s, 4) @ rot
    arm.matrix_world = m @ arm.matrix_world
    if mesh.parent != arm:
        mesh.matrix_world = m @ mesh.matrix_world
    bpy.context.view_layer.update()
    bpy.ops.object.select_all(action="DESELECT")
    for o in (arm, mesh):
        o.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for a in bpy.data.actions:
        for fc in action_fcurves(a):
            if fc.data_path.endswith(".location"):
                for k in fc.keyframe_points:
                    k.co.y *= s
                    k.handle_left.y *= s
                    k.handle_right.y *= s
                fc.update()
    return s


def bbox_dims(obj: bpy.types.Object) -> RGB:
    from normalize_core import bbox_dims as dims_of

    return dims_of([v.co for v in mesh_of(obj).vertices])


def export_fbx(obj: bpy.types.Object, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=str(path), check_existing=False, use_selection=True, global_scale=1.0,
        apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL", axis_forward="-Z", axis_up="Y",
        object_types={"MESH"}, use_mesh_modifiers=True, mesh_smooth_type="FACE", colors_type="LINEAR",
        prioritize_active_color=True, use_triangles=True, use_tspace=True, bake_anim=False,
        path_mode="STRIP", embed_textures=False,
    )


def export_fbx_rig(mesh: bpy.types.Object, arm: bpy.types.Object, path: Path) -> None:
    """Malla con esqueleto y una toma por acción (Unreal crea SK_, SKEL_ y las AnimSequence)."""
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    for o in (arm, mesh):
        o.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.fbx(
        filepath=str(path), check_existing=False, use_selection=True, global_scale=1.0,
        apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL", axis_forward="-Z", axis_up="Y",
        object_types={"MESH", "ARMATURE"}, use_mesh_modifiers=False, mesh_smooth_type="FACE",
        colors_type="LINEAR", prioritize_active_color=True, use_triangles=True, use_tspace=False,
        add_leaf_bones=False, armature_nodetype="NULL", bake_anim=True, bake_anim_use_all_bones=True,
        bake_anim_use_nla_strips=False, bake_anim_use_all_actions=True,
        bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0,
        path_mode="STRIP", embed_textures=False,
    )


def pose_at(arm: bpy.types.Object, action_name: str, frame: float) -> None:
    ad = arm.animation_data or arm.animation_data_create()
    if ad is None:
        raise RuntimeError(f"{arm.name}: sin datos de animación")
    act = bpy.data.actions[action_name]
    ad.action = act
    if len(act.slots):
        ad.action_slot = act.slots[0]
    bpy.context.scene.frame_set(int(frame))


# ---------------------------------------------------------------------------
# Viñetas para la hoja de contacto (Workbench, color de vértice)
# ---------------------------------------------------------------------------

def render_tile(orig: bpy.types.Object, norm: bpy.types.Object, path: Path, px: int = 360) -> None:
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    sh = scene.display.shading
    sh.light = "STUDIO"
    sh.color_type = "VERTEX"
    sh.show_cavity = True
    sh.cavity_type = "WORLD"
    sh.show_shadows = False
    sh.show_object_outline = False
    sh.background_type = "VIEWPORT"
    sh.background_color = (0.33, 0.36, 0.40)
    scene.render.resolution_x = px * 2
    scene.render.resolution_y = px
    scene.render.film_transparent = False
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGB"
    scene.view_settings.view_transform = "Standard"

    dims = bbox_dims(norm)
    gap = max(dims) * 1.25
    orig.location = (-gap / 2, 0, 0)
    # Con rig se mueve la armadura: si la malla se separa de sus huesos, la pose la deforma
    # alrededor de articulaciones que ya no están donde toca.
    (norm.parent or norm).location = (gap / 2, 0, 0)
    cam_data = bpy.data.cameras.new("Cam")
    cam_data.type = "ORTHO"
    cam = bpy.data.objects.new("Cam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    bpy.context.view_layer.update()
    pts = [o.matrix_world @ Vector(c) for o in (orig, norm) for c in o.bound_box]
    center = sum(pts, Vector()) / len(pts)
    direction = Vector((0.55, -1.0, 0.55)).normalized()
    cam.location = center + direction * 10
    cam.rotation_euler = (-direction).to_track_quat("-Z", "Y").to_euler()
    bpy.context.view_layer.update()
    inv = cam.matrix_world.inverted()
    local = [inv @ p for p in pts]
    w = max(p.x for p in local) - min(p.x for p in local)
    h = max(p.y for p in local) - min(p.y for p in local)
    cam_data.ortho_scale = max(w, h * 2) * 1.12
    scene.render.filepath = str(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.render.render(write_still=True)


# ---------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> None:
    if argv is None:
        argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    args = parse_args(argv)
    catalog = load_json(CATALOG)
    packs = {p["id"]: p for p in load_json(PACKS_JSON)["packs"]}
    palette = load_json(PALETTE)["islas"][args["isla"]]["colores"]
    entries = [e for e in catalog["entries"] if args["lote"] in (None, e["lote"])]
    if args["ids"]:
        entries = [e for e in entries if e["gameId"] in args["ids"]]
    if not entries:
        raise SystemExit("sin entradas para ese lote/ids")
    root = cache_dir()
    report: dict[str, Any] = {}
    for e in entries:
        pack = packs[e["pack"]]
        src = root / pack["id"] / e["file"]
        if not src.exists():
            raise SystemExit(f"falta {src}: ejecuta `uv run python fetch_packs.py {pack['id']}` en Tools/Packs")
        reset_scene()
        if e.get("rig"):
            report[e["gameId"]] = normalize_rigged(e, src, palette, args)
            continue
        obj = import_file(src)
        if args["analyze"]:
            transform_mesh(obj, euler_matrix(e.get("rotateDeg")))
            analyze(obj, f"{e['gameId']} <- {e['file']}")
            continue
        # Copia con el color original (mismo encuadre) para la viñeta de antes/después.
        orig = obj.copy()
        orig.data = mesh_of(obj).copy()
        bpy.context.scene.collection.objects.link(orig)
        write_color_attr(orig, "Orig", _loop_orig(orig, face_colors(orig)))
        # Orientar antes de recolorear: el degradado de la paleta va por la altura del
        # objeto ya girado (mango en +Z), no por la del fichero del pack.
        orient_scale_pivot(obj, e)
        orient_scale_pivot(orig, e)
        share = recolor(obj, e, palette)
        dims = bbox_dims(obj)
        tris = triangle_count(mesh_of(obj))
        report[e["gameId"]] = {"mesh": e["mesh"], "dims": [round(d, 3) for d in dims], "tris": tris, "paleta": share}
        print(f"NORM {e['gameId']:16} {e['mesh']:24} dims={report[e['gameId']]['dims']} tris={tris} {share}")
        if args["export"]:
            export_fbx(obj, EXPORT / e["lote"] / f"{e['mesh']}.fbx")
        if args["tiles"]:
            render_tile(orig, obj, EXPORT / e["lote"] / "_tiles" / f"{e['gameId']}.png")
    if report and args["lote"]:
        out = EXPORT / args["lote"] / "_tiles" / "report.json"
        out.parent.mkdir(parents=True, exist_ok=True)
        # Con --ids solo se renormaliza una parte del lote: se conserva lo demás para que
        # contact_sheet.py encuentre todas las entradas.
        if out.exists():
            report = {**load_json(out), **report}
        out.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


def normalize_rigged(e: dict[str, Any], src: Path, palette: dict[str, Any], args: dict[str, Any]) -> dict[str, Any]:
    mesh, arm = import_rigged(src)
    rig = e["rig"]
    missing = [a for a in rig["animations"] if a not in bpy.data.actions]
    if missing:
        raise SystemExit(f"{e['gameId']}: faltan acciones {missing} en {src.name}")
    # Copia estática con el color original para la viñeta (sin armadura: solo reposo).
    orig = mesh.copy()
    orig.data = mesh_of(mesh).copy()
    bpy.context.scene.collection.objects.link(orig)
    mw = orig.matrix_world.copy()
    orig.parent = None
    orig.matrix_world = mw
    for m in list(orig.modifiers):
        orig.modifiers.remove(m)
    bpy.context.view_layer.update()
    mesh_of(orig).transform(orig.matrix_world)
    orig.matrix_world = Matrix.Identity(4)
    if args["analyze"]:
        transform_mesh(orig, euler_matrix(e.get("rotateDeg")))
        analyze(orig, f"{e['gameId']} <- {e['file']}")
        return {}
    write_color_attr(orig, "Orig", _loop_orig(orig, face_colors(orig)))
    share = recolor(mesh, e, palette)
    scale = orient_scale_pivot_rig(mesh, arm, e)
    orient_scale_pivot(orig, e)
    dims = bbox_dims(mesh)
    tris = triangle_count(mesh_of(mesh))
    out = {"mesh": e["mesh"], "dims": [round(d, 3) for d in dims], "tris": tris,
           "bones": len(armature_of(arm).bones), "scale": round(scale, 5),
           "animations": sorted(rig["animations"]), "paleta": share}
    print(f"NORM {e['gameId']:16} {e['mesh']:24} dims={out['dims']} tris={tris} huesos={out['bones']} {share}")
    if args["export"]:
        export_fbx_rig(mesh, arm, EXPORT / e["lote"] / f"{e['mesh']}.fbx")
    if args["tiles"]:
        # Una viñeta por pose: la primera es la del catálogo; las demás (<id>@<acción>.png)
        # sirven para ver que la escala de las claves de location aguanta en movimiento.
        poses = rig.get("tilePoses") or [{"action": rig["animations"][0], "frame": 0}]
        for i, pose in enumerate(poses):
            pose_at(arm, pose["action"], pose["frame"])
            name = e["gameId"] if i == 0 else f"{e['gameId']}@{pose['action']}"
            render_tile(orig, mesh, EXPORT / e["lote"] / "_tiles" / f"{name}.png")
            for o in bpy.data.objects:
                if o.type == "CAMERA":
                    bpy.data.objects.remove(o, do_unlink=True)
    return out


def _loop_orig(obj: bpy.types.Object, cols: list[RGB]) -> list[list[float]]:
    """Color lineal de cada esquina a partir del color sRGB de su polígono."""
    me = mesh_of(obj)
    out: list[list[float]] = [[0.0, 0.0, 0.0] for _ in range(len(me.loops))]
    for p in me.polygons:
        lin = [srgb_to_linear(c) for c in cols[p.index]]
        for li in p.loop_indices:
            out[li] = lin
    return out


if __name__ == "__main__":
    main()
