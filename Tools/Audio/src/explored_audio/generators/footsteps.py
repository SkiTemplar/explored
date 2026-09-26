"""Veinte pasos mono (5 materiales x 4 variaciones).

Cada paso se modela como dos contactos, talon y punta (el apoyo real de un
pie descalzo o con calzado blando), separados 60-120 ms, con la punta mas
floja que el talon. Cada contacto suma tres capas:

- un golpe grave ("cuerpo" del peso del jugador sobre el suelo: ruido paso
  bajo con decaimiento rapido), que es lo que da peso al paso y faltaba en
  la primera version (todo el contenido quedaba por encima de 400 Hz y el
  paso sonaba a siseo);
- la textura del material (granos de ruido con densidad que decae tras el
  contacto: crujido de arena, chasquido de hierba, grava sobre roca...);
- donde el material lo pide, una resonancia modal breve y poco tonal
  (tablon de madera) o burbujas de Minnaert (agua somera).

La sonoridad final se fija por material (`_TARGET_MOMENTARY`) con un margen
aleatorio pequeño, para que las cuatro variantes de un material suenen al
mismo nivel y los materiales guarden una jerarquia coherente (la hierba mas
suave que la madera hueca).
"""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import fit_length
from ..filters import static_filter, time_varying_filter
from ..levels import k_weighted_momentary_max
from ..modal import modal_hit
from ..rng import rng_for

SR = SAMPLE_RATE

# Sonoridad objetivo por material: maximo de sonoridad momentanea en ventana
# de 100 ms (K-weighting de `levels.py`), no LUFS integrados. Un paso es un
# transitorio con cola: la cifra integrada sin gating depende de cuanto
# silencio quede al final del fichero, la momentanea mide lo que se oye.
_TARGET_MOMENTARY = {
    "sand": -14.5,
    "grass": -15.0,
    "rock": -14.5,
    "wood": -14.0,
    "water": -14.5,
}
_PEAK_CAP = 0.85


def _thump(n: int, onset: int, rng: np.random.Generator, fc: float, tau_s: float) -> np.ndarray:
    """Golpe grave del peso sobre el suelo: ruido paso bajo con ataque de
    ~2 ms y decaimiento exponencial `tau_s`."""
    out = np.zeros(n)
    length = min(int(tau_s * 6 * SR), n - onset)
    if length <= 8:
        return out
    t = np.arange(length) / SR
    env = (1.0 - np.exp(-t / 0.002)) * np.exp(-t / tau_s)
    burst = static_filter(rng.standard_normal(length), SR, fc=fc, q=0.8, kind="lowpass")
    burst = static_filter(burst, SR, fc=45.0, q=0.7, kind="highpass")
    out[onset : onset + length] = burst * env
    peak = np.max(np.abs(out))
    return out / peak if peak > 1e-9 else out


def _grain_burst(
    n: int,
    onset: int,
    rng: np.random.Generator,
    count: int,
    spread_s: float,
    grain_len_s: tuple[float, float],
    band_hz: tuple[float, float],
    q: float,
) -> np.ndarray:
    """Racimo de granos pasabanda tras un contacto. Las posiciones siguen
    una exponencial de media `spread_s`: densos justo al pisar, cada vez mas
    escasos despues (el material se asienta), y su amplitud decae igual."""
    out = np.zeros(n)
    for _ in range(count):
        # Acotado a 3 medias: un grano suelto muy tardio se oye como un
        # "tic" desligado del paso, no como su cola.
        delay = min(rng.exponential(spread_s), 3.0 * spread_s)
        pos = onset + int(delay * SR)
        if pos >= n:
            continue
        glen = max(int(rng.uniform(*grain_len_s) * SR), 16)
        fc = np.exp(rng.uniform(np.log(band_hz[0]), np.log(band_hz[1])))
        grain = static_filter(rng.standard_normal(glen), SR, fc=fc, q=q, kind="bandpass")
        # Envolvente percusiva (ataque corto, cola larga) en vez de Hann:
        # cada grano es un micro-impacto, no un soplo.
        k = np.arange(glen) / glen
        grain *= np.minimum(k * 8.0, 1.0) * (1.0 - k) ** 2
        amp = rng.uniform(0.4, 1.0) * np.exp(-delay / (spread_s * 2.0))
        end = min(pos + glen, n)
        out[pos:end] += grain[: end - pos] * amp
    peak = np.max(np.abs(out))
    return out / peak if peak > 1e-9 else out


