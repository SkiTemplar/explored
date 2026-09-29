"""Construccion (biblia de contenido §3.10): encajar una pieza en su sitio,
el clic de un encaje a presion, el crujido de un techo de hojas/paja, clavar
una clavija con un mazo de piedra y desmontar una pieza."""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import fit_length, smooth_random_walk
from ..filters import static_filter, time_varying_filter
from ..modal import modal_hit
from ..noise import pink_noise
from ..rng import rng_for

SR = SAMPLE_RATE
# Ganancia de desmontar: sonoridad momentanea como la de clavar (~ -13 LUFS).
GAIN_DISMANTLE = 0.85
# Ganancia de techar: algo mas flojo que colocar pieza (hojas, no madera maciza).
GAIN_THATCH = 0.62


def build_place(name: str) -> np.ndarray:
    """Encajar una pieza de madera en su sitio. La pieza no cae de una vez:
    toca con un canto, bascula y asienta con un segundo golpe mas flojo
    20-60 ms despues. Cada contacto es un chasquido corto de madera contra
    madera, los modos de flexion de la viga (razones de barra libre 1 :
    2,76 : 5,4 : 8,9, a 180-260 Hz) y un golpe grave de la estructura que
    la recibe. Al final la union cruje un poco al cargar el peso: unos
    pocos micro-deslizamientos a traves de la resonancia de la fibra.

    Antes era un modal de 140-200 Hz con un 83 % de la energia por debajo
    de 150 Hz: en altavoces pequeños casi no se oia y sonaba a bombo, no a
    madera."""
    rng = rng_for(name)
    dur = rng.uniform(0.42, 0.52)
    n = int(dur * SR)
    out = np.zeros(n)
    beam_freq = rng.uniform(180.0, 260.0)
    settle_s = rng.uniform(0.02, 0.06)
    for k, (pos_s, force) in enumerate(((0.0, 1.0), (settle_s, rng.uniform(0.4, 0.55)))):
        body = modal_hit(
            SR, 0.3, base_freq=beam_freq * (1.0 + 0.03 * k),
            mode_ratios=[1.0, 2.76, 5.4, 8.9],
            mode_dampings_s=[0.055, 0.03, 0.014, 0.006],
            mode_amps=[1.0, 0.6, 0.3, 0.12], rng=rng, detune=0.02,
        )
        thump = modal_hit(SR, 0.15, base_freq=rng.uniform(95.0, 125.0), mode_ratios=[1.0], mode_dampings_s=[0.03], mode_amps=[1.0])
        click_n = int(0.004 * SR)
        click = static_filter(rng.standard_normal(click_n), SR, fc=2500.0, q=0.7, kind="highpass") * np.exp(-np.arange(click_n) / SR / 0.0008)
        pos = int(pos_s * SR)
        _place(out, body * 0.62 * force, pos)
        _place(out, thump * 0.3 * force, pos)
        _place(out, click * 0.45 * force, pos)

    creak_start = int((settle_s + rng.uniform(0.07, 0.12)) * SR)
    t = creak_start
    for _ in range(int(rng.integers(4, 8))):
        tick = modal_hit(
            SR, 0.03, base_freq=rng.uniform(900.0, 1500.0),
            mode_ratios=[1.0, 2.3], mode_dampings_s=[0.006, 0.003],
            mode_amps=[1.0, 0.4], rng=rng, detune=0.05,
        )
        _place(out, tick * rng.uniform(0.06, 0.12), t)
        t += int(rng.uniform(0.012, 0.03) * SR)

    # Los senos amortiguados que arrancan en fase cero dejan continua; sin
    # este filtro, `remove_dc` la convertiria en un escalon al final.
    out = static_filter(out, SR, fc=40.0, q=0.707, kind="highpass")
    fade_n = int(0.03 * SR)
    out[-fade_n:] *= np.linspace(1.0, 0.0, fade_n) ** 2
    return out


def build_snap(name: str) -> np.ndarray:
    """Encaje a presion: clic corto y nitido de confirmacion."""
    rng = rng_for(name)
    dur = rng.uniform(0.08, 0.12)
    n = int(dur * SR)
    click = modal_hit(
        SR, dur, base_freq=rng.uniform(1600, 2200),
        mode_ratios=[1.0, 1.5], mode_dampings_s=[0.012, 0.008],
        mode_amps=[1.0, 0.4], rng=rng, detune=0.02,
    )
    snap_noise = static_filter(rng.standard_normal(n), SR, fc=3000, q=0.7, kind="highpass")
    snap_env = fit_length(np.exp(-np.arange(n) / SR / 0.01), n)
    return click * 0.7 + snap_noise * snap_env * 0.4


