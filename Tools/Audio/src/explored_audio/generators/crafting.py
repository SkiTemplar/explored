"""Herramientas y fabricacion (biblia de contenido §3.4, §5.3): tallar, atar,
golpear piedra (talla de pedernal), cortar madera con sierra, encender una
hoguera y el chisporroteo de cocinar. Reutiliza el mismo instrumental DSP que
los golpes de `impacts.py` (transitorio + modal) y las texturas granulares de
`footsteps.py`/`ambience.py`."""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, fit_length, smooth_random_walk
from ..filters import static_filter
from ..granular import render_noise_grains
from ..levels import k_weighted_momentary_max
from ..loop import seamless_loop
from ..modal import modal_hit
from ..noise import pink_noise
from ..rng import rng_for

SR = SAMPLE_RATE


def carve(name: str) -> np.ndarray:
    """Tallar: varias pasadas de rasguño (granulado de banda alta), cada una
    con su propia envolvente de vaiven."""
    rng = rng_for(name)
    n_strokes = int(rng.integers(3, 6))
    pieces = []
    for _ in range(n_strokes):
        dur = rng.uniform(0.12, 0.22)
        n = int(dur * SR)
        scrape = render_noise_grains(n, SR, rng, rate_hz=60.0, grain_len_s_range=(0.006, 0.018), band_hz_range=(1800, 5200), q=1.3, amp_scale=0.5)
        env = fit_length(ar_envelope(SR, dur * 0.15, dur * 0.7, shape=1.3), n)
        pieces.append(scrape * env)
        pieces.append(np.zeros(int(rng.uniform(0.04, 0.09) * SR)))
    return np.concatenate(pieces) if pieces else np.zeros(1)


def tie_cord(name: str) -> np.ndarray:
    """Atar con cuerda de fibra (coco, hibisco): dos o tres tirones que
    pasan la cuerda por la lazada y un ultimo tiron que aprieta el nudo.

    La fibra no silba como un ruido continuo: cruje por friccion a saltos
    (stick-slip). Cada tiron es un tren de micro-deslizamientos cuya
    frecuencia sube con la tension (de ~40 a ~140 por segundo) y que suena
    a traves de la resonancia de la fibra; encima va un roce suave de la
    cuerda contra si misma. El apriete final se oye como un crujido mas
    denso que se frena de golpe y el golpe grave del nudo al asentarse."""
    rng = rng_for(name)
    n_pulls = int(rng.integers(2, 4))
    pieces: list[np.ndarray] = [np.zeros(int(0.01 * SR))]
    for k in range(n_pulls + 1):
        final = k == n_pulls
        dur = rng.uniform(0.16, 0.22) if final else rng.uniform(0.12, 0.2)
        n = int(dur * SR)
        u = np.arange(n) / n
        tension = u ** (0.6 if final else 1.0)
        rate = (40.0 + (140.0 if final else 90.0) * tension) * smooth_random_walk(n, rng, smoothing_hz=25.0, sr=SR, low=0.7, high=1.3)
        slips = np.nonzero(np.diff(np.floor(np.cumsum(rate) / SR)) > 0)[0] + 1
        pulses = np.zeros(n)
        pulses[slips] = rng.uniform(0.4, 1.0, slips.size)
        fibre = static_filter(pulses, SR, fc=rng.uniform(1300, 1900) * (1.15 if final else 1.0), q=4.0, kind="bandpass")
        fibre += 0.5 * static_filter(pulses, SR, fc=rng.uniform(3200, 4200), q=3.0, kind="bandpass")
        # Los saltos mas fuertes asoman ~15 dB sobre el crujido: se recortan
        # con una saturacion suave para que el conjunto no quede bajo.
        fibre = np.tanh(3.0 * fibre / (np.max(np.abs(fibre)) + 1e-9)) / np.tanh(3.0)
        rub = static_filter(pink_noise(n, rng), SR, fc=rng.uniform(900, 1400), q=0.9, kind="bandpass")
        rub /= np.max(np.abs(rub)) + 1e-9
        # Entra suave y se frena en seco: la cuerda deja de correr.
        env = np.minimum(u / 0.25, 1.0) * np.where(u > 0.9, (1.0 - u) / 0.1, 1.0)
        pull = (0.8 * fibre + 0.25 * rub) * env * (1.0 if final else rng.uniform(0.6, 0.8))
        if final:
            knot = modal_hit(SR, 0.08, base_freq=rng.uniform(150, 200), mode_ratios=[1.0, 1.7, 2.9],
                             mode_dampings_s=[0.02, 0.012, 0.006], mode_amps=[1.0, 0.5, 0.25], rng=rng, detune=0.03)
            pull = np.concatenate([pull, np.zeros(len(knot))])
            pull[n - int(0.004 * SR) : n - int(0.004 * SR) + len(knot)] += 0.22 * knot
        pieces.append(pull)
        pieces.append(np.zeros(int(rng.uniform(0.05, 0.1) * SR)))
    return _to_momentary(_tail_fade(np.concatenate(pieces)), -16.5)


