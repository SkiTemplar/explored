"""
run_animals.py — genera el kit de fauna completo de «Explored» (biblia §6).

Se ejecuta dentro de Blender 5.2 en modo headless:

    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" ^
        -b --factory-startup --python Tools\\Blender\\run_animals.py

A diferencia de run_all.py (vegetación: una malla = un FBX), cada especie de
fauna es un KIT DE PIEZAS: tantas mallas independientes como huesos del rig
(cuerpo/caparazón, cabeza, patas, cola, alas, aletas...), cada una con el
origen de objeto en el pivote de su articulación y SIN NINGUNA rotación de
objeto (la nota de Tools/Blender/animals/rig.py). Por cada variante:

    1. Construye las piezas (mod.build(variant)).
    2. Genera un LOD1 opcional (decimate) para las piezas de bulto pesadas.
    3. Exporta cada pieza (y su LOD1 si existe) a su propio FBX en
       Art/Export/Meshes/Fauna/<Especie>/SM_<Especie>_<Pieza>[_LOD1].fbx.
    4. Anota en animals.json, por pieza: nombre, «role» (bone lógico:
       body/root, head, neck, jaw, leg, tail, wing, fin, flipper, tentacle,
       spine, ear, eye, beak, shell, bell), padre y «local_offset_cm» — el
       offset de traslación PURA respecto al padre (sin rotación posible en
       este rig), tal y como pide la nota de rig.py: pivot_cm que build()
       devuelve es absoluto respecto a la raíz de la especie, y aquí se
       resta el pivote del padre para dejar el offset local que el C++
       necesita para reconstruir la jerarquía. «pivot_cm» (absoluto) se deja
       también, solo como referencia de depuración.

Fuera de alcance a propósito: luciérnagas, mariposas, abejas, libélulas y
peces voladores (biblia §6, "fauna ambiental") — el diseño los resuelve con
sistemas de partículas/bandadas (boids), no con mallas rigged individuales.

3ª pasada (encargo del autor, 2026-09-26): la fauna terrestre grande/mediana
(perro, mono, jabalí, cocodrilo, iguana, gecko, serpiente, murciélago,
gallina) y el pulpo se retiran del juego; el kit se queda con fauna marina y
aves, SIN esqueleto — nada de huesos ni de jerarquía animada en C++
(ProceduralGait.h queda sin usar por este kit). La animación es o bien un
shader de vértices (peces, tortuga, medusa, aves) o piezas rígidas sin más
animación (patas de cangrejo). Para eso, además del color visual «Col» de
siempre, cada pieza exportada lleva un SEGUNDO atributo de color de vértice
«Anim» (FLOAT_COLOR, CORNER) que consumirá el material de Unreal — el
convenio completo está documentado en _write_anim_channel() más abajo y
repetido en animals.json como referencia rápida para quien escriba ese
material. Resumen: Anim.R = posición a lo largo del cuerpo (0 cola -> 1
cabeza, sobre el eje X de todo el kit), Anim.G = 1.0 si la pieza es un
apéndice deformable (aleta/ala/pata/tentáculo/garra) o 0.0 si es
cuerpo/cabeza rígido, Anim.B = lado (0 izquierda, 0.5 centro, 1 derecha).
Cada especie anota además su «anim» en animals.json: swim | flap | pulse |
scuttle.
"""

import importlib
import json
import os
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
BLENDER_DIR = os.path.join(REPO_ROOT, 'Tools', 'Blender')
LIB_DIR = os.path.join(BLENDER_DIR, 'lib')
ANIMALS_DIR = os.path.join(BLENDER_DIR, 'animals')
EXPORT_DIR = os.path.join(REPO_ROOT, 'Art', 'Export', 'Meshes', 'Fauna')

for _p in (LIB_DIR, ANIMALS_DIR):
    if _p not in sys.path:
        sys.path.insert(0, _p)

import common as C  # noqa: E402
import rig  # noqa: E402