def build_thatch(name: str) -> np.ndarray:
    """Colocar una plancha de techo de palma (hojas de cocotero atadas a una
    vara) sobre las correas, por capas.

    Antes era 0,4-0,6 s de granos de ruido a 2,2-6,5 kHz con una envolvente
    plana: el 96 % de la energia por encima de 2 kHz (centroide ~6 kHz), un
    siseo sin peso ni golpe. Una plancha de palma pesa: se oye

    - el vuelo de la plancha al bajarla: un soplo de hojas en el aire que
      sube de 700 Hz a ~2,5 kHz en 60-100 ms, flojo;
    - el golpe: la vara contra la correa (chasquido de contacto y modos
      inarmonicos de madera a 260-360 Hz, muy amortiguados) y la masa de
      hojas que cae a la vez (golpe sordo de ruido por debajo de ~300 Hz);
    - el chasquido de los foliolos al aplastarse: una rafaga densa de
      micro-chasquidos secos de 1-3 ms entre 1,5 y 6 kHz;
    - el asiento: los foliolos siguen crujiendo cada vez menos (la tasa cae
      de ~180 a ~12 por segundo) sobre un roce de hojas que se apaga;
    - a veces la plancha resbala un poco en la correa: un roce corto de
      adherencia-deslizamiento de la vara (tension que baja).
    """
    rng = rng_for(name)
    dur = rng.uniform(0.6, 0.75)
    n = int(dur * SR)
    out = np.zeros(n)
    hit = int(rng.uniform(0.07, 0.1) * SR)

    # Vuelo de la plancha: soplo de hojas que sube de tono y crece hasta el golpe.
    swish_n = hit
    u = np.arange(swish_n) / swish_n
    fc = 700.0 * (rng.uniform(3.2, 3.8)) ** u
    swish = time_varying_filter(pink_noise(swish_n, rng), SR, fc, q=0.9, kind="bandpass")
    swish /= np.max(np.abs(swish)) + 1e-9
    _place(out, swish * u**2 * 0.12, 0)

    # Golpe: contacto de la vara, modos de la vara/correa y masa de hojas.
    _place(out, _hammer_burst(rng, int(0.001 * SR), fc=rng.uniform(2200, 2800), q=0.7, kind="highpass", decay_s=0.0003) * 0.3, hit)
    pole = modal_hit(
        SR, 0.12, base_freq=rng.uniform(260, 360),
        mode_ratios=[1.0, 2.3, 3.9], mode_dampings_s=[0.03, 0.016, 0.008],
        mode_amps=[1.0, 0.5, 0.25], rng=rng, detune=0.04,
    )
    _place(out, pole * 0.2, hit)
    _place(out, _hammer_burst(rng, int(0.05 * SR), fc=rng.uniform(600, 900), q=1.0, kind="bandpass", decay_s=0.01) * 0.25, hit)
    _place(out, _hammer_burst(rng, int(0.09 * SR), fc=300.0, q=0.7, kind="lowpass", decay_s=0.025) * 0.3, hit + int(0.002 * SR))

    # Foliolos: micro-chasquidos secos, rafaga al aplastarse y asiento cada vez
    # mas espaciado (proceso de Poisson con tasa que decae).
    tail_n = n - hit
    t = 0.0
    rate0, rate1 = rng.uniform(160, 200), rng.uniform(10, 14)
    tail_s = tail_n / SR
    while True:
        frac = t / tail_s
        rate = rate0 * (rate1 / rate0) ** frac
        # La rafaga del aplastamiento (primeros 40 ms) es mucho mas densa.
        if t < 0.04:
            rate *= 6.0
        t += rng.exponential(1.0 / rate)
        if t >= tail_s:
            break
        glen = int(rng.uniform(0.001, 0.003) * SR)
        g = static_filter(rng.standard_normal(glen), SR, fc=rng.uniform(1500, 6000), q=1.3, kind="bandpass")
        g *= np.exp(-np.arange(glen) / (glen / 4.0))
        g /= np.max(np.abs(g)) + 1e-9
        amp = rng.uniform(0.3, 1.0) * (1.0 - 0.75 * (t / tail_s)) * (1.4 if t < 0.04 else 1.0)
        _place(out, g * amp * 0.32, hit + int(t * SR))

    # Roce de las hojas contra hojas: colchon que se apaga tras el golpe.
    bed = static_filter(pink_noise(tail_n, rng), SR, fc=rng.uniform(1800, 2600), q=0.8, kind="bandpass")
    bed /= np.max(np.abs(bed)) + 1e-9
    tt = np.arange(tail_n) / SR
    bed *= np.exp(-tt / rng.uniform(0.09, 0.13)) * np.minimum(tt / 0.004, 1.0)
    _place(out, bed * 0.22, hit)

    # A veces la plancha resbala en la correa: la vara roza y se frena.
    if rng.uniform() < 0.5:
        slide_n = int(rng.uniform(0.05, 0.08) * SR)
        rate = np.linspace(rng.uniform(260, 340), 60.0, slide_n)
        ticks = np.zeros(slide_n)
        ticks[np.nonzero(np.diff(np.floor(np.cumsum(rate) / SR), prepend=0.0))[0]] = 1.0
        ticks *= rng.uniform(0.5, 1.0, slide_n)
        slide = static_filter(ticks, SR, fc=rng.uniform(900, 1300), q=3.5, kind="bandpass")
        slide /= np.max(np.abs(slide)) + 1e-9
        slide *= np.linspace(1.0, 0.0, slide_n) ** 1.2
        _place(out, slide * 0.14, hit + int(rng.uniform(0.12, 0.2) * SR))

    out = static_filter(out, SR, fc=40.0, q=0.707, kind="highpass")
    out = np.tanh(2.0 * out / (np.max(np.abs(out)) + 1e-9)) / np.tanh(2.0)
    fade = int(0.02 * SR)
    out[-fade:] *= np.linspace(1.0, 0.0, fade) ** 2
    return out * GAIN_THATCH


