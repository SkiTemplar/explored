"""Golpes de herramienta contra el mundo: hacha en un tronco en pie y
herramienta de piedra contra roca.

Un golpe real no es una nota: es un contacto de banda ancha, un cuerpo muy
amortiguado (un tronco vivo o una roca apoyada en el suelo apenas resuenan)
y los restos que se desprenden (fibras que se rasgan, arenilla que cae).
Antes se modelaban solo con modos resonantes largos y sonaban a marimba y
a campana (el 95 % de la energia en una banda y 300 ms hasta -30 dB)."""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..filters import static_filter
from ..modal import modal_hit
from ..rng import rng_for

SR = SAMPLE_RATE
ONSET_S = 0.003
# Pico de salida antes del techo comun de build.finalize: deja margen al limitador.
OUT_PEAK = 0.85
# Saturacion suave del conjunto: el chasquido de contacto dura 1-2 ms y, sin
# ella, fija el pico y deja el golpe ~8 dB por debajo del resto de efectos.
DRIVE = 3.0


def _burst(rng: np.random.Generator, n: int, fc: float, q: float, kind: str, attack_s: float, decay_s: float) -> np.ndarray:
    """Ruido filtrado con ataque y caida exponenciales, normalizado a pico 1."""
    t = np.arange(n) / SR
    x = static_filter(rng.standard_normal(n), SR, fc=fc, q=q, kind=kind)
    x *= (1.0 - np.exp(-t / attack_s)) * np.exp(-t / decay_s)
    return x / (np.max(np.abs(x)) + 1e-9)


def _ticks(rng: np.random.Generator, n: int, times_s: np.ndarray, amps: np.ndarray, band_hz: tuple[float, float], q: float) -> np.ndarray:
    """Tren de impulsos sueltos a traves de una resonancia: fibras que saltan,
    granos que rebotan. Cada impulso lleva su propia frecuencia de banda."""
    out = np.zeros(n)
    tick_n = int(0.004 * SR)
    for t0, amp in zip(times_s, amps, strict=True):
        pos = int(t0 * SR)
        if pos + tick_n >= n:
            continue
        imp = np.zeros(tick_n)
        imp[0] = 1.0
        imp[1] = -0.6
        ring = static_filter(imp, SR, fc=rng.uniform(*band_hz), q=q, kind="bandpass")
        out[pos : pos + tick_n] += amp * ring / (np.max(np.abs(ring)) + 1e-9)
    return out


def _add(out: np.ndarray, x: np.ndarray, pos: int, gain: float) -> None:
    end = min(out.size, pos + x.size)
    out[pos:end] += gain * x[: end - pos]


def _finish(out: np.ndarray) -> np.ndarray:
    # El golpe grave es casi de una sola polaridad: con continua dentro del
    # cuerpo, la media que retira build.finalize desplaza el silencio de los
    # extremos hasta 3e-3. Un paso alto por debajo de lo audible la quita.
    out = static_filter(out, SR, fc=30.0, q=0.707, kind="highpass")
    out = np.tanh(DRIVE * out / (np.max(np.abs(out)) + 1e-9)) / np.tanh(DRIVE)
    fade = int(0.005 * SR)
    out[-fade:] *= np.linspace(1.0, 0.0, fade)
    return out * OUT_PEAK


