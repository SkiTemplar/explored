"""Huerto (GDD: cultivos de la base): cavar con pala, regar con un recipiente
y cosechar arrancando la planta. Todo es tierra, agua y hojas: se construye
con granos de ruido filtrado (terrones, salpicaduras, desgarro de raices),
un golpe grave para el peso de la pala y burbujas resonantes para el chorro
de agua."""

from __future__ import annotations

import numpy as np
from scipy.signal import fftconvolve

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, fit_length, smooth_random_walk
from ..filters import static_filter, time_varying_filter
from ..granular import render_noise_grains
from ..levels import k_weighted_momentary_max
from ..modal import modal_hit
from ..noise import pink_noise
from ..rng import rng_for

SR = SAMPLE_RATE


def _place(out: np.ndarray, x: np.ndarray, pos: int) -> None:
    end = min(pos + len(x), len(out))
    if end > pos:
        out[pos:end] += x[: end - pos]


def _bubble(rng: np.random.Generator, radius_mm: float) -> np.ndarray:
    """Burbuja de Minnaert: seno amortiguado cuya frecuencia sube al
    acercarse a la superficie (f0 ~ 3.26 kHz / radio en mm)."""
    f0 = 3260.0 / radius_mm
    tau = 0.004 + 0.012 * rng.random()
    n = int(tau * 6 * SR)
    t = np.arange(n) / SR
    rise = 1.0 + rng.uniform(0.5, 1.8) * t / (tau * 6)
    phase = 2 * np.pi * np.cumsum(f0 * rise) / SR
    return np.sin(phase) * np.exp(-t / tau)


def _burst(rng: np.random.Generator, dur_s: float, fc: float, tau_s: float, kind: str = "lowpass", q: float = 0.7, attack_s: float = 0.002) -> np.ndarray:
    """Rafaga de ruido filtrado con ataque corto y caida exponencial: el
    golpe de una masa blanda (tierra, cepellon) no tiene tono, es un soplo
    grave de banda ancha que se apaga enseguida."""
    n = int(dur_s * SR)
    t = np.arange(n) / SR
    env = np.minimum(t / attack_s, 1.0) * np.exp(-t / tau_s)
    x = static_filter(rng.standard_normal(n), SR, fc=fc, q=q, kind=kind)
    if kind == "lowpass":
        x = static_filter(x, SR, fc=60.0, q=0.7, kind="highpass")
    return x * env


def _crackle_train(n: int, rng: np.random.Generator, rate: np.ndarray, band: tuple[float, float], click_s: tuple[float, float] = (0.0006, 0.003)) -> np.ndarray:
    """Chasquidos de Poisson con tasa instantanea `rate` (por muestra, en
    chasquidos/s): cada uno es un impulso de ruido pasabanda de pocos ms,
    como una fibra de raiz o un terron que se rompe."""
    out = np.zeros(n)
    prob = np.clip(rate / SR, 0.0, 1.0)
    for pos in np.nonzero(rng.random(n) < prob)[0]:
        cn = max(int(rng.uniform(*click_s) * SR), 8)
        fc = float(np.exp(rng.uniform(np.log(band[0]), np.log(band[1]))))
        click = static_filter(rng.standard_normal(cn), SR, fc=fc, q=1.2, kind="bandpass")
        click *= np.exp(-np.arange(cn) / cn * 4.0) * rng.uniform(0.3, 1.0)
        _place(out, click, int(pos))
    return out


def _to_loudness(out: np.ndarray, target_lufs: float) -> np.ndarray:
    """Lleva el maximo de sonoridad momentanea (100 ms) a `target_lufs`, y si
    algun chasquido suelto se pasa del techo lo recorta con suavidad: con
    chasquidos de 1 ms el pico no dice nada de lo fuerte que se oye."""
    out = out * 10 ** ((target_lufs - k_weighted_momentary_max(out, sr=SR)) / 20.0)
    if np.max(np.abs(out)) > 0.85:
        out = 0.85 * np.tanh(out / 0.85)
    return out


