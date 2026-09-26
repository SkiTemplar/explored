"""
Tools/Blender/animals/jellyfish.py — medusa (Aguas abiertas, biblia §6).

Campana (blob aplanado, forma de cúpula) con una corona de tentáculos finos
de 2 segmentos colgando por debajo. Sin patas ni aletas: la medusa deriva
con la corriente y su pulsación la resuelve el material (vertex shader),
igual que la ondulación del resto de nadadores —de ahí locomotion=swimmer,
el mismo tipo que peces y tiburones—.
"""

import math
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import rig  # noqa: E402

CATEGORY = 'jellyfish'

VARIANTS = [
    dict(
        species='Jellyfish', seed=4601,
        bell_radii_cm=(11.0, 11.0, 7.0), bell_height_cm=14.0,
        color=(0.72, 0.56, 0.86), color_dark=(0.42, 0.30, 0.54),
        n_tentacles=8,
        tentacle_seg_len_cm=[14.0, 12.0],
        tentacle_seg_radii_cm=[[0.55, 0.35], [0.35, 0.12]],
        stride_length_cm=0.0, total_length_cm=40.0,
        habitat='Aguas abiertas', behavior='Deriva',
        use='Peligro', diet='planctívoro', locomotion_type='swimmer',
    ),
]


def _tentacles(species, cfg, color_fn):
    rx, ry, rz = cfg['bell_radii_cm']
    base_h = cfg['bell_height_cm']
    n = cfg['n_tentacles']
    seg_lens = cfg['tentacle_seg_len_cm']
    seg_radii = cfg['tentacle_seg_radii_cm']
    pieces = []
    for i in range(n):
        ang = 2.0 * math.pi * i / n
        cos_a, sin_a = math.cos(ang), math.sin(ang)
        hip = (rx * 0.55 * cos_a, ry * 0.55 * sin_a, base_h - rz * 0.85)
        d = tuple(C.Vector((cos_a * 0.12, sin_a * 0.12, -1.0)).normalized())
        chain = rig.build_chain(
            [f'Tentacle{i + 1}_Seg1', f'Tentacle{i + 1}_Seg2'], 'Bell', hip, 'tentacle',
            species, directions=[d, d], lengths_cm=seg_lens, radii_profiles_cm=seg_radii,
            color_fn=color_fn, segments=5,
        )
        pieces.extend(chain)
    return pieces


def build(variant):
    cfg = variant
    species = cfg['species']
    seed = cfg['seed']
    rnd = C.seeded_rng(seed)
    pieces = []

    bell_pivot = (0.0, 0.0, cfg['bell_height_cm'])
    bell_fn = C.constant_tint(cfg['color'], alpha=0.0, jitter=0.03, rnd=rnd)
    bell = rig.build_blob_piece('Bell', None, bell_pivot, 'bell', species,
                                 center_offset_cm=(0.0, 0.0, 0.0),
                                 radii_cm=cfg['bell_radii_cm'], color_fn=bell_fn,
                                 seed=seed, subdivisions=2, noise_strength=0.05, relax=2)
    pieces.append(bell)

    tentacle_fn = C.constant_tint(cfg['color_dark'], alpha=0.0, jitter=0.02, rnd=rnd)
    pieces.extend(_tentacles(species, cfg, tentacle_fn))

    locomotion = dict(type=cfg.get('locomotion_type', 'swimmer'),
                       stride_length_cm=cfg['stride_length_cm'],
                       hip_height_cm=cfg['bell_height_cm'], total_length_cm=cfg['total_length_cm'])
    return dict(species=species, pieces=pieces, locomotion=locomotion)
