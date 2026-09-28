"""Supervivencia: medicinas y avisos interiores de survival_needs.json y sus tablas C++.

Los modelos puros (``FMedicineModel``, ``FInnerVoiceModel``) no leen JSON: sus tablas se
generan con ``uv run datacheck --write-survival`` en ``MedicineData.inl`` e
``InnerVoiceData.inl``. La comprobación falla si un ``.inl`` no coincide con los datos,
así que survival_needs.json es la única fuente de verdad.
"""

from __future__ import annotations

import re
from pathlib import Path

from .cooking import _f, _name, _s

MEDICINE_INL = Path("Source/Explored/Survival/MedicineData.inl")
INNER_VOICE_INL = Path("Source/Explored/Survival/InnerVoiceData.inl")
WRITE_HINT = "ejecuta «uv run datacheck --write-survival» en Tools/DataCheck"
MEDICINE_TAG = "medicinal"


def _header(what: str) -> list[str]:
    return [
        f"// Generado por Tools/DataCheck (uv run datacheck --write-survival) desde Content/Data/survival_needs.json ({what}).",
        "// No editar a mano: se incluye dentro de una función de rellenado con el parámetro D.",
    ]


def render_medicines_inl(doc: dict) -> str:
    out = _header("medicines")
    for med in doc.get("medicines", []):
        out += ["{", "\tFMedicineDef& M = D.AddDefaulted_GetRef();", f"\tM.ItemId = {_name(med.get('item'))};"]
        for cond in med.get("cures", []):
            out.append(f"\tM.Effects.Cures |= SurvivalCureBit(ECondition::{cond});")
        if med.get("healing"):
            out.append(f"\tM.Effects.Healing = {_f(med['healing'])};")
        if med.get("water"):
            out.append(f"\tM.Effects.Water = {_f(med['water'])};")
        if med.get("treatment"):
            out += ["\tM.bTreatsWounds = true;", f"\tM.Treatment = EWoundTreatment::{med['treatment']};"]
        out.append("}")
    return "\n".join(out) + "\n"


def render_inner_voice_inl(doc: dict) -> str:
    out = _header("innerVoice")
    for line in doc.get("innerVoice", []):
        out += [
            "{",
            f"\tFInnerVoiceText& T = D[static_cast<int32>(EInnerVoiceLine::{line.get('line')})];",
            f"\tT.Id = {_name(line.get('id'))};",
            f"\tT.TextEs = TEXT({_s(line.get('textEs', ''))});",
            f"\tT.TextEn = TEXT({_s(line.get('textEn', ''))});",
            "}",
        ]
    return "\n".join(out) + "\n"


def generated_files(data: dict[str, object]) -> dict[Path, str]:
    doc = data.get("survival_needs.json")
    if not isinstance(doc, dict):
        return {}
    return {MEDICINE_INL: render_medicines_inl(doc), INNER_VOICE_INL: render_inner_voice_inl(doc)}


def write_generated(repo_root: Path, data: dict[str, object]) -> list[Path]:
    written = []
    for rel, text in generated_files(data).items():
        path = repo_root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)
        written.append(rel)
    return written


def check_generated(repo_root: Path, data: dict[str, object], error) -> None:
    for rel, expected in generated_files(data).items():
        path = repo_root / rel
        current = path.read_text(encoding="utf-8") if path.exists() else None
        if current != expected:
            error(f"{rel.as_posix()} no coincide con los datos: {WRITE_HINT}")


# --------------------------------------------------------------------------- comprobaciones


def cpp_enum(text: str, name: str) -> list[str]:
    """Enumeradores de ``enum class <name> : uint8`` sin Count, ignorando comentarios."""
    m = re.search(rf"enum class {name}\s*:\s*uint8\s*\{{(.*?)\}};", text, re.S)
    if not m:
        return []
    body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
    body = re.sub(r"//[^\n]*", "", body)
    names = [re.split(r"[\s=]", v.strip())[0] for v in body.split(",")]
    return [n for n in names if n and n != "Count"]


