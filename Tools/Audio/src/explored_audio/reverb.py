"""Reverberacion de Schroeder: cuatro filtros de peine en paralelo seguidos de
dos filtros todo-paso en cascada. Es una respuesta sintetica (no una
convolucion con una IR grabada), pero es exactamente el diseño clasico de
Schroeder (1962) que pide el encargo.

Los filtros de peine/todo-paso de retardo entero se resuelven "por fase": un
retardo de D muestras con realimentacion es, en cada una de las D fases
(indice mod D), una sencilla IIR de primer orden en el tiempo lento (de D en
D muestras). Reorganizando el buffer en una matriz (pasos, fase) y filtrando
por columnas con `scipy.signal.lfilter(..., axis=0)` se evita el bucle
muestra a muestra (que con miles de muestras de retardo seria demasiado
lento) sin perder precision.
"""

from __future__ import annotations

import numpy as np
from scipy import signal


def _by_phase(x: np.ndarray, delay: int, b: list[float], a: list[float]) -> np.ndarray:
    n = len(x)
    pad = (-n) % delay
    xp = np.pad(x, (0, pad)) if pad else x
    reshaped = xp.reshape(-1, delay)
    filtered = np.asarray(signal.lfilter(b, a, reshaped, axis=0))
    return filtered.reshape(-1)[:n]


def _comb_filter(x: np.ndarray, delay: int, gain: float) -> np.ndarray:
    return _by_phase(x, delay, [1.0], [1.0, -gain])


def _allpass_filter(x: np.ndarray, delay: int, gain: float) -> np.ndarray:
    return _by_phase(x, delay, [-gain, 1.0], [1.0, -gain])


def schroeder_reverb(
    x: np.ndarray,
    sr: int,
    room_size: float = 0.5,
    damping: float = 0.3,
    wet: float = 0.25,
) -> np.ndarray:
    """Reverberacion sintetica de Schroeder (4 peines + 2 todo-paso) mezclada
    con la señal seca. `room_size` en [0,1] alarga los peines y sube su
    realimentacion; `damping` amortigua algo la cola."""
    if len(x) == 0:
        # `np.max` de un buffer vacio lanza ValueError: sin señal no hay cola.
        return np.zeros(0)
    comb_ms = [29.7, 37.1, 41.1, 43.7]
    allpass_ms = [5.0, 1.7]
    size = 0.7 + 0.6 * room_size
    comb_gain = min(0.55 + 0.35 * room_size, 0.97) * (1.0 - 0.15 * damping)

    wet_sig = np.zeros(len(x))
    for ms in comb_ms:
        delay = max(int(sr * ms * size / 1000.0), 1)
        wet_sig += _comb_filter(x, delay, comb_gain)
    wet_sig /= len(comb_ms)

    for ms in allpass_ms:
        delay = max(int(sr * ms / 1000.0), 1)
        wet_sig = _allpass_filter(wet_sig, delay, 0.5)

    wet_peak = np.max(np.abs(wet_sig))
    dry_peak = np.max(np.abs(x))
    if wet_peak > 1e-9 and dry_peak > 1e-9:
        wet_sig *= dry_peak / wet_peak

    return (1.0 - wet) * x + wet * wet_sig


# ---------------------------------------------------------------------------
# Reverb de sala por convolucion (musica). El Schroeder de arriba es barato y
# sirve para efectos, pero sus cuatro peines dejan un "zumbido metalico"
# (ecos periodicos audibles) que delata la sintesis en una mezcla musical. La
# sala se modela con una respuesta al impulso sintetica mas realista:
# pre-retardo, reflexiones tempranas dispersas y una cola de ruido
# decorrelado por canal que decae antes en agudos que en graves (el aire y
# las paredes absorben mas los agudos).
# ---------------------------------------------------------------------------

_ROOM_BANDS_HZ = (400.0, 3000.0)


def _room_rt60(room_size: float) -> tuple[float, float, float]:
    """RT60 (s) por banda grave/media/aguda para `room_size` en [0, 1]: de una
    habitacion con muebles (~0,5 s) a una sala de madera grande (~1,9 s)."""
    mid = 0.5 + 1.4 * float(np.clip(room_size, 0.0, 1.0))
    return mid * 1.15, mid, mid * 0.55