def _place(out: np.ndarray, x: np.ndarray, pos: int) -> None:
    end = min(pos + len(x), len(out))
    if end > pos:
        out[pos:end] += x[: end - pos]


def _hammer_burst(rng: np.random.Generator, n: int, fc: float, q: float, kind: str, decay_s: float) -> np.ndarray:
    """Ruido filtrado con ataque de 0,1 ms y caida exponencial, a pico 1."""
    t = np.arange(n) / SR
    x = static_filter(rng.standard_normal(n), SR, fc=fc, q=q, kind=kind)
    x *= (1.0 - np.exp(-t / 0.0001)) * np.exp(-t / decay_s)
    return x / (np.max(np.abs(x)) + 1e-9)


def build_hammer(name: str) -> np.ndarray:
    """Clavar una clavija de madera con un mazo de piedra: 3-4 golpes.

    Antes cada golpe era un modo largo de la viga a 260-340 Hz (el 77 % de la
    energia en una octava, ~150 ms hasta -30 dB) y sonaba a marimba. Un mazo
    contra una clavija es un golpe seco por capas, como `sfx_wood_chop`:

    - el contacto piedra-madera: chasquido de ~1 ms por encima de 2,5 kHz;
    - el "toc" de la cabeza de la clavija: modos inarmonicos a 420-520 Hz
      muy amortiguados (25 ms) con ruido de banda media que les quita la
      nota; suben de tono golpe a golpe porque queda menos clavija libre;
    - el golpe sordo de la viga que recibe: ruido grave de ~20 ms y un modo
      a 110-150 Hz amortiguado (la viga esta atada, apenas resuena);
    - la clavija que entra en el agujero: 20-50 ms de roce (adherencia y
      deslizamiento de fibra contra fibra) justo despues del contacto, mas
      largo en los primeros golpes, cuando todavia avanza;
    - a veces el mazo rebota y da un segundo toque flojo 15-30 ms despues.
    El ultimo golpe asienta la clavija: sin roce y con el cuerpo mas seco.
    """
    rng = rng_for(name)
    n_hits = int(rng.integers(3, 5))
    gaps = [rng.uniform(0.3, 0.4) for _ in range(n_hits - 1)]
    n = int((sum(gaps) + 0.35) * SR)
    out = np.zeros(n)
    peg_freq = rng.uniform(420, 520)
    beam_freq = rng.uniform(110, 150)
    pos = int(0.003 * SR)
    for i in range(n_hits):
        last = i == n_hits - 1
        force = rng.uniform(0.85, 1.0) * (1.1 if last else 1.0)
        dry = 0.6 if last else 1.0

        click_n = int(rng.uniform(0.0008, 0.0014) * SR)
        _place(out, _hammer_burst(rng, click_n, fc=rng.uniform(2500, 3500), q=0.7, kind="highpass", decay_s=click_n / SR / 3.0) * 0.5, pos)

        peg = modal_hit(
            SR, 0.12, base_freq=peg_freq * (1.0 + 0.07 * i),
            mode_ratios=[1.0, 1.62, 2.37, 3.3], mode_dampings_s=[0.025 * dry, 0.017 * dry, 0.011, 0.007],
            mode_amps=[1.0, 0.55, 0.35, 0.2], rng=rng, detune=0.04,
        )
        _place(out, peg * 0.55 * force, pos)
        _place(out, _hammer_burst(rng, int(0.06 * SR), fc=rng.uniform(800, 1200), q=1.0, kind="bandpass", decay_s=0.012) * 0.35 * force, pos)

        _place(out, _hammer_burst(rng, int(0.08 * SR), fc=240.0, q=0.8, kind="lowpass", decay_s=0.02) * 0.5 * force, pos)
        beam = modal_hit(SR, 0.15, base_freq=beam_freq, mode_ratios=[1.0, 2.4], mode_dampings_s=[0.03 * dry, 0.015], mode_amps=[1.0, 0.3], rng=rng, detune=0.03)
        _place(out, beam * 0.3 * force, pos)

        if not last:
            # Roce de la clavija al avanzar: tren de micro-chasquidos cada vez
            # mas lentos (se frena), filtrados por la resonancia de la madera.
            slide_s = rng.uniform(0.02, 0.05) * (1.0 - 0.25 * i)
            slide_n = int(slide_s * SR)
            rate = np.linspace(rng.uniform(900, 1300), 250.0, slide_n)
            phase = np.cumsum(rate / SR)
            ticks = np.zeros(slide_n)
            ticks[np.nonzero(np.diff(np.floor(phase), prepend=0.0))[0]] = 1.0
            ticks *= rng.uniform(0.5, 1.0, slide_n)
            slide = static_filter(ticks, SR, fc=rng.uniform(1400, 2200), q=4.0, kind="bandpass")
            slide *= np.linspace(1.0, 0.0, slide_n) ** 1.5
            slide /= np.max(np.abs(slide)) + 1e-9
            _place(out, slide * 0.18 * force, pos + int(0.004 * SR))

        if rng.uniform() < 0.35:
            bounce = int(rng.uniform(0.015, 0.03) * SR)
            _place(out, _hammer_burst(rng, int(0.03 * SR), fc=rng.uniform(1500, 2500), q=1.2, kind="bandpass", decay_s=0.004) * 0.15, pos + bounce)

        if not last:
            pos += int(gaps[i] * SR)

    # Sin continua del golpe grave y saturacion suave, como en `impacts`:
    # el chasquido de 1 ms fija el pico y sin ella el golpe queda muy bajo.
    out = static_filter(out, SR, fc=30.0, q=0.707, kind="highpass")
    out = np.tanh(2.5 * out / (np.max(np.abs(out)) + 1e-9)) / np.tanh(2.5)
    fade = int(0.005 * SR)
    out[-fade:] *= np.linspace(1.0, 0.0, fade)
    return out * 0.85


