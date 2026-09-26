"""Propiedades basicas de todo el catalogo: duracion, canales, sin clipping y
sin continua. Se apoya en la fixture `rendered` (sesion) para no regenerar
cada sonido una vez por test."""

from __future__ import annotations

import numpy as np

from explored_audio.constants import PEAK_CEILING_LINEAR, SAMPLE_RATE


def test_catalogo_tiene_124_sonidos(catalog):
    assert len(catalog) == 124


def test_nombres_unicos(catalog):
    names = [spec.name for spec in catalog]
    assert len(names) == len(set(names))


def test_categorias_validas(catalog):
    assert {spec.category for spec in catalog} == {"Ambiente", "Efectos", "Musica"}


def test_musica_es_estereo(catalog, rendered):
    for spec in catalog:
        if spec.category != "Musica":
            continue
        audio = rendered[spec.name]
        assert audio.ndim == 2 and audio.shape[0] == 2, f"{spec.name} deberia ser estereo"


def test_ambiente_es_estereo_y_bucle(catalog, rendered):
    for spec in catalog:
        if spec.category != "Ambiente":
            continue
        audio = rendered[spec.name]
        assert audio.ndim == 2 and audio.shape[0] == 2, f"{spec.name} deberia ser estereo"
        assert spec.is_loop, f"{spec.name} deberia ser un bucle"


def test_ambiente_dura_entre_30_y_60_segundos(catalog, rendered):
    for spec in catalog:
        if spec.category != "Ambiente":
            continue
        duration_s = rendered[spec.name].shape[-1] / SAMPLE_RATE
        assert 30.0 <= duration_s <= 60.0, f"{spec.name} dura {duration_s:.1f}s, fuera de 30-60s"


def test_efectos_no_ambiente_son_mono(catalog, rendered):
    for spec in catalog:
        if spec.category != "Efectos":
            continue
        audio = rendered[spec.name]
        assert audio.ndim == 1, f"{spec.name} deberia ser mono"


def test_sin_clipping(catalog, rendered):
    """Pico <= techo de seguridad (~-1 dBFS) en todos los ficheros."""
    for spec in catalog:
        audio = rendered[spec.name]
        peak = float(np.max(np.abs(audio)))
        assert peak <= PEAK_CEILING_LINEAR + 1e-6, f"{spec.name} supera el techo de pico ({peak:.4f})"
        assert peak > 0.0, f"{spec.name} esta en silencio"


def test_sin_continua(catalog, rendered):
    """Sin desplazamiento de DC: la media por canal debe ser proxima a cero."""
    for spec in catalog:
        audio = rendered[spec.name]
        dc = np.mean(audio, axis=-1)
        assert np.all(np.abs(dc) < 5e-3), f"{spec.name} tiene continua residual ({dc})"
