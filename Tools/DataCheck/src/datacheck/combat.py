"""Combate de H1 (biblia 05 §3 y §5): ``Content/Data/combat.json`` contra ``FCombatModel``.

- Constantes: cada número del JSON es el ``static constexpr`` del mismo nombre en
  ``Combat/CombatModel.h`` (multiplicadores, tiempos en ms, tramos del arco, bytes de red).
- Tramos del arco: empiezan en 0, son contiguos y la precisión no sube con la distancia.
- Animales: mismo orden y nombres que ``ECombatCreature``, iguales a la tabla
  ``CreatureTable`` de ``CombatModel.cpp``; el daño es el de la fórmula con su propiedad
  equivalente; lo contundente no corta. Los que también están en ``fauna.json`` (cerdo,
  cabra y cangrejo) coinciden allí en vida, ataque, daño, aturdimiento y huida.
"""

from __future__ import annotations

import math
import re

from . import mining

FILE = "combat.json"
MODEL_H = "Source/Explored/Combat/CombatModel.h"
MODEL_CPP = "Source/Explored/Combat/CombatModel.cpp"
KIND_OF_PROPERTY = {"Filo": "Cut", "Punta": "Pierce", "Contundente": "Blunt"}

# Clave del JSON → constante del C++.
CONSTANTS = {
    ("damage", "cutDamagePerPoint"): "CutDamagePerPoint",
    ("damage", "bluntDamagePerPoint"): "BluntDamagePerPoint",
    ("damage", "cutDepthDivisor"): "CutDepthDivisor",
    ("damage", "maxProperty"): "MaxProperty",
    ("damage", "bluntStunMinProperty"): "BluntStunMinProperty",
    ("damage", "bluntStunMs"): "BluntStunMs",
    ("melee", "quick", "multiplierPct"): "QuickMultiplierPct",
    ("melee", "quick", "executeMs"): "QuickExecuteMs",
    ("melee", "quick", "chainMax"): "QuickChainMax",
    ("melee", "quick", "chainPauseMs"): "ChainPauseMs",
    ("melee", "charged", "multiplierPct"): "ChargedMultiplierPct",
    ("melee", "charged", "telegraphMs"): "ChargeTelegraphMs",
    ("melee", "charged", "recoveryMs"): "ChargeRecoveryMs",
    ("melee", "reachM", "corto"): "ReachShortM",
    ("melee", "reachM", "lanza"): "ReachSpearM",
    ("dodge", "invulnerableMs"): "DodgeInvulnerableMs",
    ("dodge", "cooldownMs"): "DodgeCooldownMs",
}
BOW_BANDS = [("BowFullBandM", "BowFullAccuracyPct"), ("BowMidBandM", "BowMidAccuracyPct"),
             ("BowFarBandM", "BowFarAccuracyPct")]
NET_BYTES = {"accion": "ActionMsgBytes", "impacto": "ImpactMsgBytes", "estado": "NetStateBytes"}

CREATURE_ROW = re.compile(
    r'\{\s*TEXT\("(\w+)"\),\s*(\d+),\s*ECombatDamageKind::(\w+),\s*(\d+),\s*(\d+),\s*([\d.]+)f,\s*(\d+),'
    r'\s*([\d.]+)f,\s*(\d+),\s*(\d+)\s*\}')


def cpp_constants(ds) -> dict[str, float]:
    text = mining._read(ds, MODEL_H)
    return {name: float(value) for name, value in
            re.findall(r"static constexpr (?:int32|float) (\w+) = (-?[\d.]+)f?;", text)}


def cpp_creature_enum(ds) -> list[str]:
    text = mining._read(ds, MODEL_H)
    m = re.search(r"enum class ECombatCreature : uint8\s*\{(.*?)\};", text, re.S)
    if not m:
        return []
    body = re.sub(r"//[^\n]*", "", m.group(1))
    return [n for n in re.findall(r"(\w+)\s*,", body) if n != "Count"]


def cpp_creatures(ds) -> list[dict]:
    text = mining._read(ds, MODEL_CPP)
    m = re.search(r"CreatureTable\[\]\s*=\s*\{(.*?)\n\t\};", text, re.S)
    if not m:
        return []
    rows = []
    for row in CREATURE_ROW.findall(m.group(1)):
        rows.append({
            "id": row[0], "healthPoints": int(row[1]), "kind": row[2], "value": int(row[3]),
            "damage": int(row[4]), "cutDepth": float(row[5]), "stunMs": int(row[6]),
            "healthFraction": float(row[7]), "afterHits": int(row[8]), "capsizePct": int(row[9]),
        })
    return rows