def _num(value) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def check_medicines(doc: dict, items: list[dict], conditions: list[str], treatments: list[str], error) -> None:
    by_id = {i.get("id"): i for i in items}
    seen: set[str] = set()
    for med in doc.get("medicines", []):
        iid = med.get("item")
        where = f"survival_needs.json medicines «{iid}»"
        if iid in seen:
            error(f"{where}: medicina repetida")
        seen.add(iid)
        item = by_id.get(iid)
        if item is None:
            error(f"{where}: no existe en items.json")
        elif MEDICINE_TAG not in item.get("tags", []):
            error(f"{where}: el objeto no lleva la etiqueta «{MEDICINE_TAG}»")
        cures = med.get("cures", [])
        if not isinstance(cures, list):
            error(f"{where}: cures debe ser una lista")
            cures = []
        for cond in cures:
            if conditions and cond not in conditions:
                error(f"{where}: cura «{cond}», que no es un ECondition ({', '.join(conditions)})")
        if len(set(cures)) != len(cures):
            error(f"{where}: cures repite un estado")
        treatment = med.get("treatment")
        if treatment is not None and treatments and treatment not in treatments:
            error(f"{where}: treatment «{treatment}» no es un EWoundTreatment ({', '.join(treatments)})")
        for key in ("healing", "water"):
            value = med.get(key, 0)
            if not _num(value) or not 0 <= value <= 100:
                error(f"{where}: {key}={value!r} fuera de [0, 100]")
        if not cures and treatment is None and not med.get("healing"):
            error(f"{where}: no cura nada, no trata heridas ni da salud")


def check_inner_voice(doc: dict, lines: list[str], error) -> None:
    ids: set[str] = set()
    used: dict[str, str] = {}
    for entry in doc.get("innerVoice", []):
        vid = entry.get("id")
        where = f"survival_needs.json innerVoice «{vid}»"
        if not isinstance(vid, str) or not re.fullmatch(r"[a-z0-9_]+", vid):
            error(f"survival_needs.json innerVoice: id inválido {vid!r}")
        elif vid in ids:
            error(f"{where}: id repetido")
        ids.add(vid)
        line = entry.get("line")
        if lines and line not in lines:
            error(f"{where}: line «{line}» no es un EInnerVoiceLine")
        elif line in used:
            error(f"{where}: EInnerVoiceLine::{line} ya lo usa «{used[line]}»")
        used[line] = vid
        for key in ("textEs", "textEn", "trigger"):
            if not isinstance(entry.get(key), str) or not entry.get(key).strip():
                error(f"{where}: falta {key}")
    for line in lines:
        if line not in used:
            error(f"survival_needs.json innerVoice: EInnerVoiceLine::{line} no tiene texto")


def check_survival_data(repo_root: Path, data: dict[str, object], error) -> None:
    doc = data.get("survival_needs.json")
    if not isinstance(doc, dict):
        return
    src = repo_root / "Source" / "Explored" / "Survival"

    def read(name: str) -> str:
        path = src / name
        return path.read_text(encoding="utf-8") if path.exists() else ""

    survival_h, body_h, voice_h = read("SurvivalModel.h"), read("BodyModel.h"), read("InnerVoiceModel.h")
    check_medicines(doc, data.get("items.json", []), cpp_enum(survival_h, "ECondition"),
                    cpp_enum(body_h, "EWoundTreatment"), error)
    check_inner_voice(doc, cpp_enum(voice_h, "EInnerVoiceLine"), error)

    wet = doc.get("wetness")
    if wet is not None:
        init = wet.get("initial")
        if not _num(init) or not 0 <= init <= 1:
            error(f"survival_needs.json wetness.initial={init!r} fuera de [0, 1]")
        m = re.search(r"float Wetness = ([0-9.]+)f;", survival_h)
        if m is None:
            error("survival_needs.json wetness: FSurvivalState no tiene el campo Wetness")
        elif float(m.group(1)) != init:
            error(f"survival_needs.json wetness.initial={init} pero SurvivalModel.h dice {m.group(1)}")
    check_generated(repo_root, data, error)
