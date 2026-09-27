"""Comprobaciones de Content/Data/music_layers.json (GDD §14.3) contra el director de música.

El JSON lo genera Tools/Audio (`explored-audio music-layers`) y sus tests comprueban que
coincide con las partituras. Aquí se mira el otro lado: lo que lee el C++
(`ExploredMusicSubsystem` y `FMusicDirectorModel`). El lector ignora sin avisar los papeles
y las islas que no conoce, y una variante de final mal escrita nunca se elegiría, así que
estas claves tienen que coincidir con las de `MusicDirectorModel.cpp`.
"""

from __future__ import annotations

import math
import re
from pathlib import Path

MODEL_CPP = Path("Source") / "Explored" / "Audio" / "MusicDirectorModel.cpp"
MODEL_H = Path("Source") / "Explored" / "Audio" / "MusicDirectorModel.h"
ID_RE = re.compile(r"^mus_[a-z0-9_]+$")
# Rango de tempo razonable para la banda sonora (lento de noche, rápido en el mar).
BPM_RANGE = (40.0, 200.0)
# «Motivo corto» del GDD §14.3: un descubrimiento no debería pasar de unos segundos.
DISCOVERY_MAX_S = 5.0
# GDD §2 y §14.3: el único final es la partida del «Limón».
GDD_FINALE = "voyage"


def _cpp_keys(text: str, array: str) -> list[str]:
    """Claves de ``const TCHAR* const <array>[...] = { TEXT("a"), ... };``."""
    m = re.search(rf"{array}\[[^\]]*\]\s*=\s*\{{(.*?)\}};", text, re.S)
    return re.findall(r'TEXT\("([^"]+)"\)', m.group(1)) if m else []


def _cpp_required_roles(text: str) -> list[str]:
    """Papeles que exige ``FMusicCatalog::Validate`` (``EMusicRole Required[]``)."""
    m = re.search(r"EMusicRole Required\[\]\s*=\s*\{(.*?)\};", text, re.S)
    return re.findall(r"EMusicRole::(\w+)", m.group(1)) if m else []


def _cpp_finale_keys(text: str) -> tuple[set[str], str | None]:
    """Claves de ``FinaleKey`` y la que devuelve por defecto."""
    m = re.search(r"FinaleKey\(EMusicFinale Finale\)\s*\{(.*?)\n\t\}", text, re.S)
    if not m:
        return set(), None
    body = m.group(1)
    keys = set(re.findall(r'return TEXT\("([^"]+)"\)', body))
    default = re.search(r'default: return TEXT\("([^"]+)"\)', body)
    return keys, default.group(1) if default else None


def _cpp_num_notes(text: str) -> int | None:
    m = re.search(r"static constexpr int32 NumNotes = (\d+);", text)
    return int(m.group(1)) if m else None


def _num(v) -> bool:
    return isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v)


