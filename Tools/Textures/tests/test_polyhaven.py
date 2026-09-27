"""Validación de entradas de la descarga de Poly Haven (sin red)."""

import pytest

from texgen.polyhaven import _check_download_url, asset_dir


@pytest.mark.parametrize("asset", ["../evil", "a/b", "Dark_Rock", "", "x" * 65])
def test_rechaza_identificadores_que_saldrian_de_la_cache(asset):
    with pytest.raises(ValueError):
        asset_dir(asset)


@pytest.mark.parametrize("url", ["http://dl.polyhaven.org/a.png", "https://evil.example/a.png", None, 42])
def test_rechaza_urls_fuera_del_cdn_oficial(url):
    with pytest.raises(ValueError):
        _check_download_url(url)


def test_acepta_el_cdn_oficial():
    url = "https://dl.polyhaven.org/file/ph-assets/Textures/png/2k/dark_rock/dark_rock_diff_2k.png"
    assert _check_download_url(url) == url
