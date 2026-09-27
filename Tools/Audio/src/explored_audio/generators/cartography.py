"""Cartografia (mesa de cartografia y cuaderno): la pluma rasgando papel,
desplegar un mapa grande y estampar un sello. Papel = crujidos impulsivos de
fibras (clics filtrados en banda alta); la pluma = friccion estrecha con la
resonancia del plumin; el sello = golpe de madera sobre una mesa."""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, fit_length, smooth_random_walk
from ..filters import static_filter, time_varying_filter
from ..granular import render_noise_grains
from ..modal import modal_hit
from ..noise import pink_noise
from ..rng import rng_for

SR = SAMPLE_RATE
# Ganancia de la pluma, calibrada para quedar en torno a -24 LUFS.
GAIN_PEN = 1.15


def _place(out: np.ndarray, x: np.ndarray, pos: int) -> None:
    end = min(pos + len(x), len(out))
    if end > pos:
        out[pos:end] += x[: end - pos]


def _crackle(n: int, rng: np.random.Generator, density: np.ndarray, band=(1500.0, 8000.0)) -> np.ndarray:
    """Crujido de papel: impulsos cortisimos (0.3-2 ms) de ruido filtrado,
    con una probabilidad por muestra que sigue `density` (clics/segundo)."""
    out = np.zeros(n)
    probs = density / SR
    hits = np.nonzero(rng.random(n) < probs)[0]
    for pos in hits:
        glen = int(rng.uniform(0.0003, 0.002) * SR) + 4
        click = static_filter(rng.standard_normal(glen), SR, fc=rng.uniform(*band), q=0.9, kind="bandpass")
        click *= np.exp(-np.arange(glen) / glen * 5.0) * rng.uniform(0.3, 1.0)
        _place(out, click, int(pos))
    return out


def _pen_gesture(rng: np.random.Generator, kind: str) -> np.ndarray:
    """Rapidez del plumin (0-1) a lo largo de un trazo, a partir de una
    trayectoria 2D. Al escribir la mano oscila a 5-7 Hz (cada letra es ida y
    vuelta) y la rapidez cae casi a cero en cada cambio de sentido: eso es lo
    que da el "scritch-scritch" de la escritura. Al sombrear la oscilacion es
    mas rapida y recta; una linea de costa es un avance lento que serpentea."""
    if kind == "linea":
        dur = rng.uniform(0.35, 0.7)
        osc_hz, osc_amp, drift = rng.uniform(1.2, 2.5), 0.35, 1.0
    elif kind == "sombreado":
        dur = rng.uniform(0.3, 0.55)
        osc_hz, osc_amp, drift = rng.uniform(7.0, 9.5), 1.6, 0.1
    else:  # escritura
        dur = rng.uniform(0.25, 0.55)
        osc_hz, osc_amp, drift = rng.uniform(5.0, 7.0), 1.1, 0.45
    n = int(dur * SR)
    t = np.arange(n) / SR
    # El ritmo de la mano no es un metronomo: la fase acumula una frecuencia que deriva.
    freq = osc_hz * smooth_random_walk(n, rng, smoothing_hz=3.0, sr=SR, low=0.85, high=1.15)
    phase = 2.0 * np.pi * np.cumsum(freq) / SR + rng.uniform(0.0, 2.0 * np.pi)
    vx = drift + osc_amp * np.cos(phase)
    vy = 0.35 * osc_amp * np.sin(2.0 * phase + rng.uniform(0.0, np.pi)) + 0.15 * np.sin(2.0 * np.pi * 1.3 * t)
    speed = np.hypot(vx, vy)
    speed /= speed.max() + 1e-12
    # Apoyar y levantar la pluma: la presion entra y sale en unos milisegundos.
    press = fit_length(ar_envelope(SR, 0.012, 0.03, hold_s=max(dur - 0.042, 0.0), shape=1.0), n)
    return speed * press