def check_music(ds, r) -> None:
    doc = ds.data.get("music_layers.json")
    if not doc:
        return
    where = "music_layers.json"
    root = ds.repo_root
    cpp = (root / MODEL_CPP).read_text(encoding="utf-8") if (root / MODEL_CPP).exists() else ""
    header = (root / MODEL_H).read_text(encoding="utf-8") if (root / MODEL_H).exists() else ""
    role_keys = _cpp_keys(cpp, "RoleKeys")
    island_keys = _cpp_keys(cpp, "IslandKeys")
    finale_keys, finale_default = _cpp_finale_keys(cpp)
    if not role_keys or not island_keys:
        r.warn(f"{where}: no se encuentran RoleKeys/IslandKeys en {MODEL_CPP.as_posix()}; no se compara con el C++")

    if not isinstance(doc.get("version"), int) or isinstance(doc.get("version"), bool):
        r.error(f"{where}: «version» debe ser un entero")
    pieces = doc.get("pieces")
    if not isinstance(pieces, list) or not pieces:
        r.error(f"{where}: «pieces» vacío o ausente")
        return

    ids: list[str] = []
    by_id: dict[str, dict] = {}
    for p in pieces:
        pid = p.get("id")
        if not isinstance(pid, str) or not ID_RE.match(pid):
            r.error(f"{where}: id de pieza inválido {pid!r} (mus_ + minúsculas ASCII)")
            continue
        ids.append(pid)
        by_id[pid] = p
        tag = f"{where} «{pid}»"
        role = p.get("role")
        if role_keys and role not in role_keys:
            r.error(f"{tag}: papel «{role}» desconocido para EMusicRole {role_keys}; el juego lo ignoraría")
        if not isinstance(p.get("variant", ""), str):
            r.error(f"{tag}: «variant» debe ser texto")
        asset = f"/Game/Generated/Audio/Musica/{pid}.{pid}"
        if p.get("asset") != asset:
            r.error(f"{tag}: asset {p.get('asset')!r}, se esperaba {asset!r}")
        bpm, beats, bars = p.get("bpm"), p.get("beats_per_bar"), p.get("bars")
        if not _num(bpm) or not BPM_RANGE[0] <= bpm <= BPM_RANGE[1]:
            r.error(f"{tag}: bpm {bpm!r} fuera de {BPM_RANGE[0]:g}-{BPM_RANGE[1]:g}")
            continue
        if not isinstance(beats, int) or isinstance(beats, bool) or beats <= 0:
            r.error(f"{tag}: beats_per_bar {beats!r} debe ser un entero > 0")
            continue
        if not _num(bars) or bars <= 0:
            r.error(f"{tag}: bars {bars!r} debe ser > 0")
            continue
        if not isinstance(p.get("loop"), bool):
            r.error(f"{tag}: «loop» debe ser booleano")
        elif p["loop"] and not float(bars).is_integer():
            r.error(f"{tag}: bucle con {bars} compases; el director cuantiza al compás y necesita compases enteros")
        bar_s = 60.0 / bpm * beats
        if not _num(p.get("seconds_per_bar")) or abs(p["seconds_per_bar"] - bar_s) > 1e-3:
            r.error(f"{tag}: seconds_per_bar {p.get('seconds_per_bar')!r} no es 60/bpm×beats = {bar_s:.6f}")
        if not _num(p.get("duration_s")) or abs(p["duration_s"] - bar_s * bars) > 1e-2:
            r.error(f"{tag}: duration_s {p.get('duration_s')!r} no es compases×seconds_per_bar = {bar_s * bars:.6f}")
        if role == "discovery" and bar_s * bars > DISCOVERY_MAX_S:
            r.error(f"{tag}: motivo de descubrimiento de {bar_s * bars:.1f} s; el GDD §14.3 pide un motivo corto "
                    f"(≤ {DISCOVERY_MAX_S:g} s)")
    for dup in sorted({i for i in ids if ids.count(i) > 1}):
        r.error(f"{where}: id de pieza duplicado «{dup}»")

    # Papeles que exige FMusicCatalog::Validate: sin ellos el catálogo no carga.
    roles_present = {p.get("role") for p in by_id.values()}
    for enum_name in _cpp_required_roles(cpp):
        key = enum_name.lower()
        if key not in roles_present:
            r.error(f"{where}: falta una pieza con papel «{key}» (lo exige FMusicCatalog::Validate)")

    # Exploración diurna: una pieza por isla y las variaciones del día.
    explore = [p for p in by_id.values() if p.get("role") == "explore"]
    explore_vars = [p.get("variant") for p in explore]
    if island_keys and sorted(explore_vars) != sorted(island_keys):
        r.error(f"{where}: las piezas de exploración deben cubrir una vez cada isla {island_keys}, "
                f"hay {sorted(explore_vars)}")
    days = doc.get("day_variants")
    if not isinstance(days, dict):
        r.error(f"{where}: «day_variants» debe ser un objeto isla → piezas")
        days = {}
    if island_keys and set(days) != set(island_keys):
        r.error(f"{where}: day_variants debe tener exactamente las islas {island_keys}, tiene {sorted(days)}")
    own = {p.get("variant"): p.get("id") for p in explore}
    for island, lst in days.items():
        if not isinstance(lst, list) or not lst:
            r.error(f"{where}: day_variants «{island}» vacío")
            continue
        if own.get(island) and lst[0] != own[island]:
            r.error(f"{where}: day_variants «{island}» debe empezar por su propia pieza «{own[island]}»")
        if len(set(lst)) != len(lst):
            r.error(f"{where}: day_variants «{island}» repite piezas")
        for pid in lst:
            if pid not in by_id:
                r.error(f"{where}: day_variants «{island}» apunta a «{pid}», que no existe")
            elif by_id[pid].get("role") != "explore":
                r.error(f"{where}: day_variants «{island}» incluye «{pid}», que no es de exploración")

    # Finales: la variante tiene que ser una clave de FinaleKey o nunca sonaría.
    finales = [p for p in by_id.values() if p.get("role") == "finale"]
    finale_vars = {p.get("variant") for p in finales}
    if finale_keys:
        for p in finales:
            if p.get("variant") not in finale_keys:
                r.error(f"{where} «{p.get('id')}»: final «{p.get('variant')}» desconocido para "
                        f"EMusicFinale {sorted(finale_keys)}; nunca se elegiría")
    if finale_default and finale_default not in finale_vars:
        r.error(f"{where}: falta el final por defecto «{finale_default}» (partida del «Limón»)")
    extra = sorted(v for v in finale_vars if v != GDD_FINALE)
    if extra:
        r.info.append(f"{where}: finales de música fuera del GDD §2/§14.3 (solo la partida del «Limón»): "
                      f"{', '.join(extra)}")

    # Flauta de bambú (§8.12).
    flute = doc.get("flute")
    if not isinstance(flute, dict):
        r.error(f"{where}: falta la sección «flute»")
        return
    sample = flute.get("sample")
    if not isinstance(sample, str) or not re.fullmatch(r"sfx_[a-z0-9_]+", sample):
        r.error(f"{where}: flute.sample inválido {sample!r}")
    elif flute.get("asset") != f"/Game/Generated/Audio/Efectos/{sample}.{sample}":
        r.error(f"{where}: flute.asset {flute.get('asset')!r} no apunta a la muestra «{sample}»")
    if not _num(flute.get("sample_hz")) or not 50.0 <= flute["sample_hz"] <= 2000.0:
        r.error(f"{where}: flute.sample_hz {flute.get('sample_hz')!r} fuera de 50-2000 Hz")
    semis = flute.get("semitones")
    notes = _cpp_num_notes(header)
    if not isinstance(semis, list) or not all(isinstance(s, int) and not isinstance(s, bool) for s in semis):
        r.error(f"{where}: flute.semitones debe ser una lista de enteros")
    else:
        if notes is not None and len(semis) != notes:
            r.error(f"{where}: flute.semitones tiene {len(semis)} notas; FFluteModel::NumNotes = {notes}")
        if semis != sorted(set(semis)) or any(not 0 <= s <= 12 for s in semis):
            r.error(f"{where}: flute.semitones debe ir en orden creciente, sin repetir, dentro de una octava (0-12)")
