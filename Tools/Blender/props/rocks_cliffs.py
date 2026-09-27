"""
rocks_cliffs.py — kit de rocas y acantilados de «Explored»: paredes de
acantilado con estratos y grietas, espolones, farallones, un arco marino,
bloques caidos, cantos y losas de caliza. Se coloca sobre el terreno
volumetrico en las laderas empinadas para que los cortados dejen de
parecer terreno liso (encargo de arte 2026-09-27).

Direccion de arte: cartoon tipo Sea of Thieves — roca facetada en planos
grandes con bordes biselados que atrapan la luz, estratos marcados,
siluetas quebradas (nunca paredes planas ni simetricas) y color de vertice
que oscurece grietas/huecos (AO pintado) y aclara las aristas. El color
real de superficie lo pone el material triplanar M_Stone en Unreal; el
color de vertice de aqui es la capa de sombreado/AO sobre ese material,
igual que en el resto del kit de props.

Vive en Tools/Blender/props/ (no toca common.py, _shapes.py ni ningun
fichero de vegetacion). Utilidades propias en _cliffkit.py.
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import _materials as M  # noqa: E402
import _cliffkit as K  # noqa: E402

from mathutils import Matrix  # noqa: E402

# ---------------------------------------------------------------------------
# Paletas (tinte de vertice; el albedo real es el M_Stone triplanar de Unreal)
# ---------------------------------------------------------------------------
_SANDSTONE = (0.50, 0.42, 0.32)   # arenisca/toba calida, tono base del archipielago
_BASALT = (0.20, 0.19, 0.19)      # basalto gris-negro volcanico
_LIMESTONE = (0.80, 0.75, 0.65)   # caliza clara y calida
_WET_DARK = (0.09, 0.09, 0.11)
_CRACK_DARK = (0.05, 0.04, 0.04)

GROUP_FORMACIONES = 'AcantiladoFormaciones'
GROUP_BLOQUES = 'AcantiladoBloques'

VARIANTS = [
    # -- Paredes de acantilado (8-20 m alto, 10-25 m ancho) -----------------
    dict(name='CliffWall_Basalt01', seed=8001, kind='wall', group=GROUP_FORMACIONES,
         width=18.0, height=16.0, depth=7.0, n_bands=7, crack_count=3,
         palette=_BASALT, n_ledges=2, tri_budget=(2500, 9000)),
    dict(name='CliffWall_Basalt02', seed=8002, kind='wall', group=GROUP_FORMACIONES,
         width=22.0, height=12.0, depth=6.0, n_bands=6, crack_count=4,
         palette=_BASALT, n_ledges=1, tri_budget=(2500, 9000)),
    dict(name='CliffWall_Sandstone01', seed=8003, kind='wall', group=GROUP_FORMACIONES,
         width=14.0, height=20.0, depth=8.0, n_bands=9, crack_count=2,
         palette=_SANDSTONE, n_ledges=3, concave_back=0.15, tri_budget=(2500, 9500)),
    dict(name='CliffWall_Sandstone02', seed=8004, kind='wall', group=GROUP_FORMACIONES,
         width=25.0, height=9.0, depth=5.0, n_bands=5, crack_count=5,
         palette=_SANDSTONE, n_ledges=1, tri_budget=(2500, 9500)),

    # -- Espolones (salientes que rompen la linea recta del cortado) --------
    dict(name='CliffSpur01', seed=8101, kind='wall', group=GROUP_FORMACIONES,
         width=9.0, height=12.0, depth=5.0, n_bands=5, crack_count=2,
         palette=_SANDSTONE, n_ledges=1, taper=0.55, lean=1.6, tri_budget=(1200, 5500)),
    dict(name='CliffSpur02', seed=8102, kind='wall', group=GROUP_FORMACIONES,
         width=7.0, height=9.0, depth=4.0, n_bands=4, crack_count=2,
         palette=_BASALT, n_ledges=0, taper=0.65, lean=-1.1, tri_budget=(1200, 5500)),
    dict(name='CliffSpur03', seed=8103, kind='wall', group=GROUP_FORMACIONES,
         width=8.0, height=14.0, depth=4.5, n_bands=6, crack_count=3,
         palette=_SANDSTONE, n_ledges=1, taper=0.5, lean=0.9, tri_budget=(1200, 5500)),

    # -- Farallones (agujas marinas, 10-25 m) --------------------------------
    dict(name='SeaStack01', seed=8201, kind='stack', group=GROUP_FORMACIONES,
         height=18.0, base_radius=4.5, tip_radius=1.8, sides=11, segments=11,
         waist=0.18, twist=0.3, palette=_SANDSTONE, tri_budget=(500, 2500)),
    dict(name='SeaStack02', seed=8202, kind='stack', group=GROUP_FORMACIONES,
         height=24.0, base_radius=5.5, tip_radius=1.4, sides=12, segments=12,
         waist=0.24, twist=-0.4, palette=_BASALT, tri_budget=(500, 2800)),

    # -- Arco marino (~20 m) --------------------------------------------------
    dict(name='SeaArch01', seed=8301, kind='arch', group=GROUP_FORMACIONES,
         pier_height=13.0, span=14.0, rise=7.0, base_radius=2.6, tip_radius=2.1,
         palette=_SANDSTONE, tri_budget=(1800, 6000)),

    # -- Losas planas de caliza ------------------------------------------------
    dict(name='LimestoneSlab01', seed=8401, kind='slab', group=GROUP_BLOQUES,
         radius=1.8, thickness=0.35, palette=_LIMESTONE, tri_budget=(80, 600)),
    dict(name='LimestoneSlab02', seed=8402, kind='slab', group=GROUP_BLOQUES,
         radius=1.4, thickness=0.28, palette=_LIMESTONE, tri_budget=(80, 600)),
    dict(name='LimestoneSlab03', seed=8403, kind='slab', group=GROUP_BLOQUES,
         radius=2.2, thickness=0.40, palette=_LIMESTONE, tri_budget=(80, 600)),

    # -- Bloques caidos grandes (2-5 m) -----------------------------------------
    dict(name='RockBoulder01', seed=8501, kind='boulder', group=GROUP_BLOQUES,
         size=2.6, palette=_SANDSTONE, tri_budget=(120, 500)),
    dict(name='RockBoulder02', seed=8502, kind='boulder', group=GROUP_BLOQUES,
         size=3.6, palette=_BASALT, tri_budget=(120, 500)),
    dict(name='RockBoulder03', seed=8503, kind='boulder', group=GROUP_BLOQUES,
         size=4.4, palette=_SANDSTONE, tri_budget=(120, 500)),
    dict(name='RockBoulder04', seed=8504, kind='boulder', group=GROUP_BLOQUES,
         size=5.0, palette=_LIMESTONE, tri_budget=(120, 500)),

    # -- Cantos medianos (0.5-2 m) -----------------------------------------------
    dict(name='RockCobble01', seed=8601, kind='boulder', group=GROUP_BLOQUES,
         size=0.55, palette=_SANDSTONE, tri_budget=(120, 500)),
    dict(name='RockCobble02', seed=8602, kind='boulder', group=GROUP_BLOQUES,
         size=0.85, palette=_BASALT, tri_budget=(120, 500)),
    dict(name='RockCobble03', seed=8603, kind='boulder', group=GROUP_BLOQUES,
         size=1.1, palette=_SANDSTONE, tri_budget=(120, 500)),
    dict(name='RockCobble04', seed=8604, kind='boulder', group=GROUP_BLOQUES,
         size=1.4, palette=_LIMESTONE, tri_budget=(120, 500)),
    dict(name='RockCobble05', seed=8605, kind='boulder', group=GROUP_BLOQUES,
         size=1.7, palette=_SANDSTONE, tri_budget=(120, 500)),
    dict(name='RockCobble06', seed=8606, kind='boulder', group=GROUP_BLOQUES,
         size=2.0, palette=_BASALT, tri_budget=(120, 500)),
]

_BUILDERS = {}


def _register(key):
    def deco(fn):
        _BUILDERS[key] = fn
        return fn
    return deco


def build(variant):
    rnd = C.seeded_rng(variant['seed'])
    return _BUILDERS[variant['kind']](variant, rnd)


# ---------------------------------------------------------------------------
# Paredes y espolones (build_strata_wall)
# ---------------------------------------------------------------------------
@_register('wall')
def _build_wall(v, rnd):
    obj, crack_xs = K.build_strata_wall(
        'Wall', seed=v['seed'], width=v['width'], height=v['height'], depth=v['depth'],
        n_bands=v['n_bands'], crack_count=v['crack_count'],
        concave_back=v.get('concave_back', 0.0),
        taper=v.get('taper', 0.0), lean=v.get('lean', 0.0),
    )

    parts = [obj]
    n_ledges = v.get('n_ledges', 0)
    for i in range(n_ledges):
        band_t = rnd.uniform(0.25, 0.8)
        z = v['height'] * band_t
        ledge_w = v['width'] * rnd.uniform(0.14, 0.24)
        x = rnd.uniform(-v['width'] * 0.5 + ledge_w, v['width'] * 0.5 - ledge_w)
        ledge = K.build_ledge(f'Ledge{i}', v['seed'] + 100 + i, width=ledge_w,
                               out_depth=v['depth'] * rnd.uniform(0.18, 0.32),
                               thickness=v['height'] * 0.03, front_y=-v['depth'] * 0.5)
        ledge.data.transform(Matrix.Translation((x, 0.0, z)))
        ledge.data.update()
        parts.append(ledge)

    merged = C.join_objects(parts, 'Wall') if len(parts) > 1 else parts[0]
    C.merge_by_distance(merged, dist=0.01)

    M.assign(merged, ['M_Stone'])
    C.set_vertex_colors(merged, K.rock_tint(
        v['palette'], v['seed'], rnd, height=v['height'], band_count=v['n_bands'],
        crack_positions=crack_xs, crack_width=0.4, wet_height=v['height'] * 0.08,
        dark_rgb=_CRACK_DARK,
    ))

    bevel_w = max(0.08, min(0.4, v['depth'] * 0.035))
    return K.finish_rock(merged, 'SM_' + v['name'], bevel_width=bevel_w,
                          bevel_segments=2, bevel_angle_deg=38.0, smooth_angle_deg=24.0)


# ---------------------------------------------------------------------------
# Farallones (columnas irregulares)
# ---------------------------------------------------------------------------
@_register('stack')
def _build_stack(v, rnd):
    obj = K.build_irregular_column(
        'Stack', seed=v['seed'], height=v['height'], base_radius=v['base_radius'],
        tip_radius=v['tip_radius'], sides=v['sides'], segments=v['segments'],
        radius_jitter=0.24, waist=v.get('waist', 0.0), twist=v.get('twist', 0.0),
        taper_curve=1.3, peak=True,
    )
    C.displace_mesh_noise(obj, v['seed'], strength=v['base_radius'] * 0.10,
                           scale=1.6 / max(v['base_radius'], 0.5), octaves=2)

    M.assign(obj, ['M_Stone'])
    C.set_vertex_colors(obj, K.rock_tint(
        v['palette'], v['seed'], rnd, height=v['height'], band_count=8,
        wet_height=v['height'] * 0.1, dark_rgb=_WET_DARK,
    ))

    bevel_w = max(0.08, min(0.35, v['base_radius'] * 0.07))
    return K.finish_rock(obj, 'SM_' + v['name'], bevel_width=bevel_w,
                          bevel_segments=2, bevel_angle_deg=38.0, smooth_angle_deg=25.0)


# ---------------------------------------------------------------------------
# Arco marino: dos pilares + puente curvado
# ---------------------------------------------------------------------------
@_register('arch')
def _build_arch(v, rnd):
    span = v['span']
    pier_h = v['pier_height']
    base_r = v['base_radius']
    tip_r = v['tip_radius']

    left = K.build_irregular_column(
        'PierL', seed=v['seed'] + 1, height=pier_h, base_radius=base_r * 1.15,
        tip_radius=tip_r, sides=9, segments=7, radius_jitter=0.2,
        waist=0.12, twist=0.15, taper_curve=1.1, peak=False,
    )
    left.data.transform(Matrix.Translation((-span * 0.5, 0.0, 0.0)))
    left.data.update()

    right = K.build_irregular_column(
        'PierR', seed=v['seed'] + 2, height=pier_h * rnd.uniform(0.9, 1.05),
        base_radius=base_r, tip_radius=tip_r * rnd.uniform(0.9, 1.1), sides=9,
        segments=7, radius_jitter=0.2, waist=0.14, twist=-0.2, taper_curve=1.1,
        peak=False,
    )
    right.data.transform(Matrix.Translation((span * 0.5, 0.0, 0.0)))
    right.data.update()

    bridge = K.build_arch_bridge(
        'Bridge', seed=v['seed'] + 3,
        p0=(-span * 0.5, 0.0, pier_h), p1=(span * 0.5, 0.0, pier_h * rnd.uniform(0.9, 1.05)),
        rise=v['rise'], base_radius=tip_r, tip_radius=tip_r * 0.85, wobble=0.25,
    )

    merged = C.join_objects([left, right, bridge], 'Arch')
    C.merge_by_distance(merged, dist=0.02)
    C.displace_mesh_noise(merged, v['seed'], strength=base_r * 0.08,
                           scale=1.3 / max(base_r, 0.5), octaves=2)

    M.assign(merged, ['M_Stone'])
    C.set_vertex_colors(merged, K.rock_tint(
        v['palette'], v['seed'], rnd, height=pier_h + v['rise'], band_count=6,
        wet_height=(pier_h + v['rise']) * 0.12, dark_rgb=_WET_DARK,
    ))

    bevel_w = max(0.08, min(0.3, base_r * 0.08))
    return K.finish_rock(merged, 'SM_' + v['name'], bevel_width=bevel_w,
                          bevel_segments=2, bevel_angle_deg=38.0, smooth_angle_deg=25.0)


# ---------------------------------------------------------------------------
# Losas de caliza
# ---------------------------------------------------------------------------
@_register('slab')
def _build_slab(v, rnd):
    obj = K.build_slab('Slab', seed=v['seed'], radius=v['radius'], thickness=v['thickness'])
    C.displace_mesh_noise(obj, v['seed'], strength=v['thickness'] * 0.25,
                           scale=2.0 / max(v['radius'], 0.3), octaves=2)

    M.assign(obj, ['M_Stone'])
    C.set_vertex_colors(obj, K.rock_tint(
        v['palette'], v['seed'], rnd, rim_light=0.10, rim_shadow=0.22, jitter=0.025,
    ))

    return K.finish_rock(obj, 'SM_' + v['name'], bevel_width=v['thickness'] * 0.35,
                          bevel_segments=2, bevel_angle_deg=40.0, smooth_angle_deg=24.0)


# ---------------------------------------------------------------------------
# Bloques caidos y cantos (mismo builder, distinto rango de tamano)
# ---------------------------------------------------------------------------
@_register('boulder')
def _build_boulder(v, rnd):
    obj = K.build_boulder('Boulder', seed=v['seed'], size=v['size'])

    M.assign(obj, ['M_Stone'])
    C.set_vertex_colors(obj, K.rock_tint(
        v['palette'], v['seed'], rnd, rim_light=0.12, rim_shadow=0.20, jitter=0.03,
    ))

    # build_boulder ya aplica su propio bisel (proporcional a `size`, antes
    # de pintar el color de vertice); aqui solo falta el sombreado faceteado
    # cartoon (angulo bajo: la mayoria de facetas quedan planas, solo se
    # suavizan las casi coplanares) y el UV.
    return K.finish_rock(obj, 'SM_' + v['name'], bevel_width=0.0, apply_bevel=False,
                          smooth_angle_deg=12.0)
