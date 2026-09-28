"""
lighthouse.py — Faro abandonado en la isla Los Dientes: torre de piedra en
ruinas, sala de la lampara rota, fragmento suelto de escalera de caracol y
monticulo de escombros en la base.
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import _materials as M  # noqa: E402
import _shapes as S  # noqa: E402
import common as C  # noqa: E402

VARIANTS = [
    dict(name='Lighthouse_Tower_Ruin', seed=1101, builder='tower',
         tri_budget=(200, 1000), needs_collision=True, collision_complex=True),
    dict(name='Lighthouse_LanternRoom_Broken', seed=1102, builder='lantern_room',
         tri_budget=(150, 1800), needs_collision=True),
    dict(name='Lighthouse_SpiralStair_Fragment', seed=1103, builder='stair_fragment',
         tri_budget=(40, 900), needs_collision=True),
    dict(name='Lighthouse_BaseRubble', seed=1104, builder='rubble',
         tri_budget=(300, 1200), needs_collision=False),
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


def _finish(parts, name, bevel_width=0.02):
    obj = C.join_objects(parts, name) if len(parts) > 1 else parts[0]
    S.bevel_obj(obj, width=bevel_width, segments=2)
    C.shade_smooth_auto(obj, angle_deg=35.0)
    C.add_basic_uv(obj)
    return obj


def _drop_to_ground(obj):
    """Traslada el objeto en Z para que su punto mas bajo quede en z=0
    (regla de pivote en la base). Red de seguridad para piezas con bordes
    rotos, peldanos en espiral o montones de escombros donde el minimo no
    sale exacto a mano."""
    obj.data.update()
    min_z = min(v[2] for v in obj.bound_box)
    if abs(min_z) > 1e-6:
        C.select_only(obj)
        import bpy
        bpy.ops.transform.translate(value=(0.0, 0.0, -min_z))
        bpy.ops.object.transform_apply(location=True)
    return obj


def _finish_grounded(parts, name, bevel_width=0.02):
    obj = C.join_objects(parts, name) if len(parts) > 1 else parts[0]
    _drop_to_ground(obj)
    S.bevel_obj(obj, width=bevel_width, segments=2)
    C.shade_smooth_auto(obj, angle_deg=35.0)
    C.add_basic_uv(obj)
    return obj


# Piedra caliza/arenisca clara y calida al sol (nunca gris apagado): el
# rango va de un sillar claro a uno mas tostado, con musgo vivo en parches
# para el remate roto y los escombros.
_STONE = (0.70, 0.55, 0.34)
_STONE_LIGHT = (0.80, 0.66, 0.42)
_STONE_DARK = (0.46, 0.35, 0.20)
_MOSS = (0.35, 0.55, 0.15)


# ---------------------------------------------------------------------------
# 1. Torre en ruinas: conica truncada, 5 tramos de piedra, remate roto
# ---------------------------------------------------------------------------
@_register('tower')
def _build_tower(variant, rnd):
    height = 12.0
    n_tramos = 5
    r0, r1 = 2.2, 1.4
    tramo_h = height / n_tramos

    parts = []
    for i in range(n_tramos):
        z0 = i * tramo_h
        z1 = (i + 1) * tramo_h
        rr0 = r0 + (r1 - r0) * (i / n_tramos)
        rr1 = r0 + (r1 - r0) * ((i + 1) / n_tramos)
        cap = i != n_tramos - 1  # el ultimo tramo queda abierto: remate roto
        tramo = C.make_cylinder(f'Tramo{i}', radius=rr0, depth=tramo_h, segments=16,
                                 center=(0.0, 0.0, (z0 + z1) * 0.5), cap_ends=cap, radius2=rr1)
        parts.append(tramo)

    # perfil irregular del borde superior roto: bloques que sobresalen o
    # faltan, en vez de un corte perfectamente limpio. Bloques algo mas
    # gruesos que el original para que no se lean como palillos de piedra.
    n_shards = rnd.randint(6, 9)
    for i in range(n_shards):
        if rnd.random() < 0.4:
            continue  # hueco: falta piedra en ese punto del borde
        ang = 2.0 * math.pi * i / n_shards
        sx = math.cos(ang) * r1 * 0.9
        sy = math.sin(ang) * r1 * 0.9
        sh = rnd.uniform(0.15, 0.6)
        shard = C.make_box(f'TopShard{i}', (0.62, 0.62, sh),
                            center=(sx, sy, height + sh * 0.5 - rnd.uniform(0.0, 0.3)))
        parts.append(shard)

    tower = C.join_objects(parts, 'Tower')
    C.merge_by_distance(tower, dist=0.01)
    M.assign(tower, ['M_Stone'])

    # bandas de sillares en tonos calidos alternos + musgo vivo en el
    # remate roto (parches organicos, no un tinte plano en toda la torre).
    banded_fn = S.banded_tint(
        [_STONE_LIGHT, _STONE, _STONE_DARK, _STONE, _STONE_LIGHT],
        'z', 0.0, height, jitter=0.03, rnd=rnd)
    moss_fn = S.weathered_tint(
        _STONE_LIGHT, _MOSS, seed=variant['seed'] * 3, patchiness=2.0,
        wear_amount=0.55, jitter=0.03, rnd=rnd)

    def _tower_color(v):
        if v.co.z > height * 0.78:
            r, g, b, a = moss_fn(v)
        else:
            r, g, b, a = banded_fn(v)
        # franja de puerta: mas oscura (recorte de sombra), en la cara -Y
        # de la base, sin caer a un negro plano
        if v.co.z < tramo_h * 1.1 and v.co.y < -r0 * 0.5 and abs(v.co.x) < r0 * 0.35:
            return (r * 0.45, g * 0.45, b * 0.45, a)
        return (r, g, b, a)

    C.set_vertex_colors(tower, _tower_color)

    return _finish_grounded([tower], 'SM_' + variant['name'], bevel_width=0.05)


# ---------------------------------------------------------------------------
# 2. Sala de la lampara rota: jaula de postes, algunos paneles de cristal,
#    remate conico hundido/inclinado
# ---------------------------------------------------------------------------
@_register('lantern_room')
def _build_lantern_room(variant, rnd):
    radius = 1.3
    post_h = 1.8
    n_posts = 8
    post_radius = 0.045  # +28% sobre el original: lectura de cerca menos "alambre"

    posts = []
    for i in range(n_posts):
        ang = 2.0 * math.pi * i / n_posts
        px = math.cos(ang) * radius
        py = math.sin(ang) * radius
        post = C.make_cylinder(f'Post{i}', radius=post_radius, depth=post_h, segments=12,
                                center=(px, py, post_h * 0.5))
        posts.append(post)
    post_grp = C.join_objects(posts, 'Posts')
    M.assign(post_grp, ['M_Metal'])
    C.set_vertex_colors(post_grp, S.weathered_tint(
        (0.55, 0.54, 0.52), (0.48, 0.28, 0.15), seed=variant['seed'] * 2,
        patchiness=4.0, wear_amount=0.4, jitter=0.03, rnd=rnd))

    glass_idx = rnd.sample(range(n_posts), k=rnd.randint(3, 4))
    panels = []
    for i in glass_idx:
        ang0 = 2.0 * math.pi * i / n_posts
        ang1 = 2.0 * math.pi * (i + 1) / n_posts
        mid_ang = (ang0 + ang1) * 0.5
        px = math.cos(mid_ang) * radius
        py = math.sin(mid_ang) * radius
        panel = C.make_box(f'Panel{i}', (radius * 0.7, 0.02, post_h * 0.75),
                            center=(0.0, 0.0, post_h * 0.5))
        C.select_only(panel)
        import bpy
        bpy.ops.transform.rotate(value=mid_ang, orient_axis='Z')
        bpy.ops.object.transform_apply(rotation=True)
        bpy.ops.transform.translate(value=(px * 0.98, py * 0.98, 0.0))
        bpy.ops.object.transform_apply(location=True)
        panels.append(panel)
    panel_grp = C.join_objects(panels, 'Panels')
    M.assign(panel_grp, ['M_Glass'])
    C.set_vertex_colors(panel_grp, C.constant_tint((0.55, 0.85, 0.80), alpha=0.45, jitter=0.02, rnd=rnd))

    roof = C.make_cylinder('Roof', radius=radius * 1.05, depth=0.5, segments=16,
                            center=(0.0, 0.0, post_h + 0.25), cap_ends=True, radius2=0.06)
    M.assign(roof, ['M_Metal'])
    C.set_vertex_colors(roof, S.weathered_tint(
        (0.55, 0.54, 0.52), (0.48, 0.28, 0.15), seed=variant['seed'] * 5,
        patchiness=3.5, wear_amount=0.4, jitter=0.03, rnd=rnd))
    C.select_only(roof)
    import bpy
    bpy.ops.transform.rotate(value=math.radians(12.0), orient_axis='X')
    bpy.ops.object.transform_apply(rotation=True)

    return _finish_grounded([post_grp, panel_grp, roof], 'SM_' + variant['name'], bevel_width=0.02)


# ---------------------------------------------------------------------------
# 3. Fragmento suelto de escalera de caracol: 6-8 peldanos ascendentes,
#    claramente incompleto (no forma una escalera entera)
# ---------------------------------------------------------------------------
@_register('stair_fragment')
def _build_stair_fragment(variant, rnd):
    n_steps = rnd.randint(6, 8)
    radius = 1.0
    step_h = 0.19
    ang_step = math.radians(28.0)

    steps = []
    for i in range(n_steps):
        ang = i * ang_step
        z = i * step_h
        # peldanos algo mas anchos/gruesos que el original: silueta menos
        # "tecnica" de cerca.
        step = C.make_box(f'Step{i}', (0.64, 0.36, step_h * 1.05),
                           center=(radius, 0.0, z + step_h * 0.45))
        C.select_only(step)
        import bpy
        bpy.ops.transform.rotate(value=ang, orient_axis='Z')
        bpy.ops.object.transform_apply(rotation=True)
        steps.append(step)

    stair = C.join_objects(steps, 'Stair')
    C.merge_by_distance(stair, dist=0.005)
    M.assign(stair, ['M_Stone'])
    C.set_vertex_colors(stair, S.weathered_tint(
        _STONE_LIGHT, _STONE_DARK, seed=variant['seed'], patchiness=2.5,
        wear_amount=0.4, jitter=0.04, rnd=rnd))

    return _finish_grounded([stair], 'SM_' + variant['name'], bevel_width=0.015)


# ---------------------------------------------------------------------------
# 4. Monticulo de escombros de piedra en la base de la torre
# ---------------------------------------------------------------------------
@_register('rubble')
def _build_rubble(variant, rnd):
    n_blobs = rnd.randint(5, 8)
    blobs = []
    for i in range(n_blobs):
        ang = rnd.uniform(0.0, 2.0 * math.pi)
        dist = rnd.uniform(0.0, 1.4)
        cx = math.cos(ang) * dist
        cy = math.sin(ang) * dist
        r = rnd.uniform(0.18, 0.38)
        cz = r * rnd.uniform(0.4, 0.7)
        blob = C.make_blob(f'Rubble{i}', (cx, cy, cz), r, variant['seed'] * 13 + i,
                            subdivisions=2, noise_scale=1.7, noise_strength=0.35,
                            scale=(1.0, rnd.uniform(0.85, 1.1), rnd.uniform(0.6, 0.85)),
                            relax_iterations=1)
        blobs.append(blob)
    rubble = C.join_objects(blobs, 'Rubble')
    C.merge_by_distance(rubble, dist=0.01)
    M.assign(rubble, ['M_Stone'])
    C.set_vertex_colors(rubble, S.weathered_tint(
        _STONE_LIGHT, _MOSS, seed=variant['seed'] * 7, patchiness=2.8,
        wear_amount=0.5, jitter=0.04, rnd=rnd))

    return _finish_grounded([rubble], 'SM_' + variant['name'], bevel_width=0.012)
