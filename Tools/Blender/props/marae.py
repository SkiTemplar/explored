"""
marae.py — plataforma ceremonial de piedra (marae/ahu polinesio) del antiguo
pueblo de navegantes.

3 props: plataforma escalonada, piedra vertical y altar. Punto de interés
secreto ligado al hilo narrativo de los Navegantes.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import _materials as M  # noqa: E402
import common as C  # noqa: E402

VARIANTS = [
    dict(name='Marae_Platform', seed=1601, builder='platform',
         tri_budget=(20, 200), needs_collision=True, collision_complex=True),
    dict(name='Marae_StandingStone', seed=1602, builder='standing_stone',
         tri_budget=(50, 300), needs_collision=True),
    dict(name='Marae_Altar', seed=1603, builder='altar',
         tri_budget=(50, 300), needs_collision=True),
]

_BUILDERS = {}


def _register(key):
    def deco(fn):
        _BUILDERS[key] = fn
        return fn
    return deco


def build(variant):
    rnd = C.seeded_rng(variant['seed'])
    return _BUILDERS[variant['builder']](variant, rnd)


def _finish(parts, name):
    obj = C.join_objects(parts, name) if len(parts) > 1 else parts[0]
    C.shade_smooth_auto(obj, angle_deg=35.0)
    C.add_basic_uv(obj)
    return obj


# ---------------------------------------------------------------------------
# 1. Plataforma escalonada (ahu/marae, 3 niveles tipo terraza)
# ---------------------------------------------------------------------------
@_register('platform')
def _build_platform(variant, rnd):
    base_stone = (0.36, 0.35, 0.32)

    level1 = C.make_box('Level1', (6.0, 4.0, 0.5), center=(0.0, 0.0, 0.25))
    M.assign(level1, ['M_Stone'])
    C.set_vertex_colors(level1, C.constant_tint(base_stone, alpha=0.0, jitter=0.04, rnd=rnd))

    level2 = C.make_box('Level2', (4.4, 3.0, 0.4), center=(0.0, 0.0, 0.7))
    M.assign(level2, ['M_Stone'])
    C.set_vertex_colors(level2, C.constant_tint(base_stone, alpha=0.0, jitter=0.04, rnd=rnd))

    level3 = C.make_box('Level3', (3.0, 2.0, 0.3), center=(0.0, 0.0, 1.05))
    M.assign(level3, ['M_Stone'])
    C.set_vertex_colors(level3, C.constant_tint(base_stone, alpha=0.0, jitter=0.04, rnd=rnd))

    return _finish([level1, level2, level3], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 2. Piedra vertical (losa tosca sin grabar, silueta irregular)
# ---------------------------------------------------------------------------
@_register('standing_stone')
def _build_standing_stone(variant, rnd):
    import bmesh

    slab = C.make_box('Slab', (0.5, 0.25, 1.5), center=(0.0, 0.0, 0.75))

    bm = bmesh.new()
    bm.from_mesh(slab.data)
    bmesh.ops.subdivide_edges(bm, edges=bm.edges, cuts=2, use_grid_fill=True)
    for v in bm.verts:
        if v.co.z < 1e-6:
            continue  # deja la base intacta: el pivote debe quedar en z=0
        t = v.co.z / 1.5
        jitter = 0.025 + 0.03 * t
        v.co.x += rnd.uniform(-jitter, jitter)
        v.co.y += rnd.uniform(-jitter, jitter)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(slab.data)
    bm.free()
    slab.data.update()

    M.assign(slab, ['M_Stone'])
    C.set_vertex_colors(slab, C.constant_tint((0.33, 0.32, 0.30), alpha=0.0, jitter=0.07, rnd=rnd))

    return _finish([slab], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 3. Altar (bloque bajo con superficie superior algo cóncava)
# ---------------------------------------------------------------------------
@_register('altar')
def _build_altar(variant, rnd):
    import bmesh

    slab = C.make_box('Slab', (1.2, 0.8, 0.5), center=(0.0, 0.0, 0.25))

    bm = bmesh.new()
    bm.from_mesh(slab.data)
    bmesh.ops.subdivide_edges(bm, edges=bm.edges, cuts=2, use_grid_fill=True)
    bm.to_mesh(slab.data)
    bm.free()
    slab.data.update()

    # los 4 vertices superiores mas cercanos al centro en X/Y: se hunden un
    # poco para sugerir una superficie tipo cuenco, sin tocar el resto del
    # borde (que se queda plano, como una mesa ceremonial).
    top_z = max(v.co.z for v in slab.data.vertices)
    top_verts = [v for v in slab.data.vertices if abs(v.co.z - top_z) < 1e-5]
    top_verts.sort(key=lambda v: v.co.x ** 2 + v.co.y ** 2)
    for v in top_verts[:4]:
        v.co.z -= 0.05
    slab.data.update()

    M.assign(slab, ['M_Stone'])
    C.set_vertex_colors(slab, C.constant_tint((0.37, 0.36, 0.33), alpha=0.0, jitter=0.04, rnd=rnd))

    return _finish([slab], 'SM_' + variant['name'])
