"""Lista de la banda sonora grabada (music_sources.json) y sus creditos.

No necesitan red ni audio: se ejecutan siempre, tambien en CI."""

from __future__ import annotations

import copy
import json

import pytest

from explored_audio.recorded.credits import default_credits_path, render_credits
from explored_audio.recorded.sources import (
    ATTRIBUTION_LICENSES,
    LICENSE_URLS,
    MOMENTS,
    SOURCES,
    default_sources_path,
    load_layer_roles,
    load_sources,
    parse_sources,
    validate,
)


@pytest.fixture(scope="module")
def raw() -> dict:
    return json.loads(default_sources_path().read_text(encoding="utf-8"))


@pytest.fixture(scope="module")
def sources():
    return load_sources()


@pytest.fixture(scope="module")
def roles() -> set[str]:
    return load_layer_roles()


def test_la_lista_valida_sin_errores(sources, roles):
    assert validate(sources, roles) == []


def test_todas_las_licencias_estan_en_la_lista_permitida(sources):
    for p in sources.pieces:
        allowed, _ = SOURCES[p.source]
        assert p.license in allowed, f"{p.id}: {p.license} no admitida en {p.source}"
        assert p.license_url in LICENSE_URLS[p.license], p.id


def test_ninguna_licencia_nc_ni_nd(sources):
    # Doble cierre: ni el identificador ni la URL pueden mencionar NC o ND.
    for p in sources.pieces:
        text = f"{p.license} {p.license_url}".lower()
        for bad in ("-nc", "nc-", "-nd", "nd-", "noncommercial", "noderiv"):
            assert bad not in text, f"{p.id}: licencia con {bad}"


def test_ningun_nc_nd_ni_sa_en_la_lista_permitida():
    for allowed, _ in SOURCES.values():
        for lic in allowed:
            assert not any(tag in lic for tag in ("NC", "ND", "SA")), lic


def test_entre_15_y_25_piezas_y_todos_los_momentos(sources):
    assert 15 <= len(sources.pieces) <= 25
    moments = {p.moment for p in sources.pieces}
    assert moments == set(MOMENTS), f"faltan momentos: {set(MOMENTS) - moments}"


def test_cada_momento_tiene_al_menos_dos_piezas(sources):
    # Con una sola pieza por momento, la misma grabacion se repetiria cada vez.
    for m in MOMENTS:
        n = sum(1 for p in sources.pieces if p.moment == m)
        assert n >= 2, f"{m}: solo {n} pieza(s)"


def test_layer_role_existe_en_music_layers(sources, roles):
    for p in sources.pieces:
        assert p.layer_role in roles, f"{p.id}: {p.layer_role}"


def test_hashes_con_formato_sha256_y_unicos(sources):
    hashes = [p.sha256 for p in sources.pieces]
    for p in sources.pieces:
        assert len(p.sha256) == 64 and all(c in "0123456789abcdef" for c in p.sha256), p.id
    assert len(set(hashes)) == len(hashes)


def test_el_json_esta_ordenado_y_formateado(raw):
    # Formato estable para que los diffs de otras sesiones sean legibles.
    text = default_sources_path().read_text(encoding="utf-8")
    assert text == json.dumps(raw, indent=2, ensure_ascii=False) + "\n"


# --- Creditos -------------------------------------------------------------


def test_creditos_versionados_coinciden_con_los_generados(sources):
    path = default_credits_path()
    assert path.exists(), "falta docs/creditos-musica.md: uv run explored-music credits"
    assert path.read_text(encoding="utf-8") == render_credits(sources), (
        "docs/creditos-musica.md esta desfasado: uv run explored-music credits"
    )


def test_ninguna_pieza_cc_by_sin_credito_en_es_en_y_steam(sources):
    doc = render_credits(sources)
    es = doc.split("## Español", 1)[1].split("## English", 1)[0]
    en = doc.split("## English", 1)[1].split("## Steam", 1)[0]
    steam = doc.split("## Steam", 1)[1].split("```text", 1)[1].split("```", 1)[0]
    credited = [p for p in sources.pieces if p.license in ATTRIBUTION_LICENSES]
    assert credited, "se esperaba al menos una pieza CC-BY"
    for p in credited:
        for name, section in (("ES", es), ("EN", en), ("Steam", steam)):
            assert p.title in section, f"{p.id}: sin titulo en la seccion {name}"
            assert p.performer in section, f"{p.id}: sin autor en la seccion {name}"
            assert p.license_url in section, f"{p.id}: sin enlace de licencia en {name}"


def test_las_piezas_libres_no_aparecen_como_cc_by(sources):
    doc = render_credits(sources)
    steam = doc.split("```text", 1)[1].split("```", 1)[0]
    for p in sources.pieces:
        if p.license not in ATTRIBUTION_LICENSES:
            assert f'"{p.title}"' not in steam, p.id


# --- Entradas corruptas ---------------------------------------------------


def _mutate(raw: dict, **changes) -> list[str]:
    data = copy.deepcopy(raw)
    data["pieces"][0].update(changes)
    return validate(parse_sources(data), load_layer_roles())


@pytest.mark.parametrize(
    "changes",
    [
        {"license": "CC-BY-NC-4.0"},
        {"license": "CC-BY-SA-4.0"},
        {"license": "CC-BY-4.0", "license_url": "https://creativecommons.org/licenses/by/4.0/"},
        {"license_url": "https://creativecommons.org/licenses/by-nd/4.0/"},
        {"source": "youtube"},
        {"download_url": "http://upload.wikimedia.org/x.ogg"},
        {"download_url": "https://upload.wikimedia.org.evil.example/x.ogg"},
        {"source_url": "https://example.com/pieza"},
        {"sha256": ""},
        {"sha256": "ABC"},
        {"moment": "combate"},
        {"layer_role": "boss"},
        {"id": "Gymnopedie"},
        {"performer": "   "},
    ],
)
def test_validate_rechaza_entradas_corruptas(raw, changes):
    assert _mutate(raw, **changes), f"no se detecto {changes}"


def test_validate_rechaza_ids_y_hashes_duplicados(raw):
    data = copy.deepcopy(raw)
    data["pieces"].append(copy.deepcopy(data["pieces"][0]))
    errors = validate(parse_sources(data), load_layer_roles())
    assert any("id repetido" in e for e in errors)
    assert any("sha256 repetido" in e for e in errors)
    assert any("download_url repetida" in e for e in errors)


@pytest.mark.parametrize(
    "breaker",
    [
        lambda d: d.pop("pieces"),
        lambda d: d.__setitem__("pieces", {}),
        lambda d: d["pieces"][0].pop("sha256"),
        lambda d: d["pieces"][0].__setitem__("extra", "x"),
        lambda d: d["pieces"][0].__setitem__("title", 3),
        lambda d: d.__setitem__("target_lufs", "alto"),
        lambda d: d.__setitem__("target_lufs", True),
    ],
)
def test_parse_rechaza_estructura_invalida(raw, breaker):
    data = copy.deepcopy(raw)
    breaker(data)
    with pytest.raises(ValueError):
        parse_sources(data)


def test_parse_rechaza_raiz_no_objeto():
    with pytest.raises(ValueError):
        parse_sources([])
