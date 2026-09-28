"""Utilidades de nivel: retirada de continua, techo de pico de seguridad y
sonoridad LUFS aproximada.

La cifra LUFS usa el filtro K-weighting de ITU-R BS.1770 (coeficientes
estandar a 48 kHz) pero SIN puertas de silencio (gating): es una sonoridad
integrada aproximada, util para comparar ficheros entre si y para detectar un fichero silencioso
o desproporcionadamente alto, no una medida de certificacion de broadcast.
"""

from __future__ import annotations

import numpy as np
from scipy import signal

from .constants import PEAK_CEILING_LINEAR

_STAGE1_B = [1.53512485958697, -2.69169618940638, 1.19839281085285]
_STAGE1_A = [1.0, -1.69065929318241, 0.73248077421585]
_STAGE2_B = [1.0, -2.0, 1.0]
_STAGE2_A = [1.0, -1.99004745483398, 0.99007225036621]


def remove_dc(x: np.ndarray) -> np.ndarray:
    """Resta la media por canal (el ultimo eje es el tiempo)."""
    return x - np.mean(x, axis=-1, keepdims=True)


def peak(x: np.ndarray) -> float:
    return float(np.max(np.abs(x))) if x.size else 0.0


def enforce_peak_ceiling(x: np.ndarray, ceiling: float = PEAK_CEILING_LINEAR) -> np.ndarray:
    """Red de seguridad final: si el pico supera el techo, lo limita.

    Se usa un limitador suave (saturacion `tanh`) en vez de un escalado
    lineal de todo el buffer: para `|x| << techo` es practicamente la
    identidad (no toca la parte tranquila de la señal), y solo comprime los
    picos ocasionales que de verdad se pasan. Un escalado lineal global
    arrastraria hacia abajo TODO el fichero solo por un puñado de picos
    (le paso justo a `amb_underwater`, con burbujas puntuales muy por encima
    de su lecho continuo: escalar linealmente lo dejaba mucho mas flojo de
    lo que pedia su sonoridad objetivo).
    """
    p = peak(x)
    if p <= ceiling or p <= 1e-9:
        return x
    return ceiling * np.tanh(x / ceiling)


def linear_safety_clamp(x: np.ndarray, ceiling: float = PEAK_CEILING_LINEAR) -> np.ndarray:
    """Ultimo escalon, lineal (no `tanh`): un reescalado uniforme conserva
    exactamente la media (si `x` ya no tiene continua, sigue sin tenerla), a
    diferencia del limitador no lineal. Solo hace falta cuando el pico se ha
    movido una pizca por haber retirado la continua justo despues de aquel."""
    p = peak(x)
    if p <= ceiling or p <= 1e-9:
        return x
    return x * (ceiling / p)


def _k_weight(mono: np.ndarray) -> np.ndarray:
    stage1 = signal.lfilter(_STAGE1_B, _STAGE1_A, mono)
    return np.asarray(signal.lfilter(_STAGE2_B, _STAGE2_A, stage1))


def match_lufs(x: np.ndarray, target_lufs: float, max_gain_db: float = 24.0) -> np.ndarray:
    """Aplica una ganancia lineal para acercar `x` a `target_lufs`.

    Se usa en los colchones de ambiente: son bucles de fondo que en el juego
    conviene mezclar a un nivel consistente entre si, a diferencia de un
    efecto puntual (un paso, un golpe) donde interesa conservar el factor de
    cresta natural de cada diseño. La ganancia se acota (`max_gain_db`) para
    no amplificar de forma disparatada un fichero casi silencioso, y el techo
    de pico se aplica siempre despues, aparte.
    """
    current = lufs_approx(x)
    gain_db = float(np.clip(target_lufs - current, -max_gain_db, max_gain_db))
    return x * (10.0 ** (gain_db / 20.0))


def lufs_approx(x: np.ndarray) -> float:
    """Sonoridad integrada aproximada (sin gating), estilo BS.1770.

    `x` puede ser mono (N,) o estereo (2, N).
    """
    if x.shape[-1] == 0:
        # Sin muestras la media es NaN, y NaN acababa en el manifiesto (JSON invalido).
        return -120.0
    channels = [x] if x.ndim == 1 else [x[c] for c in range(x.shape[0])]
    weighted_mean_sq = sum(float(np.mean(_k_weight(ch) ** 2)) for ch in channels) / len(channels)
    if weighted_mean_sq <= 1e-12:
        return -120.0
    return -0.691 + 10.0 * np.log10(weighted_mean_sq)


def k_weighted_momentary_max(x: np.ndarray, window_s: float = 0.1, sr: int = 48_000) -> float:
    """Maximo de sonoridad momentanea (ventana deslizante de `window_s`,
    K-weighting BS.1770) de una señal mono, en LUFS aproximados.

    Para transitorios cortos (pasos, golpes) es mas representativo de lo que
    se oye que `lufs_approx`, que promedia tambien la cola y el silencio."""
    if len(x) == 0:
        return -120.0
    weighted_sq = _k_weight(x) ** 2
    win = max(int(window_s * sr), 1)
    if len(weighted_sq) <= win:
        mean_sq = float(np.mean(weighted_sq))
    else:
        mean_sq = float(np.max(np.convolve(weighted_sq, np.ones(win) / win, mode="valid")))
    if mean_sq <= 1e-12:
        return -120.0
    return -0.691 + 10.0 * np.log10(mean_sq)
