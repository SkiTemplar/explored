"""Estiliza una malla generada por IA (Hunyuan3D) al estilo de Explored.

Pasos: elimina piezas sueltas, reduce a un presupuesto low-poly con sombreado suave,
proyecta los colores de la imagen de referencia (vista lateral) sobre los vértices,
los cuantiza a una paleta cartoon limpia y suaviza el resultado por vecindad.

Uso dentro de Blender (MCP o headless):
    import sys; sys.path.insert(0, r"<repo>/Tools/Blender/ai")
    import stylize
    stylize.stylize(obj, image_path, palette, target_faces=4500)
"""

from __future__ import annotations

import bmesh
import bpy


def _srgb_to_linear(c: float) -> float:
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def remove_loose_parts(obj: bpy.types.Object, min_fraction: float = 0.02) -> int:
    """Borra las islas de malla con menos de min_fraction de los vértices totales."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    seen: set[int] = set()
    islands: list[list[bmesh.types.BMVert]] = []
    for start in bm.verts:
        if start.index in seen:
            continue
        stack = [start]
        island = []
        seen.add(start.index)
        while stack:
            v = stack.pop()
            island.append(v)
            for e in v.link_edges:
                w = e.other_vert(v)
                if w.index not in seen:
                    seen.add(w.index)
                    stack.append(w)
        islands.append(island)
    total = len(bm.verts)
    doomed = [v for isl in islands if len(isl) < total * min_fraction for v in isl]
    bmesh.ops.delete(bm, geom=doomed, context="VERTS")
    bm.to_mesh(obj.data)
    bm.free()
    return len(doomed)


def decimate(obj: bpy.types.Object, target_faces: int) -> None:
    faces = len(obj.data.polygons)
    if faces > target_faces:
        mod = obj.modifiers.new("Decimate", "DECIMATE")
        mod.ratio = target_faces / faces
        with bpy.context.temp_override(object=obj, active_object=obj):
            bpy.ops.object.modifier_apply(modifier=mod.name)
    for poly in obj.data.polygons:
        poly.use_smooth = True


def _reference_sampler(image_path: str):
    img = bpy.data.images.load(image_path, check_existing=True)
    width, height = img.size
    pixels = list(img.pixels)

    def pixel(x: int, y: int) -> tuple[float, float, float]:
        i = (y * width + x) * 4
        return pixels[i], pixels[i + 1], pixels[i + 2]

    def is_background(c) -> bool:
        return sum(c) > 2.7

    xs, ys = [], []
    for y in range(0, height, 2):
        for x in range(0, width, 2):
            if not is_background(pixel(x, y)):
                xs.append(x)
                ys.append(y)
    box = (min(xs), max(xs), min(ys), max(ys))

    def sample(u: float, t: float):
        x = int(box[0] + u * (box[1] - box[0]))
        y = int(box[2] + t * (box[3] - box[2]))
        x = min(max(x, 0), width - 1)
        for dy in range(0, 24):
            for yy in (y - dy, y + dy):
                if 0 <= yy < height and not is_background(pixel(x, yy)):
                    return pixel(x, yy)
        return None

    return sample


def paint_from_reference(obj, image_path: str, palette: list[tuple[int, int, int]],
                         head_on_left: bool = True, smooth_passes: int = 2,
                         head_only: dict[int, int] | None = None, head_fraction: float = 0.3) -> None:
    """Proyecta la referencia lateral y cuantiza a la paleta (sRGB 0-255).

    head_only: {índice de paleta: sustituto} para colores que solo pueden aparecer en la
    cabeza (ojos, nariz). Fuera de ella, las sombras de la referencia caerían en ese color.
    """
    mesh = obj.data
    xs = [v.co.x for v in mesh.vertices]
    zs = [v.co.z for v in mesh.vertices]
    min_x, max_x, min_z, max_z = min(xs), max(xs), min(zs), max(zs)
    top = [v.co.x for v in mesh.vertices if v.co.z > max_z - 0.25 * (max_z - min_z)]
    head_x = sum(top) / len(top)
    flip = (head_x > (min_x + max_x) / 2) == head_on_left

    sample = _reference_sampler(image_path)
    pal = [tuple(c / 255.0 for c in p) for p in palette]

    def nearest(c) -> int:
        return min(range(len(pal)), key=lambda i: sum((a - b) ** 2 for a, b in zip(c, pal[i])))

    labels = []
    for v in mesh.vertices:
        u = (v.co.x - min_x) / (max_x - min_x)
        if flip:
            u = 1.0 - u
        t = (v.co.z - min_z) / (max_z - min_z)
        c = sample(u, t)
        label = nearest(c) if c else 0
        if head_only and label in head_only and u > head_fraction:
            label = head_only[label]
        labels.append(label)

    # Suavizado por mayoría entre vecinos: quita el moteado de la proyección.
    neighbours = [[] for _ in mesh.vertices]
    for e in mesh.edges:
        a, b = e.vertices
        neighbours[a].append(b)
        neighbours[b].append(a)
    for _ in range(smooth_passes):
        new = labels[:]
        for i, ns in enumerate(neighbours):
            counts: dict[int, int] = {labels[i]: 1}
            for n in ns:
                counts[labels[n]] = counts.get(labels[n], 0) + 1
            new[i] = max(counts, key=counts.get)
        labels = new

    attr = mesh.color_attributes.get("Col") or mesh.color_attributes.new("Col", "BYTE_COLOR", "POINT")
    for i, label in enumerate(labels):
        r, g, b = pal[label]
        attr.data[i].color = (_srgb_to_linear(r), _srgb_to_linear(g), _srgb_to_linear(b), 1.0)
    mesh.color_attributes.active_color = attr


def vertex_color_material(obj, name: str, roughness: float = 0.6) -> None:
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    bsdf = next(n for n in nodes if n.type == "BSDF_PRINCIPLED")
    vc = next((n for n in nodes if n.type == "VERTEX_COLOR"), None) or nodes.new("ShaderNodeVertexColor")
    vc.layer_name = "Col"
    mat.node_tree.links.new(vc.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = roughness
    obj.data.materials.clear()
    obj.data.materials.append(mat)


def stylize(obj, image_path: str, palette, target_faces: int = 4500, head_on_left: bool = True,
            head_only: dict[int, int] | None = None) -> dict:
    removed = remove_loose_parts(obj)
    decimate(obj, target_faces)
    paint_from_reference(obj, image_path, palette, head_on_left, 3, head_only)
    vertex_color_material(obj, f"M_{obj.name}")
    return {"removed_verts": removed, "faces": len(obj.data.polygons)}


# ---------------------------------------------------------------------------
# Pintado por zonas geometricas (props): la proyeccion de color de arriba
# asume una vista lateral limpia (animales) y no sirve con la vista 3/4 que
# pide el prompt de Canva para props (fragmentos de la referencia caen en
# el lado equivocado de la malla). Aqui se pinta por reglas geometricas
# (altura local, normal, isla de malla) en vez de proyectar la imagen.
# ---------------------------------------------------------------------------

def _island_groups(obj) -> list[list[int]]:
    """Islas de malla conectada (listas de indices de vertice) via bmesh."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    seen: set[int] = set()
    islands: list[list[int]] = []
    for start in bm.verts:
        if start.index in seen:
            continue
        stack = [start]
        island: list[int] = []
        seen.add(start.index)
        while stack:
            v = stack.pop()
            island.append(v.index)
            for e in v.link_edges:
                w = e.other_vert(v)
                if w.index not in seen:
                    seen.add(w.index)
                    stack.append(w)
        islands.append(island)
    bm.free()
    return islands


