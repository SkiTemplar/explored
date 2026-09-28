"""
Tools/Blender/animals/rig.py — helpers compartidos del kit de fauna.

Capa fina sobre Tools/Blender/lib/common.py con lo específico de fauna que
no tiene sentido meter en el común (compartido con vegetación): el material
único por especie (base blanca × color de vértice «Col»), el ensamblado de
piezas en dicts uniformes (name/parent/pivot_cm/role/obj) que consume
run_animals.py para escribir animals.json, y las cadenas de cápsulas
ahusadas que cubren patas, cola, alas y garras sin repetir código en cada
script de especie.

Convención de pose de reposo (regla del encargo): cada pieza es una malla
independiente con el ORIGEN DEL OBJETO en el pivote de la articulación
(rotation = identidad, nunca se llama transform_apply con rotación). El
kit entero se construye en una única base ortonormal alineada con los ejes
de la especie (X adelante, Y derecha, Z arriba, igual que Unreal tras
exportar), así que «pivot_cm» es sencillamente la posición absoluta de
cada pivote respecto a la raíz de la especie (Body/Shell/…, en (0,0,0)):
el offset relativo al padre que pide animals.json es una resta directa,
sin ninguna rotación de por medio.
"""

import os
import sys
from collections.abc import Sequence
from typing import overload

import bpy

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402

CM_PER_M = 100.0


@overload
def cm_to_m(value: float) -> float: ...
@overload
def cm_to_m(value: Sequence[float]) -> tuple[float, ...]: ...
def cm_to_m(value):
    """Centímetros a metros: un escalar o una tupla/lista de escalares."""
    if isinstance(value, (tuple, list)):
        return tuple(v / CM_PER_M for v in value)
    return value / CM_PER_M


def _radii_cm_to_m(radii_cm):
    out = []
    for r in radii_cm:
        if isinstance(r, (tuple, list)):
            out.append((r[0] / CM_PER_M, r[1] / CM_PER_M))
        else:
            out.append(r / CM_PER_M)
    return out


def get_fauna_material(species, roughness=0.65):
    """Material único por especie: base blanca constante multiplicada por
    el atributo de color de vértice «Col» (toda la variación de tono vive
    en el color de vértice de cada pieza, no en el material)."""
    name = f'M_Fauna_{species}'
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get('Principled BSDF')
    attr = nt.nodes.new('ShaderNodeAttribute')
    attr.attribute_name = 'Col'
    attr.attribute_type = 'GEOMETRY'
    nt.links.new(attr.outputs['Color'], bsdf.inputs['Base Color'])
    bsdf.inputs['Roughness'].default_value = roughness
    if 'Metallic' in bsdf.inputs:
        bsdf.inputs['Metallic'].default_value = 0.0
    return mat


def finalize_piece(name, parent, pivot_cm, role, obj, species, color_fn,
                    smooth_angle=45.0):
    """Cierre común de toda pieza: color de vértice, sombreado suave y el
    material único de la especie, devuelto ya como el dict uniforme que
    consume run_animals.py (name/parent/pivot_cm/role/obj)."""
    C.set_vertex_colors(obj, color_fn)
    C.assign_materials(obj, [])  # limpia slots heredados antes de asignar
    obj.data.materials.append(get_fauna_material(species))
    C.shade_smooth_auto(obj, angle_deg=smooth_angle)
    return dict(name=name, parent=parent, pivot_cm=tuple(pivot_cm), role=role, obj=obj)


def build_blob_piece(name, parent, pivot_cm, role, species, center_offset_cm,
                      radii_cm, color_fn, seed=0, subdivisions=2,
                      noise_strength=0.0, relax=1, scale_extra=(1.0, 1.0, 1.0),
                      axis='x', end_taper=0.6, extra_nodes_cm=None):
    """Pieza de bulto redondeado (cuerpo, cabeza, caparazón...) vía el
    modificador Skin (common.make_skin_blob): una cadena de 3 nodos a lo
    largo de «axis» -por defecto X, «adelante» en todo el kit- centrada en
    «center_offset_cm» (offset local desde el pivote, para que la cabeza
    no infle alrededor de la propia articulación del cuello), con remates
    redondeados y una superficie ya suave por construcción -sin el ruido
    picudo de la esfera de icosaedro deformada de antes-.
    «extra_nodes_cm» (opcional) suelda bultos secundarios al nodo central
    en una sola superficie continua: lista de (offset_xyz_cm, (rx,ry,rz)
    cm), para por ejemplo el morro de un perro saliendo del cráneo sin la
    costura de unir dos blobs aparte. seed/noise_strength/relax se
    conservan solo por compatibilidad de llamada (el aspecto orgánico ya
    lo da el propio modificador Skin, no hace falta desplazar vértices)."""
    del seed, noise_strength, relax  # compatibilidad de firma, sin uso
    center_m = cm_to_m(center_offset_cm)
    rx, ry, rz = (r * s / CM_PER_M for r, s in zip(radii_cm, scale_extra, strict=True))
    extra = None
    if extra_nodes_cm:
        extra = [(cm_to_m(off), tuple(v / CM_PER_M for v in r)) for off, r in extra_nodes_cm]
    subsurf = 2 if subdivisions >= 2 else 1
    obj = C.make_skin_blob(name, center_m, (rx, ry, rz), subsurf_levels=subsurf,
                            extra_nodes=extra, axis=axis, end_taper=end_taper)
    return finalize_piece(name, parent, pivot_cm, role, obj, species, color_fn)


