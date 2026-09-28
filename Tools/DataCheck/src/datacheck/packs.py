"""Comprobaciones de Content/Data/packs_catalogo.json y Tools/Packs/packs.json (GDD v2 §7.1).

El catálogo dice qué fichero de qué pack CC0 cubre cada id de juego; ``Tools/Packs/normalize.py``
lo aplica en Blender. Aquí se mira que los ids existan, que todo pack sea CC0 verificado y
con sha256, que no haya duplicados y que cada regla de color apunte a una muestra real de
``Tools/Textures/paleta.json``. La fauna (``kind: fauna``, ids de ``fauna_terrestre.json`` o ``fauna.json``)
lleva malla con esqueleto ``SK_Pack_*`` y un bloque ``rig`` con sus acciones. Los iconos de UI
(``icons``) apuntan a pistas ``icon`` de ``achievements.json`` o a un widget ``SExplored*`` y se
tiñen con colores de ``ExploredUIStyle.h``; toda pista de logro queda cubierta o en ``iconsPending``.
"""

from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Callable

CATALOG = "packs_catalogo.json"
MANIFEST = Path("Tools") / "Packs" / "packs.json"
PALETTE = Path("Tools") / "Textures" / "paleta.json"
SPDX_CC0 = "CC0-1.0"
KINDS = {"item", "pieza", "planta", "fauna"}
FAUNA = "fauna_terrestre.json"
ISLANDS = {"Landing", "Emerald", "Smoke", "Teeth", "Mangrove", "WhiteSands", "Mesa"}
PHASES = {"AA", "F2", "F3"}
AXES = {"x", "y", "z", "max"}
PIVOTS = {"base", "agarre"}
HAND_SOCKETS = {"hand_r"}
SHEET_MAX_BYTES = 1_000_000
ASCII_ID = re.compile(r"^[a-z0-9_]+$")
# Ids de juego: item o pieza, o <planta>.<etapa> para las fases del huerto.
GAME_ID = re.compile(r"^[a-z0-9_]+(\.[a-z0-9_]+)?$")
LOTE_ID = re.compile(r"^lote[0-9]+-[a-z0-9-]+$")
MESH = re.compile(r"^SM_Pack_[A-Za-z0-9]+$")
SK_MESH = re.compile(r"^SK_Pack_[A-Za-z0-9]+$")
SKELETON = re.compile(r"^SKEL_Pack_[A-Za-z0-9]+$")
ACTION = re.compile(r"^[A-Za-z][A-Za-z0-9_]*$")
HEX = re.compile(r"^#[0-9a-f]{6}$")
SHA256 = re.compile(r"^[0-9a-f]{64}$")
DATE = re.compile(r"^\d{4}-\d{2}-\d{2}$")
SM_NAME = re.compile(r"/(SM_[A-Za-z0-9_]+)\.\1$")
TEXTURE = re.compile(r"^T_Pack_Icon_[A-Za-z0-9]+$")
WIDGET = re.compile(r"^SExplored[A-Za-z0-9]+$")
ICON_ROLE = re.compile(r"^[a-z][a-z0-9_]*$")
UI_WIDGETS = Path("Source") / "Explored" / "UI" / "Widgets"
UI_STYLE = Path("Source") / "Explored" / "UI" / "ExploredUIStyle.h"
STYLE_COLOR = re.compile(r"FLinearColor (Color[A-Za-z]+)\(\) const")

Err = Callable[[str], None]


def load_manifest(repo_root: Path) -> dict | None:
    path = repo_root / MANIFEST
    return json.loads(path.read_text(encoding="utf-8")) if path.exists() else None


def palette_samples(repo_root: Path) -> set[str]:
    path = repo_root / PALETTE
    if not path.exists():
        return set()
    doc = json.loads(path.read_text(encoding="utf-8"))
    return set(doc["islas"]["Landing"]["colores"])