# 3ª pasada (encargo del autor): fuera los animales terrestres grandes y
# medianos (perro, mono, jabalí, cocodrilo, iguana, gecko, serpiente,
# murciélago, gallina) y el pulpo — el juego se queda con fauna marina y
# aves, SIN esqueleto (animada en shader de vértices o piezas rígidas, ver
# ANIM_MASK_ROLES/_write_anim_channel más abajo). quadrupeds.py, reptiles.py,
# serpent.py, bat.py y cephalopod.py se dejan en el repo tal cual (no hace
# falta borrarlos) pero ya no se generan.
MODULE_NAMES = ['turtle', 'fish', 'jellyfish', 'birds']

# Presupuesto orientativo de triángulos TOTAL por especie (suma de todas sus
# piezas, LOD0). Orientativo aquí (solo se avisa); validate.py es quien lo
# hace cumplir de verdad antes de dar el kit por bueno.
TRIANGLE_BUDGET_BY_CATEGORY = {
    'jellyfish': (300, 4200),
    'bird': (300, 3000),
    'turtle': (400, 4500),
    'fish': (100, 6000),
}

# Tipo de animación que espera el shader de Unreal (documentado también en
# animals.json de salida): swim = onda de columna con ProceduralGait-style
# fuera de la CPU (todo en shader, ver _write_anim_channel); pulse = bulto
# de la medusa contrayéndose; scuttle = ciclo de patas de artrópodo, aquí
# piezas rígidas sin más -no se anima por shader, solo se listan-; flap =
# aleteo de ave.
ANIM_BY_CATEGORY = {
    'jellyfish': 'pulse',
    'bird': 'flap',
    'turtle': 'swim',
    'fish': 'swim',
}

# Roles que el shader debe tratar como APÉNDICE deformable (aleteo/vaivén
# propio, encima de la onda de columna del cuerpo principal) frente al
# cuerpo/cráneo/caparazón rígido que solo seguiría la onda de columna.
ANIM_MASK_ROLES = {'fin', 'wing', 'flipper', 'leg', 'tentacle', 'claw', 'tail'}

LOD1_MIN_TRIS = 140
LOD1_RATIO = 0.5


def _import_or_reload(name):
    if name in sys.modules:
        return importlib.reload(sys.modules[name])
    return importlib.import_module(name)


def _species_bbox_cm(pieces, abs_by_name):
    xs, ys, zs = [], [], []
    for p in pieces:
        piv = abs_by_name[p['name']]
        for v in p['obj'].data.vertices:
            xs.append(v.co.x * 100.0 + piv[0])
            ys.append(v.co.y * 100.0 + piv[1])
            zs.append(v.co.z * 100.0 + piv[2])
    if not xs:
        return (0.0, 0.0, 0.0)
    return (max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs))


def _species_x_range(pieces, abs_by_name):
    """Rango de X absoluto (cm) de toda la especie: la referencia para
    normalizar spine_t en _write_anim_channel («X adelante» es la
    convención de eje de todo el kit, así que X ES el eje del cuerpo)."""
    xs = []
    for p in pieces:
        piv = abs_by_name[p['name']]
        for v in p['obj'].data.vertices:
            xs.append(v.co.x * 100.0 + piv[0])
    if not xs:
        return (0.0, 1.0)
    return (min(xs), max(xs))


