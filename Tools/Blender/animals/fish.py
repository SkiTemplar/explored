"""
Tools/Blender/animals/fish.py — 6 peces de arrecife, raya, tiburón de
arrecife, tiburón tigre, delfín y ballena jorobada (biblia §6). Todos
comparten locomotion=swimmer: ProceduralGait.cpp devuelve una pose vacía
para Swimmer (cuerpo Y patas) porque la ondulación de nadar la resuelve el
material (vertex shader), no el hueso — así que aquí no hace falta ninguna
cadena de columna, solo un cuerpo rígido (torpedo o disco, según la
especie) con aletas decorativas ancladas.

Dos estilos de cuerpo:
    - 'torpedo' (peces y tiburones): una única cápsula ahusada de 3-4
      puntos de radio (rx, ry) a lo largo de +X, con aleta dorsal y
      pectorales opcionales y una aleta caudal al final.
    - 'disc' (raya): un blob muy aplanado y ancho (el cuerpo ES las
      «alas» pectorales fusionadas) con una cola larga en forma de látigo
      (cadena de segmentos, no una aleta única).

Los mamíferos marinos (delfín, ballena) llevan aletas pectorales de 2
segmentos («flipper», como en turtle.py) en vez de la pectoral de una sola
pieza de los peces, y la caudal se construye con up_hint=(0,1,0) para que
quede APLANADA EN HORIZONTAL (aleta de mamífero) en vez de en vertical
(aleta de pez, el up_hint por defecto de make_tapered_capsule).
"""

import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import rig  # noqa: E402

CATEGORY = 'fish'