def check_manifest(manifest: dict, error: Err) -> set[str]:
    """Valida packs.json y devuelve los ids de pack utilizables (CC0 verificados)."""
    ok: set[str] = set()
    seen: set[str] = set()
    for p in manifest.get("packs", []):
        pid = p.get("id", "")
        where = f"packs.json: pack «{pid}»"
        if not ASCII_ID.match(pid):
            error(f"{where}: id no ASCII en minúsculas")
            continue
        if pid in seen:
            error(f"{where}: id duplicado")
        seen.add(pid)
        for key in ("name", "author", "page", "version"):
            if not isinstance(p.get(key), str) or not p[key]:
                error(f"{where}: falta «{key}»")
        if not str(p.get("page", "")).startswith("https://"):
            error(f"{where}: la página oficial debe ser https")
        src = p.get("source", {})
        kind = src.get("kind")
        if kind == "direct":
            if not str(src.get("url", "")).startswith("https://"):
                error(f"{where}: fuente direct sin URL https")
        elif kind == "itch":
            if not str(src.get("page", "")).startswith("https://") or ".itch.io/" not in src.get("page", ""):
                error(f"{where}: fuente itch sin página de itch.io")
            if not isinstance(src.get("uploadId"), int) or src["uploadId"] <= 0:
                error(f"{where}: fuente itch sin uploadId entero")
        else:
            error(f"{where}: fuente «{kind}» desconocida (direct o itch)")
        lic = p.get("license", {})
        licensed = True
        if lic.get("spdx") != SPDX_CC0:
            error(f"{where}: licencia «{lic.get('spdx')}» no es {SPDX_CC0}")
            licensed = False
        if not DATE.match(str(lic.get("verifiedOn", ""))):
            error(f"{where}: licencia sin fecha de verificación (verifiedOn AAAA-MM-DD)")
            licensed = False
        if not lic.get("evidence"):
            error(f"{where}: licencia sin evidencia (texto leído en la página del autor)")
            licensed = False
        if not SHA256.match(str(p.get("sha256", ""))):
            error(f"{where}: sha256 ausente o mal formado (uv run python fetch_packs.py --update-sha)")
            licensed = False
        if licensed:
            ok.add(pid)
    return ok


def _game_ids(data: dict) -> dict[str, set[str]]:
    items = {i.get("id") for i in data.get("items.json", [])}
    pieces = {p.get("id") for p in data.get("building_pieces.json", {}).get("pieces", [])}
    stages = {
        f"{pl['id']}.{s['id']}"
        for pl in data.get("plants.json", {}).get("plants", [])
        for s in pl.get("stages", [])
    }
    # fauna.json trae también la fauna de ambiente (cangrejo, gaviota, fragata), que no está
    # en fauna_terrestre.json pero sale en meshes_pendientes.json y se busca en los packs.
    fauna = {f.get("id") for f in data.get(FAUNA, {}).get("species", [])}
    fauna |= {f.get("id") for f in data.get("fauna.json", {}).get("species", [])}
    return {"item": items, "pieza": pieces, "planta": stages, "fauna": fauna}


def check_fauna(data: dict, error: Err) -> None:
    """fauna_terrestre.json: ids únicos, islas de EIslandArchetype y fase."""
    doc = data.get(FAUNA)
    if doc is None:
        return
    seen: set[str] = set()
    for f in doc.get("species", []):
        fid = f.get("id", "")
        where = f"{FAUNA}: «{fid}»"
        if not ASCII_ID.match(fid):
            error(f"{where}: id no ASCII en minúsculas")
        if fid in seen:
            error(f"{where}: id duplicado")
        seen.add(fid)
        for key in ("nameEs", "nameEn", "source"):
            if not f.get(key):
                error(f"{where}: falta «{key}»")
        if not isinstance(f.get("wild"), bool):
            error(f"{where}: wild debe ser true o false")
        bad = [i for i in f.get("islands", []) if i not in ISLANDS]
        if bad:
            error(f"{where}: islas desconocidas {bad} (EIslandArchetype)")
        if f.get("wild") and not f.get("islands"):
            error(f"{where}: especie salvaje sin isla")
        if f.get("phase") not in PHASES:
            error(f"{where}: fase «{f.get('phase')}» desconocida ({', '.join(sorted(PHASES))})")


def _check_rig(e: dict, where: str, error: Err) -> None:
    rig = e.get("rig")
    if e.get("kind") != "fauna":
        if rig is not None:
            error(f"{where}: rig solo va en kind fauna")
        return
    if not isinstance(rig, dict):
        error(f"{where}: la fauna necesita un bloque rig (esqueleto y acciones)")
        return
    if not SKELETON.match(str(rig.get("skeleton", ""))):
        error(f"{where}: rig.skeleton «{rig.get('skeleton')}» no sigue SKEL_Pack_<Nombre>")
    anims = rig.get("animations", [])
    if not anims or not all(isinstance(a, str) and ACTION.match(a) for a in anims):
        error(f"{where}: rig.animations debe listar las acciones del pack")
        anims = []
    if len(set(anims)) != len(anims):
        error(f"{where}: rig.animations con acciones repetidas")
    for beh, clip in (rig.get("behaviors") or {}).items():
        if clip is not None and clip not in anims:
            error(f"{where}: el comportamiento {beh} usa «{clip}», que no está en rig.animations")
    for pose in rig.get("tilePoses", []):
        if pose.get("action") not in anims:
            error(f"{where}: tilePoses usa «{pose.get('action')}», que no está en rig.animations")
    if "stretch" in e:
        error(f"{where}: stretch no se admite con rig (deformaría los huesos)")


