"""Tests de la lógica pura de normalize.py (sin Blender)."""

from __future__ import annotations

import json
import math
from dataclasses import dataclass
from typing import Any

import pytest

import normalize_core as nc
from fetch_packs import REPO_ROOT


@dataclass
class P:
    x: float
    y: float
    z: float


PALETTE = json.loads((REPO_ROOT / "Tools" / "Textures" / "paleta.json").read_text(encoding="utf-8"))
CATALOG = json.loads((REPO_ROOT / "Content" / "Data" / "packs_catalogo.json").read_text(encoding="utf-8"))


# --- color -----------------------------------------------------------------

def test_hex_a_srgb_con_y_sin_almohadilla() -> None:
    assert nc.hex_to_srgb("#ff0080") == (1.0, 0.0, 128 / 255)
    assert nc.hex_to_srgb("00ff00") == (0.0, 1.0, 0.0)


@pytest.mark.parametrize("bad", ["#fff", "", "#12345678", "#gg0000"])
def test_hex_corrupto_falla(bad: str) -> None:
    with pytest.raises(ValueError):
        nc.hex_to_srgb(bad)


def test_srgb_lineal_ida_y_vuelta() -> None:
    for i in range(256):
        c = i / 255
        assert nc.linear_to_srgb(nc.srgb_to_linear(c)) == pytest.approx(c, abs=1e-9)
    # Tramo lineal y recorte fuera de [0, 1].
    assert nc.srgb_to_linear(0.04) == pytest.approx(0.04 / 12.92)
    assert nc.linear_to_srgb(-1.0) == 0.0
    assert nc.linear_to_srgb(2.0) == pytest.approx(1.0)


def test_oklab_referencias() -> None:
    # Blanco: L = 1, a = b = 0; negro: todo 0 (valores de referencia de Björn Ottosson).
    L, a, b = nc.oklab((1.0, 1.0, 1.0))
    assert L == pytest.approx(1.0, abs=1e-4) and abs(a) < 1e-4 and abs(b) < 1e-4
    assert nc.oklab((0.0, 0.0, 0.0)) == pytest.approx((0.0, 0.0, 0.0))
    # Rojo sRGB puro: (0.628, 0.225, 0.126).
    assert nc.oklab((1.0, 0.0, 0.0)) == pytest.approx((0.62796, 0.22486, 0.12585), abs=1e-4)
    # Acepta RGBA y usa solo RGB.
    assert nc.oklab((1.0, 0.0, 0.0, 0.5)) == nc.oklab((1.0, 0.0, 0.0))


def test_to_hex_recorta_y_redondea() -> None:
    assert nc.to_hex((1.2, -0.1, 0.5)) == "#ff0080"
    assert nc.to_hex(nc.hex_to_srgb("#3a7b1c")) == "#3a7b1c"


def test_rgb3_descarta_alfa() -> None:
    assert nc.rgb3([0.1, 0.2, 0.3, 1.0]) == (0.1, 0.2, 0.3)
    with pytest.raises(ValueError):
        nc.rgb3([0.1, 0.2])


# --- argumentos --------------------------------------------------------------

def test_argumentos_por_defecto() -> None:
    assert nc.parse_args([]) == {"lote": None, "ids": [], "isla": "Landing", "analyze": False, "tiles": False,
                                 "export": True}


def test_argumentos_completos() -> None:
    args = nc.parse_args(["--lote", "l1", "--ids", "hacha", "pala", "--isla", "Ember", "--tiles", "--no-export",
                          "--analyze"])
    assert args == {"lote": "l1", "ids": ["hacha", "pala"], "isla": "Ember", "analyze": True, "tiles": True,
                    "export": False}


def test_ids_se_cierra_con_la_siguiente_opcion() -> None:
    assert nc.parse_args(["--ids", "a", "--lote", "l", "--tiles"])["ids"] == ["a"]


@pytest.mark.parametrize("argv", [["--lote"], ["--isla", "--tiles"], ["suelto"], ["--otra"],
                                  ["--tiles", "hacha"]])
def test_argumentos_erroneos_salen_con_mensaje(argv: list[str]) -> None:
    # Antes, «--lote» al final reventaba con StopIteration en vez de explicar el error.
    with pytest.raises(SystemExit):
        nc.parse_args(argv)