VARIANTS = [
    dict(
        species='ClownFish', seed=5301, body_style='torpedo', body_len_cm=9.0,
        body_profile_cm=[(0.5, 0.8), (1.8, 2.6), (0.6, 1.1)],
        color=(0.98, 0.86, 0.55), color_dark=(0.85, 0.36, 0.06),
        eye_offset_cm=(0.8, 0.45, 0.5), eye_radius_cm=0.16,
        dorsal=dict(len_cm=2.2, radii_cm=[(0.9, 0.18), (0.35, 0.10)], t=0.55, dir=(-0.15, 0.0, 1.0)),
        pectoral=dict(len_cm=1.4, radii_cm=[(0.6, 0.14), (0.25, 0.08)], t=0.30, dir=(0.35, 1.0, -0.15)),
        tail_fin=dict(len_cm=2.6, radii_cm=[(0.20, 1.2), (0.10, 0.4)], dir=(-1.0, 0.0, 0.0)),
        stride_length_cm=8.0, total_length_cm=9.0,
        habitat='Arrecife', behavior='Bancos', use='Pesca', diet='omnívoro',
    ),
    dict(
        species='ButterflyFish', seed=5302, body_style='torpedo', body_len_cm=14.0,
        body_profile_cm=[(0.5, 1.4), (2.2, 4.2), (0.5, 1.6)],
        color=(0.98, 0.92, 0.70), color_dark=(0.90, 0.72, 0.15),
        eye_offset_cm=(1.1, 0.7, 0.9), eye_radius_cm=0.22,
        dorsal=dict(len_cm=3.4, radii_cm=[(1.3, 0.22), (0.5, 0.12)], t=0.55, dir=(-0.1, 0.0, 1.0)),
        pectoral=dict(len_cm=2.0, radii_cm=[(0.9, 0.16), (0.35, 0.10)], t=0.30, dir=(0.3, 1.0, -0.1)),
        tail_fin=dict(len_cm=3.2, radii_cm=[(0.22, 1.8), (0.10, 0.55)], dir=(-1.0, 0.0, 0.0)),
        stride_length_cm=8.0, total_length_cm=14.0,
        habitat='Arrecife', behavior='Bancos', use='Pesca', diet='omnívoro',
    ),
    dict(
        species='ParrotFish', seed=5303, body_style='torpedo', body_len_cm=32.0,
        body_profile_cm=[(1.4, 2.0), (4.6, 5.4), (1.6, 2.4)],
        color=(0.35, 0.72, 0.68), color_dark=(0.06, 0.32, 0.34),
        eye_offset_cm=(2.4, 1.6, 1.4), eye_radius_cm=0.35,
        dorsal=dict(len_cm=6.0, radii_cm=[(2.0, 0.30), (0.8, 0.18)], t=0.5, dir=(-0.1, 0.0, 1.0)),
        pectoral=dict(len_cm=3.6, radii_cm=[(1.5, 0.25), (0.6, 0.15)], t=0.28, dir=(0.3, 1.0, -0.12)),
        tail_fin=dict(len_cm=5.5, radii_cm=[(0.30, 3.0), (0.15, 0.9)], dir=(-1.0, 0.0, 0.0)),
        stride_length_cm=14.0, total_length_cm=32.0,
        habitat='Arrecife', behavior='Bancos', use='Pesca', diet='herbívoro',
    ),
    dict(
        species='SurgeonFish', seed=5304, body_style='torpedo', body_len_cm=20.0,
        body_profile_cm=[(0.8, 1.6), (2.6, 3.6), (0.9, 1.8)],
        color=(0.30, 0.42, 0.85), color_dark=(0.06, 0.12, 0.38),
        eye_offset_cm=(1.6, 1.0, 1.0), eye_radius_cm=0.22,
        dorsal=dict(len_cm=4.2, radii_cm=[(1.5, 0.26), (0.6, 0.14)], t=0.5, dir=(-0.1, 0.0, 1.0)),
        pectoral=dict(len_cm=2.4, radii_cm=[(1.0, 0.18), (0.4, 0.10)], t=0.28, dir=(0.3, 1.0, -0.1)),
        tail_fin=dict(len_cm=4.0, radii_cm=[(0.24, 2.0), (0.12, 0.6)], dir=(-1.0, 0.0, 0.0)),
        stride_length_cm=10.0, total_length_cm=20.0,
        habitat='Arrecife', behavior='Bancos', use='Pesca', diet='herbívoro',
    ),
    dict(
        species='Grouper', seed=5305, body_style='torpedo', body_len_cm=45.0,
        body_profile_cm=[(2.0, 2.6), (6.5, 7.5), (2.4, 3.2)],
        color=(0.55, 0.46, 0.30), color_dark=(0.22, 0.18, 0.11),
        eye_offset_cm=(3.4, 2.2, 2.0), eye_radius_cm=0.5,
        dorsal=dict(len_cm=7.0, radii_cm=[(2.6, 0.35), (1.0, 0.20)], t=0.5, dir=(-0.1, 0.0, 1.0)),
        pectoral=dict(len_cm=5.0, radii_cm=[(2.0, 0.30), (0.8, 0.18)], t=0.28, dir=(0.3, 1.0, -0.1)),
        tail_fin=dict(len_cm=7.0, radii_cm=[(0.35, 3.6), (0.18, 1.1)], dir=(-1.0, 0.0, 0.0)),
        stride_length_cm=18.0, total_length_cm=45.0,
        habitat='Arrecife', behavior='Bancos', use='Pesca', diet='carnívoro',
    ),
    dict(
        species='Wrasse', seed=5306, body_style='torpedo', body_len_cm=16.0,
        body_profile_cm=[(0.7, 1.1), (2.0, 2.6), (0.7, 1.3)],
        color=(0.55, 0.80, 0.55), color_dark=(0.10, 0.38, 0.16),
        eye_offset_cm=(1.3, 0.8, 0.8), eye_radius_cm=0.20,
        dorsal=dict(len_cm=3.4, radii_cm=[(1.2, 0.20), (0.5, 0.12)], t=0.5, dir=(-0.1, 0.0, 1.0)),
        pectoral=dict(len_cm=1.9, radii_cm=[(0.8, 0.15), (0.3, 0.09)], t=0.28, dir=(0.3, 1.0, -0.1)),
        tail_fin=dict(len_cm=2.8, radii_cm=[(0.22, 1.5), (0.10, 0.45)], dir=(-1.0, 0.0, 0.0)),
        stride_length_cm=8.0, total_length_cm=16.0,
        habitat='Arrecife', behavior='Bancos', use='Pesca', diet='carnívoro',
    ),
    dict(
        species='Stingray', seed=5307, body_style='disc',
        disc_radii_cm=(28.0, 45.0, 7.0),
        color=(0.55, 0.53, 0.50), color_dark=(0.16, 0.15, 0.15),
        eye_offset_cm=(14.0, 7.0, 3.0), eye_radius_cm=0.9,
        tail_chain=dict(
            seg_len_cm=[22.0, 20.0, 18.0],
            seg_radii_cm=[[1.4, 0.9], [0.9, 0.5], [0.5, 0.15]],
            seg_dir=[(-1.0, 0.0, 0.02), (-1.0, 0.0, 0.0), (-1.0, 0.0, -0.02)],
        ),
        stride_length_cm=20.0, total_length_cm=95.0, hip_height_cm=8.0,
        habitat='Fondos arenosos', behavior='Enterrada, pica si la pisas',
        use='Peligro', diet='carnívoro',
    ),
    dict(
        species='ReefShark', seed=5308, body_style='torpedo', body_len_cm=140.0,
        body_profile_cm=[(4.0, 4.5), (11.0, 12.0), (3.0, 4.5), (1.0, 1.6)],
        color=(0.62, 0.65, 0.68), color_dark=(0.20, 0.22, 0.26),
        eye_offset_cm=(9.0, 5.5, 4.0), eye_radius_cm=0.9,
        dorsal=dict(len_cm=22.0, radii_cm=[(7.0, 1.1), (2.0, 0.5)], t=0.45, dir=(-0.15, 0.0, 1.0)),
        pectoral=dict(len_cm=18.0, radii_cm=[(6.0, 1.0), (1.8, 0.5)], t=0.30, dir=(0.25, 1.0, -0.25)),
        tail_fin=dict(len_cm=26.0, radii_cm=[(1.2, 9.0), (0.5, 2.5)], dir=(-1.0, 0.0, 0.08)),
        stride_length_cm=90.0, total_length_cm=140.0,
        habitat='Talud', behavior='Curioso, rara vez ataca', use='Tensión', diet='carnívoro',
    ),
    dict(
        species='TigerShark', seed=5309, body_style='torpedo', body_len_cm=310.0,
        body_profile_cm=[(9.0, 10.0), (24.0, 26.0), (7.0, 10.0), (2.4, 3.6)],
        color=(0.52, 0.58, 0.50), color_dark=(0.16, 0.20, 0.15),
        eye_offset_cm=(20.0, 12.0, 9.0), eye_radius_cm=1.8,
        dorsal=dict(len_cm=42.0, radii_cm=[(13.0, 2.0), (3.6, 0.9)], t=0.42, dir=(-0.15, 0.0, 1.0)),
        pectoral=dict(len_cm=38.0, radii_cm=[(12.0, 1.9), (3.4, 0.9)], t=0.28, dir=(0.25, 1.0, -0.25)),
        tail_fin=dict(len_cm=55.0, radii_cm=[(2.4, 18.0), (1.0, 5.0)], dir=(-1.0, 0.0, 0.08)),
        stride_length_cm=180.0, total_length_cm=310.0,
        habitat='Aguas profundas', behavior='Ataca en mar abierto',
        use='Límite natural del mundo', diet='carnívoro',
    ),
    dict(
        species='Dolphin', seed=5310, body_style='torpedo', is_mammal=True, body_len_cm=230.0,
        body_profile_cm=[(6.0, 7.0), (17.0, 19.0), (6.5, 8.5), (2.0, 2.6)],
        color=(0.80, 0.82, 0.84), color_dark=(0.28, 0.32, 0.38),
        eye_offset_cm=(15.0, 8.0, 7.0), eye_radius_cm=1.1,
        dorsal=dict(len_cm=16.0, radii_cm=[(6.0, 1.0), (2.0, 0.5)], t=0.48, dir=(-0.1, 0.0, 1.0)),
        flipper=dict(len_cm=20.0, radii_cm=[(5.0, 1.0), (1.4, 0.4)], t=0.32, dir=(0.2, 1.0, -0.2)),
        tail_fin=dict(len_cm=14.0, radii_cm=[(1.0, 14.0), (0.4, 3.5)], dir=(-1.0, 0.0, 0.0),
                      up_hint=(0.0, 1.0, 0.0)),
        stride_length_cm=120.0, total_length_cm=230.0,
        habitat='Mar abierto', behavior='Acompañan la canoa, saltan', use='Maravilla', diet='piscívoro',
    ),
    dict(
        species='HumpbackWhale', seed=5311, body_style='torpedo', is_mammal=True, body_len_cm=1300.0,
        body_profile_cm=[(35.0, 45.0), (95.0, 150.0), (45.0, 60.0), (12.0, 18.0)],
        color=(0.60, 0.61, 0.62), color_dark=(0.12, 0.13, 0.16),
        eye_offset_cm=(80.0, 45.0, 30.0), eye_radius_cm=4.0,
        dorsal=dict(len_cm=25.0, radii_cm=[(20.0, 4.0), (6.0, 2.0)], t=0.55, dir=(-0.2, 0.0, 1.0)),
        flipper_chain=dict(
            upper_cm=180.0, lower_cm=160.0,
            radii_cm=[[(30.0, 8.0), (20.0, 6.0)], [(20.0, 6.0), (6.0, 2.0)]],
            t=0.35, dir_upper=(0.10, 1.0, -0.05), dir_lower=(-0.05, 0.95, -0.15),
        ),
        tail_fin=dict(len_cm=90.0, radii_cm=[(8.0, 110.0), (3.0, 25.0)], dir=(-1.0, 0.0, 0.0),
                      up_hint=(0.0, 1.0, 0.0)),
        stride_length_cm=600.0, total_length_cm=1300.0,
        habitat='Mar abierto', behavior='Acompañan la canoa, saltan', use='Maravilla', diet='planctívoro',
    ),
]