def _item_meshes(data: dict) -> dict[str, str]:
    out = {}
    for i in data.get("items.json", []):
        m = SM_NAME.search(i.get("meshPath") or "")
        if m:
            out[i["id"]] = m.group(1)
    return out


def _piece_meshes(data: dict) -> dict[str, str]:
    return {
        p.get("id"): p.get("mesh")
        for p in data.get("building_pieces.json", {}).get("pieces", [])
        if p.get("mesh")
    }


def _achievement_icons(data: dict) -> set[str]:
    return {a.get("icon") for a in data.get("achievements.json", {}).get("achievements", []) if a.get("icon")}


def _style_colors(repo_root: Path) -> set[str]:
    path = repo_root / UI_STYLE
    return set(STYLE_COLOR.findall(path.read_text(encoding="utf-8"))) if path.exists() else set()


def check_icons(repo_root: Path, catalog: dict, data: dict, lotes: set[str], known_packs: set[str],
                usable: set[str], error: Err) -> None:
    """Iconos de UI: pista de logro o widget existentes, tintes de ExploredUIStyle y cobertura total."""
    hints = _achievement_icons(data)
    colors = _style_colors(repo_root)
    widgets = repo_root / UI_WIDGETS
    seen_tex: set[str] = set()
    seen_slot: set[str] = set()
    chosen: set[tuple[str, str, str]] = set()
    for icon in catalog.get("icons", []):
        tex = icon.get("texture", "")
        where = f"{CATALOG}: icono «{tex}»"
        if not TEXTURE.match(tex):
            error(f"{where}: textura no sigue T_Pack_Icon_<Nombre>")
        if tex in seen_tex:
            error(f"{where}: textura repetida")
        seen_tex.add(tex)
        if icon.get("lote") not in lotes:
            error(f"{where}: lote «{icon.get('lote')}» no declarado en «lotes»")
        pack = icon.get("pack")
        if pack not in known_packs:
            error(f"{where}: pack «{pack}» no está en packs.json")
        elif pack not in usable:
            error(f"{where}: pack «{pack}» sin licencia CC0 verificada o sin sha256")
        f = icon.get("file", "")
        if not f.endswith(".png") or f.startswith("/") or ".." in Path(f).parts:
            error(f"{where}: «file» debe ser un .png con ruta relativa dentro del pack")
        slots = icon.get("slots") or []
        if not slots:
            error(f"{where}: sin slots (dónde se usa)")
        for slot in slots:
            if "achievementIcon" in slot:
                hint = slot["achievementIcon"]
                key = f"logro:{hint}"
                chosen.add((hint, str(pack), f))
                if hint not in hints:
                    error(f"{where}: la pista «{hint}» no es un icon de achievements.json")
            else:
                w, role = slot.get("widget", ""), slot.get("role", "")
                key = f"{w}.{role}"
                if not WIDGET.match(w) or not ICON_ROLE.match(role):
                    error(f"{where}: slot «{key}» necesita widget SExplored<Nombre> y role en minúsculas")
                elif widgets.is_dir() and not (widgets / f"{w}.h").exists():
                    error(f"{where}: el widget «{w}» no existe en {UI_WIDGETS.as_posix()}")
            if key in seen_slot:
                error(f"{where}: el slot «{key}» ya tiene icono")
            seen_slot.add(key)
        tint = icon.get("tint")
        if not isinstance(tint, dict) or not tint:
            error(f"{where}: tint debe dar un color de ExploredUIStyle por estado")
        else:
            for state, color in tint.items():
                if not ICON_ROLE.match(state):
                    error(f"{where}: estado «{state}» no ASCII en minúsculas")
                if colors and color not in colors:
                    error(f"{where}: el tinte «{color}» no existe en ExploredUIStyle.h")

    covered = {k.split(":", 1)[1] for k in seen_slot if k.startswith("logro:")}
    for d in catalog.get("iconsDiscarded", []):
        hint = d.get("achievementIcon", "")
        if hint not in hints:
            error(f"{CATALOG}: iconsDiscarded: «{hint}» no es un icon de achievements.json")
        if d.get("pack") not in known_packs:
            error(f"{CATALOG}: iconsDiscarded: «{hint}» cita el pack «{d.get('pack')}», que no está en packs.json")
        if (hint, d.get("pack"), d.get("file")) in chosen:
            error(f"{CATALOG}: iconsDiscarded: «{hint}» descarta {d.get('file')}, que es el fichero de su icono")
        if not d.get("reason"):
            error(f"{CATALOG}: iconsDiscarded: «{hint}» sin motivo")
    pending: set[str] = set()
    for d in catalog.get("iconsPending", []):
        hint = d.get("achievementIcon", "")
        if hint not in hints:
            error(f"{CATALOG}: iconsPending: «{hint}» no es un icon de achievements.json")
        if hint in covered:
            error(f"{CATALOG}: iconsPending: «{hint}» ya tiene icono")
        if hint in pending:
            error(f"{CATALOG}: iconsPending: «{hint}» repetido")
        pending.add(hint)
        if not d.get("reason"):
            error(f"{CATALOG}: iconsPending: «{hint}» sin motivo")
    if "icons" in catalog:
        for hint in sorted(hints - covered - pending):
            error(f"{CATALOG}: la pista de logro «{hint}» no tiene icono ni está en iconsPending")