def _island_roles(obj, islands: list[list[int]]) -> dict[int, str]:
    """Etiqueta cada vertice segun el papel de su isla: 'largest' (cuerpo
    principal), 'thinnest' (entre el resto, la de mayor relacion
    longitud/radio: antenas, mangos, asas...) u 'other'. Con una sola isla
    (caso mas comun: malla unica watertight de Hunyuan3D) todo cae en
    'largest' y las reglas de altura/normal de zone_paint siguen
    funcionando igual."""
    if not islands:
        return {}
    coords = obj.data.vertices
    ranked = sorted(islands, key=len, reverse=True)
    labels: dict[int, str] = {i: 'largest' for i in ranked[0]}
    best_ratio, thin_island = 0.0, None
    for isl in ranked[1:]:
        xs = [coords[i].co.x for i in isl]
        ys = [coords[i].co.y for i in isl]
        zs = [coords[i].co.z for i in isl]
        span = max(max(xs) - min(xs), max(ys) - min(ys), 1e-5)
        ratio = (max(zs) - min(zs)) / span
        if ratio > best_ratio:
            best_ratio, thin_island = ratio, isl
    for isl in ranked[1:]:
        role = 'thinnest' if isl is thin_island else 'other'
        for i in isl:
            labels[i] = role
    return labels