def _fin_piece(name, parent, pivot_cm, role, species, direction, length_cm, radii_cm,
               color_fn, flatten='vertical'):
    """Aleta/aleta caudal como una «hoja» orgánica (rig.build_blade_piece)
    en vez de una cápsula elíptica: se lee como una aleta de verdad -perfil
    curvo y borde afilado- en lugar del «palillo» plano que criticó el
    encargo. «radii_cm» sigue siendo el perfil de 2 puntos (rx, ry) en la
    base y la punta -mismo dato que antes-: el ancho de la hoja en cada
    extremo es 2×max(rx, ry) (la dimensión de «envergadura» siempre fue la
    grande de las dos, la pequeña es la que se aplanaba). «flatten»
    ('vertical'|'horizontal') elige el up_hint para que la envergadura
    quede en pie (aleta de pez/tiburón) o tumbada de lado (aleta de
    mamífero: pectoral o cola de delfín/ballena)."""
    width_base = 2.0 * max(radii_cm[0])
    width_tip = 2.0 * max(radii_cm[-1])
    up_hint = (0.0, 1.0, 0.0) if flatten == 'vertical' else (0.0, 0.0, 1.0)
    curve_cm = max(width_base, width_tip) * 0.12
    return rig.build_blade_piece(name, parent, pivot_cm, role, species, direction, up_hint,
                                  length_cm, width_base, width_tip, curve_cm, color_fn)