def _to_momentary(x: np.ndarray, target: float, peak_cap: float = 0.85) -> np.ndarray:
    """Lleva `x` a una sonoridad momentanea maxima (ventana de 100 ms, como
    los pasos) sin pasar de `peak_cap`: el limitador `tanh` de `finalize`
    achataria el transitorio, que es lo que hace legible el golpe."""
    out = x * 10.0 ** ((target - k_weighted_momentary_max(x)) / 20.0)
    peak = np.max(np.abs(out))
    return out * (peak_cap / peak) if peak > peak_cap else out


def _tail_fade(x: np.ndarray, fade_s: float = 0.01) -> np.ndarray:
    fade = min(int(fade_s * SR), len(x))
    x[-fade:] *= np.linspace(1.0, 0.0, fade)
    return x


def stone_knap(name: str) -> np.ndarray:
    """Talla de piedra con percutor: el golpe seco de piedra contra piedra,
    el "tink" vitreo y breve del nucleo (el pedernal y la obsidiana suenan
    a cristal, con modos inarmonicos agudos), el cuerpo sordo de las dos
    piedras sujetas en la mano y, a veces, la lasca que cae y tintinea en
    el suelo. Mas agudo y corto que `sfx_stone_hit` (golpe generico de
    herramienta): una lasca saltando, no un golpe sordo."""
    rng = rng_for(name)
    dur = rng.uniform(0.32, 0.42)
    n = int(dur * SR)
    out = np.zeros(n)
    onset = int(0.003 * SR)

    # Contacto: chasquido de banda ancha de ~1 ms (dos superficies duras).
    click_n = int(rng.uniform(0.0008, 0.0014) * SR)
    click = static_filter(rng.standard_normal(click_n), SR, fc=rng.uniform(2500, 3500), q=0.7, kind="highpass")
    click *= np.exp(-np.arange(click_n) / (click_n / 3.0))
    out[onset : onset + click_n] += 0.3 * click / (np.max(np.abs(click)) + 1e-9)

    # Nucleo vitreo: modos agudos inarmonicos que se apagan en pocos ms (la
    # mano los amortigua). Frecuencia distinta en cada variante.
    core = modal_hit(
        SR, dur, base_freq=rng.uniform(2600, 3800),
        mode_ratios=[1.0, 1.47, 2.09, 2.81], mode_dampings_s=[0.03, 0.02, 0.012, 0.007],
        mode_amps=[1.0, 0.7, 0.45, 0.3], rng=rng, detune=0.03,
    )
    out[onset:] += 0.55 * core[: n - onset]

    # Cuerpo de las piedras en la mano: resonancia media, muy amortiguada.
    body = modal_hit(
        SR, dur, base_freq=rng.uniform(650, 950),
        mode_ratios=[1.0, 1.9], mode_dampings_s=[0.014, 0.008], mode_amps=[1.0, 0.4], rng=rng, detune=0.05,
    )
    out[onset:] += 0.35 * body[: n - onset]

    # Golpe grave del percutor: da peso al impacto sin volverlo sordo.
    thump_n = int(0.04 * SR)
    t = np.arange(thump_n) / SR
    thump = static_filter(rng.standard_normal(thump_n), SR, fc=320.0, q=0.8, kind="lowpass")
    thump *= (1.0 - np.exp(-t / 0.001)) * np.exp(-t / 0.007)
    out[onset : onset + thump_n] += 0.35 * thump / (np.max(np.abs(thump)) + 1e-9)

    # La lasca cae: dos o tres toques agudos y flojos tras 90-200 ms.
    if rng.uniform() < 0.75:
        pos = onset + int(rng.uniform(0.09, 0.2) * SR)
        freq = rng.uniform(4200, 6500)
        for _ in range(int(rng.integers(2, 4))):
            if pos >= n - int(0.03 * SR):
                break
            tick = modal_hit(SR, 0.03, base_freq=freq * rng.uniform(0.97, 1.03), mode_ratios=[1.0, 1.73],
                             mode_dampings_s=[0.005, 0.003], mode_amps=[1.0, 0.4], rng=rng, detune=0.02)
            out[pos : pos + len(tick)] += rng.uniform(0.05, 0.1) * tick
            pos += int(rng.uniform(0.03, 0.06) * SR)

    return _to_momentary(_tail_fade(out), rng.uniform(-15.5, -14.5))