def _write_anim_channel(pieces, abs_by_name, x_min, x_max):
    """Escribe el segundo canal de color de vértice «Anim» (FLOAT_COLOR,
    CORNER) que consumirá el shader de animación en Unreal (sin esqueleto:
    todo el movimiento sale de estos datos + tiempo, en el material):

        R = spine_t: posición a lo largo del cuerpo normalizada al bbox
            entero de la especie, 0 = extremo de cola, 1 = extremo de
            cabeza (recto en X porque «X adelante» es la convención de eje
            de todo el kit: basta proyectar la posición absoluta de cada
            vértice sobre X, no hace falta ningún caso especial por
            archetype). Pensado para una onda de columna tipo
            sin(spine_t * frecuencia - fase(tiempo)) que crece hacia la
            cola.
        G = mask: 1.0 si la pieza es un apéndice deformable (aleta, ala,
            pata, tentáculo, garra, aleta caudal — ANIM_MASK_ROLES), 0.0 si
            es cuerpo/cabeza/caparazón rígido que solo sigue spine_t.
        B = side: 0.0 pieza del lado IZQUIERDO (Y absoluto < -0.5 cm),
            1.0 lado DERECHO (Y > +0.5 cm), 0.5 centrada (aleta dorsal,
            caudal, cuerpo...) — para que el shader pueda invertir el signo
            del aleteo/aleta entre lados opuestos.
        A = sin usar, 1.0 (reservado).

    IMPORTANTE para quien importe estos FBX (Blender o Unreal): «Anim» es
    DATO NUMÉRICO, no color visual — hay que leerlo/reimportarlo en LINEAL,
    nunca con la decodificación sRGB por defecto de un importador de FBX
    (en Blender: bpy.ops.import_scene.fbx(..., colors_type='LINEAR'), igual
    que ya hace render_preview.py con «Col»). Verificado en runtime: sin
    ese flag, un 0.5 escrito aquí vuelve como ~0.216 al reimportar (la
    conversión sRGB->lineal aplicada de más), lo que rompería spine_t/
    mask/side en el material si el shader los diera por sentado tal cual.

    «Col» (el primer color de vértice) sigue siendo SOLO el aspecto visual
    -no toca esta función-; se deja como el color activo tras escribir
    «Anim» para que los materiales/preview existentes seteados por
    finalize_piece no cambien de comportamiento.
    """
    span = max(x_max - x_min, 1e-6)
    for p in pieces:
        obj = p['obj']
        me = obj.data
        piv = abs_by_name[p['name']]
        mask = 1.0 if p['role'] in ANIM_MASK_ROLES else 0.0
        if piv[1] > 0.5:
            side = 1.0
        elif piv[1] < -0.5:
            side = 0.0
        else:
            side = 0.5
        if 'Anim' in me.color_attributes:
            me.color_attributes.remove(me.color_attributes['Anim'])
        attr = me.color_attributes.new('Anim', 'FLOAT_COLOR', 'CORNER')
        for poly in me.polygons:
            for li in poly.loop_indices:
                v = me.vertices[me.loops[li].vertex_index]
                world_x = piv[0] + v.co.x * 100.0
                t = max(0.0, min(1.0, (world_x - x_min) / span))
                attr.data[li].color = (t, mask, side, 1.0)
        if 'Col' in me.color_attributes:
            me.color_attributes.active_color_name = 'Col'


def _export_piece(species, out_dir, piece):
    fname = f"SM_{species}_{piece['name']}.fbx"
    fpath = os.path.join(out_dir, fname)
    C.export_fbx(fpath, [piece['obj']])
    return (os.path.relpath(fpath, EXPORT_DIR).replace('\\', '/'),
            C.triangle_count(piece['obj']))


