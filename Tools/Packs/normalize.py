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
from pathlib import Path

import bpy
import bmesh
from mathutils import Euler, Matrix, Vector

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
CATALOG = REPO / "Content" / "Data" / "packs_catalogo.json"
PALETTE = REPO / "Tools" / "Textures" / "paleta.json"
EXPORT = REPO / "Art" / "Export" / "Packs"
PACKS_JSON = HERE / "packs.json"


def cache_dir() -> Path:
    env = os.environ.get("EXPLORED_PACKS_CACHE")
    return Path(env) if env else REPO / "Art" / "Packs"


# ---------------------------------------------------------------------------
# Color: sRGB <-> lineal <-> Oklab (mismas fórmulas que Tools/Textures/texgen)
# ---------------------------------------------------------------------------

def hex_to_srgb(h: str) -> tuple[float, float, float]:
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4))


def srgb_to_linear(c: float) -> float:
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def linear_to_srgb(c: float) -> float:
    c = min(max(c, 0.0), 1.0)
    return 12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055


def oklab(srgb) -> tuple[float, float, float]:
    r, g, b = (srgb_to_linear(c) for c in srgb)
    l_ = 0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b
    m_ = 0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b
    s_ = 0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b
    l_, m_, s_ = (math.copysign(abs(v) ** (1 / 3), v) for v in (l_, m_, s_))
    return (
        0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_,
        1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_,
        0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_,
    )


def delta_e(a, b) -> float:
    return math.dist(a, b)


def to_hex(srgb) -> str:
    return "#" + "".join(f"{round(min(max(c, 0), 1) * 255):02x}" for c in srgb)


# ---------------------------------------------------------------------------
# Datos
# ---------------------------------------------------------------------------

def load_json(p: Path) -> dict:
    return json.loads(p.read_text(encoding="utf-8"))


def parse_args(argv: list[str]) -> dict:
    args = {"lote": None, "ids": [], "isla": "Landing", "analyze": False, "tiles": False, "export": True}
    it = iter(argv)
    key = None
    for a in it:
        if a == "--lote":
            args["lote"] = next(it)
        elif a == "--isla":
            args["isla"] = next(it)
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
        if o.data.shape_keys:
            o.shape_key_clear()
        o.parent_type = "OBJECT"
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        mw = o.matrix_world.copy()
        o.parent = None
        o.matrix_world = mw
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
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
        with bpy.data.libraries.load(str(path), link=False) as (src, dst):
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
    if not any(m.type == "ARMATURE" and m.object == arm for m in mesh.modifiers):
        raise RuntimeError(f"{path.name}: la malla no está deformada por la armadura")
    if mesh.data.shape_keys:
        raise RuntimeError(f"{path.name}: claves de forma en una malla con rig (no soportado)")
    for a in bpy.data.actions:
        for fc in action_fcurves(a):
            if not fc.data_path.startswith("pose.bones["):
                raise RuntimeError(f"{path.name}: la acción {a.name} anima el objeto ({fc.data_path}), no solo huesos")
    drop_orphan_channels(arm)
    mesh.name = path.stem
    return mesh, arm


def drop_orphan_channels(arm) -> None:
    """Quita curvas de huesos que la armadura no tiene.

    Quaternius Farm Animals comparte acciones entre especies: las del cerdo animan
    ``Tail1``..``Tail4``, que su esqueleto no trae. El exportador FBX descarta entera
    cualquier acción con una curva que no resuelve (por eso el FBX del pack solo trae Idle
    y Jump); sin esas curvas salen las seis.
    """
    bones = {b.name for b in arm.data.bones}
    for a in bpy.data.actions:
        dropped = set()
        for layer in getattr(a, "layers", []):
            for strip in layer.strips:
                for bag in strip.channelbags:
                    for fc in list(bag.fcurves):
                        name = fc.data_path.split('"')[1] if '"' in fc.data_path else None
                        if name is not None and name not in bones:
                            dropped.add(name)
                            bag.fcurves.remove(fc)
        if dropped:
            print(f"   {a.name}: sin curvas de huesos ausentes {sorted(dropped)}")


def action_fcurves(action) -> list:
    """Curvas de una acción (acciones por capas de Blender 4.4+ o clásicas)."""
    if getattr(action, "layers", None):
        return [fc for layer in action.layers for strip in layer.strips
                for bag in strip.channelbags for fc in bag.fcurves]
    return list(getattr(action, "fcurves", []))