def _torpedo_body(species, cfg, seed):
    length = cfg['body_len_cm']
    profile = cfg['body_profile_cm']
    hip_h = cfg.get('hip_height_cm', length * 0.6)
    rnd = C.seeded_rng(seed)
    max_ry = max(r[1] for r in profile)
    body_fn = C.gradient_along_axis(cfg['color'], cfg['color_dark'], 'z',
                                     -max_ry / 100.0, max_ry / 100.0, curve=1.0,
                                     jitter=0.02, rnd=rnd)
    # perfil de radios muestreado desde el pivote (el morro, en el origen)
    # hacia -X: nariz -> cuerpo -> base de cola, igual que el resto del kit
    # deja la cola hacia -X y el morro hacia +X respecto al pivote de cada
    # pieza («X adelante» de rig.py). El pivote de Body ES el morro (una
    # cápsula crece desde su pivote, no está centrada como un blob), así que
    # dorsal/pectoral/aleta caudal se anclan con offsets negativos en X.
    # Vía Skin (rig.skin_chain) en vez de anillos hechos a mano: el mismo
    # perfil de N puntos da ahora un torpedo con transiciones suaves y sin
    # facetas duras.
    body_obj = rig.skin_chain((-1.0, 0.0, 0.0), length, profile,
                               overlap_start=False, overlap_end=False)
    body_obj.name = 'Body'
    eye_off = cfg['eye_offset_cm']
    eye_r = cfg['eye_radius_cm']
    body_obj, eye_centers = rig.attach_eyes(
        body_obj, [(-eye_off[0], eye_off[1], eye_off[2]),
                   (-eye_off[0], -eye_off[1], eye_off[2])], eye_r, seed=seed)
    body_fn = C.with_eye_dots(body_fn, [tuple(c) for c in eye_centers], eye_r / 100.0 * 1.05)
    body_pivot = (0.0, 0.0, hip_h)
    body = rig.finalize_piece('Body', None, body_pivot, 'body', body_obj, species, body_fn)
    return body, body_pivot, body_fn