def _get(doc: dict, path: tuple[str, ...]):
    for key in path:
        if not isinstance(doc, dict):
            return None
        doc = doc.get(key)
    return doc


def _num(v) -> bool:
    return isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v)


def _same(a, b) -> bool:
    return _num(a) and _num(b) and math.isclose(float(a), float(b), rel_tol=0, abs_tol=1e-6)


def formula_damage(prop: str, value) -> int | None:
    if prop not in KIND_OF_PROPERTY or not _num(value):
        return None
    return int(value) * (4 if prop == "Contundente" else 3)


def check_combat(ds, r) -> None:
    doc = ds.data.get(FILE)
    if not isinstance(doc, dict):
        return
    cpp = cpp_constants(ds)
    if not cpp:
        r.warn(f"{FILE}: no se leen las constantes de FCombatModel; no se compara con el C++")

    for path, name in CONSTANTS.items():
        value = _get(doc, path)
        if not _num(value):
            r.error(f"{FILE}/{'.'.join(path)}: falta o no es un número")
        elif name in cpp and not _same(value, cpp[name]):
            r.error(f"{FILE}/{'.'.join(path)}={value!r} pero FCombatModel::{name} = {cpp[name]:g}")

    _check_bow(doc, cpp, r)
    _check_creatures(ds, doc, r)

    messages = {m.get("id"): m for m in _get(doc, ("red", "mensajes")) or [] if isinstance(m, dict)}
    if _get(doc, ("red", "autoridad")) != "servidor":
        r.error(f"{FILE}/red: los impactos los resuelve el servidor (biblia 08 §1.2)")
    for mid, name in NET_BYTES.items():
        msg = messages.get(mid)
        if msg is None:
            r.error(f"{FILE}/red: falta el mensaje «{mid}»")
        elif name in cpp and not _same(msg.get("bytes"), cpp[name]):
            r.error(f"{FILE}/red «{mid}»: {msg.get('bytes')!r} bytes pero FCombatModel::{name} = {cpp[name]:g}")


def _check_bow(doc: dict, cpp: dict[str, float], r) -> None:
    bands = _get(doc, ("bow", "bands")) or []
    if len(bands) != len(BOW_BANDS):
        r.error(f"{FILE}/bow: deben ser {len(BOW_BANDS)} tramos (100/70/40 %) y luego beyondPct")
        return
    prev_to, prev_pct = 0, 101
    for band, (to_name, pct_name) in zip(bands, BOW_BANDS):
        frm, to, pct = band.get("fromM"), band.get("toM"), band.get("accuracyPct")
        if not (_num(frm) and _num(to) and _num(pct)):
            r.error(f"{FILE}/bow: tramo {band!r} incompleto")
            return
        if frm != prev_to or to <= frm:
            r.error(f"{FILE}/bow: el tramo [{frm}, {to}) no sigue al anterior (acaba en {prev_to})")
        if pct > prev_pct or not 0 <= pct <= 100:
            r.error(f"{FILE}/bow: la precisión {pct} % sube con la distancia o sale de 0-100")
        if to_name in cpp and not _same(to, cpp[to_name]):
            r.error(f"{FILE}/bow: tramo hasta {to} m pero FCombatModel::{to_name} = {cpp[to_name]:g}")
        if pct_name in cpp and not _same(pct, cpp[pct_name]):
            r.error(f"{FILE}/bow: {pct} % pero FCombatModel::{pct_name} = {cpp[pct_name]:g}")
        prev_to, prev_pct = to, pct
    beyond = _get(doc, ("bow", "beyondPct"))
    if beyond != 0:
        r.error(f"{FILE}/bow: beyondPct={beyond!r}; más allá del último tramo no se acierta (0 %)")