# ---------------------------------------------------------------------------
# Muestreo del color original de cada cara
# ---------------------------------------------------------------------------

class ImageSampler:
    def __init__(self, image: bpy.types.Image):
        self.w, self.h = image.size
        self.px = list(image.pixels[:])  # RGBA, valores tal como se guardan (sRGB en PNG de 8 bits)

    def sample(self, u: float, v: float) -> tuple[float, float, float]:
        x = int((u % 1.0) * self.w) % self.w
        y = int((v % 1.0) * self.h) % self.h
        i = (y * self.w + x) * 4
        return tuple(self.px[i:i + 3])


def material_source(mat) -> tuple[ImageSampler | None, tuple[float, float, float]]:
    """Textura de color base del material (si la hay) o su color base en sRGB."""
    if mat is None:
        return None, (0.8, 0.8, 0.8)
    if mat.use_nodes:
        for node in mat.node_tree.nodes:
            if node.type == "BSDF_PRINCIPLED":
                inp = node.inputs["Base Color"]
                if inp.is_linked:
                    src = inp.links[0].from_node
                    if src.type == "TEX_IMAGE" and src.image is not None:
                        return ImageSampler(src.image), (0.8, 0.8, 0.8)
                col = inp.default_value
                return None, tuple(linear_to_srgb(c) for c in col[:3])
    col = mat.diffuse_color
    return None, tuple(linear_to_srgb(c) for c in col[:3])


def face_colors(obj) -> list[tuple[float, float, float]]:
    """Color sRGB original de cada polígono (centro UV sobre la textura, o color del material)."""
    me = obj.data
    sources = [material_source(m) for m in me.materials] or [(None, (0.8, 0.8, 0.8))]
    uv = me.uv_layers.active.data if me.uv_layers.active else None
    out = []
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


def write_color_attr(obj, name: str, loop_colors: list) -> None:
    me = obj.data
    if name in me.color_attributes:
        me.color_attributes.remove(me.color_attributes[name])
    attr = me.color_attributes.new(name=name, type="FLOAT_COLOR", domain="CORNER")
    flat = []
    for c in loop_colors:
        flat.extend((c[0], c[1], c[2], 1.0))
    attr.data.foreach_set("color", flat)
    me.color_attributes.active_color = attr
    me.color_attributes.render_color_index = me.color_attributes.find(name)


# ---------------------------------------------------------------------------
# Análisis de colores del pack
# ---------------------------------------------------------------------------

def analyze(obj, label: str) -> None:
    me = obj.data
    cols = face_colors(obj)
    zs = [v.co.z for v in me.vertices]
    zmin, zmax = min(zs), max(zs)
    span = max(zmax - zmin, 1e-6)
    clusters: list[dict] = []
    total = 0.0
    for p, c in zip(me.polygons, cols):
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
    print(f"ANALYZE {label}  dims={tuple(round(d, 3) for d in obj.dimensions)}  tris={sum(len(p.vertices) - 2 for p in me.polygons)}")
    for cl in clusters[:10]:
        print(f"   {to_hex(cl['srgb'])}  {100 * cl['area'] / total:5.1f}%  h={cl['h'] / cl['area']:.2f}")


# ---------------------------------------------------------------------------
# Normalización
# ---------------------------------------------------------------------------