def _pebbles(out: np.ndarray, rng: np.random.Generator, start: int, span_s: float, count: int, gain: float) -> None:
    """Chinas que suenan contra la hoja o entre si: tics cortos y agudos,
    con un modo pequeño (piedra de 1-2 cm, 2.5-5 kHz)."""
    for _ in range(count):
        pos = start + int(rng.uniform(0.0, span_s) * SR)
        tick = modal_hit(SR, 0.03, base_freq=rng.uniform(2500, 5000), mode_ratios=[1.0, 1.6], mode_dampings_s=[0.004, 0.002], mode_amps=[1.0, 0.4], rng=rng, detune=0.05)
        _place(out, tick * rng.uniform(0.05, 0.14) * gain, pos)


def garden_dig(name: str) -> np.ndarray:
    """Cavar con pala: la hoja entra en la tierra (roce granular denso y un
    "chuf" grave sin tono al compactarla, con alguna china contra el metal y
    un breve timbre de la hoja), el pie empuja y la hoja se hunde un poco
    mas con fibras que se rompen, y la palada cae a un lado: golpe sordo de
    la masa de tierra y un reguero de terrones que se va espaciando.

    El peso viene de ruido grave filtrado, no de un modo resonante: un seno
    a 80-100 Hz sonaba a tambor."""
    rng = rng_for(name)
    dur = rng.uniform(1.0, 1.25)
    n = int(dur * SR)
    out = np.zeros(n)

    # 1) Entrada de la hoja: compresion de la tierra (grave de banda ancha)
    # mas el roce del metal contra el grano, que se frena en 150-200 ms.
    _place(out, _burst(rng, 0.25, fc=rng.uniform(220, 300), tau_s=0.045) * 1.5, 0)
    _place(out, _burst(rng, 0.12, fc=rng.uniform(700, 1000), tau_s=0.02, kind="bandpass", q=0.8) * 0.5, 0)
    bite_n = int(rng.uniform(0.15, 0.2) * SR)
    rate = 900.0 * np.linspace(1.0, 0.15, bite_n) ** 1.5
    grind = _crackle_train(bite_n, rng, rate, band=(600.0, 4500.0))
    _place(out, grind * 0.5, int(0.003 * SR))
    ring = modal_hit(SR, 0.12, base_freq=rng.uniform(1700, 2300), mode_ratios=[1.0, 2.7, 5.2], mode_dampings_s=[0.03, 0.015, 0.008], mode_amps=[1.0, 0.5, 0.3], rng=rng, detune=0.02)
    _place(out, ring * 0.035, 0)
    _pebbles(out, rng, 0, 0.12, int(rng.integers(1, 4)), 1.0)

    # 2) Empuje con el pie: la hoja se hunde otro poco (grave mas debil y
    # fibras de raiz que ceden).
    push = int(rng.uniform(0.2, 0.28) * SR)
    _place(out, _burst(rng, 0.18, fc=rng.uniform(250, 350), tau_s=0.035) * 0.8, push)
    push_n = int(0.12 * SR)
    _place(out, _crackle_train(push_n, rng, 350.0 * np.linspace(1.0, 0.2, push_n), band=(500.0, 3000.0)) * 0.35, push)

    # Se levanta la palada: la tierra se asienta y resbala sobre la hoja.
    dump = int(rng.uniform(0.52, 0.62) * SR)
    lift = push + int(0.1 * SR)
    lift_n = dump - lift
    settle = _crackle_train(lift_n, rng, np.full(lift_n, 70.0), band=(500.0, 2500.0), click_s=(0.002, 0.006))
    _place(out, settle * np.sin(np.pi * np.arange(lift_n) / lift_n) * 0.2, lift)

    # 3) La palada cae a un lado: golpe sordo de la masa y terrones.
    _place(out, _burst(rng, 0.3, fc=rng.uniform(300, 420), tau_s=0.06, attack_s=0.008) * 1.2, dump)
    trickle_n = n - dump
    t = np.arange(trickle_n) / SR
    trickle_rate = 260.0 * np.exp(-t / 0.12) + 12.0 * np.exp(-t / 0.3)
    _place(out, _crackle_train(trickle_n, rng, trickle_rate, band=(400.0, 3500.0), click_s=(0.002, 0.008)) * 0.45, dump)
    for _ in range(int(rng.integers(3, 6))):
        pos = dump + int(rng.exponential(0.12) * SR)
        _place(out, _burst(rng, 0.06, fc=rng.uniform(350, 700), tau_s=0.012) * rng.uniform(0.15, 0.35), pos)
    _pebbles(out, rng, dump, 0.3, int(rng.integers(1, 3)), 0.8)

    out *= fit_length(ar_envelope(SR, 0.001, 0.06, hold_s=dur - 0.061), n)
    return _to_loudness(out, -12.0)