def skin_chain(direction, length_cm, radii_cm, overlap_start=True, overlap_end=False):
    """Versión pública de _skin_segment: para cuando un archetype necesita
    el objeto EN BRUTO (sin finalize_piece todavía) porque aún tiene que
    unirle ojos u otro detalle antes de pintar el color de vértice -la
    cabeza con hocico soldado de quadrupeds.py, por ejemplo-."""
    return _skin_segment(direction, length_cm, radii_cm, overlap_start, overlap_end)


def skin_profile_chain(direction, segment_lengths_cm, radii_cm, overlap_start=True,
                        overlap_end=False):
    """Como skin_chain, pero con la distancia ENTRE nodos consecutivos dada
    explícitamente («segment_lengths_cm», N-1 valores para N radios) en
    vez de repartir la longitud total a partes iguales -necesario cuando
    el perfil tiene tramos de proporción muy distinta, como el cráneo
    corto y el hocico largo de un cocodrilo, donde muestrear a intervalos
    regulares comprimiría el hocico en vez de dejarlo largo de verdad-."""
    d = C.Vector(direction)
    if d.length < 1e-8:
        d = C.Vector((0.0, 0.0, 1.0))
    d.normalize()
    radii_m = _radii_cm_to_m(radii_cm)
    seg_m = [cm_to_m(s) for s in segment_lengths_cm]
    positions = [0.0]
    for s in seg_m:
        positions.append(positions[-1] + s)
    verts = [tuple(d * p) for p in positions]
    if overlap_start and seg_m:
        verts[0] = tuple(d * (-min(seg_m[0] * 0.30, 0.03)))
    if overlap_end and seg_m:
        verts[-1] = tuple(d * (positions[-1] + min(seg_m[-1] * 0.30, 0.03)))
    edges = [(i, i + 1) for i in range(len(radii_m) - 1)]
    return C.make_skin_mesh('Segment', verts, edges, radii_m, subsurf_levels=1)


def _skin_segment(direction, length_cm, radii_cm, overlap_start, overlap_end):
    """Segmento orgánico común a build_capsule_piece y build_chain: cadena
    de Skin a lo largo de «direction» con un nodo por muestra de
    «radii_cm», igual que antes (misma unidad de muestreo regular
    pivote->punta), pero con remates redondeados por el propio modificador
    en vez de un domo hecho a mano. «overlap_start»/«overlap_end» alargan
    ese extremo un poco MÁS ALLÁ del pivote/la punta -hacia dentro del
    padre o de la siguiente pieza de la cadena- para que el remate quede
    oculto dentro del volumen vecino en vez de leerse como la costura
    entre piezas que dejaba make_tapered_capsule."""
    d = C.Vector(direction)
    if d.length < 1e-8:
        d = C.Vector((0.0, 0.0, 1.0))
    d.normalize()
    radii_m = _radii_cm_to_m(radii_cm)
    length_m = cm_to_m(length_cm)
    n = len(radii_m)
    overlap_m = min(length_m * 0.30, 0.03)
    verts = [tuple(d * (length_m * i / (n - 1))) for i in range(n)]
    if overlap_start:
        verts[0] = tuple(d * (-overlap_m))
    if overlap_end:
        verts[-1] = tuple(d * (length_m + overlap_m))
    edges = [(i, i + 1) for i in range(n - 1)]
    # el propio modificador Skin añade más anillos por su cuenta cuanto más
    # fino y alargado es el segmento (para que un tubo muy fino no se vea
    # facetado): un tentáculo de medusa de 0,5 cm de radio generaba x7 más
    # triángulos que una pata de perro de 4 cm ANTES incluso de sumar
    # Subsurf. Con radios pequeños (patas finas, tentáculos, dedos de ala)
    # esa resolución ya basta de sobra, así que Subsurf encima solo
    # encarece el presupuesto sin aportar suavidad perceptible.
    max_r = max(max(r) if isinstance(r, (tuple, list)) else r for r in radii_m)
    subsurf = 1 if max_r >= 0.015 else 0
    return C.make_skin_mesh('Segment', verts, edges, radii_m, subsurf_levels=subsurf)


def build_capsule_piece(name, parent, pivot_cm, role, species, direction,
                         length_cm, radii_cm, color_fn, segments=8,
                         cap_start=True, cap_end=True, dome=0.55,
                         overlap_start=True, overlap_end=False):
    """Pieza única orgánica (cuello, morro, mandíbula, aleta...) vía Skin.
    segments/cap_start/cap_end/dome quedan solo por compatibilidad de
    firma: el modificador Skin siempre da remates redondeados, no hace
    falta elegir anillos ni domos a mano."""
    del segments, cap_start, cap_end, dome  # compatibilidad de firma, sin uso
    obj = _skin_segment(direction, length_cm, radii_cm, overlap_start, overlap_end)
    obj.name = name
    return finalize_piece(name, parent, pivot_cm, role, obj, species, color_fn)