def recolor(obj, entry: dict, palette: dict) -> dict:
    """Asigna UV de paleta y color de vértice «Col». Devuelve {muestra: % de área}."""
    rules = entry["recolor"]
    tol = rules.get("tolerance", 0.12)
    compiled = [(oklab(hex_to_srgb(r["from"])), r["to"]) for r in rules.get("rules", [])]
    default = rules.get("default")
    me = obj.data
    cols = face_colors(obj)
    zs = [v.co.z for v in me.vertices]
    zmin, zmax = min(zs), max(zs)
    span = max(zmax - zmin, 1e-6)

    targets = []
    share: dict[str, float] = {}
    for p, c in zip(me.polygons, cols):
        lab = oklab(c)
        best, dist = None, 1e9
        for flab, to in compiled:
            d = delta_e(flab, lab)
            if d < dist:
                best, dist = to, d
        if best is None or dist > tol:
            if default is None:
                raise RuntimeError(f"{entry['gameId']}: color {to_hex(c)} sin regla y sin default")
            best = default
        targets.append(best)
        share[best] = share.get(best, 0.0) + p.area

    # UV de paleta en una capa nueva, única.
    while me.uv_layers:
        me.uv_layers.remove(me.uv_layers[0])
    uvl = me.uv_layers.new(name="UVMap")
    loop_cols = [None] * len(me.loops)
    for p, t in zip(me.polygons, targets):
        s = palette[t]
        u = s["uv"][0]
        v0, v1 = s["v_rango"]
        top = s["arriba"]["lineal"]
        mid = s["medio"]["lineal"]
        bot = s["abajo"]["lineal"]
        for li in p.loop_indices:
            z = me.vertices[me.loops[li].vertex_index].co.z
            h = (z - zmin) / span  # 1 arriba, 0 abajo
            # En Blender v = 0 es el borde inferior de la imagen; paleta.json da v desde arriba.
            v_img = v0 + (1.0 - h) * (v1 - v0)
            uvl.data[li].uv = (u, 1.0 - v_img)
            if h >= 0.5:
                t2 = (h - 0.5) * 2
                col = [mid[i] + (top[i] - mid[i]) * t2 for i in range(3)]
            else:
                t2 = h * 2
                col = [bot[i] + (mid[i] - bot[i]) * t2 for i in range(3)]
            loop_cols[li] = col
    write_color_attr(obj, "Col", loop_cols)

    mat = bpy.data.materials.get("M_LowPoly") or bpy.data.materials.new("M_LowPoly")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    attr = nt.nodes.new("ShaderNodeVertexColor")
    attr.layer_name = "Col"
    nt.links.new(attr.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.85
    me.materials.clear()
    me.materials.append(mat)
    for p in me.polygons:
        p.material_index = 0
    total = sum(share.values()) or 1.0
    return {k: round(100 * v / total, 1) for k, v in sorted(share.items(), key=lambda kv: -kv[1])}


def transform_mesh(obj, m: Matrix) -> None:
    obj.data.transform(m)
    obj.data.update()


def orient_scale_pivot(obj, entry: dict) -> None:
    rot = entry.get("rotateDeg") or [0, 0, 0]
    transform_mesh(obj, Euler([math.radians(a) for a in rot], "XYZ").to_matrix().to_4x4())

    stretch = entry.get("stretch")
    if stretch:
        transform_mesh(obj, Matrix.Diagonal((*stretch, 1.0)))

    vs = [v.co.copy() for v in obj.data.vertices]
    s = size_factor(vs, entry["size"])
    transform_mesh(obj, Matrix.Scale(s, 4))
    transform_mesh(obj, Matrix.Translation(-pivot_origin([v * s for v in vs], entry["pivot"])))


def size_factor(vs: list, size: dict) -> float:
    dims = [max(getattr(v, a) for v in vs) - min(getattr(v, a) for v in vs) for a in "xyz"]
    axis = size.get("axis", "max")
    cur = max(dims) if axis == "max" else dims["xyz".index(axis)]
    return size["m"] / cur


def pivot_origin(vs: list, pivot: dict) -> Vector:
    zmin = min(v.z for v in vs)
    if pivot["kind"] == "base":
        cx = (min(v.x for v in vs) + max(v.x for v in vs)) / 2
        cy = (min(v.y for v in vs) + max(v.y for v in vs)) / 2
        return Vector((cx, cy, zmin))
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
        return Vector((cx, cy, z))
    raise ValueError(pivot["kind"])


def orient_scale_pivot_rig(mesh, arm, entry: dict) -> float:
    """Gira, escala y centra armadura y malla juntas; devuelve la escala aplicada.

    La medida y el pivote salen de la malla en pose de reposo. Las claves de ``location``
    de los huesos están en el espacio local de cada hueso: el giro no les afecta, pero la
    escala sí, y ``transform_apply`` no la lleva a las acciones.
    """
    if entry.get("stretch"):
        raise ValueError(f"{entry['gameId']}: stretch no se admite con rig (deformaría los huesos)")
    rot = Euler([math.radians(a) for a in entry.get("rotateDeg") or [0, 0, 0]], "XYZ").to_matrix().to_4x4()
    bpy.context.view_layer.update()
    vs = [rot @ (mesh.matrix_world @ v.co) for v in mesh.data.vertices]
    s = size_factor(vs, entry["size"])
    origin = pivot_origin([v * s for v in vs], entry["pivot"])
    m = Matrix.Translation(-origin) @ Matrix.Scale(s, 4) @ rot
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


def bbox_dims(obj) -> tuple[float, float, float]:
    vs = [v.co for v in obj.data.vertices]
    return tuple(max(getattr(v, a) for v in vs) - min(getattr(v, a) for v in vs) for a in "xyz")


def export_fbx(obj, path: Path) -> None:
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


def export_fbx_rig(mesh, arm, path: Path) -> None:
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


def pose_at(arm, action_name: str, frame: float) -> None:
    arm.animation_data_create()
    act = bpy.data.actions[action_name]
    arm.animation_data.action = act
    if getattr(act, "slots", None) and len(act.slots):
        arm.animation_data.action_slot = act.slots[0]
    bpy.context.scene.frame_set(int(frame))


# ---------------------------------------------------------------------------
# Viñetas para la hoja de contacto (Workbench, color de vértice)
# ---------------------------------------------------------------------------

def render_tile(orig, norm, path: Path, px: int = 360) -> None:
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

def main() -> None:
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
    report = {}
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
            transform_mesh(obj, Euler([math.radians(a) for a in (e.get("rotateDeg") or [0, 0, 0])], "XYZ")
                           .to_matrix().to_4x4())
            analyze(obj, f"{e['gameId']} <- {e['file']}")
            continue
        # Copia con el color original (mismo encuadre) para la viñeta de antes/después.
        orig = obj.copy()
        orig.data = obj.data.copy()
        bpy.context.scene.collection.objects.link(orig)
        write_color_attr(orig, "Orig", _loop_orig(orig, face_colors(orig)))
        share = recolor(obj, e, palette)
        orient_scale_pivot(obj, e)
        orient_scale_pivot(orig, e)
        dims = bbox_dims(obj)
        tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
        report[e["gameId"]] = {"mesh": e["mesh"], "dims": [round(d, 3) for d in dims], "tris": tris, "paleta": share}
        print(f"NORM {e['gameId']:16} {e['mesh']:24} dims={report[e['gameId']]['dims']} tris={tris} {share}")
        if args["export"]:
            export_fbx(obj, EXPORT / e["lote"] / f"{e['mesh']}.fbx")
        if args["tiles"]:
            render_tile(orig, obj, EXPORT / e["lote"] / "_tiles" / f"{e['gameId']}.png")
    if report and args["lote"]:
        out = EXPORT / args["lote"] / "_tiles" / "report.json"
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


def normalize_rigged(e: dict, src: Path, palette: dict, args: dict) -> dict:
    mesh, arm = import_rigged(src)
    rig = e["rig"]
    missing = [a for a in rig["animations"] if a not in bpy.data.actions]
    if missing:
        raise SystemExit(f"{e['gameId']}: faltan acciones {missing} en {src.name}")
    # Copia estática con el color original para la viñeta (sin armadura: solo reposo).
    orig = mesh.copy()
    orig.data = mesh.data.copy()
    bpy.context.scene.collection.objects.link(orig)
    mw = orig.matrix_world.copy()
    orig.parent = None
    orig.matrix_world = mw
    for m in list(orig.modifiers):
        orig.modifiers.remove(m)
    bpy.context.view_layer.update()
    orig.data.transform(orig.matrix_world)
    orig.matrix_world = Matrix.Identity(4)
    if args["analyze"]:
        transform_mesh(orig, Euler([math.radians(a) for a in (e.get("rotateDeg") or [0, 0, 0])], "XYZ")
                       .to_matrix().to_4x4())
        analyze(orig, f"{e['gameId']} <- {e['file']}")
        return {}
    write_color_attr(orig, "Orig", _loop_orig(orig, face_colors(orig)))
    share = recolor(mesh, e, palette)
    scale = orient_scale_pivot_rig(mesh, arm, e)
    orient_scale_pivot(orig, e)
    dims = bbox_dims(mesh)
    tris = sum(len(p.vertices) - 2 for p in mesh.data.polygons)
    out = {"mesh": e["mesh"], "dims": [round(d, 3) for d in dims], "tris": tris, "bones": len(arm.data.bones),
           "scale": round(scale, 5), "animations": sorted(rig["animations"]), "paleta": share}
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


def _loop_orig(obj, cols) -> list:
    me = obj.data
    out = [None] * len(me.loops)
    for p in me.polygons:
        lin = [srgb_to_linear(c) for c in cols[p.index]]
        for li in p.loop_indices:
            out[li] = lin
    return out


if __name__ == "__main__":
    main()