def _stick_slip(speed: np.ndarray, rng: np.random.Generator, max_rate_hz: float) -> np.ndarray:
    """Tren de impulsos de adherencia-deslizamiento: el plumin se engancha en
    una fibra y salta a la siguiente. Los saltos por segundo son proporcionales
    a la rapidez (fibras cruzadas por segundo), con separacion irregular porque
    las fibras no estan en rejilla, y cada salto es mas fuerte cuanto mas rapido."""
    n = len(speed)
    jitter = rng.uniform(0.55, 1.45, n)
    phase = np.cumsum(max_rate_hz * speed * jitter) / SR
    idx = np.nonzero(np.diff(np.floor(phase)) > 0)[0] + 1
    out = np.zeros(n)
    out[idx] = speed[idx] ** 1.3 * rng.uniform(0.4, 1.0, len(idx)) * rng.choice([-1.0, 1.0], len(idx))
    return out


def map_pen_scratch(name: str) -> np.ndarray:
    """Pluma sobre papel: una frase de 3-5 trazos (escribir, sombrear o tirar
    una linea de costa) separados por pausas en las que la pluma se levanta.

    Modelo fisico: el ruido no es un siseo continuo, es adherencia-
    deslizamiento del plumin contra las fibras. La tasa de saltos y su fuerza
    siguen la rapidez del gesto (cae a casi cero en cada cambio de sentido, de
    ahi la modulacion a 5-9 Hz de la escritura) y excitan dos resonancias
    anchas: el plumin metalico (3-5 kHz) y la hoja apoyada en la mesa
    (2-3 kHz). Encima, el roce de la fibra, cuyo brillo sube con la rapidez,
    y el grano de crujidos del papel."""
    rng = rng_for(name)
    pieces = [np.zeros(int(0.01 * SR))]
    kinds = ["escritura", "escritura", "sombreado", "linea"]
    for _ in range(int(rng.integers(3, 6))):
        kind = kinds[int(rng.integers(0, len(kinds)))]
        speed = _pen_gesture(rng, kind)
        n = len(speed)

        slips = _stick_slip(speed, rng, max_rate_hz=rng.uniform(900.0, 1400.0))
        nib = static_filter(slips, SR, fc=rng.uniform(3200.0, 4800.0), q=1.6, kind="bandpass")
        sheet = static_filter(slips, SR, fc=rng.uniform(2100.0, 2800.0), q=1.2, kind="bandpass")
        # Roce continuo de la fibra: mas brillante cuanto mas rapido va el plumin.
        rub_fc = 2600.0 + 3800.0 * speed
        rub = time_varying_filter(rng.standard_normal(n), SR, rub_fc, q=0.9, kind="bandpass")
        fibre = _crackle(n, rng, 500.0 * speed, band=(2500.0, 7500.0))

        # A mas rapidez los saltos son mas bruscos y domina el plumin sobre la hoja: brilla.
        # La fibra tambien cruje mas fuerte cuanto mas rapido se la rasga.
        stroke = nib * (0.3 + 1.3 * speed) + sheet * (1.0 - 0.6 * speed) + (rub * 0.12 + fibre * 0.4) * speed
        # El papel absorbe lo mas agudo: sin esto domina un siseo de 10 kHz.
        stroke = static_filter(stroke, SR, fc=8000.0, q=0.707, kind="lowpass")
        stroke = static_filter(stroke, SR, fc=1500.0, q=0.707, kind="highpass")
        # Toque del plumin al apoyarse: un clic seco al principio del trazo.
        touch_n = int(0.004 * SR)
        stroke[:touch_n] += static_filter(rng.standard_normal(touch_n), SR, fc=3500.0, q=1.0, kind="bandpass") * np.exp(-np.arange(touch_n) / touch_n * 4) * 0.35
        pieces.append(stroke)
        pieces.append(np.zeros(int(rng.uniform(0.07, 0.2) * SR)))
    out = np.concatenate(pieces)
    # Gesto discreto: por debajo de las herramientas (~ -24 LUFS, como antes).
    return out * GAIN_PEN