def _check_creatures(ds, doc: dict, r) -> None:
    creatures = doc.get("creatures") or []
    enum = cpp_creature_enum(ds)
    table = cpp_creatures(ds)
    fauna = {s.get("id"): s for s in ds.data.get("fauna.json", {}).get("species", [])}
    if enum and [c.get("cpp") for c in creatures] != enum:
        r.error(f"{FILE}/creatures: el orden de «cpp» debe ser el de ECombatCreature {enum}")
    if enum and len(table) != len(enum):
        r.error(f"{MODEL_CPP}: CreatureTable tiene {len(table)} fichas legibles y ECombatCreature {len(enum)}")
    seen: set[str] = set()
    for i, c in enumerate(creatures):
        cid = c.get("id")
        where = f"{FILE} «{cid}»"
        if cid in seen:
            r.error(f"{where}: id repetido")
        seen.add(cid)
        if not c.get("nameEs") or not c.get("nameEn"):
            r.error(f"{where}: falta nameEs o nameEn")
        attack = c.get("attack") or {}
        prop, value = attack.get("property"), attack.get("value")
        if prop not in KIND_OF_PROPERTY or not _num(value) or not 1 <= value <= 5:
            r.error(f"{where}: attack {attack!r} no es Filo, Punta o Contundente de 1 a 5 (biblia 05 §3.0)")
        elif formula_damage(prop, value) != c.get("damage"):
            r.error(f"{where}: damage={c.get('damage')!r} pero {prop} {value} da {formula_damage(prop, value)} "
                    "(× 3 cortante o perforante, × 4 contundente)")
        if not _num(c.get("healthPoints")) or c["healthPoints"] <= 0:
            r.error(f"{where}: healthPoints debe ser > 0")
        depth = c.get("cutDepth")
        if not _num(depth) or not 0 <= depth <= 1:
            r.error(f"{where}: cutDepth fuera de [0, 1]")
        elif prop == "Contundente" and depth != 0:
            r.error(f"{where}: un ataque contundente no abre corte (cutDepth debe ser 0)")
        if not isinstance(c.get("stunMs"), int) or c["stunMs"] < 0:
            r.error(f"{where}: stunMs debe ser un entero ≥ 0")
        flee = c.get("flee") or {}
        hf, hits = flee.get("healthFraction"), flee.get("afterHits")
        if not _num(hf) or not 0 <= hf <= 1 or not isinstance(hits, int) or hits < 0:
            r.error(f"{where}: flee.healthFraction en [0, 1] y flee.afterHits entero ≥ 0")
        if not isinstance(c.get("capsizePct"), int) or not 0 <= c["capsizePct"] <= 100:
            r.error(f"{where}: capsizePct entero de 0 a 100")

        if i < len(table):
            _compare_cpp(c, table[i], where, r)
        if cid in fauna:
            _compare_fauna(c, fauna[cid], where, r)
        elif c.get("cpp") != "ReefShark":
            r.error(f"{where}: animal terrestre que no está en fauna.json")


def _compare_cpp(c: dict, row: dict, where: str, r) -> None:
    attack = c.get("attack") or {}
    flee = c.get("flee") or {}
    pairs = [
        ("id", c.get("id"), row["id"]),
        ("healthPoints", c.get("healthPoints"), row["healthPoints"]),
        ("attack.property", KIND_OF_PROPERTY.get(attack.get("property")), row["kind"]),
        ("attack.value", attack.get("value"), row["value"]),
        ("damage", c.get("damage"), row["damage"]),
        ("cutDepth", c.get("cutDepth"), row["cutDepth"]),
        ("stunMs", c.get("stunMs"), row["stunMs"]),
        ("flee.healthFraction", flee.get("healthFraction"), row["healthFraction"]),
        ("flee.afterHits", flee.get("afterHits"), row["afterHits"]),
        ("capsizePct", c.get("capsizePct"), row["capsizePct"]),
    ]
    for key, json_value, cpp_value in pairs:
        same = json_value == cpp_value if isinstance(cpp_value, str) else _same(json_value, cpp_value)
        if not same:
            r.error(f"{where}: {key}={json_value!r} pero CreatureTable dice {cpp_value!r}")


def _compare_fauna(c: dict, s: dict, where: str, r) -> None:
    attack = s.get("attack") or {}
    equiv = attack.get("equiv") or {}
    flee = s.get("flee") or {}
    stun_s = attack.get("stunSeconds") or 0
    pairs = [
        ("nameEs", c.get("nameEs"), s.get("nameEs")),
        ("nameEn", c.get("nameEn"), s.get("nameEn")),
        ("healthPoints", c.get("healthPoints"), s.get("healthPoints")),
        ("attack", c.get("attack"), {"property": equiv.get("property"), "value": equiv.get("value")}),
        ("damage", c.get("damage"), attack.get("damage")),
        ("stunMs", c.get("stunMs"), round(stun_s * 1000) if _num(stun_s) else stun_s),
        ("flee.healthFraction", (c.get("flee") or {}).get("healthFraction"), flee.get("healthFraction") or 0),
        ("flee.afterHits", (c.get("flee") or {}).get("afterHits"), flee.get("afterHits") or 0),
    ]
    for key, mine, theirs in pairs:
        if mine != theirs:
            r.error(f"{where}: {key}={mine!r} pero fauna.json dice {theirs!r}")