def wood_saw(name: str) -> np.ndarray:
    """Serrar madera con sierra de dientes de tiburon, a vaiven.

    Cada diente que muerde la fibra es un micro-impacto: el ritmo al que
    pasan los dientes sigue la velocidad de la hoja (0 en los extremos,
    unos 110-150 Hz a media pasada), asi que el sonido tiene un "zumbido"
    que sube y baja con cada pasada en vez de ser ruido estacionario. Esos
    impactos excitan las resonancias del tronco (lo que hace que suene a
    madera y no a lija) y un raspado fino de banda alta. La ida corta y
    suena mas fuerte y aspera que la vuelta, y al final de cada ida cae un
    poco de serrin."""
    rng = rng_for(name)
    dur = rng.uniform(1.6, 2.0)
    n = int(dur * SR)

    # Posicion de la hoja: sinusoide con un vaiven de 1,7-2,1 Hz y algo de
    # irregularidad de pasada a pasada (la mano no es un metronomo).
    stroke_hz = rng.uniform(1.7, 2.1)
    wobble = smooth_random_walk(n, rng, smoothing_hz=1.0, sr=SR, low=0.9, high=1.1)
    phase = 2 * np.pi * np.cumsum(stroke_hz * wobble) / SR + rng.uniform(0, 2 * np.pi)
    velocity = np.cos(phase)  # >0: ida (corta), <0: vuelta
    speed = np.abs(velocity)
    bite = np.where(velocity > 0, 1.0, rng.uniform(0.35, 0.5))

    # Tren de impulsos de los dientes: un impulso cada vez que la hoja avanza
    # un paso de diente. Amplitud segun velocidad, mordida y variacion.
    tooth_rate = speed * rng.uniform(110, 150)
    teeth_phase = np.cumsum(tooth_rate) / SR
    hits = np.nonzero(np.diff(np.floor(teeth_phase)) > 0)[0] + 1
    pulses = np.zeros(n)
    pulses[hits] = (speed[hits] ** 1.3) * bite[hits] * rng.uniform(0.5, 1.0, hits.size)

    # Cada diente: chasquido corto de fibra (ruido de ~1 ms) ...
    click_kernel = static_filter(rng.standard_normal(96), SR, fc=2600.0, q=0.9, kind="bandpass")
    click_kernel *= np.exp(-np.arange(96) / 24.0)
    teeth = np.convolve(pulses, click_kernel)[:n]
    # ... y el tronco que resuena con cada mordida.
    log = np.zeros(n)
    for fc, q, amp in ((rng.uniform(300, 380), 5.0, 1.0), (rng.uniform(720, 860), 6.0, 0.8), (rng.uniform(1450, 1700), 5.0, 0.5)):
        band = static_filter(pulses, SR, fc=fc, q=q, kind="bandpass")
        log += amp * band / (np.max(np.abs(band)) + 1e-9)

    # Raspado continuo de la hoja en el corte, mas fuerte al ir mas rapido.
    rasp = static_filter(rng.standard_normal(n), SR, fc=3200.0, q=0.8, kind="bandpass")
    rasp *= (0.08 + speed) * bite

    teeth /= np.max(np.abs(teeth)) + 1e-9
    log /= np.max(np.abs(log)) + 1e-9
    rasp /= np.max(np.abs(rasp)) + 1e-9
    out = 0.4 * teeth + 0.6 * log + 0.25 * rasp
    # Nada de siseo por encima de 7 kHz: la madera humeda no chirria.
    out = static_filter(out, SR, fc=7000.0, q=0.7, kind="lowpass")

    # Serrin al final de cada ida: granos agudos y muy flojos.
    turns = np.nonzero((velocity[:-1] > 0) & (velocity[1:] <= 0))[0]
    for pos in turns:
        dust = render_noise_grains(int(0.12 * SR), SR, rng, rate_hz=90.0, grain_len_s_range=(0.002, 0.006),
                                   band_hz_range=(3500, 8000), q=1.5, amp_scale=0.5)
        dust *= np.exp(-np.arange(len(dust)) / SR / 0.04)
        end = min(pos + len(dust), n)
        out[pos:end] += 0.12 * dust[: end - pos] / (np.max(np.abs(dust)) + 1e-9)

    # Los dientes que coinciden con el tronco ya resonando dan picos sueltos
    # de ~18 dB sobre el nivel medio que obligarian a bajar todo el sonido.
    # Una saturacion suave los recorta sin tocar la textura (es ruido).
    out = np.tanh(2.0 * out / (np.max(np.abs(out)) + 1e-9)) / np.tanh(2.0)
    env = fit_length(ar_envelope(SR, 0.06, 0.12, hold_s=dur - 0.18, shape=1.0), n)
    return _to_momentary(_tail_fade(out * env), -15.5)


