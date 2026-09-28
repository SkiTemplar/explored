"""Tests de fetch_packs.py sin red (fuente direct con file://) y del manifiesto real."""

from __future__ import annotations

import hashlib
import json
import zipfile
from pathlib import Path

import pytest

import fetch_packs as fp


def _zip(path: Path, files: dict[str, str]) -> str:
    with zipfile.ZipFile(path, "w") as z:
        for name, text in files.items():
            z.writestr(name, text)
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _pack(src: Path, sha: str) -> dict:
    return {"id": "demo", "source": {"kind": "direct", "url": src.as_uri()}, "sha256": sha}


def test_descarga_verifica_y_extrae(tmp_path: Path) -> None:
    src = tmp_path / "src.zip"
    sha = _zip(src, {"Models/a.glb": "x", "License.txt": "CC0"})
    out = fp.fetch(_pack(src, sha), tmp_path / "cache")
    assert (out / "Models" / "a.glb").read_text() == "x"
    assert (out / ".sha256").read_text().strip() == sha
    # Segunda vez: no vuelve a extraer si el sello coincide.
    (out / "Models" / "a.glb").write_text("tocado")
    fp.fetch(_pack(src, sha), tmp_path / "cache")
    assert (out / "Models" / "a.glb").read_text() == "tocado"


def test_sha_distinto_falla_y_borra(tmp_path: Path) -> None:
    src = tmp_path / "src.zip"
    _zip(src, {"a.txt": "x"})
    with pytest.raises(RuntimeError, match="sha256"):
        fp.fetch(_pack(src, "0" * 64), tmp_path / "cache")
    assert not (tmp_path / "cache" / "demo.zip").exists()


def test_sin_sha_exige_update(tmp_path: Path) -> None:
    src = tmp_path / "src.zip"
    sha = _zip(src, {"a.txt": "x"})
    pack = _pack(src, "")
    with pytest.raises(RuntimeError, match="update-sha"):
        fp.fetch(pack, tmp_path / "cache")
    fp.fetch(pack, tmp_path / "cache2", update_sha=True)
    assert pack["sha256"] == sha


def test_rechaza_rutas_fuera_del_zip(tmp_path: Path) -> None:
    src = tmp_path / "src.zip"
    sha = _zip(src, {"../fuera.txt": "x"})
    with pytest.raises(RuntimeError, match="fuera"):
        fp.fetch(_pack(src, sha), tmp_path / "cache")
    assert not (tmp_path / "cache" / "fuera.txt").exists()


def test_manifiesto_real_cc0_con_sha() -> None:
    """Los packs con un unico zip verificable (direct/itch, los que baja
    fetch_packs.py) son siempre CC0 con su sha256 real. Los packs "polypizza"
    (modelo a modelo, sin zip; ver download_polypizza.py) son una excepcion
    documentada: se verifican por autor+licencia scrapeados de cada pagina, no
    por hash, y pueden ser CC0 o CC-BY 3.0 (con atribucion, ver
    Tools/Packs/creditos_cc_by.md)."""
    manifest = fp.load_manifest()
    ids = [p["id"] for p in manifest["packs"]]
    assert len(ids) == len(set(ids))
    for p in manifest["packs"]:
        if p["source"]["kind"] == "polypizza":
            assert p["license"]["spdx"] in ("CC0-1.0", "CC-BY-3.0"), p["id"]
            assert p["sha256"].startswith("n/a"), p["id"]
            assert "note" in p["source"], p["id"]
            continue
        assert p["license"]["spdx"] == "CC0-1.0", p["id"]
        assert len(p["sha256"]) == 64, p["id"]
        assert p["source"]["kind"] in ("direct", "itch")


def test_cache_por_defecto_ignorada_en_git() -> None:
    gitignore = (fp.REPO_ROOT / ".gitignore").read_text(encoding="utf-8").splitlines()
    assert "Art/Packs/" in gitignore and "Art/Export/Packs/" in gitignore
    assert fp.cache_dir() == fp.REPO_ROOT / "Art" / "Packs" or "EXPLORED_PACKS_CACHE" in __import__("os").environ


def test_catalogo_usa_packs_del_manifiesto() -> None:
    catalog = json.loads((fp.REPO_ROOT / "Content" / "Data" / "packs_catalogo.json").read_text(encoding="utf-8"))
    ids = {p["id"] for p in fp.load_manifest()["packs"]}
    assert {e["pack"] for e in catalog["entries"]} <= ids