def room_impulse_response(sr: int, room_size: float = 0.6, damping: float = 0.35, seed: int = 0) -> np.ndarray:
    """Respuesta al impulso estereo `(2, N)` de una sala sintetica, con
    energia unitaria por canal. Determinista para una misma `seed`."""
    rng = np.random.default_rng(seed)
    rt_low, rt_mid, rt_high = _room_rt60(room_size)
    rt_high *= 1.0 - 0.5 * float(np.clip(damping, 0.0, 1.0))
    length = int(sr * max(rt_low, rt_mid) * 1.1)
    predelay = int(sr * (0.008 + 0.017 * room_size))
    t = np.arange(length) / sr

    low_sos = signal.butter(2, _ROOM_BANDS_HZ[0], btype="lowpass", fs=sr, output="sos")
    mid_sos = signal.butter(2, _ROOM_BANDS_HZ, btype="bandpass", fs=sr, output="sos")
    high_sos = signal.butter(2, _ROOM_BANDS_HZ[1], btype="highpass", fs=sr, output="sos")

    channels = []
    for _ in range(2):
        noise = rng.standard_normal(length)
        tail = np.zeros(length)
        for sos, rt in ((low_sos, rt_low), (mid_sos, rt_mid), (high_sos, rt_high)):
            # -60 dB en `rt` segundos: amplitud exp(-6,91 t / rt).
            tail += signal.sosfilt(sos, noise) * np.exp(-6.907755 * t / rt)
        # Entrada suave de la cola (difusion que crece): sin ella la cola
        # arranca de golpe y suena a "eco" en vez de a sala.
        onset = int(sr * 0.03)
        tail[:onset] *= np.linspace(0.0, 1.0, onset) ** 2

        ir = np.zeros(length + predelay)
        ir[predelay:] = tail * 0.6
        # Reflexiones tempranas: unos pocos ecos discretos entre 4 y 45 ms
        # tras el pre-retardo, cada vez mas debiles.
        for _k in range(10):
            delay_s = rng.uniform(0.004, 0.045) * (0.7 + 0.6 * room_size)
            idx = predelay + int(delay_s * sr)
            ir[idx] += rng.choice([-1.0, 1.0]) * 3.0 * np.exp(-delay_s / 0.03)
        ir /= np.sqrt(np.sum(ir**2))
        channels.append(ir)
    return np.stack(channels)


def room_reverb(
    x: np.ndarray,
    sr: int,
    room_size: float = 0.6,
    damping: float = 0.35,
    wet: float = 0.3,
    seed: int = 0,
) -> np.ndarray:
    """Reverb de sala estereo por convolucion. `x` es `(2, N)` (o mono `(N,)`,
    que se trata como dos canales iguales); devuelve `(2, N)`: la cola que
    pasa del final se descarta (quien llama ya reserva margen para ella).

    Mezcla: la señal seca baja poco (`1 - wet/2`) y la humeda entra con la
    misma energia que la seca multiplicada por `wet`, asi `wet` en torno a
    0,3-0,45 suena a sala, no a catedral."""
    stereo = np.stack([x, x]) if x.ndim == 1 else x
    n = stereo.shape[-1]
    ir = room_impulse_response(sr, room_size, damping, seed)
    # Un poco de cruce entre canales: en una sala cada oido recibe tambien la
    # reverberacion de lo que suena al otro lado.
    mono = stereo.mean(axis=0)
    out = np.empty_like(stereo, dtype=np.float64)
    for c in range(2):
        source = 0.8 * stereo[c] + 0.2 * mono
        wet_sig = signal.oaconvolve(source, ir[c])[:n]
        dry_rms = float(np.sqrt(np.mean(stereo[c] ** 2)))
        wet_rms = float(np.sqrt(np.mean(wet_sig**2)))
        if wet_rms > 1e-12:
            wet_sig *= dry_rms / wet_rms
        out[c] = (1.0 - 0.5 * wet) * stereo[c] + wet * wet_sig
    return out
