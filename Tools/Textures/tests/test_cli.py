"""CLI de gen_textures.py y gen_palette.py: siempre escriben en tmp_path, nunca en el repo."""

import json
import subprocess
import sys
from pathlib import Path

import numpy as np
import pytest
from PIL import Image

import gen_palette
import gen_textures
from texgen import palette_sheet
from texgen.legacy import LEGACY_NAMES, generate_legacy
from texgen.materials import MATERIALS, generate
from texgen.palette import ISLANDS, PALETTE_TEXTURES, harmonize_albedo

HERE = Path(__file__).resolve().parents[1]
ROOT = HERE.parents[1]


def _run(tmp_path, *args):
    gen_textures.main(["--size", "64", "--out", str(tmp_path), "--no-legacy", "--no-palette", *args])


# --- gen_textures.py ------------------------------------------------------------------

@pytest.mark.parametrize("size", ["48", "100", "32", "0", "-64"])
def test_rechaza_tamanos_que_no_son_potencia_de_2_o_muy_pequenos(tmp_path, size, capsys):
    with pytest.raises(SystemExit) as exc:
        gen_textures.main(["--size", size, "--out", str(tmp_path)])
    assert exc.value.code == 2
    assert "potencia de 2" in capsys.readouterr().err
    assert not list(tmp_path.iterdir()), "no escribe nada si los argumentos son malos"


def test_rechaza_materiales_desconocidos(tmp_path, capsys):
    with pytest.raises(SystemExit) as exc:
        gen_textures.main(["--size", "64", "--only", "Rope", "Lava", "--out", str(tmp_path)])
    assert exc.value.code == 2
    err = capsys.readouterr().err
    assert "Lava" in err and "Rope" not in err.split("hay")[0]
    assert not list(tmp_path.iterdir())


def test_semilla_global_es_determinista_y_cambia_el_resultado(tmp_path):
    a, b, c = tmp_path / "a", tmp_path / "b", tmp_path / "c"
    _run(a, "--only", "Rope", "--seed", "5")
    _run(b, "--only", "Rope", "--seed", "5")
    _run(c, "--only", "Rope")
    for suffix in ("BC", "N", "ARH"):
        name = f"T_Rope_{suffix}.png"
        assert (a / name).read_bytes() == (b / name).read_bytes(), name
    assert (a / "T_Rope_BC.png").read_bytes() != (c / "T_Rope_BC.png").read_bytes()


def test_semilla_cero_no_se_confunde_con_la_de_por_defecto(tmp_path):
    _run(tmp_path, "--only", "Canvas", "--seed", "0")
    got = np.asarray(Image.open(tmp_path / "T_Canvas_BC.png"), dtype=np.float64) / 255.0
    np.testing.assert_allclose(got, generate("Canvas", 64, 0)["BC"], atol=1 / 255 + 1e-9)


def test_no_palette_no_escribe_atlas_pero_los_declara(tmp_path):
    _run(tmp_path, "--only", "Canvas")
    assert not list(tmp_path.glob("T_Palette_*.png"))
    assert not list(tmp_path.glob("T_Terrain*.png")), "--no-legacy tampoco escribe legado"
    manifest = json.loads((tmp_path / "textures.json").read_text(encoding="utf-8"))
    for name in PALETTE_TEXTURES:
        assert manifest[name] == {"kind": "palette", "srgb": True}
    # Cada textura que se puede generar está en el manifiesto, y nada más.
    expected = set(LEGACY_NAMES) | set(PALETTE_TEXTURES) | {
        f"T_{n}_{s}" for n, spec in MATERIALS.items() for s in spec.outputs}
    assert set(manifest) == expected


def test_escribe_las_texturas_legado(tmp_path, monkeypatch, capsys):
    """Sin --no-legacy escribe las cinco texturas legado (aquí a 64 px para no tardar)."""
    monkeypatch.setattr(gen_textures, "generate_legacy", lambda: generate_legacy(64))
    gen_textures.main(["--size", "64", "--only", "Rope", "--out", str(tmp_path), "--no-palette"])
    out = capsys.readouterr().out
    for name in LEGACY_NAMES:
        assert Image.open(tmp_path / f"{name}.png").size == (64, 64)
        assert f"{tmp_path / name}.png" in out
    manifest = json.loads((tmp_path / "textures.json").read_text(encoding="utf-8"))
    assert manifest["T_TerrainNormal"] == {"kind": "normal", "srgb": False}


def test_crea_la_carpeta_de_salida(tmp_path):
    out = tmp_path / "no" / "existe"
    _run(out, "--only", "SeaFoam")
    assert Image.open(out / "T_SeaFoam_M.png").mode == "RGBA"


@pytest.mark.parametrize("script", ["gen_textures.py", "gen_palette.py"])
def test_el_uso_antiguo_desde_la_raiz_sigue_arrancando(script):
    """`python Tools/Textures/<script>` desde la raíz del repo: los imports de texgen resuelven."""
    res = subprocess.run([sys.executable, f"Tools/Textures/{script}", "--help"], cwd=ROOT,
                         capture_output=True, text=True, timeout=60)
    assert res.returncode == 0, res.stderr
    assert "uv run" in res.stdout


# --- gen_palette.py -------------------------------------------------------------------

def test_gen_palette_escribe_json_y_atlas(tmp_path, capsys):
    out, js = tmp_path / "atlas", tmp_path / "sub" / "dir" / "paleta.json"
    gen_palette.main(["--out", str(out), "--json", str(js)])
    # El JSON regenerado es idéntico al versionado (lo comprueba también test_palette_atlas).
    assert js.read_text(encoding="utf-8") == (HERE / "paleta.json").read_text(encoding="utf-8")
    for isl in ISLANDS:
        img = Image.open(out / f"T_Palette_{isl.key}.png")
        assert img.mode == "RGB" and img.size == (512, 512)
    assert "[paleta]" in capsys.readouterr().out


def test_gen_palette_sheet_pasa_terreno_armonizado_y_sin_armonizar(tmp_path, monkeypatch):
    """La hoja compara el BC armonizado (generate) con el crudo (fn del material)."""
    targets = {"SandDry": ("arena_seca", 0.84, 0.075, 80.0, "Sand"), "Ash": ("ceniza", 0.745, 0.012, 40.0, "Sand")}
    monkeypatch.setattr(gen_palette, "TERRAIN_TARGETS", targets)
    seen = {}

    def fake_sheet(path, terrain, raw, title):
        seen.update(terrain=terrain, raw=raw, title=title)
        path.write_bytes(b"png")
        return path

    monkeypatch.setattr(palette_sheet, "contact_sheet", fake_sheet)
    gen_palette.main(["--out", str(tmp_path), "--json", str(tmp_path / "p.json"),
                      "--sheet", str(tmp_path / "hoja.png"), "--sheet-size", "32"])
    assert set(seen["terrain"]) == set(seen["raw"]) == set(targets)
    for name in targets:
        np.testing.assert_array_equal(seen["terrain"][name], generate(name, 32)["BC"])
        np.testing.assert_array_equal(seen["terrain"][name], harmonize_albedo(seen["raw"][name], name))
        assert not np.array_equal(seen["raw"][name], seen["terrain"][name])
    assert (tmp_path / "hoja.png").read_bytes() == b"png"