# --- reglas de recoloreo -------------------------------------------------------

def test_regla_mas_cercana_y_default() -> None:
    compiled, tol, default = nc.compile_rules({
        "rules": [{"from": "#ff0000", "to": "rojo"}, {"from": "#0000ff", "to": "azul"}],
        "tolerance": 0.1, "default": "gris"})
    assert tol == 0.1 and default == "gris"
    assert nc.pick_swatch(nc.hex_to_srgb("#f00a0a"), compiled, tol, default) == "rojo"
    assert nc.pick_swatch(nc.hex_to_srgb("#0a0af0"), compiled, tol, default) == "azul"
    # Verde: lejos de ambas reglas → default.
    assert nc.pick_swatch((0.0, 1.0, 0.0), compiled, tol, default) == "gris"


def test_sin_regla_ni_default_falla_con_el_color() -> None:
    compiled, tol, default = nc.compile_rules({"rules": [{"from": "#ff0000", "to": "rojo"}]})
    assert tol == 0.12 and default is None
    with pytest.raises(RuntimeError, match="hacha: color #00ff00"):
        nc.pick_swatch((0.0, 1.0, 0.0), compiled, tol, default, "hacha")


def test_sin_reglas_usa_default() -> None:
    compiled, tol, default = nc.compile_rules({"default": "madera.clara"})
    assert nc.pick_swatch((0.3, 0.2, 0.1), compiled, tol, default) == "madera.clara"


def test_empate_gana_la_primera_regla() -> None:
    compiled, tol, default = nc.compile_rules({
        "rules": [{"from": "#808080", "to": "primera"}, {"from": "#808080", "to": "segunda"}]})
    assert nc.pick_swatch(nc.hex_to_srgb("#808080"), compiled, tol, default) == "primera"


def test_degradado_de_paleta_real() -> None:
    swatch = PALETTE["islas"]["Landing"]["colores"]["madera.clara"]
    (u, v_top), top = nc.gradient(swatch, 1.0)
    (_, v_mid), mid = nc.gradient(swatch, 0.5)
    (_, v_bot), bot = nc.gradient(swatch, 0.0)
    assert u == swatch["uv"][0]
    assert top == swatch["arriba"]["lineal"] and mid == pytest.approx(swatch["medio"]["lineal"])
    assert bot == swatch["abajo"]["lineal"]
    v0, v1 = swatch["v_rango"]
    # Blender: v = 0 abajo; paleta.json: v desde arriba. Arriba del objeto → v0 de la paleta.
    assert v_top == pytest.approx(1.0 - v0) and v_bot == pytest.approx(1.0 - v1)
    assert v_bot < v_mid < v_top
    # Fuera de [0, 1] se recorta en vez de salirse de la celda de la paleta.
    assert nc.gradient(swatch, 1.7) == nc.gradient(swatch, 1.0)
    assert nc.gradient(swatch, -0.3) == nc.gradient(swatch, 0.0)


def test_todas_las_muestras_de_las_reglas_existen_en_todas_las_islas() -> None:
    """Una regla con destino inexistente solo fallaría dentro de Blender, a mitad de lote."""
    for isla, datos in PALETTE["islas"].items():
        colores = datos["colores"]
        for e in CATALOG["entries"]:
            rc = e["recolor"]
            destinos = {r["to"] for r in rc.get("rules", [])} | ({rc["default"]} if rc.get("default") else set())
            assert destinos <= colores.keys(), (isla, e["gameId"], destinos - colores.keys())


def test_reparto_en_porcentaje() -> None:
    assert nc.share_percent({"a": 1.0, "b": 3.0}) == {"b": 75.0, "a": 25.0}
    assert list(nc.share_percent({"a": 1.0, "b": 3.0})) == ["b", "a"]
    assert nc.share_percent({}) == {}


# --- medida y pivote -------------------------------------------------------------

CUBE = [P(x, y, z) for x in (-1, 1) for y in (-2, 2) for z in (0, 4)]