def _write_vertex_colors(obj, color_fn) -> None:
    """Misma convencion que common.set_vertex_colors: atributo «Col»,
    FLOAT_COLOR, dominio CORNER (el que sobrevive el viaje FBX de ida y
    vuelta, ver notas de render_preview.py)."""
    mesh = obj.data
    if "Col" in mesh.color_attributes:
        mesh.color_attributes.remove(mesh.color_attributes["Col"])
    attr = mesh.color_attributes.new(name="Col", type="FLOAT_COLOR", domain="CORNER")
    for poly in mesh.polygons:
        for li in poly.loop_indices:
            v = mesh.vertices[mesh.loops[li].vertex_index]
            attr.data[li].color = color_fn(v)
    mesh.color_attributes.active_color_name = "Col"
    mesh.color_attributes.render_color_index = list(mesh.color_attributes).index(attr)


def zone_paint(obj, base_color: tuple[float, float, float], rules: list[dict],
               jitter: float = 0.02, rnd=None) -> None:
    """Pinta por vertice segun una lista de reglas geometricas, en vez de
    proyectar la imagen de referencia. Cada regla es un dict con 'color'
    (RGB 0-1) y condiciones opcionales que deben cumplirse TODAS:
      - 'z_frac': (lo, hi) fraccion 0-1 de la altura local del objeto
      - 'island': 'largest' | 'thinnest' | 'other'
      - 'normal_axis' ('x'/'y'/'z') + 'normal_sign' (1/-1) + 'normal_thresh':
        producto escalar de la normal del vertice contra ese eje >= threshold
    Gana la primera regla que casa; si ninguna casa, se usa base_color.
    """
    mesh = obj.data
    islands = _island_groups(obj)
    roles = _island_roles(obj, islands)
    normals = {v.index: v.normal for v in mesh.vertices}

    zs = [v.co.z for v in mesh.vertices]
    z_min, z_max = min(zs), max(zs)
    span = max(1e-6, z_max - z_min)
    axis_idx = {'x': 0, 'y': 1, 'z': 2}
    cache: dict[int, float] = {}

    def pick_color(v) -> tuple[float, float, float]:
        t = (v.co.z - z_min) / span
        n = normals[v.index]
        role = roles.get(v.index, 'largest')
        for rule in rules:
            if 'z_frac' in rule:
                lo, hi = rule['z_frac']
                if not (lo <= t <= hi):
                    continue
            if 'island' in rule and rule['island'] != role:
                continue
            if 'normal_axis' in rule:
                idx = axis_idx[rule['normal_axis']]
                sign = rule.get('normal_sign', 1)
                if n[idx] * sign < rule.get('normal_thresh', 0.5):
                    continue
            return rule['color']
        return base_color

    def color_fn(v):
        if v.index not in cache:
            cache[v.index] = rnd.uniform(-jitter, jitter) if (jitter and rnd is not None) else 0.0
        j = cache[v.index]
        r, g, b = pick_color(v)
        return (max(0.0, min(1.0, r + j)), max(0.0, min(1.0, g + j)),
                max(0.0, min(1.0, b + j)), 1.0)

    _write_vertex_colors(obj, color_fn)


def stylize_prop(obj, base_color: tuple[float, float, float], rules: list[dict],
                  target_faces: int = 4500, jitter: float = 0.02, rnd=None) -> dict:
    """Version del pipeline `stylize()` para props (sin cabeza / vista
    lateral): quita piezas sueltas, decima al presupuesto y pinta por
    zonas geometricas en vez de proyectar la referencia."""
    removed = remove_loose_parts(obj)
    decimate(obj, target_faces)
    zone_paint(obj, base_color, rules, jitter=jitter, rnd=rnd)
    return {"removed_verts": removed, "faces": len(obj.data.polygons)}