def main():
    os.makedirs(EXPORT_DIR, exist_ok=True)
    species_list = []

    for mod_name in MODULE_NAMES:
        mod = _import_or_reload(mod_name)

        for variant in mod.VARIANTS:
            C.reset_scene()
            result = mod.build(variant)
            species = result['species']
            pieces = result['pieces']
            locomotion = result['locomotion']

            out_dir = os.path.join(EXPORT_DIR, species)
            os.makedirs(out_dir, exist_ok=True)

            abs_by_name = {p['name']: p['pivot_cm'] for p in pieces}
            dims_cm = _species_bbox_cm(pieces, abs_by_name)
            x_min, x_max = _species_x_range(pieces, abs_by_name)
            _write_anim_channel(pieces, abs_by_name, x_min, x_max)

            piece_entries = []
            total_tris = 0
            for p in pieces:
                file_rel, tris = _export_piece(species, out_dir, p)
                total_tris += tris

                parent = p['parent']
                if parent is None:
                    local_offset = p['pivot_cm']
                else:
                    parent_abs = abs_by_name[parent]
                    local_offset = tuple(round(a - b, 4) for a, b in
                                          zip(p['pivot_cm'], parent_abs, strict=True))

                entry = dict(
                    name=p['name'], parent=parent, role=p['role'],
                    local_offset_cm=list(local_offset),
                    pivot_cm=list(p['pivot_cm']),
                    file=file_rel, triangles=tris,
                )

                lod = rig.make_lod1(p, ratio=LOD1_RATIO, min_tris=LOD1_MIN_TRIS)
                if lod is not None:
                    lod_file, lod_tris = _export_piece(species, out_dir, lod)
                    entry['lod1_file'] = lod_file
                    entry['lod1_triangles'] = lod_tris

                piece_entries.append(entry)

            lo, hi = TRIANGLE_BUDGET_BY_CATEGORY.get(mod.CATEGORY, (0, 999999))
            in_budget = lo <= total_tris <= hi
            species_list.append(dict(
                species=species, category=mod.CATEGORY, locomotion=locomotion,
                anim=variant.get('anim_override', ANIM_BY_CATEGORY.get(mod.CATEGORY, 'swim')),
                habitat=variant.get('habitat', ''), behavior=variant.get('behavior', ''),
                use=variant.get('use', ''), diet=variant.get('diet', ''),
                material=f'M_Fauna_{species}',
                triangles_total=total_tris,
                triangle_budget=dict(min=lo, max=hi, in_budget=in_budget),
                dimensions_cm=dict(x=round(dims_cm[0], 1), y=round(dims_cm[1], 1),
                                    z=round(dims_cm[2], 1)),
                piece_count=len(piece_entries),
                pieces=piece_entries,
            ))
            flag = 'OK' if in_budget else 'FUERA DE PRESUPUESTO'
            print(f"[run_animals] {species}: {total_tris} tris en {len(piece_entries)} "
                  f"piezas ({flag}), dims_cm={tuple(round(d, 1) for d in dims_cm)}")

    anim_channel_doc = dict(
        summary='Fauna sin esqueleto: cada FBX lleva un 2º color de vertice '
                '"Anim" (FLOAT_COLOR, CORNER) ademas de "Col" (aspecto '
                'visual). Reimportar SIEMPRE en lineal (colors_type=\'LINEAR\' '
                'en Blender) - un byte-color por defecto aplica sRGB de mas '
                'y rompe los valores.',
        R_spine_t='0.0 = extremo de cola, 1.0 = extremo de cabeza; posicion '
                  'absoluta del vertice sobre X normalizada al bbox de TODA '
                  'la especie (no solo la pieza).',
        G_mask='1.0 = apendice deformable (fin/wing/flipper/leg/tentacle/'
               'claw/tail), 0.0 = cuerpo/cabeza/caparazon rigido.',
        B_side='0.0 = lado izquierdo (Y<-0.5cm), 1.0 = derecho (Y>+0.5cm), '
               '0.5 = centrado (aleta dorsal/caudal, cuerpo...).',
        A='sin usar, siempre 1.0 (reservado).',
        anim_values='swim (peces/tortuga), flap (aves), pulse (medusa).',
    )
    manifest_path = os.path.join(EXPORT_DIR, 'animals.json')
    with open(manifest_path, 'w', encoding='utf-8') as f:
        json.dump(dict(generated_by='Tools/Blender/run_animals.py',
                        anim_channel=anim_channel_doc,
                        species_count=len(species_list), species=species_list),
                  f, indent=2, ensure_ascii=False)
    print(f"[run_animals] manifest escrito en {manifest_path} ({len(species_list)} especies)")


if __name__ == '__main__':
    main()