def _swish(n: int, onset: int, rng: np.random.Generator, dur_s: float, f_start: float, f_end: float, q: float) -> np.ndarray:
    """Roce de ruido con barrido de filtro (hojas de hierba, agua desplazada)."""
    out = np.zeros(n)
    length = min(int(dur_s * SR), n - onset)
    if length <= 64:
        return out
    k = np.arange(length) / length
    track = f_start * (f_end / f_start) ** k
    body = time_varying_filter(rng.standard_normal(length), SR, track, q=q, kind="bandpass", block_size=128)
    env = np.minimum(k * 6.0, 1.0) * (1.0 - k) ** 1.5
    out[onset : onset + length] = body * env
    peak = np.max(np.abs(out))
    return out / peak if peak > 1e-9 else out


def _click(n: int, onset: int, rng: np.random.Generator, fc: float, dur_s: float = 0.004) -> np.ndarray:
    """Transitorio de contacto muy corto (suela contra superficie dura)."""
    out = np.zeros(n)
    length = min(max(int(dur_s * SR), 8), n - onset)
    if length <= 0:
        return out
    burst = static_filter(rng.standard_normal(length), SR, fc=fc, q=0.7, kind="highpass")
    burst *= np.exp(-np.arange(length) / (length / 4.0))
    out[onset : onset + length] = burst
    peak = np.max(np.abs(out))
    return out / peak if peak > 1e-9 else out


def _bubble(n: int, onset: int, freq: float, tau_s: float, rise: float) -> np.ndarray:
    """Burbuja de Minnaert: seno amortiguado cuyo tono sube al acercarse la
    burbuja a la superficie (`rise` = subida relativa de frecuencia)."""
    out = np.zeros(n)
    length = min(int(tau_s * 6 * SR), n - onset)
    if length <= 8:
        return out
    t = np.arange(length) / SR
    inst_freq = freq * (1.0 + rise * t / (tau_s * 6))
    phase = 2 * np.pi * np.cumsum(inst_freq) / SR
    env = (1.0 - np.exp(-t / 0.0008)) * np.exp(-t / tau_s)
    out[onset : onset + length] = np.sin(phase) * env
    return out


def _contacts(rng: np.random.Generator, n: int, gap_range: tuple[float, float]) -> list[tuple[int, float]]:
    """Talon y punta: (muestra de inicio, peso relativo)."""
    heel = int(rng.uniform(0.002, 0.006) * SR)
    toe = heel + int(rng.uniform(*gap_range) * SR)
    toe = min(toe, n - int(0.04 * SR))
    return [(heel, 1.0), (toe, rng.uniform(0.35, 0.6))]


def _footstep_sand(rng: np.random.Generator) -> np.ndarray:
    n = int(rng.uniform(0.30, 0.36) * SR)
    out = np.zeros(n)
    for onset, w in _contacts(rng, n, (0.07, 0.11)):
        out += w * 0.24 * _thump(n, onset, rng, fc=rng.uniform(160, 220), tau_s=0.028)
        # Crujido de la arena al compactarse: granos cortos y densos en banda media.
        out += w * 0.6 * _grain_burst(n, onset, rng, count=int(rng.uniform(70, 110)), spread_s=0.035,
                                      grain_len_s=(0.002, 0.007), band_hz=(600, 3200), q=1.1)
        # Deslizamiento de arena suelta bajo el pie.
        out += w * 0.18 * _swish(n, onset, rng, rng.uniform(0.08, 0.12), 2500, 900, q=0.6)
    return out


