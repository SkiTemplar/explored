"""Comprobaciones especificas de la tanda nueva: musica adaptativa completa
(incluida la guitarra Karplus-Strong) y la ampliacion de SFX. Las
propiedades genericas (sin clic en el bucle, sin clipping, LUFS por familia)
ya las cubren `test_basic_properties.py`, `test_loop_continuity.py` y
`test_loudness.py` en todo el catalogo; aqui solo lo que es propio de este
contenido: duraciones esperadas por tipo de pieza y el instrumento nuevo."""

from __future__ import annotations

import numpy as np

from explored_audio.constants import SAMPLE_RATE
from explored_audio.music import compose
from explored_audio.music.instruments import karplus_strong_pluck


def _duration_s(name: str) -> float:
    return compose.generate(name).shape[-1] / SAMPLE_RATE


def test_stingers_de_descubrimiento_son_cortos():
    """Un motivo de descubrimiento debe poder sonar sin tapar el gameplay:
    unos pocos segundos, nunca una pieza entera."""
    for variant in ("01", "02", "03", "04"):
        name = f"mus_discovery_{variant}"
        assert not compose.is_loop(name)
        assert 1.5 <= _duration_s(name) <= 10.0, f"{name} dura fuera de lo esperado para un stinger"


def test_bucles_de_exploracion_e_isla_duran_una_pieza_entera(catalog):
    """Las 7 variantes de exploracion por isla, la noche, la tension, la
    tormenta, el mar y el menu son bucles: deben durar lo bastante para no
    notarse repetitivos en segundos."""
    loop_names = [
        *(f"mus_explore_{island}" for island in ("landing", "emerald", "smoke", "teeth", "mangrove", "whitesands", "mesa")),
        "mus_night", "mus_tension", "mus_storm", "mus_sea", "mus_menu",
    ]
    for name in loop_names:
        assert compose.is_loop(name)
        dur = _duration_s(name)
        assert 20.0 <= dur <= 160.0, f"{name} dura {dur:.1f}s, fuera de lo esperado para un bucle de musica"


def test_finales_y_creditos_no_son_bucles_y_duran_una_pieza_completa():
    for name in ("mus_finale_rescue", "mus_finale_voyage", "mus_finale_stay", "mus_credits", "mus_theme"):
        assert not compose.is_loop(name)
        dur = _duration_s(name)
        assert 60.0 <= dur <= 160.0, f"{name} dura {dur:.1f}s, fuera de lo esperado para una pieza completa"


def test_musica_esta_en_el_catalogo_como_estereo_y_sin_clipping(specs_by_name):
    from explored_audio.build import render_sound
    from explored_audio.constants import PEAK_CEILING_LINEAR

    for name in ("mus_storm", "mus_credits"):
        spec = specs_by_name[name]
        audio = render_sound(spec)
        assert audio.ndim == 2 and audio.shape[0] == 2
        assert np.max(np.abs(audio)) <= PEAK_CEILING_LINEAR + 1e-6


def test_guitarra_karplus_strong_decae_sin_clic_ni_valores_no_finitos():
    """La cuerda punteada debe decaer de forma continua (sin escalones) y
    quedarse dentro de rango tras normalizar por su propio pico -exactamente
    lo que exige un digital waveguide estable (ganancia de bucle < 1)."""
    rng = np.random.default_rng(7)
    note = karplus_strong_pluck(220.0, 1.2, 0.8, SAMPLE_RATE, rng, tau_s=1.0)

    assert np.all(np.isfinite(note))
    assert np.max(np.abs(note)) <= 0.8 + 1e-6

    # Sin escalones anormales: el salto muestra a muestra en el "cuerpo" de la
    # nota (tras el ataque) no debe ser mayor que unas pocas veces la
    # desviacion tipica de la propia señal.
    body = note[int(0.05 * SAMPLE_RATE):]
    diffs = np.abs(np.diff(body))
    ceiling = max(float(np.std(body)) * 0.5, 1e-4)
    assert float(np.percentile(diffs, 99.5)) <= ceiling * 6.0


def test_guitarra_karplus_strong_es_determinista():
    rng_a = np.random.default_rng(42)
    rng_b = np.random.default_rng(42)
    note_a = karplus_strong_pluck(196.0, 0.6, 0.9, SAMPLE_RATE, rng_a)
    note_b = karplus_strong_pluck(196.0, 0.6, 0.9, SAMPLE_RATE, rng_b)
    assert np.array_equal(note_a, note_b)


def test_nuevos_sfx_duracion_esperada(specs_by_name):
    bounds = {
        "sfx_monkey_01": (0.3, 2.0),
        "sfx_crocodile_01": (0.8, 2.5),
        "sfx_boar_01": (0.1, 1.0),
        "sfx_crab_01": (0.05, 1.0),
        "sfx_turtle_01": (0.5, 2.0),
        "sfx_carve_01": (0.5, 2.5),
        "sfx_tie_cord": (0.4, 1.5),
        "sfx_stone_knap_01": (0.1, 0.5),
        "sfx_wood_saw": (1.0, 2.5),
        "sfx_fire_ignite": (0.7, 2.0),
        "sfx_cooking_sizzle_loop": (7.0, 11.0),
        "sfx_build_place": (0.2, 1.0),
        "sfx_build_snap": (0.03, 0.3),
        "sfx_build_thatch": (0.2, 1.0),
        "sfx_swim_stroke_01": (0.3, 1.0),
        "sfx_dive_splash": (0.6, 1.5),
        "sfx_bubbles_01": (0.3, 1.2),
        "sfx_paddle_stroke_01": (0.4, 1.2),
        "sfx_sail_flap": (0.3, 1.0),
        "sfx_wind_gust_01": (1.5, 3.5),
        "sfx_eat_01": (0.2, 1.5),
        "sfx_drink": (0.3, 1.0),
        "sfx_breath_tired": (1.0, 2.5),
        "sfx_heartbeat_low_loop": (3.5, 5.0),
        "amb_rain_on_leaves": (30.0, 60.0),
    }
    from explored_audio.build import render_sound

    for name, (low, high) in bounds.items():
        spec = specs_by_name[name]
        dur = render_sound(spec).shape[-1] / SAMPLE_RATE
        assert low <= dur <= high, f"{name} dura {dur:.2f}s, fuera de [{low}, {high}]"
