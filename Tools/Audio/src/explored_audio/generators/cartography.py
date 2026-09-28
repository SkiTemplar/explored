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
# Ganancia del sello: misma sonoridad integrada que el modal de antes (~ -17,6 LUFS).
GAIN_STAMP = 0.85
# Ganancia de desplegar el mapa: sonoridad integrada como la version anterior (~ -20 LUFS).
GAIN_UNFOLD = 1.2


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


def _sheet_swish(rng: np.random.Generator, n: int, lo: float, hi: float) -> np.ndarray:
    """Aire que desplaza la hoja al moverse: ruido rosa en banda que sube de
    `lo` a `hi` (la hoja acelera) con una campana de sonoridad, a pico 1."""
    t = np.linspace(0.0, 1.0, n)
    fc = lo * (hi / lo) ** t
    x = time_varying_filter(pink_noise(n, rng), SR, fc, q=1.1, kind="bandpass")
    x *= np.sin(np.pi * t) ** 1.5
    return x / (np.max(np.abs(x)) + 1e-9)


def map_unfold(name: str) -> np.ndarray:
    """Desplegar un mapa grande sobre la mesa, pliegue a pliegue.

    Antes era un crujido continuo con un soplo de ruido rosa por debajo de
    350 Hz: el 27 % de la energia por debajo de 150 Hz (un retumbo que el
    papel no tiene), el 22 % por encima de 8 kHz (siseo) y una envolvente
    plana en la que no se distinguian los pliegues. Ahora, por capas:

    - cada pliegue (3 o 4) se abre con un "clac" de pandeo: la hoja doblada
      salta de golpe al otro lado (2-6 ms en banda 1,2-3 kHz) y suelta una
      rafaga de crujidos de la arruga que se desacelera en ~120 ms;
    - el aire que mueve la hoja al girar: un soplo en banda que sube de
      ~300 a ~1100 Hz en 90-160 ms, por delante del pandeo;
    - entre pliegues, la hoja se desliza sobre si misma: roce flojo en
      2-4 kHz con crujidos sueltos;
    - al final la hoja se posa (un "fump" corto de aire apretado contra la
      mesa, 150-500 Hz, 40 ms) y las dos manos la tensan: dos chasquidos
      secos muy seguidos y unos crujidos que se asientan.
    """
    rng = rng_for(name)
    folds = int(rng.integers(3, 5))
    gaps = rng.uniform(0.2, 0.3, folds)
    first = rng.uniform(0.12, 0.18)
    starts = first + np.concatenate([[0.0], np.cumsum(gaps[:-1])])
    land_t = starts[-1] + gaps[-1] + rng.uniform(0.02, 0.06)
    dur = land_t + rng.uniform(0.3, 0.38)
    n = int(dur * SR)
    t = np.arange(n) / SR
    out = np.zeros(n)

    # Roce de hoja contra hoja mientras se abre, con crujidos sueltos.
    slide_env = np.clip((t - starts[0] + 0.1) / 0.1, 0.0, 1.0) * np.clip((land_t - t) / 0.08, 0.0, 1.0)
    slide = static_filter(rng.standard_normal(n), SR, fc=rng.uniform(2400.0, 3400.0), q=0.8, kind="bandpass")
    slide *= smooth_random_walk(n, rng, smoothing_hz=6.0, sr=SR, low=0.3, high=1.0) * slide_env
    density = 10.0 + 25.0 * slide_env

    for i, st in enumerate(starts):
        # Soplo por delante del pandeo: la hoja gira y luego salta.
        sw_n = int(rng.uniform(0.09, 0.16) * SR)
        _place(out, _sheet_swish(rng, sw_n, rng.uniform(260.0, 340.0), rng.uniform(900.0, 1200.0)) * rng.uniform(0.18, 0.26), max(int(st * SR) - sw_n // 2, 0))
        # Pandeo: el pliegue se da la vuelta de golpe.
        pop_pos = int((st + rng.uniform(0.03, 0.05)) * SR)
        pop = _burst(rng, int(0.012 * SR), fc=rng.uniform(1200.0, 3000.0), q=0.9, kind="bandpass", decay_s=rng.uniform(0.0015, 0.004))
        _place(out, pop * rng.uniform(0.55, 0.8), pop_pos)
        # Rafaga de la arruga que se abre: densa al saltar, se desacelera.
        rel = t - pop_pos / SR
        density += np.where(rel >= 0.0, rng.uniform(1600.0, 2400.0) * np.exp(-np.maximum(rel, 0.0) / rng.uniform(0.05, 0.08)), 0.0)

    # Se posa: aire apretado contra la mesa.
    land = int(land_t * SR)
    fump_n = int(0.06 * SR)
    fump = static_filter(rng.standard_normal(fump_n), SR, fc=rng.uniform(220.0, 320.0), q=0.7, kind="bandpass")
    fump *= np.sin(np.pi * np.linspace(0.0, 1.0, fump_n)) ** 2
    _place(out, fump / (np.max(np.abs(fump)) + 1e-9) * 0.35, land - fump_n // 2)
    # Las manos tensan la hoja: dos chasquidos secos casi juntos.
    snap_pos = land + int(rng.uniform(0.06, 0.1) * SR)
    for k in range(2):
        snap = _burst(rng, int(0.02 * SR), fc=rng.uniform(1800.0, 3200.0), q=0.7, kind="bandpass", decay_s=rng.uniform(0.002, 0.004))
        _place(out, snap * (0.9 if k == 0 else 0.55), snap_pos + k * int(rng.uniform(0.012, 0.025) * SR))
    rel = t - snap_pos / SR
    density += np.where(rel >= 0.0, 900.0 * np.exp(-np.maximum(rel, 0.0) / 0.05), 0.0)

    crackle = _crackle(n, rng, density, band=(1500.0, 6500.0))
    out += crackle * 0.8 + slide * 0.035
    # El papel no tiene retumbo ni siseo por encima de ~10 kHz.
    out = static_filter(out, SR, fc=140.0, q=0.707, kind="highpass")
    out = static_filter(out, SR, fc=10000.0, q=0.707, kind="lowpass")
    return out * fit_length(ar_envelope(SR, 0.005, 0.06, hold_s=dur - 0.065), n) * GAIN_UNFOLD


def _burst(rng: np.random.Generator, n: int, fc: float, q: float, kind: str, decay_s: float) -> np.ndarray:
    """Ruido filtrado con ataque de 0,1 ms y caida exponencial, a pico 1."""
    t = np.arange(n) / SR
    x = static_filter(rng.standard_normal(n), SR, fc=fc, q=q, kind=kind)
    x *= (1.0 - np.exp(-t / 0.0001)) * np.exp(-t / decay_s)
    return x / (np.max(np.abs(x)) + 1e-9)


def map_stamp(name: str) -> np.ndarray:
    """Sello de madera sobre el mapa, por capas secas.

    Antes era un modo de la mesa a 95-125 Hz: el 86 % de la energia por
    debajo de 150 Hz, el 82 % en una octava y ~200 ms hasta -30 dB. Sonaba a
    bombo. Un sello apretado contra papel sobre una mesa es un golpe corto y
    amortiguado (el papel y la tinta hacen de fieltro):

    - el contacto de la cara del sello con el papel: un "tap" de ~2 ms en
      banda media-alta, suave porque el papel lo amortigua;
    - el mango: modos inarmonicos a 650-850 Hz muy amortiguados (15 ms) con
      ruido de banda media que les quita la nota;
    - el tablero que recibe: ruido grave de ~15 ms y un modo a 150-200 Hz
      que muere en 25 ms (la mesa es gruesa y el mapa la apaga);
    - el papel aplastado: una rafaga corta de crujidos;
    - a veces se balancea el sello para marcar bien: un segundo apoyo flojo
      con un crujido lento de la hoja;
    - al levantar, la tinta despega: un tren de micro-chasquidos que se
      acelera durante 15-25 ms y se suelta de golpe, con un "tic" final.
    """
    rng = rng_for(name)
    dur = rng.uniform(0.5, 0.62)
    n = int(dur * SR)
    out = np.zeros(n)
    pos = int(0.003 * SR)

    _place(out, _burst(rng, int(0.003 * SR), fc=rng.uniform(1800, 2600), q=0.8, kind="bandpass", decay_s=0.0007) * 0.35, pos)
    handle = modal_hit(
        SR, 0.08, base_freq=rng.uniform(650, 850),
        mode_ratios=[1.0, 1.58, 2.41, 3.37], mode_dampings_s=[0.015, 0.01, 0.007, 0.004],
        mode_amps=[1.0, 0.5, 0.3, 0.15], rng=rng, detune=0.04,
    )
    _place(out, handle * 0.45, pos)
    _place(out, _burst(rng, int(0.05 * SR), fc=rng.uniform(900, 1300), q=1.0, kind="bandpass", decay_s=0.008) * 0.25, pos)
    _place(out, _burst(rng, int(0.07 * SR), fc=260.0, q=0.8, kind="lowpass", decay_s=0.015) * 0.55, pos)
    table = modal_hit(SR, 0.12, base_freq=rng.uniform(150, 200), mode_ratios=[1.0, 2.3], mode_dampings_s=[0.025, 0.012], mode_amps=[1.0, 0.3], rng=rng, detune=0.03)
    _place(out, table * 0.22, pos)
    crush_n = int(0.035 * SR)
    crush = _crackle(crush_n, rng, np.full(crush_n, 2500.0), band=(1500.0, 6000.0))
    crush *= np.exp(-np.arange(crush_n) / SR / 0.01)
    _place(out, crush * 0.5, pos)

    lift_at = rng.uniform(0.3, 0.38)
    if rng.uniform() < 0.5:
        rock = int(rng.uniform(0.09, 0.14) * SR)
        _place(out, _burst(rng, int(0.04 * SR), fc=220.0, q=0.8, kind="lowpass", decay_s=0.01) * 0.15, pos + rock)
        creak_n = int(0.08 * SR)
        creak = _crackle(creak_n, rng, np.full(creak_n, 350.0), band=(1200.0, 4000.0))
        creak *= np.hanning(creak_n)
        _place(out, creak * 0.35, pos + rock)

    # Despegue de la tinta: cada vez mas chasquidos por segundo hasta soltarse.
    peel_n = int(rng.uniform(0.015, 0.025) * SR)
    rate = np.linspace(300.0, rng.uniform(2000.0, 2800.0), peel_n)
    ticks = np.zeros(peel_n)
    ticks[np.nonzero(np.diff(np.floor(np.cumsum(rate) / SR), prepend=0.0))[0]] = 1.0
    ticks *= rng.uniform(0.4, 1.0, peel_n) * np.linspace(0.3, 1.0, peel_n)
    peel = static_filter(ticks, SR, fc=rng.uniform(2800, 3800), q=2.0, kind="bandpass")
    peel /= np.max(np.abs(peel)) + 1e-9
    lift = int(lift_at * SR)
    _place(out, peel * 0.12, lift)
    _place(out, _burst(rng, int(0.012 * SR), fc=rng.uniform(3000, 4000), q=1.5, kind="bandpass", decay_s=0.002) * 0.1, lift + peel_n)

    # Sin continua del golpe grave y saturacion suave: el tap de 2 ms fija el
    # pico y sin ella el golpe queda bajo (como `build_hammer`).
    out = static_filter(out, SR, fc=40.0, q=0.707, kind="highpass")
    out = np.tanh(2.5 * out / (np.max(np.abs(out)) + 1e-9)) / np.tanh(2.5)
    fade = int(0.005 * SR)
    out[-fade:] *= np.linspace(1.0, 0.0, fade)
    return out * GAIN_STAMP
