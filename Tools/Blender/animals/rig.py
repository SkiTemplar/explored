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

import bpy

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402

CM_PER_M = 100.0


def cm_to_m(value):
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
                      radii_cm, color_fn, seed, subdivisions=2,
                      noise_strength=0.06, relax=1, scale_extra=(1.0, 1.0, 1.0)):
    """Pieza de bulto redondeado (cuerpo, cabeza, caparazón...): esfera de
    icosaedro con radios elípticos independientes por eje, en cm, con el
    pivote de la pieza en (0,0,0) y el bulto centrado en «center_offset_cm»
    (offset local desde el pivote, para que la cabeza no infle alrededor
    de la propia articulación del cuello)."""
    center_m = cm_to_m(center_offset_cm)
    rx, ry, rz = (r / CM_PER_M for r in radii_cm)
    obj = C.make_blob(name, center_m, 1.0, seed=seed, subdivisions=subdivisions,
                       noise_strength=noise_strength,
                       scale=(rx * scale_extra[0], ry * scale_extra[1], rz * scale_extra[2]),
                       relax_iterations=relax)
    return finalize_piece(name, parent, pivot_cm, role, obj, species, color_fn)


def build_capsule_piece(name, parent, pivot_cm, role, species, direction,
                         length_cm, radii_cm, color_fn, segments=8,
                         cap_start=True, cap_end=True, dome=0.55):
    """Pieza única de cápsula ahusada (cuello, morro, mandíbula, aleta...)."""
    obj = C.make_tapered_capsule(name, direction, cm_to_m(length_cm),
                                  _radii_cm_to_m(radii_cm), segments=segments,
                                  cap_start=cap_start, cap_end=cap_end, dome=dome)
    return finalize_piece(name, parent, pivot_cm, role, obj, species, color_fn)


def build_chain(names, parent0, base_pivot_cm, role, species, directions,
                 lengths_cm, radii_profiles_cm, color_fn, segments=8):
    """Cadena de cápsulas ahusadas (pata de 2 segmentos, cola de 3, ala de
    2, garra de 2...): cada segmento es su propia pieza, con el pivote en
    la punta del segmento anterior y el padre encadenado. «color_fn» puede
    ser una única función (mismo tinte en toda la cadena) o una lista con
    una por segmento (p.ej. pata más clara hacia la pezuña)."""
    pieces = []
    pivot = list(base_pivot_cm)
    parent_name = parent0
    color_fns = color_fn if isinstance(color_fn, (list, tuple)) else [color_fn] * len(names)
    for i, name in enumerate(names):
        d = directions[i]
        length = lengths_cm[i]
        radii = radii_profiles_cm[i]
        obj = C.make_tapered_capsule(name, d, cm_to_m(length), _radii_cm_to_m(radii),
                                      segments=segments, cap_start=True, cap_end=True)
        pieces.append(finalize_piece(name, parent_name, tuple(pivot), role, obj,
                                      species, color_fns[i]))
        pivot = [pivot[0] + d[0] * length, pivot[1] + d[1] * length, pivot[2] + d[2] * length]
        parent_name = name
    return pieces


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