def _impact_bank(rng: np.random.Generator, count: int, length_s: float, fc_range: tuple[float, float],
                 q: float, tau_range: tuple[float, float], kind: str) -> list[np.ndarray]:
    """Nucleos de impacto de gota: rafaga de ruido de pocos ms con caida
    exponencial y filtrada. Se preparan unas pocas variantes y cada gota usa
    una al azar (convolucion por variante: miles de gotas sin filtrar una a una)."""
    n = int(length_s * SR)
    t = np.arange(n) / SR
    bank = []
    for _ in range(count):
        burst = rng.standard_normal(n) * np.exp(-t / rng.uniform(*tau_range))
        burst = static_filter(burst, SR, fc=rng.uniform(*fc_range), q=q, kind=kind)
        bank.append(burst / (np.abs(burst).max() + 1e-12))
    return bank


def _scatter_impacts(n: int, rate: np.ndarray, rng: np.random.Generator, bank: list[np.ndarray]) -> tuple[np.ndarray, list[int]]:
    """Gotas de Poisson con tasa instantanea `rate` (gotas/s por muestra):
    cada una suena con un nucleo del banco y amplitud aleatoria. Devuelve la
    senal y las posiciones (para colgar burbujas de algunas de ellas)."""
    hits = rng.random(n) < rate / SR
    positions = np.nonzero(hits)[0]
    trains = np.zeros((len(bank), n))
    choice = rng.integers(0, len(bank), len(positions))
    amps = rng.uniform(0.3, 1.0, len(positions)) ** 1.5 * rng.choice([-1.0, 1.0], len(positions))
    trains[choice, positions] = amps
    out = np.zeros(n)
    for train, kernel in zip(trains, bank):
        out += fftconvolve(train, kernel)[:n]
    return out, positions.tolist()