def _footstep_grass(rng: np.random.Generator) -> np.ndarray:
    n = int(rng.uniform(0.30, 0.36) * SR)
    out = np.zeros(n)
    for onset, w in _contacts(rng, n, (0.08, 0.12)):
        out += w * 0.28 * _thump(n, onset, rng, fc=rng.uniform(130, 180), tau_s=0.022)
        # Hojas que se doblan y rozan: barrido ascendente brillante.
        out += w * 0.4 * _swish(n, onset, rng, rng.uniform(0.10, 0.15), 1800, 6000, q=0.9)
        # Tallos que se quiebran: chasquidos dispersos.
        out += w * 0.35 * _grain_burst(n, onset, rng, count=int(rng.uniform(18, 30)), spread_s=0.05,
                                       grain_len_s=(0.0015, 0.005), band_hz=(2000, 7000), q=1.6)
    return out


def _footstep_rock(rng: np.random.Generator) -> np.ndarray:
    n = int(rng.uniform(0.26, 0.32) * SR)
    out = np.zeros(n)
    for onset, w in _contacts(rng, n, (0.06, 0.09)):
        out += w * 0.3 * _thump(n, onset, rng, fc=rng.uniform(220, 300), tau_s=0.014)
        out += w * 0.3 * _click(n, onset, rng, fc=rng.uniform(2500, 4000), dur_s=0.006)
        # Grava suelta: pocos guijarros que saltan con un "tic" modal breve.
        for _ in range(int(rng.integers(2, 6))):
            pos = onset + int(min(rng.exponential(0.025), 0.075) * SR)
            if pos >= n - 64:
                continue
            pebble = modal_hit(SR, 0.03, base_freq=rng.uniform(2200, 4800), mode_ratios=[1.0, 1.6, 2.3],
                               mode_dampings_s=[0.004, 0.003, 0.002], mode_amps=[1.0, 0.5, 0.3], rng=rng, detune=0.05)
            end = min(pos + len(pebble), n)
            out[pos:end] += w * rng.uniform(0.06, 0.14) * pebble[: end - pos]
        # Roce de la suela: raspado corto de media frecuencia.
        out += w * 0.3 * _swish(n, onset, rng, rng.uniform(0.06, 0.09), 3000, 1200, q=0.8)
    return out


def _footstep_wood(rng: np.random.Generator) -> np.ndarray:
    n = int(rng.uniform(0.30, 0.36) * SR)
    out = np.zeros(n)
    plank_freq = rng.uniform(150, 230)  # mismo tablon para talon y punta
    for onset, w in _contacts(rng, n, (0.07, 0.10)):
        out += w * 0.18 * _thump(n, onset, rng, fc=rng.uniform(180, 260), tau_s=0.02)
        out += w * 0.15 * _click(n, onset, rng, fc=rng.uniform(1800, 2800), dur_s=0.006)
        # Tablon hueco: modos inharmonicos de viga libre, amortiguados rapido
        # (madera, no campana) y excitados con ruido para que no suene a seno.
        length = n - onset
        knock = modal_hit(SR, length / SR, base_freq=plank_freq, mode_ratios=[1.0, 2.76, 5.40, 8.93],
                          mode_dampings_s=[0.055, 0.032, 0.018, 0.009], mode_amps=[0.7, 0.6, 0.4, 0.25],
                          rng=rng, detune=0.01)
        body = static_filter(rng.standard_normal(length), SR, fc=plank_freq * 4.0, q=1.0, kind="bandpass")
        body *= np.exp(-np.arange(length) / SR / 0.02)
        knock = fit_length(knock, length)
        knock = knock / (np.max(np.abs(knock)) + 1e-9) * 0.5 + body / (np.max(np.abs(body)) + 1e-9) * 0.5
        out[onset:] += w * 0.45 * knock
    # Crujido ocasional del tablon al soltar el peso: pulsos de friccion
    # (stick-slip) a ~20-40 Hz filtrados en banda media.
    if rng.uniform() < 0.5:
        start = int(rng.uniform(0.14, 0.18) * SR)
        length = min(int(rng.uniform(0.07, 0.11) * SR), n - start)
        if length > 64:
            pulses = np.zeros(length)
            t = 0.0
            while True:
                t += 1.0 / rng.uniform(20, 40)
                idx = int(t * SR)
                if idx >= length:
                    break
                pulses[idx] = rng.uniform(0.5, 1.0)
            creak = static_filter(pulses, SR, fc=rng.uniform(700, 1100), q=6.0, kind="bandpass")
            k = np.arange(length) / length
            creak *= np.sin(np.pi * k)
            peak = np.max(np.abs(creak))
            if peak > 1e-9:
                out[start : start + length] += 0.08 * creak / peak
    return out


