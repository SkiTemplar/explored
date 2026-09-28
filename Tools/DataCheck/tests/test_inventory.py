"""Pilas y tabla de ids de red: espejos del C++ y regresiones típicas en los datos."""

from __future__ import annotations

import shutil

import pytest

from datacheck import inventory
from datacheck.checks import DataSet, Report, run_all


@pytest.fixture(scope="module")
def real() -> DataSet:
    return DataSet.load()


@pytest.fixture
def ds(real: DataSet) -> DataSet:
    return real.copy()


def item(ds: DataSet, iid: str) -> dict:
    return next(i for i in ds.items if i["id"] == iid)


def inventory_report(ds: DataSet) -> Report:
    r = Report()
    inventory.check_inventory(ds, r)
    return r


def fake_repo(tmp_path, real: DataSet):
    """Copia de los ficheros de C++ que lee el espejo, para poder romperlos."""
    for rel in (inventory.INVENTORY_H, inventory.INVENTORY_CPP, inventory.CONTENT_IDS_CPP):
        dest = tmp_path / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(real.repo_root / rel, dest)
    (tmp_path / "Content" / "Data").mkdir(parents=True)
    return tmp_path


# --------------------------------------------------------------------------- hash

def test_fnv1a64_reference_values():
    assert inventory.fnv1a64(b"") == 0xCBF29CE484222325
    assert inventory.fnv1a64(b"a") == 0xAF63DC4C8601EC8C
    assert inventory.fnv1a64(b"foobar") == 0x85944171F73967E8


def test_content_hash_matches_cpp_spec():
    # El mismo valor está en Source/Explored/Tests/ContentIdTableModelSpec.cpp.
    assert inventory.content_hash({"a.json": b"[]", "b.json": b"{}"}) == 0x2867C518F7EAF856
    assert inventory.content_hash({"b.json": b"{}", "a.json": b"[]"}) == 0x2867C518F7EAF856


def test_content_hash_sees_moved_bytes_and_renames():
    base = inventory.content_hash({"a.json": b"[]", "b.json": b"{}"})
    assert inventory.content_hash({"a.json": b"[]{", "b.json": b"}"}) != base
    assert inventory.content_hash({"a.json": b"[]", "c.json": b"{}"}) != base
    assert inventory.content_hash({"a.json": b"[]", "b.json": b"{} "}) != base


def test_repo_hash_is_stable(real: DataSet):
    assert inventory.content_hash_of_repo(real.repo_root) == inventory.content_hash_of_repo(real.repo_root)


# --------------------------------------------------------------------------- espejos

def test_cpp_mirrors_are_found(real: DataSet):
    assert inventory.cpp_max_stack(real.repo_root) == inventory.BIBLIA_STACK
    tags = inventory.cpp_non_stackable_tags(real.repo_root)
    assert tags and {"mochila", "cinturon", "angarillas", "contenedor"} <= set(tags)
    assert inventory.cpp_content_tables(real.repo_root) == inventory.CONTENT_TABLES
    equipment = inventory.cpp_equipment_ids(real.repo_root)
    assert equipment["mochila"] == "mochila" and "mochila_fibra" in equipment


def test_real_data_passes(real: DataSet):
    r = inventory_report(real)
    assert r.errors == []
    assert any("Tabla de ids de red" in m for m in r.info)
    assert not any("inventario" in e.lower() or "pila" in e.lower() for e in run_all(real).errors)


@pytest.mark.parametrize("iid,expected", [
    ("rama_seca", 10),   # recurso en bruto
    ("coco_maduro", 10),  # comida
    ("flecha", 10),       # arma sin durabilidad: apila
    ("cuchillo", 1),      # durabilidad
    ("cantimplora", 1),   # líquido
    ("bambu_fino", 1),    # Recipiente 1: guarda agua (biblia 03 §1.3)
    ("tronco_pequeno", 1),  # DosManos
    ("mochila", 1),       # equipo sin durabilidad
    ("bolsa_impermeable", 1),  # contenedor
])
def test_stack_rule_on_real_items(real: DataSet, iid: str, expected: int):
    tags = inventory.cpp_non_stackable_tags(real.repo_root)
    assert inventory.max_stack(item(real, iid), tags) == expected


# --------------------------------------------------------------------------- regresiones

def test_equipment_without_its_tag_is_an_error(ds: DataSet):
    item(ds, "mochila")["tags"] = ["rescatado"]
    errors = inventory_report(ds).errors
    assert any("«mochila»" in e and "etiqueta «mochila»" in e for e in errors)
    assert any("«mochila»" in e and "apilaría" in e for e in errors)


def test_bad_or_duplicate_ids_break_the_net_table(ds: DataSet):
    ds.templates.append(dict(ds.templates[0]))
    ds.building["pieces"].append({"id": "Suelo Nuevo"})
    errors = inventory_report(ds).errors
    assert any(e.startswith("templates.json") and "repetido" in e for e in errors)
    assert any(e.startswith("building_pieces.json") and "Suelo Nuevo" in e for e in errors)


def test_too_many_ids_for_uint16(ds: DataSet):
    ds.data["boats.json"]["boats"] = [{"id": f"barco_{n}"} for n in range(inventory.UINT16_IDS + 1)]
    assert any("uint16" in e for e in inventory_report(ds).errors)


def test_cpp_stack_size_must_match_the_bible(tmp_path, real: DataSet):
    root = fake_repo(tmp_path, real)
    header = root / inventory.INVENTORY_H
    header.write_text(header.read_text(encoding="utf-8").replace("MaxStackSize = 10;", "MaxStackSize = 12;"), encoding="utf-8")
    errors = inventory_report(DataSet(real.data, root)).errors
    assert any("MaxStackSize=12" in e for e in errors)


def test_missing_mirrors_are_errors(tmp_path, real: DataSet):
    root = fake_repo(tmp_path, real)
    cpp = root / inventory.INVENTORY_CPP
    cpp.write_text(cpp.read_text(encoding="utf-8").replace("NonStackableTagNames", "OtroNombre"), encoding="utf-8")
    (root / inventory.CONTENT_IDS_CPP).unlink()
    errors = inventory_report(DataSet(real.data, root)).errors
    assert any("NonStackableTagNames" in e for e in errors)
    assert any("EContentKind" in e for e in errors)


def test_content_table_order_must_match_cpp(tmp_path, real: DataSet):
    root = fake_repo(tmp_path, real)
    cpp = root / inventory.CONTENT_IDS_CPP
    text = cpp.read_text(encoding="utf-8")
    text = text.replace('return TEXT("plants.json")', 'return TEXT("TMP")')
    text = text.replace('return TEXT("boats.json")', 'return TEXT("plants.json")').replace('return TEXT("TMP")', 'return TEXT("boats.json")')
    cpp.write_text(text, encoding="utf-8")
    assert any("no coinciden" in e for e in inventory_report(DataSet(real.data, root)).errors)