def garden_water(name: str) -> np.ndarray:
    """Regar con un recipiente (coco, calabaza o cantimplora): el agua no es
    un soplido continuo sino miles de gotas.

    - Caudal Q(t): sube al inclinar el recipiente, se sostiene y se corta al
      enderezarlo. Cada pocos decimos de segundo entra aire por la boca del
      recipiente: el chorro titubea y suena el "glup" grave de esa burbuja
      grande (Minnaert de 8-14 mm, 230-400 Hz), el borboteo tipico de verter.
    - Impactos: gotas de Poisson con tasa proporcional al caudal. Sobre la
      tierra mojada son sordas (paso bajo); una fraccion salpica las hojas del
      cultivo y suena mas brillante.
    - Charco: se llena con el caudal y lo absorbe la tierra; cuanto mas lleno,
      mas gotas dejan una burbuja pequena (1-3 kHz, el "chapoteo" del agua
      sobre agua).
    - Cola: al cortar quedan unas gotas sueltas que caen al charco (plic) y el
      siseo de la tierra que bebe."""
    rng = rng_for(name)
    dur = rng.uniform(2.1, 2.5)
    n = int(dur * SR)
    t = np.arange(n) / SR

    # --- Caudal -----------------------------------------------------------
    t_on = rng.uniform(0.12, 0.18)
    t_off = 0.76 * dur
    taper = rng.uniform(0.14, 0.2)
    flow = np.clip(t / t_on, 0.0, 1.0) ** 1.5
    flow *= np.clip(1.0 - (t - t_off) / taper, 0.0, 1.0) ** 1.2
    flow *= smooth_random_walk(n, rng, smoothing_hz=2.0, sr=SR, low=0.8, high=1.0)

    # Borboteo: el aire entra por la boca a 4-7 veces por segundo.
    glug_rate = rng.uniform(4.0, 6.5)
    glug_times = []
    g = t_on * 0.8
    while g < t_off:
        glug_times.append(g)
        g += rng.uniform(0.7, 1.3) / glug_rate
    stall = np.zeros(n)
    glugs = np.zeros(n)
    for gt in glug_times:
        pos = int(gt * SR)
        width = int(rng.uniform(0.04, 0.06) * SR)
        dip = np.hanning(2 * width)
        end = min(pos + len(dip), n)
        stall[pos:end] = np.maximum(stall[pos:end], dip[: end - pos] * rng.uniform(0.4, 0.55))
        # Burbuja grande que entra por la boca: resuena mas que las del charco
        # (25-45 ms) y sube de tono al ascender por el recipiente.
        f0 = 3260.0 / rng.uniform(8.0, 14.0)
        tau = rng.uniform(0.025, 0.045)
        gt_n = int(tau * 5 * SR)
        tt = np.arange(gt_n) / SR
        phase = 2 * np.pi * np.cumsum(f0 * (1.0 + rng.uniform(0.3, 0.7) * tt / (tau * 5))) / SR
        big = np.sin(phase) * np.exp(-tt / tau) * np.clip(tt / 0.003, 0.0, 1.0)
        _place(glugs, big * rng.uniform(0.6, 1.0), pos + int(0.01 * SR))
    flow *= 1.0 - stall

    # --- Impactos del chorro ----------------------------------------------
    soil_bank = _impact_bank(rng, 6, 0.012, (900.0, 2600.0), 0.8, (0.0008, 0.0025), "lowpass")
    leaf_bank = _impact_bank(rng, 6, 0.010, (2400.0, 5200.0), 1.6, (0.0006, 0.0016), "bandpass")
    soil, soil_pos = _scatter_impacts(n, 650.0 * flow, rng, soil_bank)
    leaves, _ = _scatter_impacts(n, 90.0 * flow, rng, leaf_bank)

    # Peso de la columna de agua que entra en la tierra: grave y sordo, sigue al caudal.
    plunge = static_filter(pink_noise(n, rng), SR, fc=420.0, q=0.7, kind="lowpass")
    plunge = static_filter(plunge, SR, fc=90.0, q=0.7, kind="highpass") * flow

    # --- Charco y burbujas ------------------------------------------------
    puddle = np.zeros(n)
    level = 0.0
    fill, soak = 1.4 / SR, 0.9 / SR
    for i in range(0, n, 64):
        level = min(1.0, level + (flow[i] * fill - level * soak) * 64)
        puddle[i : i + 64] = level
    bubbles = np.zeros(n)
    for pos in soil_pos:
        if rng.random() < 0.02 + 0.22 * puddle[pos]:
            _place(bubbles, _bubble(rng, rng.uniform(1.1, 3.0)) * rng.uniform(0.2, 0.55), pos)

    # --- Cola: gotas sueltas y tierra que bebe -------------------------------
    tail = np.zeros(n)
    drip_t = t_off + taper + rng.uniform(0.02, 0.06)
    gap = rng.uniform(0.07, 0.1)
    while drip_t < dur - 0.08:
        pos = int(drip_t * SR)
        drop = soil_bank[int(rng.integers(0, len(soil_bank)))] * rng.uniform(0.35, 0.6)
        _place(tail, drop, pos)
        _place(tail, _bubble(rng, rng.uniform(1.4, 2.6)) * rng.uniform(0.35, 0.6), pos + int(0.002 * SR))
        drip_t += gap
        gap *= rng.uniform(1.3, 1.7)
    soak_env = np.clip((t - t_off) / 0.2, 0.0, 1.0) * np.exp(-np.clip(t - t_off - 0.2, 0.0, None) / 0.35)
    fizz = render_noise_grains(n, SR, rng, rate_hz=260.0, grain_len_s_range=(0.0005, 0.0015), band_hz_range=(3000, 7000), q=1.2, amp_scale=0.08)
    tail += fizz * soak_env

    out = soil * 0.55 + leaves * 0.6 + plunge * 0.15 + bubbles * 0.4 + glugs * 0.35 + tail
    out = static_filter(out, SR, fc=70.0, q=0.7, kind="highpass")
    fade = fit_length(ar_envelope(SR, 0.004, 0.04, hold_s=dur - 0.044), n)
    return out * fade * 0.5


