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
  (tablon de madera) o burbujas de Minnaert (agua somera: la cavidad que
  se cierra al hundir el pie y las gotas que vuelven a caer).

La sonoridad final se fija por material (`_TARGET_MOMENTARY`) con un margen
aleatorio pequeño, para que las cuatro variantes de un material suenen al
mismo nivel y los materiales guarden una jerarquia coherente (la hierba mas
suave que la madera hueca).
"""

from __future__ import annotations

import numpy as np
from scipy.signal import fftconvolve

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


def _crunch(n: int, rng: np.random.Generator, rate: np.ndarray, band_hz: tuple[float, float],
            click_s: tuple[float, float], kernels: int = 8) -> np.ndarray:
    """Crujido granular: chasquidos de Poisson con tasa instantanea `rate`
    (chasquidos/s, por muestra): muchos roces pequeños y un 3 % de granos
    que ceden de golpe, algo mas fuertes. Cada chasquido es una
    rafaga pasabanda de `click_s` segundos; se preparan `kernels` nucleos y
    cada uno se convoluciona con su tren de impulsos (miles de chasquidos sin
    filtrarlos uno a uno)."""
    prob = np.clip(rate / SR, 0.0, 1.0)
    hits = np.nonzero(rng.random(n) < prob)[0]
    out = np.zeros(n)
    if hits.size == 0:
        return out
    amps = rng.uniform(0.3, 1.0, hits.size)
    snap = rng.random(hits.size) < 0.03
    amps[snap] = rng.uniform(1.3, 1.8, int(snap.sum()))
    amps *= rng.choice((-1.0, 1.0), hits.size)
    which = rng.integers(0, kernels, hits.size)
    for k in range(kernels):
        sel = which == k
        if not np.any(sel):
            continue
        cn = max(int(rng.uniform(*click_s) * SR), 8)
        fc = float(np.exp(rng.uniform(np.log(band_hz[0]), np.log(band_hz[1]))))
        kernel = static_filter(rng.standard_normal(cn), SR, fc=fc, q=0.9, kind="bandpass")
        kernel *= np.exp(-np.arange(cn) / cn * 5.0)
        kernel /= np.max(np.abs(kernel)) + 1e-12
        train = np.zeros(n)
        train[hits[sel]] = amps[sel]
        out += fftconvolve(train, kernel)[:n]
    peak = np.max(np.abs(out))
    return out / peak if peak > 1e-9 else out


def _soft_thump(n: int, onset: int, rng: np.random.Generator, fc: float, attack_s: float, tau_s: float) -> np.ndarray:
    """Golpe grave de un apoyo blando: como `_thump`, pero el pie se hunde
    en `attack_s` (la arena cede) en vez de pararse en 2 ms."""
    out = np.zeros(n)
    length = min(int((attack_s + tau_s * 6) * SR), n - onset)
    if length <= 8:
        return out
    t = np.arange(length) / SR
    env = np.sin(0.5 * np.pi * np.minimum(t / attack_s, 1.0)) ** 2 * np.exp(-np.maximum(t - attack_s, 0.0) / tau_s)
    burst = static_filter(rng.standard_normal(length), SR, fc=fc, q=0.8, kind="lowpass")
    burst = static_filter(burst, SR, fc=45.0, q=0.7, kind="highpass")
    out[onset : onset + length] = burst * env
    peak = np.max(np.abs(out))
    return out / peak if peak > 1e-9 else out


def _footstep_sand(rng: np.random.Generator) -> np.ndarray:
    """Paso en arena seca de playa. La arena no para el pie en seco: cede.
    Por eso no hay chasquido de contacto, sino una compresion:

    - el pie se hunde 15-30 ms (ataque blando) y el peso llega como un
      golpe grave y sordo;
    - mientras se hunde, miles de granos rozan y ceden (crujido denso de
      chasquidos de 0,5-2 ms en 250-2500 Hz, la arena se come los agudos)
      cuya tasa sigue a la compresion: sube con el hundimiento y se apaga
      cuando el pie se asienta;
    - al despegar la punta, el pie empuja arena hacia atras: un soplo corto
      y una lluvia fina de granos que caen 40-160 ms despues, algo mas
      agudos y cada vez mas escasos.

    La version anterior eran granos de 2-7 ms en 600-3200 Hz con un golpe de
    2 ms: sonaba a grava y el pico llegaba al techo antes que la sonoridad."""
    n = int(rng.uniform(0.34, 0.40) * SR)
    out = np.zeros(n)
    t = np.arange(n) / SR
    contacts = _contacts(rng, n, (0.08, 0.12))
    for onset, w in contacts:
        sink = rng.uniform(0.015, 0.03)
        out += w * 0.18 * _soft_thump(n, onset, rng, fc=rng.uniform(140, 200), attack_s=sink * 0.7,
                                       tau_s=rng.uniform(0.025, 0.035))
        # Compresion: sube en `sink` y cae con el asentamiento.
        tc = np.clip(t - onset / SR, 0.0, None)
        comp = np.where(t * SR < onset, 0.0,
                        np.sin(0.5 * np.pi * np.minimum(tc / sink, 1.0)) ** 2
                        * np.exp(-np.maximum(tc - sink, 0.0) / rng.uniform(0.05, 0.07)))
        crunch = _crunch(n, rng, 10000.0 * comp, band_hz=(250.0, 2500.0), click_s=(0.0005, 0.002))
        crunch = static_filter(crunch, SR, fc=2400.0, q=0.6, kind="lowpass")
        out += w * 0.9 * crunch * np.sqrt(comp)
    # Despegue de la punta: arena empujada hacia atras.
    toe = contacts[-1][0]
    kick = toe + int(rng.uniform(0.05, 0.08) * SR)
    if kick < n - int(0.05 * SR):
        out += 0.10 * _swish(n, kick, rng, rng.uniform(0.05, 0.08), 1800.0, 900.0, q=0.6)
        tk = np.clip(t - kick / SR - 0.04, 0.0, None)
        fall = np.where(t * SR < kick + int(0.04 * SR), 0.0, np.exp(-tk / 0.05))
        out += 0.12 * _crunch(n, rng, 900.0 * fall, band_hz=(800.0, 5000.0), click_s=(0.0003, 0.001))
    return out


def _footstep_grass(rng: np.random.Generator) -> np.ndarray:
    """Paso en hierba de claro (mata baja sobre tierra humeda). La hierba no
    sisea: son tallos fibrosos que se doblan y se quiebran bajo el pie.

    - El peso llega amortiguado por la mata: golpe grave con un hundimiento
      breve (6-10 ms), mas sordo que en roca y mas firme que en arena.
    - Al comprimirse la mata, decenas de tallos se doblan y parten: crujido
      de chasquidos de 1-3 ms en 900-5000 Hz cuya tasa sigue a la compresion,
      con algun tallo mas grueso que se quiebra claramente (el "crac" que
      distingue la hierba de la arena).
    - Las hojas rozan entre si mientras bajan: un roce corto y apagado.
    - Al levantar la punta, las hojas vuelven a su sitio: un roce breve y
      suave y unos pocos chasquidos sueltos.

    La version anterior era un barrido de ruido de 1,8 a 6 kHz con granos de
    2-7 kHz: centroide de 2,7-3,4 kHz y un 7-9 % de la energia sobre 8 kHz,
    un siseo de papel mas que un paso."""
    n = int(rng.uniform(0.32, 0.38) * SR)
    out = np.zeros(n)
    t = np.arange(n) / SR
    contacts = _contacts(rng, n, (0.08, 0.12))
    for onset, w in contacts:
        sink = rng.uniform(0.006, 0.010)
        out += w * 0.13 * _soft_thump(n, onset, rng, fc=rng.uniform(120, 170), attack_s=sink,
                                       tau_s=rng.uniform(0.02, 0.028))
        tc = np.clip(t - onset / SR, 0.0, None)
        comp = np.where(t * SR < onset, 0.0,
                        np.sin(0.5 * np.pi * np.minimum(tc / (sink * 2.5), 1.0)) ** 2
                        * np.exp(-np.maximum(tc - sink * 2.5, 0.0) / rng.uniform(0.035, 0.05)))
        stems = _crunch(n, rng, 2600.0 * comp, band_hz=(900.0, 5000.0), click_s=(0.001, 0.003), kernels=10)
        stems = static_filter(stems, SR, fc=4500.0, q=0.6, kind="lowpass")
        out += w * 0.75 * stems * np.sqrt(comp)
        # Tallos gruesos que se quiebran: pocos chasquidos claros al pisar.
        out += w * 0.12 * _grain_burst(n, onset + int(sink * SR), rng, count=int(rng.integers(2, 5)), spread_s=0.018,
                                       grain_len_s=(0.002, 0.004), band_hz=(1500, 3500), q=2.0)
        # Hojas que rozan al bajar: roce corto que se apaga hacia los medios.
        out += w * 0.10 * _swish(n, onset, rng, rng.uniform(0.05, 0.08), 3200, 1400, q=0.7)
    # Las hojas vuelven a su sitio tras despegar la punta.
    lift = contacts[-1][0] + int(rng.uniform(0.06, 0.09) * SR)
    if lift < n - int(0.06 * SR):
        out += 0.06 * _swish(n, lift, rng, rng.uniform(0.05, 0.07), 1600, 3200, q=0.8)
        tl = np.clip(t - lift / SR, 0.0, None)
        spring = np.where(t * SR < lift, 0.0, np.exp(-tl / 0.03))
        out += 0.10 * _crunch(n, rng, 300.0 * spring, band_hz=(1200.0, 4500.0), click_s=(0.0008, 0.002))
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


def _droplet(n: int, onset: int, rng: np.random.Generator, amp: float) -> np.ndarray:
    """Gota que cae de vuelta al agua: un tic de impacto de ~1 ms y, casi
    siempre, la burbuja que atrapa (Pumphrey y Crum: 1,5-5 kHz, tono que
    sube). La burbuja es lo que hace que suene a agua y no a grava."""
    out = np.zeros(n)
    if onset >= n - 64:
        return out
    out += 0.35 * amp * _click(n, onset, rng, fc=rng.uniform(3000, 6000), dur_s=0.0012)
    if rng.uniform() < 0.8:
        pos = min(onset + int(rng.uniform(0.0005, 0.003) * SR), n - 64)
        out += amp * _bubble(n, pos, rng.uniform(1500, 5000), rng.uniform(0.003, 0.008), rng.uniform(0.1, 0.3))
    return out


def _footstep_water(rng: np.random.Generator) -> np.ndarray:
    """Paso en agua somera (por el tobillo), por fases como en el agua real:

    - la planta golpea la superficie: palmetazo ancho y corto, y el golpe
      amortiguado del pie en el fondo de arena;
    - el pie arrastra aire al hundirse y la cavidad se cierra con un «plonc»
      de burbuja grande (Minnaert de 1-2 cm, 250-600 Hz, tono que sube);
    - el agua desplazada rodea el tobillo (roce grave a medio);
    - las gotas levantadas vuelven a caer 60-250 ms despues: tics con su
      burbuja aguda, cada vez mas escasas;
    - al despegar la punta, el agua escurre del pie: unos goteos sueltos.
    """
    n = int(rng.uniform(0.42, 0.50) * SR)
    out = np.zeros(n)
    for onset, w in _contacts(rng, n, (0.10, 0.14)):
        out += w * 0.20 * _thump(n, onset, rng, fc=rng.uniform(110, 160), tau_s=0.035)
        out += w * 0.22 * _click(n, onset, rng, fc=rng.uniform(900, 1500), dur_s=0.008)
        # Cavidad que se cierra: una burbuja grande (y a veces otra menor).
        pos = onset + int(rng.uniform(0.012, 0.025) * SR)
        out += w * 0.28 * _bubble(n, pos, rng.uniform(280, 480), rng.uniform(0.025, 0.04), rng.uniform(0.4, 0.9))
        if rng.uniform() < 0.6:
            pos = onset + int(rng.uniform(0.025, 0.05) * SR)
            out += w * 0.22 * _bubble(n, pos, rng.uniform(500, 900), rng.uniform(0.012, 0.022), rng.uniform(0.3, 0.8))
        # Agua desplazada alrededor del tobillo, mas grave y floja que antes:
        # un roce ancho enmascaraba las burbujas.
        out += w * 0.22 * _swish(n, onset, rng, rng.uniform(0.12, 0.18), 350, 1400, q=0.7)
        # Lamina de agua que se rompe al pisar: salpicadura corta y brillante.
        out += w * 0.16 * _grain_burst(n, onset, rng, count=int(rng.uniform(15, 25)), spread_s=0.02,
                                       grain_len_s=(0.002, 0.006), band_hz=(1500, 6000), q=1.2)
        # Gotas que vuelven a caer: densas al principio, luego sueltas.
        for _ in range(int(rng.integers(14, 24))):
            delay = 0.06 + min(rng.exponential(0.06), 0.19)
            out += w * rng.uniform(0.12, 0.28) * np.exp(-(delay - 0.06) / 0.12) * _droplet(n, onset + int(delay * SR), rng, 1.0)
    # El pie sale del agua: goteo suelto al final.
    lift = int(rng.uniform(0.26, 0.32) * SR)
    for _ in range(int(rng.integers(2, 5))):
        out += rng.uniform(0.05, 0.1) * _droplet(n, lift + int(rng.uniform(0.0, 0.1) * SR), rng, 1.0)
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