def test_escala_por_eje() -> None:
    assert nc.bbox_dims(CUBE) == (2, 4, 4)
    assert nc.size_factor(CUBE, {"axis": "x", "m": 1.0}) == 0.5
    assert nc.size_factor(CUBE, {"axis": "z", "m": 2.0}) == 0.5
    assert nc.size_factor(CUBE, {"m": 8.0}) == 2.0  # max por defecto


def test_escala_de_malla_plana_o_eje_raro_falla_claro() -> None:
    plano = [P(x, y, 0) for x in (0, 1) for y in (0, 1)]
    with pytest.raises(ValueError, match="plana"):
        nc.size_factor(plano, {"axis": "z", "m": 1.0})
    with pytest.raises(ValueError, match="eje"):
        nc.size_factor(CUBE, {"axis": "w", "m": 1.0})
    with pytest.raises(ValueError, match="sin vértices"):
        nc.size_factor([], {"m": 1.0})


def test_pivote_base() -> None:
    assert nc.pivot_origin(CUBE, {"kind": "base"}) == (0.0, 0.0, 0)


def _mango() -> list[Any]:
    # Mango vertical centrado en (0.1, 0) de z 0 a 1 y una cabeza desplazada en +X arriba.
    pts = [P(0.1 + dx, dy, z / 10) for z in range(11) for dx in (-0.02, 0.02) for dy in (-0.02, 0.02)]
    pts += [P(0.5, 0.0, 0.95), P(0.8, 0.0, 1.0)]
    return pts


def test_pivote_agarre_desde_abajo() -> None:
    x, y, z = nc.pivot_origin(_mango(), {"kind": "agarre", "gripFromEndM": 0.12})
    assert (x, y, z) == pytest.approx((0.1, 0.0, 0.12))


def test_pivote_agarre_desde_arriba_toma_el_anillo_de_arriba() -> None:
    x, _, z = nc.pivot_origin(_mango(), {"kind": "agarre", "gripFromEndM": 0.2, "end": "top"})
    assert z == pytest.approx(0.8)
    # El anillo de arriba incluye la cabeza: el eje se desplaza hacia +X.
    assert x > 0.1


def test_pivote_agarre_centrado_en_el_agarre() -> None:
    x, _, z = nc.pivot_origin(_mango(), {"kind": "agarre", "gripFromEndM": 0.2, "end": "top", "centerAt": "grip"})
    assert z == pytest.approx(0.8) and x == pytest.approx(0.1)


def test_pivote_agarre_sin_vertices_en_la_banda_usa_el_extremo() -> None:
    pts = [P(0, 0, 0), P(0, 0, 1), P(1, 0, 1)]
    x, _, z = nc.pivot_origin(pts, {"kind": "agarre", "gripFromEndM": 0.5, "centerAt": "grip"})
    assert z == 0.5 and x == 0.0


def test_pivote_desconocido_o_malla_vacia() -> None:
    with pytest.raises(ValueError, match="pivote"):
        nc.pivot_origin(CUBE, {"kind": "centro"})
    with pytest.raises(ValueError):
        nc.pivot_origin([], {"kind": "base"})


def test_catalogo_real_bien_formado_para_normalize() -> None:
    """Cada entrada tiene lo que normalize.py lee sin comprobar."""
    for e in CATALOG["entries"]:
        assert e["size"]["m"] > 0, e["gameId"]
        assert e["size"].get("axis", "max") in ("x", "y", "z", "max"), e["gameId"]
        assert e["pivot"]["kind"] in ("base", "agarre"), e["gameId"]
        if e["pivot"]["kind"] == "agarre":
            assert 0 < e["pivot"]["gripFromEndM"] < e["size"]["m"] * 2, e["gameId"]
        rot = e.get("rotateDeg") or [0, 0, 0]
        assert len(rot) == 3 and all(math.isfinite(a) for a in rot), e["gameId"]
        for r in e["recolor"].get("rules", []):
            nc.hex_to_srgb(r["from"])
        if e.get("rig"):
            assert not e.get("stretch"), e["gameId"]
            acciones = set(e["rig"]["animations"])
            assert {p["action"] for p in e["rig"].get("tilePoses", [])} <= acciones, e["gameId"]


def test_load_json(tmp_path: Any) -> None:
    p = tmp_path / "a.json"
    p.write_text('{"á": 1}', encoding="utf-8")
    assert nc.load_json(p) == {"á": 1}