def _footstep_water(rng: np.random.Generator) -> np.ndarray:
    n = int(rng.uniform(0.40, 0.48) * SR)
    out = np.zeros(n)
    for onset, w in _contacts(rng, n, (0.10, 0.14)):
        out += w * 0.22 * _thump(n, onset, rng, fc=rng.uniform(120, 170), tau_s=0.04)
        # Agua desplazada: roce ancho que abre de grave a medio.
        out += w * 0.5 * _swish(n, onset, rng, rng.uniform(0.14, 0.2), 500, 2200, q=0.7)
        # Salpicadura fina.
        out += w * 0.3 * _grain_burst(n, onset, rng, count=int(rng.uniform(25, 40)), spread_s=0.06,
                                      grain_len_s=(0.003, 0.01), band_hz=(2500, 8000), q=1.4)
        # Burbujas: pocas, con el tono de Minnaert de burbujas de unos mm.
        for _ in range(int(rng.integers(3, 7))):
            pos = onset + int(rng.uniform(0.01, 0.16) * SR)
            if pos >= n - 64:
                continue
            out += w * rng.uniform(0.06, 0.14) * _bubble(n, pos, rng.uniform(500, 1600), rng.uniform(0.008, 0.02), rng.uniform(0.3, 0.8))
    return out


_MATERIAL_FUNCS = {
    "sand": _footstep_sand,
    "grass": _footstep_grass,
    "rock": _footstep_rock,
    "wood": _footstep_wood,
    "water": _footstep_water,
}


def footstep(name: str, material: str) -> np.ndarray:
    rng = rng_for(name)
    raw = _MATERIAL_FUNCS[material](rng)
    # Recorta el silencio final (por debajo de -60 dB del pico, con 20 ms de
    # margen) y cierra con un fundido de 10 ms: ninguna cola se corta en seco.
    above = np.nonzero(np.abs(raw) > np.max(np.abs(raw)) * 1e-3)[0]
    if above.size:
        raw = raw[: min(int(above[-1]) + int(0.02 * SR), len(raw))]
    fade = min(int(0.01 * SR), len(raw))
    raw[-fade:] *= np.linspace(1.0, 0.0, fade)
    target = _TARGET_MOMENTARY[material] + rng.uniform(-0.75, 0.75)
    out = raw * 10.0 ** ((target - k_weighted_momentary_max(raw)) / 20.0)
    # Si alcanzar la sonoridad objetivo exige un pico por encima de
    # `_PEAK_CAP`, se prefiere quedarse algo mas bajo: el limitador `tanh`
    # de `finalize` achataria el transitorio del contacto, que es justo lo
    # que hace legible el paso.
    peak = np.max(np.abs(out))
    return out * (_PEAK_CAP / peak) if peak > _PEAK_CAP else out
