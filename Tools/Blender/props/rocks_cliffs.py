"""
rocks_cliffs.py — kit de rocas y acantilados de «Explored»: paredes de
acantilado, espolones, farallones, un arco marino, bloques caidos, cantos
y losas de caliza. Se coloca sobre el terreno volumetrico en las laderas
empinadas para que los cortados dejen de parecer terreno liso (encargo de
arte 2026-09-27; AcantiladoFormaciones rehecho el mismo dia tras rechazo
en revision — ver _cliffscan.py).

Direccion de arte: cartoon tipo Sea of Thieves, referencia caliza gris
karstica (El Nido / Ha Long) para las formaciones grandes — roca facetada
en planos grandes con bordes biselados que atrapan la luz, siluetas
quebradas (nunca paredes planas ni simetricas) y color de vertice que
oscurece grietas/huecos (AO real horneado) y aclara las aristas. El color
de superficie final lo pone el material triplanar M_Stone en Unreal; el
color de vertice de aqui es la capa de sombreado/AO sobre ese material.

Dos pipelines conviven en este fichero:
- AcantiladoBloques (bloques, cantos, losas): geometria 100% procedural en
  _cliffkit.py, aprobada tal cual en la revision de arte.
- AcantiladoFormaciones (paredes, espolones, farallones, arco): escaneos
  CC0 de Poly Haven estilizados en _cliffscan.py (voxel remesh + decimate
  planar + bisel) — la version procedural por bmesh se rechazo por leerse
  como geometria generada, no roca natural.

Vive en Tools/Blender/props/ (no toca common.py, _shapes.py ni ningun
fichero de vegetacion).
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import _cliffkit as K  # noqa: E402
import _cliffscan as SC  # noqa: E402
import _materials as M  # noqa: E402
import common as C  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

# ---------------------------------------------------------------------------
# Paletas (tinte de vertice; el albedo real es el M_Stone triplanar de Unreal)
# ---------------------------------------------------------------------------
_SANDSTONE = (0.50, 0.42, 0.32)   # arenisca/toba calida — SOLO AcantiladoBloques (aprobado, no tocar)
_BASALT = (0.20, 0.19, 0.19)      # basalto gris-negro volcanico (variedad oscura, ambos grupos)
_LIMESTONE = (0.80, 0.75, 0.65)   # caliza clara y calida — SOLO AcantiladoBloques (aprobado, no tocar)
_KARST = (0.58, 0.57, 0.52)       # caliza gris karstica (El Nido / Ha Long) — SOLO AcantiladoFormaciones
_WET_DARK = (0.09, 0.09, 0.11)

GROUP_FORMACIONES = 'AcantiladoFormaciones'
GROUP_BLOQUES = 'AcantiladoBloques'

VARIANTS = [
    # -- Paredes de acantilado (8-20 m alto, 10-25 m ancho) -----------------
    # A partir de escaneos CC0 de Poly Haven (ver _cliffscan.py y
    # docs/art/rocas/README.md para la atribucion): voxel remesh + decimate
    # planar + bisel, nunca la geometria fotogrametrica en crudo.
    dict(name='CliffWall_Basalt01', seed=8001, kind='wall', group=GROUP_FORMACIONES,
         width=18.0, height=16.0, depth=7.0,
         sources=[
             dict(id='namaqualand_cliff_01', width_frac=0.60, x_frac=-0.22),
             dict(id='namaqualand_cliff_01', width_frac=0.60, x_frac=0.24, mirror=True),
         ],
         palette=_BASALT, tri_budget=(2000, 14000)),
    dict(name='CliffWall_Basalt02', seed=8002, kind='wall', group=GROUP_FORMACIONES,
         width=22.0, height=12.0, depth=6.0,
         sources=[dict(id='namaqualand_cliff_02', width_frac=1.0, x_frac=0.0)],
         palette=_BASALT, tri_budget=(2000, 14000)),
    dict(name='CliffWall_Sandstone01', seed=8003, kind='wall', group=GROUP_FORMACIONES,
         width=14.0, height=20.0, depth=8.0,
         sources=[
             dict(id='namaqualand_cliff_01', width_frac=0.68, x_frac=-0.16),
             dict(id='namaqualand_boulder_02', width_frac=0.55, x_frac=0.30, mirror=True),
         ],
         palette=_KARST, tri_budget=(2000, 14000)),
    dict(name='CliffWall_Sandstone02', seed=8004, kind='wall', group=GROUP_FORMACIONES,
         width=25.0, height=9.0, depth=5.0,
         sources=[dict(id='coastal_cliff_04', width_frac=1.0, x_frac=0.0)],
         palette=_KARST, tri_budget=(2000, 14000)),

    # -- Espolones (salientes que rompen la linea recta del cortado) --------
    dict(name='CliffSpur01', seed=8101, kind='wall', group=GROUP_FORMACIONES,
         width=9.0, height=12.0, depth=5.0,
         sources=[dict(id='namaqualand_cliff_01', width_frac=1.0, x_frac=0.0)],
         palette=_KARST, tri_budget=(1000, 15000)),
    dict(name='CliffSpur02', seed=8102, kind='wall', group=GROUP_FORMACIONES,
         width=7.0, height=9.0, depth=4.0,
         sources=[dict(id='namaqualand_boulder_02', width_frac=1.0, x_frac=0.0)],
         palette=_BASALT, tri_budget=(1000, 15000)),
    dict(name='CliffSpur03', seed=8103, kind='wall', group=GROUP_FORMACIONES,
         width=8.0, height=14.0, depth=4.5,
         sources=[dict(id='namaqualand_cliff_02', width_frac=1.0, x_frac=0.0)],
         palette=_KARST, tri_budget=(1000, 15000)),

    # -- Farallones (agujas marinas, 10-25 m): un escaneo estirado en vertical
    dict(name='SeaStack01', seed=8201, kind='stack', group=GROUP_FORMACIONES,
         source='namaqualand_boulder_02', width=6.5, depth=6.0, height=18.0,
         palette=_KARST, tri_budget=(400, 9000)),
    dict(name='SeaStack02', seed=8202, kind='stack', group=GROUP_FORMACIONES,
         source='moon_rock_01', lod='LOD1', width=8.0, depth=7.5, height=24.0,
         palette=_BASALT, tri_budget=(400, 9000)),

    # -- Arco marino (~20 m): booleano de un tunel a traves de un bloque
    # de escaneo grande, no un tubo aparte.
    dict(name='SeaArch01', seed=8301, kind='arch', group=GROUP_FORMACIONES,
         source='coastal_cliff_04', width=20.0, height=20.0, depth=7.0,
         span=12.0, leg_inset=0.0, pier_z_frac=0.3, rise=7.0,
         palette=_KARST, tri_budget=(1200, 16000)),

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
# Paredes y espolones: escaneos CC0 estilizados (_cliffscan.fuse_and_stylize),
# no geometria procedural. Rechazado en revision de arte 2026-09-27: la
# version por bmesh se leia como "extrusion en diente de sierra".
# ---------------------------------------------------------------------------
def _place_source_piece(src):
    obj = SC.import_scan(src['id'], lod_substring=src.get('lod'))
    SC.center_and_ground(obj)
    SC.predecimate_if_heavy(obj)
    if src.get('mirror'):
        obj.data.transform(Matrix.Scale(-1.0, 4, Vector((1.0, 0.0, 0.0))))
        obj.data.flip_normals()
        obj.data.update()
    return obj


@_register('wall')
def _build_wall(v, rnd):
    W, H, D = v['width'], v['height'], v['depth']
    sources = v['sources']
    pieces = []
    for src in sources:
        obj = _place_source_piece(src)
        # SOLO un escalado UNIFORME suave aqui (mantiene la proporcion
        # natural del escaneo). Un escaneo es una lamina fotogrametrica
        # ABIERTA (sin trasera, ver _cliffscan.py); deformarla de forma no
        # uniforme y fuerte ANTES del voxel remesh la pliega sobre si misma
        # y el remesh sale hecho fragmentos sueltos (roto en la revision
        # visual). El ajuste de tamano final, anisotropo, se aplica DESPUES
        # de fundir/limpiar, sobre un solido ya cerrado — ahi si es seguro.
        rough_w = W * src['width_frac'] * (1.22 if len(sources) > 1 else 1.0)
        SC.fit_dimensions(obj, target_x=rough_w, uniform=True)
        cx = W * src['x_frac']
        obj.data.transform(Matrix.Translation((cx, rnd.uniform(-D * 0.1, D * 0.1), 0.0)))
        obj.data.update()
        pieces.append(obj)

    # varias piezas solapadas generan mas superficie a resolver que una
    # sola: divisor de voxel mas bajo (voxel mas grueso) para que el
    # recuento de tris no se dispare al combinarlas.
    divisions = 30.0 if len(pieces) > 1 else 40.0
    voxel_size = SC.auto_voxel_size(pieces, divisions=divisions)
    merged = SC.fuse_and_stylize(pieces, voxel_size=voxel_size, planar_angle_deg=17.0,
                                  smooth_factor=0.3, smooth_iterations=1, name='Wall')
    SC.center_and_ground(merged)
    SC.fit_dimensions(merged, target_x=W, target_y=D, target_z=H)
    SC.cleanup_mesh(merged)

    ao = SC.bake_cavity_ao(merged)
    M.assign(merged, ['M_Stone'])
    C.set_vertex_colors(merged, SC.scan_rock_tint(
        v['palette'], ao, v['seed'], rnd, height=H, wet_height=H * 0.1, dark_rgb=_WET_DARK))

    bevel_w = max(0.10, min(0.4, D * 0.045))
    return K.finish_rock(merged, 'SM_' + v['name'], bevel_width=bevel_w,
                          bevel_segments=2, bevel_angle_deg=36.0, smooth_angle_deg=22.0)


# ---------------------------------------------------------------------------
# Farallones: un escaneo estirado en vertical (anisotropo, a proposito) y
# refundido con voxel remesh para que el estiramiento no se lea como una
# roca "chiclosa" sino como facetas grandes reales.
# ---------------------------------------------------------------------------
@_register('stack')
def _build_stack(v, rnd):
    obj = SC.import_scan(v['source'], lod_substring=v.get('lod'))
    SC.center_and_ground(obj)
    SC.predecimate_if_heavy(obj)

    voxel_size = SC.auto_voxel_size([obj], divisions=32.0)
    merged = SC.fuse_and_stylize([obj], voxel_size=voxel_size, planar_angle_deg=16.0,
                                  smooth_factor=0.3, smooth_iterations=1, name='Stack')
    SC.center_and_ground(merged)
    # el estiramiento vertical (anisotropo, deliberado: "exagera... estira
    # en vertical los farallones") se aplica AQUI, sobre el solido ya
    # fundido y limpio — nunca antes del remesh (ver nota en _build_wall).
    SC.fit_dimensions(merged, target_x=v['width'], target_y=v['depth'], target_z=v['height'])

    # notch de marea en la base (referencia caliza karstica El Nido/Ha Long):
    # la disolucion por oleaje socava el pie del farallon.
    SC.carve_tide_notch(merged, z0=v['height'] * 0.02, z1=v['height'] * 0.10, pinch=0.4)
    SC.cleanup_mesh(merged)

    ao = SC.bake_cavity_ao(merged)
    M.assign(merged, ['M_Stone'])
    C.set_vertex_colors(merged, SC.scan_rock_tint(
        v['palette'], ao, v['seed'], rnd, height=v['height'], wet_height=v['height'] * 0.12,
        dark_rgb=_WET_DARK))

    bevel_w = max(0.10, min(0.35, v['width'] * 0.045))
    return K.finish_rock(merged, 'SM_' + v['name'], bevel_width=bevel_w,
                          bevel_segments=2, bevel_angle_deg=36.0, smooth_angle_deg=22.0)


# ---------------------------------------------------------------------------
# Arco marino: booleano de un tunel curvado a traves de un bloque de
# escaneo real ya estilizado, de una sola pieza (no un tubo aparte).
# ---------------------------------------------------------------------------
@_register('arch')
def _build_arch(v, rnd):
    W, H, D = v['width'], v['height'], v['depth']

    host = SC.import_scan(v['source'])
    SC.center_and_ground(host)
    SC.predecimate_if_heavy(host)

    voxel_size = SC.auto_voxel_size([host])
    merged = SC.fuse_and_stylize([host], voxel_size=voxel_size, planar_angle_deg=13.0,
                                  smooth_factor=0.3, smooth_iterations=1, name='ArchHost')
    SC.center_and_ground(merged)
    SC.fit_dimensions(merged, target_x=W, target_y=D, target_z=H)

    pier_z = H * v.get('pier_z_frac', 0.32)
    SC.boolean_arch_negative_cut(
        merged, span=v['span'], pier_z=pier_z, rise=v['rise'],
        leg_inset=v['leg_inset'], extrude_depth=D * 1.6,
        wobble=D * 0.06, seed=v['seed'],
    )
    SC.cleanup_mesh(merged)

    ao = SC.bake_cavity_ao(merged)
    M.assign(merged, ['M_Stone'])
    C.set_vertex_colors(merged, SC.scan_rock_tint(
        v['palette'], ao, v['seed'], rnd, height=H, wet_height=H * 0.15, dark_rgb=_WET_DARK))

    bevel_w = max(0.10, min(0.35, D * 0.05))
    return K.finish_rock(merged, 'SM_' + v['name'], bevel_width=bevel_w,
                          bevel_segments=2, bevel_angle_deg=36.0, smooth_angle_deg=22.0)


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
