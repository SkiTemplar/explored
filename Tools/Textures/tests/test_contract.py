"""Contrato con Tools/Unreal/build_materials.py: toda textura que cargan los materiales
tiene que salir del generador con ese nombre exacto (si no, el import falla en el editor)."""

import re
from pathlib import Path

from texgen.legacy import LEGACY_NAMES
from texgen.materials import MATERIALS
from texgen.output import texture_name

BUILD_MATERIALS = Path(__file__).resolve().parents[2] / "Unreal" / "build_materials.py"


def generated_names() -> set[str]:
    names = set(LEGACY_NAMES)
    for name, spec in MATERIALS.items():
        names.update(texture_name(name, suffix) for suffix in spec.outputs)
    return names


def test_build_materials_only_uses_generated_textures():
    used = set(re.findall(r"/Game/Generated/Textures/(T_\w+)", BUILD_MATERIALS.read_text(encoding="utf-8")))
    assert used, "build_materials.py ya no carga texturas generadas: revisa este test"
    missing = used - generated_names()
    assert not missing, f"build_materials.py usa texturas que no se generan: {sorted(missing)}"


def test_texture_names_are_unique_and_prefixed():
    names = [texture_name(n, s) for n, spec in MATERIALS.items() for s in spec.outputs]
    assert len(names) == len(set(names))
    assert all(re.fullmatch(r"T_[A-Z][A-Za-z]+_(BC|N|ARH|M)", n) for n in names)
    assert not set(names) & set(LEGACY_NAMES)