def build_chain(names, parent0, base_pivot_cm, role, species, directions,
                 lengths_cm, radii_profiles_cm, color_fn, segments=8):
    """Cadena orgánica (pata de 2 segmentos, cola de 3, garra de 2...):
    cada segmento es su propia pieza (vía Skin, ver _skin_segment), con el
    pivote en la punta del segmento anterior y el padre encadenado.
    «color_fn» puede ser una única función (mismo tinte en toda la
    cadena) o una lista con una por segmento. Cada segmento se solapa con
    el anterior (o con el padre, en el primero) y con el siguiente -salvo
    el último, que queda como punta libre sin solape-, para que la cadena
    entera se lea como una sola extremidad continua en vez de tubos
    pegados. segments se conserva solo por compatibilidad de firma."""
    del segments  # compatibilidad de firma, sin uso
    pieces = []
    pivot = list(base_pivot_cm)
    parent_name = parent0
    color_fns = color_fn if isinstance(color_fn, (list, tuple)) else [color_fn] * len(names)
    n_segs = len(names)
    for i, name in enumerate(names):
        d = directions[i]
        length = lengths_cm[i]
        radii = radii_profiles_cm[i]
        is_last = (i == n_segs - 1)
        obj = _skin_segment(d, length, radii, overlap_start=True, overlap_end=not is_last)
        obj.name = name
        pieces.append(finalize_piece(name, parent_name, tuple(pivot), role, obj,
                                      species, color_fns[i]))
        pivot = [pivot[0] + d[0] * length, pivot[1] + d[1] * length, pivot[2] + d[2] * length]
        parent_name = name
    return pieces


def build_blade_piece(name, parent, pivot_cm, role, species, direction, up_hint,
                       length_cm, width_base_cm, width_tip_cm, curve_cm,
                       color_fn, segments=6, double_sided=True):
    """Pieza plana orgánica (ala, aleta): una «hoja» (common.make_leaf_blade,
    ya usada para el follaje) en vez de una cápsula ahusada elíptica -así
    un ala se lee como un ala de verdad (perfil curvo, borde afilado) y no
    como un «palillo» plano-. Crece en +Y por construcción; se reorienta a
    «direction» (con «up_hint» como referencia de giro) y esa rotación se
    hornea en la malla (orient_and_place aplica el transform), dejando el
    objeto con rotación identidad y el pivote en (0,0,0), igual que el
    resto de piezas del kit."""
    obj = C.make_leaf_blade(name, cm_to_m(length_cm), cm_to_m(width_base_cm),
                             cm_to_m(width_tip_cm), cm_to_m(curve_cm),
                             segments=segments, double_sided=double_sided)
    C.orient_and_place(obj, (0.0, 0.0, 0.0), direction, up_hint)
    return finalize_piece(name, parent, pivot_cm, role, obj, species, color_fn)


def attach_eyes(obj, eye_offsets_cm, eye_radius_cm, seed=0):
    """Añade los bultos de ojo (en coordenadas locales de la pieza, cm) y
    devuelve (obj_unido, eye_centers_m) para envolver el color_fn de la
    pieza con common.with_eye_dots antes de finalize_piece."""
    centers_m = [cm_to_m(c) for c in eye_offsets_cm]
    obj = C.add_eyes(obj, centers_m, eye_radius_cm / CM_PER_M, subdivisions=1, seed=seed)
    return obj, centers_m


def piece_dict(name, parent, pivot_cm, role, obj):
    return dict(name=name, parent=parent, pivot_cm=tuple(pivot_cm), role=role, obj=obj)


def make_lod1(piece, ratio=0.5, min_tris=140):
    """LOD1 opcional para piezas de bulto (body/head/shell/spine): duplica
    la malla ya finalizada (con color de vértice y material ya asignados)
    y le aplica un Decimate (COLLAPSE) para aligerarla a distancia. Solo
    compensa para piezas por encima de «min_tris» triángulos —los segmentos
    de pata/cola/aleta ya son ligeros y decimarlos los deja picudos—, así
    que devuelve None cuando no aplica (run_animals.py entonces exporta
    solo el LOD0). El decimate conserva el atributo de color de vértice
    porque Blender lo interpola igual que las UV al colapsar aristas."""
    obj = piece['obj']
    if C.triangle_count(obj) < min_tris:
        return None
    C.select_only(obj)
    bpy.ops.object.duplicate()
    lod_obj = bpy.context.view_layer.objects.active
    lod_obj.name = piece['name'] + '_LOD1'
    mod = lod_obj.modifiers.new('LOD1', 'DECIMATE')
    mod.ratio = ratio
    bpy.context.view_layer.objects.active = lod_obj
    bpy.ops.object.modifier_apply(modifier=mod.name)
    C.shade_smooth_auto(lod_obj, angle_deg=45.0)
    return dict(name=lod_obj.name, parent=piece['parent'], pivot_cm=piece['pivot_cm'],
                role=piece['role'], obj=lod_obj)