def _leaf_swish(rng: np.random.Generator, dur_s: float) -> np.ndarray:
    """Sacudida de follaje: roce de hojas (granos agudos) bajo una envolvente
    en campana, con el brillo subiendo y bajando con la velocidad del gesto."""
    n = int(dur_s * SR)
    leaves = render_noise_grains(n, SR, rng, rate_hz=420.0, grain_len_s_range=(0.004, 0.018), band_hz_range=(1500, 5500), q=1.2, amp_scale=0.5)
    speed = np.sin(np.pi * np.arange(n) / n) ** 1.5
    return time_varying_filter(leaves, SR, 2000.0 + 4000.0 * speed, q=0.6, kind="lowpass") * speed


def garden_harvest(name: str) -> np.ndarray:
    """Cosechar arrancando la planta: la mano agarra el tallo (roce breve de
    hojas), se tira y la tension crece (chasquidos de fibras de raiz cada vez
    mas seguidos sobre el crujir grave de la tierra que se agrieta), la raiz
    cede con un "tup" sordo del cepellon al soltarse y un desgarro final, y
    se sacude la planta: dos pasadas de hojas y tierra suelta que cae.

    El grave del cepellon es ruido filtrado, no un modo resonante: un seno a
    100 Hz dominaba el sonido (8 de cada 10 partes de la energia) y sonaba a
    bombo."""
    rng = rng_for(name)
    dur = rng.uniform(1.0, 1.3)
    n = int(dur * SR)
    out = np.zeros(n)

    # 1) Agarre: roce corto de hojas.
    _place(out, _leaf_swish(rng, rng.uniform(0.08, 0.12)) * 0.3, 0)

    # 2) Tiron: fibras que chasquean cada vez mas seguidas y tierra que se
    # agrieta por debajo, las dos siguiendo la tension.
    pull_start = int(rng.uniform(0.08, 0.12) * SR)
    pull_n = int(rng.uniform(0.3, 0.42) * SR)
    tension = np.linspace(0.0, 1.0, pull_n) ** 1.8
    fibers = _crackle_train(pull_n, rng, 30.0 + 420.0 * tension, band=(700.0, 3500.0), click_s=(0.0005, 0.002))
    _place(out, fibers * (0.25 + 0.75 * tension) * 0.6, pull_start)
    crack = _crackle_train(pull_n, rng, 40.0 + 160.0 * tension, band=(180.0, 700.0), click_s=(0.004, 0.012))
    _place(out, crack * tension * 0.6, pull_start)

    # 3) La raiz cede: golpe sordo del cepellon y desgarro final.
    release = pull_start + pull_n
    _place(out, _burst(rng, 0.2, fc=rng.uniform(220, 300), tau_s=0.03, kind="bandpass", q=0.9, attack_s=0.001) * 1.4, release)
    snap_n = int(0.05 * SR)
    snap = _crackle_train(snap_n, rng, np.full(snap_n, 1500.0), band=(800.0, 4000.0))
    _place(out, snap * np.exp(-np.arange(snap_n) / SR / 0.015) * 0.8, release)

    # 4) Sacudida: dos pasadas de hojas y la tierra que suelta el cepellon.
    shake = release + int(rng.uniform(0.08, 0.12) * SR)
    for k in range(2):
        pos = shake + int(k * rng.uniform(0.16, 0.22) * SR)
        _place(out, _leaf_swish(rng, rng.uniform(0.14, 0.2)) * (0.3 if k == 0 else 0.2), pos)
    soil_n = n - shake
    t = np.arange(soil_n) / SR
    soil = _crackle_train(soil_n, rng, 120.0 * np.exp(-t / 0.2), band=(400.0, 2500.0), click_s=(0.002, 0.007))
    _place(out, soil * 0.35, shake)
    for _ in range(int(rng.integers(2, 4))):
        pos = shake + int(rng.exponential(0.12) * SR)
        _place(out, _burst(rng, 0.05, fc=rng.uniform(350, 650), tau_s=0.01) * rng.uniform(0.1, 0.25), pos)

    out *= fit_length(ar_envelope(SR, 0.002, 0.06, hold_s=dur - 0.062), n)
    return _to_loudness(out, -14.0)