def wood_chop(name: str) -> np.ndarray:
    """Hachazo en un tronco en pie. Capas:

    - el filo que muerde: chasquido de 1-2 ms por encima de 2 kHz;
    - el golpe sordo del tronco (masa grande y viva, sin apenas eco): ruido
      grave de 25 ms y un modo a 90-140 Hz muy amortiguado;
    - el "toc" de la madera: modos inarmonicos a 350-520 Hz que se apagan en
      pocas decenas de ms, mas ruido de banda media para quitarles la nota;
    - las fibras que se rasgan cuando la hoja entra y se suelta: 6-12
      chasquidos a 1,8-4,5 kHz en los primeros 90 ms, cada vez mas flojos;
    - a veces una astilla que salta y cae 120-260 ms despues.
    """
    rng = rng_for(name)
    dur = rng.uniform(0.42, 0.55)
    n = int(dur * SR)
    out = np.zeros(n)
    onset = int(ONSET_S * SR)
    force = rng.uniform(0.85, 1.0)

    bite_n = int(rng.uniform(0.0012, 0.002) * SR)
    _add(out, _burst(rng, bite_n, fc=rng.uniform(2200, 3000), q=0.7, kind="highpass", attack_s=0.0001, decay_s=bite_n / SR / 3.0), onset, 0.55)

    thud_n = int(0.08 * SR)
    _add(out, _burst(rng, thud_n, fc=260.0, q=0.8, kind="lowpass", attack_s=0.0008, decay_s=0.022), onset, 0.6 * force)
    trunk = modal_hit(SR, 0.2, base_freq=rng.uniform(90, 140), mode_ratios=[1.0, 2.3], mode_dampings_s=[0.03, 0.015], mode_amps=[1.0, 0.35], rng=rng, detune=0.03)
    _add(out, trunk, onset, 0.35 * force)

    knock = modal_hit(
        SR, 0.25, base_freq=rng.uniform(350, 520),
        mode_ratios=[1.0, 1.58, 2.31, 3.12], mode_dampings_s=[0.04, 0.026, 0.017, 0.011],
        mode_amps=[1.0, 0.6, 0.4, 0.22], rng=rng, detune=0.04,
    )
    _add(out, knock, onset, 0.4 * force)
    _add(out, _burst(rng, int(0.1 * SR), fc=rng.uniform(700, 1100), q=1.1, kind="bandpass", attack_s=0.0005, decay_s=0.02), onset, 0.45 * force)

    n_fibres = int(rng.integers(6, 13))
    times = ONSET_S + 0.004 + np.sort(rng.exponential(0.03, n_fibres).clip(0.0, 0.09))
    amps = rng.uniform(0.25, 0.55, n_fibres) * np.exp(-(times - ONSET_S) / 0.05)
    out += _ticks(rng, n, times, amps, band_hz=(1800, 4500), q=3.0)

    if rng.uniform() < 0.6:
        t_chip = ONSET_S + rng.uniform(0.12, 0.26)
        chip_times = t_chip + np.array([0.0, rng.uniform(0.03, 0.06)])
        out += _ticks(rng, n, chip_times, np.array([0.12, 0.06]), band_hz=(2200, 3400), q=6.0)

    return _finish(out)


def stone_hit(name: str) -> np.ndarray:
    """Herramienta de piedra contra una roca apoyada en el suelo. Capas:

    - el contacto duro: chasquido de ~1 ms por encima de 3 kHz;
    - la grieta: ruido de banda 1,5-4 kHz que se apaga en ~8 ms;
    - el cuerpo de la roca: modos inarmonicos a 0,9-1,5 kHz muy amortiguados
      (la roca descansa en tierra y no canta como una campana);
    - el golpe grave de la masa: ruido por debajo de 300 Hz en 15 ms;
    - la arenilla que se desprende: 5-11 granos agudos que caen durante
      ~250 ms mas un siseo breve de polvo.
    Mas grave, mas corto y mas sordo que `sfx_stone_knap` (una lasca vitrea).
    """
    rng = rng_for(name)
    dur = rng.uniform(0.34, 0.44)
    n = int(dur * SR)
    out = np.zeros(n)
    onset = int(ONSET_S * SR)
    force = rng.uniform(0.85, 1.0)

    click_n = int(rng.uniform(0.0008, 0.0013) * SR)
    _add(out, _burst(rng, click_n, fc=rng.uniform(3000, 4000), q=0.7, kind="highpass", attack_s=0.0001, decay_s=click_n / SR / 3.0), onset, 0.6)
    _add(out, _burst(rng, int(0.04 * SR), fc=rng.uniform(2000, 3000), q=0.9, kind="bandpass", attack_s=0.0003, decay_s=0.008), onset, 0.5 * force)

    body = modal_hit(
        SR, 0.15, base_freq=rng.uniform(900, 1500),
        mode_ratios=[1.0, 1.71, 2.53, 3.37], mode_dampings_s=[0.02, 0.013, 0.009, 0.006],
        mode_amps=[1.0, 0.7, 0.45, 0.3], rng=rng, detune=0.05,
    )
    _add(out, body, onset, 0.4 * force)
    _add(out, _burst(rng, int(0.06 * SR), fc=300.0, q=0.8, kind="lowpass", attack_s=0.0006, decay_s=0.015), onset, 0.5 * force)

    n_grains = int(rng.integers(5, 12))
    times = ONSET_S + 0.03 + np.sort(rng.uniform(0.0, 0.22, n_grains))
    amps = rng.uniform(0.06, 0.16, n_grains) * np.exp(-(times - ONSET_S) / 0.15)
    out += _ticks(rng, n, times, amps, band_hz=(3000, 7000), q=5.0)
    _add(out, _burst(rng, int(0.2 * SR), fc=2500.0, q=0.7, kind="highpass", attack_s=0.01, decay_s=0.05), onset + int(0.01 * SR), 0.05)

    return _finish(out)