def map_unfold(name: str) -> np.ndarray:
    """Desplegar un mapa: rafagas de crujidos al abrir cada pliegue, el
    aire que mueve la hoja (soplo grave que se abre de timbre) y el
    chasquido final cuando se tensa la hoja sobre la mesa."""
    rng = rng_for(name)
    dur = rng.uniform(1.1, 1.4)
    n = int(dur * SR)

    # Densidad de crujido: dos o tres pliegues que se abren, cada uno una rafaga.
    density = np.full(n, 60.0)
    t = np.arange(n) / SR
    folds = np.sort(rng.uniform(0.05, dur * 0.65, int(rng.integers(2, 4))))
    for centre in folds:
        width = rng.uniform(0.06, 0.12)
        density += rng.uniform(1500.0, 2600.0) * np.exp(-0.5 * ((t - centre) / width) ** 2)
    crackle = _crackle(n, rng, density)

    air = pink_noise(n, rng)
    sweep = np.linspace(350.0, 1600.0, n) ** 1.0
    air = time_varying_filter(air, SR, sweep, q=0.6, kind="lowpass")
    air *= fit_length(ar_envelope(SR, dur * 0.3, dur * 0.5, shape=1.5), n) * 0.12

    out = crackle * 0.8 + air
    snap_pos = int(dur * rng.uniform(0.72, 0.8) * SR)
    snap_n = int(0.05 * SR)
    snap = static_filter(rng.standard_normal(snap_n), SR, fc=2200.0, q=0.6, kind="bandpass") * np.exp(-np.arange(snap_n) / SR / 0.008)
    flap = modal_hit(SR, 0.12, base_freq=rng.uniform(85, 115), mode_ratios=[1.0, 2.2], mode_dampings_s=[0.03, 0.015], mode_amps=[1.0, 0.3], rng=rng, detune=0.03)
    _place(out, snap * 0.9, snap_pos)
    _place(out, flap * 0.35, snap_pos)
    return out * fit_length(ar_envelope(SR, 0.01, 0.08, hold_s=dur - 0.09), n)


def map_stamp(name: str) -> np.ndarray:
    """Sello sobre el mapa: golpe firme del mango de madera contra la mesa
    (con el papel aplastado en medio) y el "tic" al levantarlo."""
    rng = rng_for(name)
    dur = rng.uniform(0.45, 0.6)
    n = int(dur * SR)
    out = np.zeros(n)

    table = modal_hit(
        SR, 0.3, base_freq=rng.uniform(95, 125),
        mode_ratios=[1.0, 2.6, 4.1, 6.3], mode_dampings_s=[0.06, 0.03, 0.015, 0.008],
        mode_amps=[1.0, 0.5, 0.3, 0.15], rng=rng, detune=0.02,
    )
    handle = modal_hit(
        SR, 0.12, base_freq=rng.uniform(420, 560),
        mode_ratios=[1.0, 2.76, 5.4], mode_dampings_s=[0.02, 0.01, 0.005],
        mode_amps=[1.0, 0.45, 0.2], rng=rng, detune=0.02,
    )
    crush_n = int(0.04 * SR)
    crush = _crackle(crush_n, rng, np.full(crush_n, 3000.0), band=(1200.0, 6000.0))
    crush *= np.exp(-np.arange(crush_n) / SR / 0.012)
    _place(out, table, 0)
    _place(out, handle * 0.5, 0)
    _place(out, crush * 0.7, 0)

    lift_pos = int(rng.uniform(0.28, 0.36) * SR)
    lift_n = int(0.02 * SR)
    lift = static_filter(rng.standard_normal(lift_n), SR, fc=3200.0, q=1.5, kind="bandpass") * np.exp(-np.arange(lift_n) / SR / 0.003)
    _place(out, lift * 0.25, lift_pos)
    return out