def check_catalog(repo_root: Path, data: dict, error: Err) -> None:
    catalog = data.get(CATALOG)
    if catalog is None:
        return
    manifest = load_manifest(repo_root)
    if manifest is None:
        error(f"{CATALOG}: falta {MANIFEST.as_posix()}")
        return
    usable = check_manifest(manifest, error)
    check_fauna(data, error)
    known_packs = {p.get("id") for p in manifest.get("packs", [])}
    samples = palette_samples(repo_root)
    ids = _game_ids(data)
    current_meshes = {"item": _item_meshes(data), "pieza": _piece_meshes(data)}

    lotes = set()
    for lote in catalog.get("lotes", []):
        lid = lote.get("id", "")
        if not LOTE_ID.match(lid):
            error(f"{CATALOG}: lote «{lid}» no sigue lote<N>-<nombre>")
        if lid in lotes:
            error(f"{CATALOG}: lote «{lid}» duplicado")
        lotes.add(lid)
        if not DATE.match(str(lote.get("date", ""))):
            error(f"{CATALOG}: lote «{lid}» sin fecha")
        sheet = lote.get("sheet", "")
        path = repo_root / sheet
        if not sheet.startswith("docs/art/packs/") or not sheet.endswith(".png"):
            error(f"{CATALOG}: lote «{lid}»: la hoja de contacto va en docs/art/packs/*.png")
        elif not path.exists():
            error(f"{CATALOG}: lote «{lid}»: no existe la hoja de contacto {sheet}")
        elif path.stat().st_size > SHEET_MAX_BYTES and not path.read_bytes().startswith(b"version https://git-lfs"):
            error(f"{CATALOG}: lote «{lid}»: {sheet} pasa de 1 MB")

    seen_game: set[tuple[str, str]] = set()
    seen_mesh: set[str] = set()
    for e in catalog.get("entries", []):
        gid, kind = e.get("gameId", ""), e.get("kind", "")
        where = f"{CATALOG}: «{gid}»"
        if kind not in KINDS:
            error(f"{where}: kind «{kind}» desconocido ({', '.join(sorted(KINDS))})")
        elif gid not in ids[kind]:
            error(f"{where}: no existe como {kind} en los datos del juego")
        if (kind, gid) in seen_game:
            error(f"{where}: entrada duplicada")
        seen_game.add((kind, gid))
        if e.get("lote") not in lotes:
            error(f"{where}: lote «{e.get('lote')}» no declarado en «lotes»")
        pack = e.get("pack")
        if pack not in known_packs:
            error(f"{where}: pack «{pack}» no está en packs.json")
        elif pack not in usable:
            error(f"{where}: pack «{pack}» sin licencia CC0 verificada o sin sha256")
        f = e.get("file", "")
        if not f or f.startswith("/") or ".." in Path(f).parts:
            error(f"{where}: «file» debe ser una ruta relativa dentro del pack")
        mesh = e.get("mesh", "")
        if kind == "fauna" and not SK_MESH.match(mesh):
            error(f"{where}: malla con esqueleto «{mesh}» no sigue SK_Pack_<Nombre>")
        elif kind != "fauna" and not MESH.match(mesh):
            error(f"{where}: malla «{mesh}» no sigue SM_Pack_<Nombre>")
        _check_rig(e, where, error)
        if mesh in seen_mesh:
            error(f"{where}: malla «{mesh}» repetida")
        seen_mesh.add(mesh)
        if "replaces" in e and kind in current_meshes and current_meshes[kind].get(gid) != e["replaces"]:
            error(f"{where}: replaces «{e['replaces']}» no es la malla actual ({current_meshes[kind].get(gid)})")
        rot = e.get("rotateDeg", [0, 0, 0])
        if not (isinstance(rot, list) and len(rot) == 3 and all(isinstance(a, (int, float)) for a in rot)):
            error(f"{where}: rotateDeg debe ser [x, y, z] en grados")
        stretch = e.get("stretch", [1, 1, 1])
        if not (isinstance(stretch, list) and len(stretch) == 3 and all(isinstance(a, (int, float)) and 0.25 <= a <= 4 for a in stretch)):
            error(f"{where}: stretch debe ser [x, y, z] entre 0.25 y 4")
        size = e.get("size", {})
        m = size.get("m")
        if size.get("axis") not in AXES or not isinstance(m, (int, float)) or not 0.01 <= m <= 30:
            error(f"{where}: size necesita axis (x, y, z, max) y m entre 0.01 y 30 metros")
            m = None
        pivot = e.get("pivot", {})
        if pivot.get("kind") not in PIVOTS:
            error(f"{where}: pivote «{pivot.get('kind')}» desconocido (base o agarre)")
        elif pivot["kind"] == "agarre":
            if pivot.get("socket") not in HAND_SOCKETS:
                error(f"{where}: pivote de agarre sin socket de mano documentado (hand_r)")
            g = pivot.get("gripFromEndM")
            if not isinstance(g, (int, float)) or g <= 0 or (m is not None and g >= m):
                error(f"{where}: gripFromEndM debe estar dentro del mango (0 < g < size.m)")
            if pivot.get("end", "bottom") not in ("bottom", "top"):
                error(f"{where}: end debe ser bottom o top")
            if pivot.get("centerAt", "end") not in ("end", "grip"):
                error(f"{where}: centerAt debe ser end o grip")
        rc = e.get("recolor", {})
        targets = [r.get("to") for r in rc.get("rules", [])] + ([rc["default"]] if "default" in rc else [])
        if not targets:
            error(f"{where}: recolor sin reglas ni default")
        for t in targets:
            if samples and t not in samples:
                error(f"{where}: muestra de paleta «{t}» no existe en paleta.json")
        for r in rc.get("rules", []):
            if not HEX.match(str(r.get("from", ""))):
                error(f"{where}: color de origen «{r.get('from')}» no es #rrggbb en minúsculas")

    covered = {g for _, g in seen_game}
    all_ids = set().union(*ids.values())
    chosen = {(e.get("gameId"), e.get("pack"), e.get("file")) for e in catalog.get("entries", [])}
    pending_seen: set[str] = set()
    for key in ("discarded", "pending"):
        for d in catalog.get(key, []):
            gid = d.get("gameId", "")
            if not GAME_ID.match(gid):
                error(f"{CATALOG}: {key}: id «{gid}» no ASCII")
            elif "." in gid and gid not in ids["planta"]:
                error(f"{CATALOG}: {key}: «{gid}» no es una etapa de plants.json")
            if not d.get("reason"):
                error(f"{CATALOG}: {key}: «{gid}» sin motivo")
            if key == "discarded" and d.get("pack") not in known_packs:
                error(f"{CATALOG}: discarded: «{gid}» cita el pack «{d.get('pack')}», que no está en packs.json")
            if key == "discarded" and gid not in all_ids:
                error(f"{CATALOG}: discarded: «{gid}» no existe en los datos del juego")
            if key == "pending" and gid in covered:
                error(f"{CATALOG}: pending: «{gid}» ya está cubierto en entries")
            if key == "discarded" and (gid, d.get("pack"), d.get("file")) in chosen:
                error(f"{CATALOG}: discarded: «{gid}» descarta {d.get('file')}, que es el fichero de su entrada")
            if key == "pending":
                if gid in pending_seen:
                    error(f"{CATALOG}: pending: «{gid}» repetido")
                pending_seen.add(gid)
                if GAME_ID.match(gid) and "." not in gid and gid not in all_ids:
                    error(f"{CATALOG}: pending: «{gid}» no existe en los datos del juego")

    check_icons(repo_root, catalog, data, lotes, known_packs, usable, error)