def _disc_body(species, cfg, seed):
    rx, ry, rz = cfg['disc_radii_cm']
    hip_h = cfg.get('hip_height_cm', rz * 1.5)
    rnd = C.seeded_rng(seed)
    body_fn = C.gradient_along_axis(cfg['color'], cfg['color_dark'], 'z',
                                     -rz / 100.0, rz / 100.0, curve=1.0, jitter=0.02, rnd=rnd)
    body_pivot = (0.0, 0.0, hip_h)
    body_obj = C.make_skin_blob('Body', (0.0, 0.0, 0.0), (rx / 100.0, ry / 100.0, rz / 100.0),
                                 subsurf_levels=2, axis='y')
    eye_off = cfg['eye_offset_cm']
    eye_r = cfg['eye_radius_cm']
    body_obj, eye_centers = rig.attach_eyes(
        body_obj, [eye_off, (eye_off[0], -eye_off[1], eye_off[2])], eye_r, seed=seed)
    body_fn = C.with_eye_dots(body_fn, [tuple(c) for c in eye_centers], eye_r / 100.0 * 1.05)
    body = rig.finalize_piece('Body', None, body_pivot, 'body', body_obj, species, body_fn)
    return body, body_pivot, body_fn


def build(variant):
    cfg = variant
    species = cfg['species']
    seed = cfg['seed']
    pieces = []

    if cfg['body_style'] == 'disc':
        body, body_pivot, body_fn = _disc_body(species, cfg, seed)
        pieces.append(body)
        rx, ry, rz = cfg['disc_radii_cm']

        tc = cfg['tail_chain']
        names = [f'Tail{i + 1}' for i in range(len(tc['seg_len_cm']))]
        dirs = [tuple(C.Vector(d).normalized()) for d in tc['seg_dir']]
        tail_pivot = (-rx * 0.85, 0.0, body_pivot[2])
        chain = rig.build_chain(names, 'Body', tail_pivot, 'tail', species,
                                 directions=dirs, lengths_cm=tc['seg_len_cm'],
                                 radii_profiles_cm=tc['seg_radii_cm'],
                                 color_fn=body_fn, segments=6)
        pieces.extend(chain)
    else:
        body, body_pivot, body_fn = _torpedo_body(species, cfg, seed)
        pieces.append(body)
        length = cfg['body_len_cm']

        dorsal = cfg.get('dorsal')
        if dorsal:
            # pivote en la parte superior del cuerpo (el punto más ancho del perfil,
            # siempre el índice 1: morro/cuerpo/[antes-de-cola]/base-de-cola), a la
            # fracción «t» desde el morro
            top_r = cfg['body_profile_cm'][1][1]
            pivot = (-length * dorsal['t'], 0.0, body_pivot[2] + top_r * 0.85)
            fin = _fin_piece('Dorsal', 'Body', pivot, 'fin', species,
                              tuple(C.Vector(dorsal['dir']).normalized()),
                              dorsal['len_cm'], dorsal['radii_cm'], body_fn)
            pieces.append(fin)

        pectoral = cfg.get('pectoral')
        if pectoral:
            side_r = cfg['body_profile_cm'][1][0]
            for side, sign in (('L', 1.0), ('R', -1.0)):
                pivot = (-length * pectoral['t'], sign * side_r * 0.6, body_pivot[2])
                d = pectoral['dir']
                d_side = (d[0], d[1] * sign, d[2])
                fin = _fin_piece(f'Pectoral{side}', 'Body', pivot, 'fin', species,
                                  tuple(C.Vector(d_side).normalized()),
                                  pectoral['len_cm'], pectoral['radii_cm'], body_fn)
                pieces.append(fin)

        flipper = cfg.get('flipper')
        if flipper:
            side_r = cfg['body_profile_cm'][1][0]
            for side, sign in (('L', 1.0), ('R', -1.0)):
                pivot = (-length * flipper['t'], sign * side_r * 0.6, body_pivot[2])
                d = flipper['dir']
                d_side = (d[0], d[1] * sign, d[2])
                fin = _fin_piece(f'Flipper{side}', 'Body', pivot, 'flipper', species,
                                  tuple(C.Vector(d_side).normalized()),
                                  flipper['len_cm'], flipper['radii_cm'], body_fn)
                pieces.append(fin)

        flipper_chain = cfg.get('flipper_chain')
        if flipper_chain:
            side_r = cfg['body_profile_cm'][1][0]
            du = tuple(C.Vector(flipper_chain['dir_upper']).normalized())
            dl = tuple(C.Vector(flipper_chain['dir_lower']).normalized())
            prof_u, prof_l = flipper_chain['radii_cm']
            for side, sign in (('L', 1.0), ('R', -1.0)):
                pivot = (-length * flipper_chain['t'], sign * side_r * 0.6, body_pivot[2])
                dirs = [(du[0], du[1] * sign, du[2]), (dl[0], dl[1] * sign, dl[2])]
                chain = rig.build_chain(
                    [f'Flipper{side}_Upper', f'Flipper{side}_Lower'], 'Body', pivot, 'flipper',
                    species, directions=dirs,
                    lengths_cm=[flipper_chain['upper_cm'], flipper_chain['lower_cm']],
                    radii_profiles_cm=[prof_u, prof_l], color_fn=body_fn, segments=7)
                pieces.extend(chain)

        tail_fin = cfg['tail_fin']
        tail_pivot = (-length, 0.0, body_pivot[2])
        is_mammal_fluke = tail_fin.get('up_hint') is not None
        fin = _fin_piece('TailFin', 'Body', tail_pivot, 'tail', species,
                          tuple(C.Vector(tail_fin['dir']).normalized()),
                          tail_fin['len_cm'], tail_fin['radii_cm'], body_fn,
                          flatten='horizontal' if is_mammal_fluke else 'vertical')
        pieces.append(fin)

    locomotion = dict(type=cfg.get('locomotion_type', 'swimmer'),
                       stride_length_cm=cfg['stride_length_cm'],
                       hip_height_cm=cfg.get('hip_height_cm', cfg.get('body_len_cm', 0.0) * 0.6),
                       total_length_cm=cfg['total_length_cm'])
    return dict(species=species, pieces=pieces, locomotion=locomotion)
