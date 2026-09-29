"""Tests de contact_sheet.py con viñetas sintéticas en tmp_path (sin Blender)."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import pytest
from PIL import Image

import contact_sheet as cs


def _entry(game_id: str, rig: Any = None) -> dict[str, Any]:
    e: dict[str, Any] = {"gameId": game_id, "lote": "lote-x", "pack": "demo", "file": f"Models/{game_id}.glb",
                         "mesh": f"SM_Pack_{game_id.title()}"}
    if rig is not None:
        e["rig"] = rig
    return e


@pytest.fixture
def repo(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    """Repositorio falso: catálogo, report.json y viñetas de colores planos."""
    entries = [
        _entry("hacha"),
        _entry("pala", rig=None),
        _entry("cerdo", rig={"animations": ["Idle", "Run"],
                             "tilePoses": [{"action": "Idle", "frame": 0}, {"action": "Run", "frame": 4}]}),
    ]
    entries[1]["rig"] = None  # rig: null en el JSON no debe romper la hoja
    catalog = {"lotes": [{"id": "lote-x", "date": "2026-09-28", "scope": "Prueba"}],
               "entries": [*entries, {**_entry("otro"), "lote": "lote-y"}]}
    cat = tmp_path / "Content" / "Data" / "packs_catalogo.json"
    cat.parent.mkdir(parents=True)
    cat.write_text(json.dumps(catalog), encoding="utf-8")
    tiles = tmp_path / "Art" / "Export" / "Packs" / "lote-x" / "_tiles"
    tiles.mkdir(parents=True)
    names = ["hacha", "pala", "cerdo", "cerdo@Run"]
    for i, name in enumerate(names):
        Image.new("RGB", (40, 20), (40 * i, 100, 200 - 40 * i)).save(tiles / f"{name}.png")
    report = {g: {"mesh": "SM", "dims": [0.1, 0.2, 0.55], "tris": 120} for g in ("hacha", "pala", "cerdo")}
    (tiles / "report.json").write_text(json.dumps(report), encoding="utf-8")
    monkeypatch.setattr(cs, "REPO", tmp_path)
    monkeypatch.setattr(cs, "CATALOG", cat)
    monkeypatch.setattr(cs, "TILES", tmp_path / "Art" / "Export" / "Packs")
    return tmp_path


def test_compone_la_hoja_con_una_celda_por_viñeta(repo: Path) -> None:
    out = cs.build("lote-x")
    assert out == repo / "docs" / "art" / "packs" / "lote-x.png"
    with Image.open(out) as img:
        # 4 celdas (3 entradas + 1 pose extra) en 3 columnas → 2 filas.
        assert img.size == (cs.COLS * 40, 64 + 2 * (20 + cs.LABEL_H))
        assert img.mode == "P"  # paleta indexada para no pasar de 1 MB
        rgb = img.convert("RGB")
        # La viñeta de la pose extra (cerdo@Run) está en la segunda fila, primera columna.
        assert rgb.getpixel((5, 64 + 20 + cs.LABEL_H + 5)) == (120, 100, 80)


def test_main_escribe_e_informa(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    cs.main(["lote-x"])
    assert "docs/art/packs/lote-x.png" in capsys.readouterr().out.replace("\\", "/")


def test_main_sin_argumento_muestra_uso(capsys: pytest.CaptureFixture[str]) -> None:
    with pytest.raises(SystemExit) as exc:
        cs.main([])
    assert exc.value.code == 1
    assert "Uso" in capsys.readouterr().out


def test_lote_desconocido_o_vacio(repo: Path) -> None:
    with pytest.raises(SystemExit, match="desconocido"):
        cs.build("lote-z")
    cat = json.loads(cs.CATALOG.read_text(encoding="utf-8"))
    cat["lotes"].append({"id": "vacio", "date": "-", "scope": "-"})
    cs.CATALOG.write_text(json.dumps(cat), encoding="utf-8")
    with pytest.raises(SystemExit, match="no tiene entradas"):
        cs.build("vacio")


def test_sin_report_o_entrada_ausente_explica_que_falta(repo: Path) -> None:
    report_path = cs.TILES / "lote-x" / "_tiles" / "report.json"
    report = json.loads(report_path.read_text(encoding="utf-8"))
    del report["pala"]
    report_path.write_text(json.dumps(report), encoding="utf-8")
    with pytest.raises(SystemExit, match="pala"):
        cs.build("lote-x")
    report_path.unlink()
    with pytest.raises(SystemExit, match="normalize.py --tiles"):
        cs.build("lote-x")


def test_hoja_de_mas_de_un_mega_falla(repo: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(cs, "MAX_BYTES", 10)
    with pytest.raises(SystemExit, match="1 MB"):
        cs.build("lote-x")


def test_fuente_de_respaldo(monkeypatch: pytest.MonkeyPatch) -> None:
    real = cs.ImageFont.truetype
    tried: list[object] = []

    def no_dejavu(font: object = None, *a: Any, **k: Any) -> Any:
        # Sin DejaVu en el sistema; la fuente interna de Pillow (BytesIO) sí carga.
        if isinstance(font, str):
            tried.append(font)
            raise OSError(font)
        return real(font, *a, **k)  # pyright: ignore[reportArgumentType] -- reenvío tal cual

    monkeypatch.setattr(cs.ImageFont, "truetype", no_dejavu)
    assert cs.font(12) is not None
    assert len(tried) == 2