def fire_ignite(name: str) -> np.ndarray:
    """Hoguera encendiendose (complementa el bucle de crepitar,
    `sfx_fire_loop`): unas pocas chispas sueltas que se espesan en un breve
    soplo de llama prendiendo."""
    rng = rng_for(name)
    dur = rng.uniform(1.0, 1.4)
    n = int(dur * SR)
    sparks = np.zeros(n)
    n_sparks = int(rng.integers(5, 9))
    for i in range(n_sparks):
        pos = int((i / n_sparks) * n * rng.uniform(0.6, 0.9))
        glen = max(int(rng.uniform(0.008, 0.02) * SR), 8)
        pop = static_filter(rng.standard_normal(glen), SR, fc=rng.uniform(2000, 5000), q=1.4, kind="highpass")
        pop = pop * np.exp(-np.arange(glen) / SR / 0.012)
        end = min(pos + glen, n)
        sparks[pos:end] += pop[: end - pos] * rng.uniform(0.4, 0.7)
    whoosh = static_filter(pink_noise(n, rng), SR, fc=1600, q=0.6, kind="lowpass")
    whoosh_env = fit_length(ar_envelope(SR, dur * 0.55, dur * 0.4, shape=1.4), n)
    return sparks * 0.8 + whoosh * whoosh_env * 0.35


def cooking_sizzle(name: str) -> np.ndarray:
    """Cocinar: chisporroteo denso en banda alta (comida sobre piedra
    caliente), en bucle mientras dura la coccion."""
    rng = rng_for(name)
    loop_s, fade_s = 8.0, 1.5
    loop_len = int(loop_s * SR)
    fade_len = int(fade_s * SR)
    n = loop_len + fade_len
    sizzle = render_noise_grains(n, SR, rng, rate_hz=140.0, grain_len_s_range=(0.006, 0.02), band_hz_range=(3000, 9000), q=2.2, amp_scale=0.35)
    amp = smooth_random_walk(n, rng, smoothing_hz=0.4, sr=SR, low=0.55, high=1.0)
    mono = sizzle * amp
    return seamless_loop(mono, loop_len, fade_len)
