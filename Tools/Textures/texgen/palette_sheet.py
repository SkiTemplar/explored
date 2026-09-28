"""Hoja de contacto de la paleta: atlas por isla + muestra aplicada a formas low poly
sencillas sobre el suelo armonizado de la isla, con cielo y mar de fondo.

Rasterizador mínimo en numpy (z-buffer, sombreado plano por cara, sol + cielo + rebote),
suficiente para juzgar legibilidad y armonía; no pretende imitar Lumen.
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageOps

from .output import SHEET_MAX_BYTES, _font, to_u8
from .palette import (
    ATLAS,
    ENTORNO,
    ENTORNO_ROW,
    FAMILIES,
    ISLANDS,
    TERRAIN_ROW,
    TERRAIN_TARGETS,
    build_atlas,
    island_swatches,
    lch,
    linear_to_srgb,
    oklab_to_srgb,
    quantize,
    srgb_to_linear,
    vertex_tint,
)

SUN = np.array([-0.45, 0.72, -0.35])     # hacia el sol (x derecha, y arriba, z adelante)
SUN = SUN / np.linalg.norm(SUN)
SUN_COLOR = np.array([1.0, 0.95, 0.86]) * 2.6
SKY_AMBIENT = np.array(ENTORNO["cielo"][0]) * 0.75


# ---------------------------------------------------------------------------
# Mallas
# ---------------------------------------------------------------------------

def _mesh(verts, faces, swatch, v=None):
    verts = np.asarray(verts, dtype=np.float64)
    faces = np.asarray(faces, dtype=np.int64)
    if v is None:  # v del atlas por altura: arriba 0, abajo 1 (degradado de la celda)
        y = verts[:, 1]
        v = 1.0 - (y - y.min()) / max(np.ptp(y), 1e-6)
    return {"v": verts, "f": faces, "sw": swatch, "tv": np.asarray(v)}


def box(center, size, swatch, rot=0.0):
    sx, sy, sz = np.asarray(size) / 2
    p = np.array([[x, y, z] for x in (-sx, sx) for y in (0, 2 * sy) for z in (-sz, sz)])
    c, s = np.cos(rot), np.sin(rot)
    p = p @ np.array([[c, 0, -s], [0, 1, 0], [s, 0, c]]).T + center
    # índices: x*4 + y*2 + z
    quads = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    faces = [t for a, b, c_, d in quads for t in ((a, b, c_), (a, c_, d))]
    return _mesh(p, faces, swatch)


def prism(center, radius, height, sides, swatch, top_scale=1.0, y0=0.0, tilt=0.0):
    ang = np.linspace(0, 2 * np.pi, sides, endpoint=False) + 0.3
    bot = np.stack([np.cos(ang) * radius, np.zeros(sides) + y0, np.sin(ang) * radius], 1)
    top = np.stack([np.cos(ang) * radius * top_scale + tilt, np.zeros(sides) + y0 + height,
                    np.sin(ang) * radius * top_scale], 1)
    p = np.concatenate([bot, top, [[0, y0, 0], [tilt, y0 + height, 0]]]) + center
    faces = []
    for i in range(sides):
        j = (i + 1) % sides
        faces += [(i, j, sides + j), (i, sides + j, sides + i),
                  (2 * sides, j, i), (2 * sides + 1, sides + i, sides + j)]
    return _mesh(p, faces, swatch)


def icosphere(center, radius, swatch, squash=(1.0, 1.0, 1.0), seed=0, jitter=0.0, subdiv=1):
    t = (1 + 5 ** 0.5) / 2
    v = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t),
         (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
    f = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2),
         (10, 7, 6), (7, 1, 8), (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5),
         (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    v = [np.array(x, dtype=np.float64) / np.linalg.norm(x) for x in v]
    for _ in range(subdiv):
        cache, nf = {}, []
        for a, b, c in f:
            m = []
            for i, j in ((a, b), (b, c), (c, a)):
                k = (min(i, j), max(i, j))
                if k not in cache:
                    mid = v[i] + v[j]
                    v.append(mid / np.linalg.norm(mid))
                    cache[k] = len(v) - 1
                m.append(cache[k])
            nf += [(a, m[0], m[2]), (b, m[1], m[0]), (c, m[2], m[1]), (m[0], m[1], m[2])]
        f = nf
    p = np.array(v)
    if jitter:
        p *= 1.0 + jitter * (np.random.default_rng(seed).random(len(p))[:, None] - 0.5)
    p = p * radius * np.asarray(squash)
    p[:, 1] -= p[:, 1].min()
    return _mesh(p + center, f, swatch)


def fronds(center, height, n, length, swatch, seed=0):
    """Copa de palmera sencilla: hojas como tiras dobladas de 3 segmentos."""
    rng = np.random.default_rng(seed)
    verts, faces = [], []
    for k in range(n):
        a = 2 * np.pi * k / n + rng.random() * 0.3
        d = np.array([np.cos(a), 0, np.sin(a)])
        side = np.array([-d[2], 0, d[0]]) * 0.16
        pts = [np.array([0, height, 0]) + d * length * s + np.array([0, 0.25 * s - 0.9 * s * s, 0]) * length
               for s in (0.0, 0.35, 0.7, 1.0)]
        base = len(verts)
        for i, q in enumerate(pts):
            w = (1.0 - i / 4) * (0.4 if i == 0 else 1.0)
            verts += [q + side * w, q - side * w]
        for i in range(3):
            a0 = base + 2 * i
            faces += [(a0, a0 + 2, a0 + 3), (a0, a0 + 3, a0 + 1)]
    verts = np.array(verts) + center
    return _mesh(verts, faces, swatch)


def scene(island: str) -> list[dict]:
    """Formas por isla: cada una usa una celda del atlas de esa isla."""
    rock = "piedra.basalto" if island in ("Humo", "Landing") else "piedra.caliza"
    ms = [
        box((-0.95, 0, 2.3), (0.55, 0.45, 0.5), "madera.miel", rot=0.35),
        box((-0.95, 0.45, 2.3), (0.35, 0.28, 0.32), "madera.clara", rot=0.1),
        prism((-0.2, 0, 2.7), 0.26, 0.62, 8, "madera.caramelo", top_scale=0.92),
        prism((-0.2, 0.08, 2.7), 0.272, 0.06, 8, "metal.hierro"),
        prism((-0.2, 0.5, 2.7), 0.262, 0.06, 8, "metal.hierro"),
        icosphere((0.75, 0, 2.9), 0.42, rock, squash=(1.2, 0.7, 1.0), seed=3, jitter=0.35),
        icosphere((1.25, 0, 3.3), 0.25, "piedra.canto", squash=(1.1, 0.6, 1.0), seed=5, jitter=0.3),
        icosphere((1.6, 0, 4.2), 0.55, "vegetacion.hoja_oscura", squash=(1.0, 0.8, 1.0), seed=7, jitter=0.3),
        icosphere((1.2, 0, 4.5), 0.4, "vegetacion.hoja", squash=(1.0, 0.9, 1.0), seed=8, jitter=0.3),
        prism((-2.0, 0, 4.2), 0.09, 0.8, 6, "vegetacion.tronco_palma", top_scale=0.7, tilt=0.1),
        fronds((-1.9, 0, 4.2), 0.8, 7, 0.9, "vegetacion.hoja_clara", seed=2),
        prism((0.35, 0, 1.75), 0.14, 0.18, 8, "metal.cobre", top_scale=1.1),
        box((-0.6, 0, 1.45), (0.5, 0.05, 0.3), "tela.lona", rot=-0.2),
        icosphere((-0.35, 0, 1.45), 0.07, "comida.mango", squash=(1.0, 1.2, 1.0)),
        icosphere((-0.52, 0, 1.4), 0.06, "comida.limon"),
        icosphere((0.05, 0, 1.3), 0.065, "comida.carne_cruda", squash=(1.4, 0.6, 1.0)),
        icosphere((-0.8, 0, 1.52), 0.07, "comida.platano", squash=(1.8, 0.6, 0.7)),
        prism((0.65, 0, 1.6), 0.02, 0.5, 5, "bambu.maduro"),
        prism((0.8, 0, 1.75), 0.022, 0.45, 5, "bambu.verde"),
        box((0.9, 0.36, 1.55), (0.2, 0.12, 0.01), "tela.rojo", rot=0.2),
        box((-1.45, 0, 1.9), (0.5, 0.04, 0.12), "madera.deriva", rot=0.6),
        icosphere((1.15, 0, 1.9), 0.09, "palma.coco", squash=(1.0, 0.95, 1.0)),
        box((0.3, 0, 2.2), (0.35, 0.22, 0.25), "palma.paja", rot=-0.4),
        # Recursos sueltos y minerales (filas 11-12): lo que se recoge del suelo.
        prism((1.45, 0, 1.55), 0.1, 0.2, 8, "mineral.terracota", top_scale=0.6),
        prism((-1.15, 0, 1.35), 0.04, 0.07, 6, "recurso.hueso", top_scale=0.8),
        icosphere((0.3, 0, 1.25), 0.05, "recurso.concha", squash=(1.3, 0.45, 1.0)),
        box((-0.15, 0, 1.2), (0.2, 0.012, 0.035), "recurso.pluma", rot=-0.5),
        icosphere((1.05, 0, 1.35), 0.05, "mineral.azufre", seed=11, jitter=0.4),
        # Fauna (fila 13): jabalí de lomo oscuro y gallina roja.
        *[prism((x, 0, z), 0.045, 0.2, 5, "fauna.jabali") for x, z in
          ((1.74, 2.41), (1.74, 2.57), (1.38, 2.41), (1.38, 2.57))],
        icosphere((1.56, 0.15, 2.49), 0.26, "fauna.jabali", squash=(1.4, 0.75, 0.75), seed=13, jitter=0.15),
        icosphere((1.22, 0.14, 2.49), 0.14, "fauna.jabali_claro", squash=(1.2, 0.95, 0.9), seed=14, jitter=0.1),
        icosphere((1.09, 0.21, 2.49), 0.05, "fauna.pezuna", squash=(1.0, 0.9, 1.0)),
        box((1.12, 0.19, 2.42), (0.09, 0.02, 0.02), "fauna.cuerno", rot=-0.3),
        box((1.27, 0.37, 2.44), (0.04, 0.07, 0.02), "fauna.jabali", rot=0.2),
        box((1.27, 0.37, 2.54), (0.04, 0.07, 0.02), "fauna.jabali", rot=-0.2),
        icosphere((-0.25, 0.0, 2.05), 0.12, "fauna.plumaje", squash=(1.25, 0.95, 0.9), seed=15, jitter=0.1),
        icosphere((-0.13, 0.19, 2.05), 0.06, "fauna.plumaje"),
        icosphere((-0.13, 0.3, 2.05), 0.03, "fauna.cresta", squash=(1.3, 1.0, 0.6)),
        icosphere((-0.07, 0.24, 2.05), 0.022, "fauna.pico", squash=(1.3, 0.8, 0.8)),
    ]
    return ms


# ---------------------------------------------------------------------------
# Render
# ---------------------------------------------------------------------------

def _sample_cell(atlas: np.ndarray, cell, tv: float) -> np.ndarray:
    col, row = cell
    c = ATLAS["cell_px"]
    m = ATLAS["margin_px"]
    y = row * c + m + float(np.clip(tv, 0, 1)) * (c - 2 * m - 1)
    y0 = int(np.floor(y))
    t = y - y0
    x = col * c + c // 2
    return atlas[y0, x] * (1 - t) + atlas[min(y0 + 1, atlas.shape[0] - 1), x] * t


def render_scene(island, atlas, swatches, ground_bc, ground2_bc, tint_sand, tint_grass, w=900, h=400):
    cells = {s["id"]: s["cell"] for s in swatches}
    cam = np.array([0.0, 1.2, -0.6])
    pitch = np.radians(15.0)
    fwd = np.array([0, -np.sin(pitch), np.cos(pitch)])
    right = np.array([1.0, 0, 0])
    up = np.cross(fwd, right)
    f = w * 0.62

    # Rayos por píxel.
    xs = (np.arange(w) + 0.5 - w / 2) / f
    ys = -(np.arange(h) + 0.5 - h * 0.38) / f
    X, Y = np.meshgrid(xs, ys)
    dirs = fwd + X[..., None] * right + Y[..., None] * up
    dirs /= np.linalg.norm(dirs, axis=-1, keepdims=True)

    # Cielo y mar (lejos).
    sky_lin = np.array(ENTORNO["cielo"][0])
    elev = np.clip(dirs[..., 1], 0, 1)[..., None]
    img = sky_lin * 1.25 * (1 - elev) ** 2 + sky_lin * 0.8 * (1 - (1 - elev) ** 2)
    img = img + np.array([0.9, 0.85, 0.75]) * 0.25 * (1 - elev) ** 6

    depth = np.full((h, w), np.inf)
    down = dirs[..., 1] < -1e-4
    tg = np.where(down, cam[1] / np.maximum(-dirs[..., 1], 1e-4), np.inf)
    # Punto de suelo solo donde el rayo baja; en el resto (cielo), 0 en vez de inf: con inf
    # salían NaN (inf * 0) que se convertían a índice entero con avisos. Esos píxeles se
    # descartan igual con las máscaras `ground`/`sea`, así que la imagen no cambia.
    gp = cam + dirs * np.where(down, tg, 0.0)[..., None]
    sea_z = 7.0
    ground = down & (gp[..., 2] < sea_z)
    sea = down & ~ground
    # Mar: laguna cerca de la orilla, arrecife más lejos, espuma en la orilla.
    zz = gp[..., 2]
    lag, arr = np.array(ENTORNO["laguna"][0]), np.array(ENTORNO["arrecife"][0])
    k = np.clip((zz - sea_z) / 14.0, 0, 1)[..., None]
    sea_col = (lag * (1 - k) + arr * k) * 0.95
    foam = np.clip(1 - (zz - sea_z) / 0.35, 0, 1)[..., None]
    sea_col = sea_col * (1 - foam) + np.array(ENTORNO["espuma"][0]) * foam
    img = np.where(sea[..., None], sea_col, img)

    # Suelo: textura de terreno armonizada (tile 2 m), teñida como en M_Terrain y mezclada
    # con la segunda capa por manchas grandes.
    n = ground_bc.shape[0]
    iu = (np.mod(gp[..., 0] / 2.0, 1.0) * n).astype(int) % n
    iv = (np.mod(gp[..., 2] / 2.0, 1.0) * n).astype(int) % n
    g1 = vertex_tint(srgb_to_linear(ground_bc[iv, iu, :3]), tint_sand)
    g2 = vertex_tint(srgb_to_linear(ground2_bc[iv, iu, :3]), tint_grass)
    blob = 0.5 + 0.5 * np.sin(gp[..., 0] * 1.3 + 0.8) * np.sin(gp[..., 2] * 0.9 + 1.7)
    mix = np.clip((blob - 0.55) * 4 + (gp[..., 2] - 3.6) * 0.8, 0, 1)[..., None]
    gcol = g1 * (1 - mix) + g2 * mix
    shade = SUN_COLOR * SUN[1] + SKY_AMBIENT
    ground_lin = gcol * shade

    meshes = scene(island)
    # Sombras de contacto: elipses oscuras desplazadas en contra del sol.
    occl = np.ones((h, w))
    for mdl in meshes:
        p = mdl["v"]
        c = p.mean(axis=0)
        r = np.ptp(p[:, [0, 2]], axis=0).max() * 0.55
        hgt = np.ptp(p[:, 1])
        off = -SUN[[0, 2]] / SUN[1] * hgt * 0.5
        d = np.hypot(gp[..., 0] - c[0] - off[0], (gp[..., 2] - c[2] - off[1]) * 1.1)
        occl = np.minimum(occl, 0.55 + 0.45 * np.clip((d / max(r + hgt * 0.3, 0.05)) ** 2, 0, 1))
    ground_lin = ground_lin * np.where(ground, occl, 1.0)[..., None]
    img = np.where(ground[..., None], ground_lin, img)
    depth = np.where(ground, tg, depth)

    for mdl in meshes:
        _raster(img, depth, mdl, atlas, cells, cam, fwd, right, up, f, w, h)
    return linear_to_srgb(img * 0.62)


def _raster(img, depth, mdl, atlas, cells, cam, fwd, right, up, f, w, h):
    P = mdl["v"] - cam
    z = P @ fwd
    sx = (P @ right) / z * f + w / 2
    sy = -(P @ up) / z * f + h * 0.38
    for tri in mdl["f"]:
        a, b, c = tri
        if min(z[a], z[b], z[c]) <= 0.05:
            continue
        nrm = np.cross(mdl["v"][b] - mdl["v"][a], mdl["v"][c] - mdl["v"][a])
        ln = np.linalg.norm(nrm)
        if ln < 1e-12:
            continue
        nrm /= ln
        centroid = (mdl["v"][a] + mdl["v"][b] + mdl["v"][c]) / 3
        if np.dot(nrm, centroid - cam) > 0:
            nrm = -nrm   # caras a doble cara (hojas): se ilumina la que mira a cámara
        tv = (mdl["tv"][a] + mdl["tv"][b] + mdl["tv"][c]) / 3
        alb = srgb_to_linear(_sample_cell(atlas, cells[mdl["sw"]], tv))
        light = SUN_COLOR * max(np.dot(nrm, SUN), 0.0) + SKY_AMBIENT * (0.6 + 0.4 * nrm[1]) \
            + np.array([0.35, 0.3, 0.22]) * 0.25 * (0.5 - 0.5 * nrm[1])
        col = alb * light
        x0, x1 = int(max(np.floor(min(sx[a], sx[b], sx[c])), 0)), int(min(np.ceil(max(sx[a], sx[b], sx[c])), w - 1))
        y0, y1 = int(max(np.floor(min(sy[a], sy[b], sy[c])), 0)), int(min(np.ceil(max(sy[a], sy[b], sy[c])), h - 1))
        if x1 < x0 or y1 < y0:
            continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        d = (sy[b] - sy[c]) * (sx[a] - sx[c]) + (sx[c] - sx[b]) * (sy[a] - sy[c])
        if abs(d) < 1e-9:
            continue
        l1 = ((sy[b] - sy[c]) * (gx - sx[c]) + (sx[c] - sx[b]) * (gy - sy[c])) / d
        l2 = ((sy[c] - sy[a]) * (gx - sx[c]) + (sx[a] - sx[c]) * (gy - sy[c])) / d
        l3 = 1 - l1 - l2
        inside = (l1 >= -1e-6) & (l2 >= -1e-6) & (l3 >= -1e-6)
        zp = 1.0 / (l1 / z[a] + l2 / z[b] + l3 / z[c])
        sub = depth[y0:y1 + 1, x0:x1 + 1]
        m = inside & (zp < sub)
        sub[m] = zp[m]
        img[y0:y1 + 1, x0:x1 + 1][m] = col


# ---------------------------------------------------------------------------
# Hoja
# ---------------------------------------------------------------------------

GROUNDS = {  # isla -> (capa 1, capa 2) del suelo de la muestra
    "Landing": ("SandDry", "Grass"),
    "Esmeralda": ("Grass", "SandDry"),
    "Humo": ("Ash", "VolcanicRock"),
    "Dientes": ("Limestone", "SandDry"),
}


def contact_sheet(path: Path, terrain: dict[str, np.ndarray], raw_terrain: dict[str, np.ndarray],
                  title: str) -> Path:
    """terrain / raw_terrain: material -> BC armonizado / sin armonizar (para antes/después)."""
    pad, atlas_px, scene_w, scene_h = 16, 400, 1000, 400
    label_w = 110
    W = pad + label_w + atlas_px + pad + scene_w + pad
    tile = 150
    strip_h = 60 + tile + 40
    H = 64 + len(ISLANDS) * (scene_h + 44 + pad) + strip_h + pad
    sheet = Image.new("RGB", (W, H), (30, 32, 38))
    draw = ImageDraw.Draw(sheet)
    draw.text((pad, 16), title, fill=(240, 236, 226), font=_font(24))
    f_big, f_small = _font(18), _font(12)
    cell_draw = atlas_px / ATLAS["grid"]
    y = 64
    for isl in ISLANDS:
        sws = island_swatches(isl)
        atlas = build_atlas(sws)
        draw.text((pad, y), f"{isl.name}  ·  T_Palette_{isl.key}", fill=(250, 244, 230), font=f_big)
        draw.text((pad + label_w + atlas_px + pad, y + 4), isl.mood, fill=(175, 178, 186), font=f_small)
        y0 = y + 30
        img = Image.fromarray(to_u8(atlas)).resize((atlas_px, atlas_px), Image.Resampling.NEAREST)
        sheet.paste(img, (pad + label_w, y0))
        rows = {f.row: f.key for f in FAMILIES}
        rows[TERRAIN_ROW], rows[ENTORNO_ROW] = "terreno", "entorno"
        for r, name in rows.items():
            draw.text((pad, y0 + r * cell_draw + 6), name, fill=(200, 200, 205), font=f_small)
        draw.text((pad, y0 + (max(rows) + 1) * cell_draw + 6), "(libres)", fill=(120, 120, 128), font=f_small)
        g1, g2 = GROUNDS[isl.key]
        layer = {m: TERRAIN_TARGETS[m][4] for m in TERRAIN_TARGETS}
        render = render_scene(isl.key, atlas, sws, terrain[g1], terrain[g2],
                              isl.vertex[layer[g1]], isl.vertex[layer[g2]], scene_w, scene_h)
        sheet.paste(Image.fromarray(to_u8(render)), (pad + label_w + atlas_px + pad, y0))
        y = y0 + scene_h + pad
    # Antes / después de la armonización del terreno.
    draw.text((pad, y + 6), "Terreno armonizado con la paleta (arriba: antes · abajo: después · cuadro: objetivo)",
              fill=(250, 244, 230), font=f_big)
    x = pad
    half = tile // 2
    for m in TERRAIN_TARGETS:
        before = Image.fromarray(to_u8(raw_terrain[m][..., :3])).resize((tile, tile), Image.Resampling.LANCZOS)
        after = Image.fromarray(to_u8(terrain[m][..., :3])).resize((tile, tile), Image.Resampling.LANCZOS)
        sheet.paste(before.crop((0, 0, tile, half)), (x, y + 40))
        sheet.paste(after.crop((0, half, tile, tile)), (x, y + 40 + half))
        _, L, C, hh, _ = TERRAIN_TARGETS[m]
        tgt = tuple(int(v) for v in to_u8(quantize(oklab_to_srgb(lch(L, C, hh)))))
        draw.rectangle((x + tile - 34, y + 40 + half - 17, x + tile - 4, y + 40 + half + 13),
                       fill=tgt, outline=(20, 20, 20))
        draw.text((x, y + 44 + tile), m, fill=(210, 210, 215), font=f_small)
        x += tile + 12
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path, optimize=True)
    for bits in (6, 5):
        if path.stat().st_size <= SHEET_MAX_BYTES:
            break
        ImageOps.posterize(sheet, bits).save(path, optimize=True)
    return path