def build_dismantle(name: str) -> np.ndarray:
    """Desmontar una pieza, por capas secas.

    Antes la atadura era ruido rosa en banda, la pieza un modo a 110-150 Hz
    (el 58 % de la energia por debajo de 150 Hz, ~265 ms hasta -30 dB) y los
    trozos caian a intervalos al azar. Ahora:

    - se aflojan las ataduras: dos tirones de cuerda de fibra por
      adherencia-deslizamiento (como `tie_cord`), pero al reves: la tension
      baja y los saltos se espacian (de ~130 a ~35 por segundo);
    - la pieza se suelta y cae: chasquido de contacto, modos de flexion de
      la viga (1 : 2,76 : 5,4 a 170-240 Hz, muy amortiguados) y un golpe
      grave corto del suelo;
    - caen dos o tres trozos sueltos (clavijas, astillas) que rebotan como
      una pelota: cada bote llega antes y mas flojo que el anterior
      (coeficiente de restitucion 0,5-0,7) y cada trozo tiene su tono.
    """
    rng = rng_for(name)
    dur = rng.uniform(1.25, 1.5)
    n = int(dur * SR)
    out = np.zeros(n)

    pos = int(0.01 * SR)
    for k in range(2):
        pull_n = int(rng.uniform(0.14, 0.2) * SR)
        u = np.arange(pull_n) / pull_n
        rate = (130.0 - 95.0 * u) * smooth_random_walk(pull_n, rng, smoothing_hz=25.0, sr=SR, low=0.7, high=1.3)
        slips = np.nonzero(np.diff(np.floor(np.cumsum(rate) / SR)) > 0)[0] + 1
        pulses = np.zeros(pull_n)
        pulses[slips] = rng.uniform(0.4, 1.0, slips.size)
        fibre = static_filter(pulses, SR, fc=rng.uniform(1200, 1700), q=4.0, kind="bandpass")
        fibre += 0.5 * static_filter(pulses, SR, fc=rng.uniform(3000, 4000), q=3.0, kind="bandpass")
        fibre = np.tanh(3.0 * fibre / (np.max(np.abs(fibre)) + 1e-9)) / np.tanh(3.0)
        rub = static_filter(pink_noise(pull_n, rng), SR, fc=rng.uniform(800, 1200), q=0.9, kind="bandpass")
        rub /= np.max(np.abs(rub)) + 1e-9
        env = np.minimum(u / 0.1, 1.0) * (1.0 - u) ** 0.7
        _place(out, (0.8 * fibre + 0.25 * rub) * env * (0.5 if k == 0 else 0.65), pos)
        pos += pull_n + int(rng.uniform(0.04, 0.08) * SR)

    # La pieza se suelta y golpea el suelo.
    pos += int(rng.uniform(0.03, 0.06) * SR)
    _place(out, _hammer_burst(rng, int(0.0012 * SR), fc=rng.uniform(2500, 3200), q=0.7, kind="highpass", decay_s=0.0004) * 0.4, pos)
    beam = modal_hit(
        SR, 0.15, base_freq=rng.uniform(170, 240),
        mode_ratios=[1.0, 2.76, 5.4, 8.9], mode_dampings_s=[0.035, 0.02, 0.01, 0.005],
        mode_amps=[1.0, 0.6, 0.3, 0.12], rng=rng, detune=0.03,
    )
    _place(out, beam * 0.55, pos)
    _place(out, _hammer_burst(rng, int(0.06 * SR), fc=rng.uniform(700, 1000), q=1.0, kind="bandpass", decay_s=0.01) * 0.3, pos)
    _place(out, _hammer_burst(rng, int(0.08 * SR), fc=220.0, q=0.8, kind="lowpass", decay_s=0.02) * 0.6, pos)

    # Trozos sueltos que rebotan: intervalos y fuerza caen con la restitucion.
    for _ in range(int(rng.integers(2, 4))):
        t = pos + int(rng.uniform(0.03, 0.12) * SR)
        gap = rng.uniform(0.09, 0.15)
        amp = rng.uniform(0.3, 0.45)
        restitution = rng.uniform(0.5, 0.7)
        freq = rng.uniform(550, 950)
        while amp > 0.03 and t < n:
            _place(out, _hammer_burst(rng, int(0.001 * SR), fc=3000.0, q=0.7, kind="highpass", decay_s=0.0003) * amp * 0.5, t)
            bit = modal_hit(
                SR, 0.06, base_freq=freq,
                mode_ratios=[1.0, 1.7, 2.9], mode_dampings_s=[0.012, 0.008, 0.004],
                mode_amps=[1.0, 0.45, 0.2], rng=rng, detune=0.05,
            )
            _place(out, bit * amp, t)
            t += int(gap * SR)
            gap *= restitution
            amp *= restitution
    out = static_filter(out, SR, fc=40.0, q=0.707, kind="highpass")
    out = np.tanh(2.5 * out / (np.max(np.abs(out)) + 1e-9)) / np.tanh(2.5)
    fade = int(0.02 * SR)
    out[-fade:] *= np.linspace(1.0, 0.0, fade) ** 2
    return out * GAIN_DISMANTLE
