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
    return signal.lfilter(_STAGE2_B, _STAGE2_A, stage1)


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
    weighted_sq = _k_weight(x) ** 2
    win = max(int(window_s * sr), 1)
    if len(weighted_sq) <= win:
        mean_sq = float(np.mean(weighted_sq))
    else:
        mean_sq = float(np.max(np.convolve(weighted_sq, np.ones(win) / win, mode="valid")))
    if mean_sq <= 1e-12:
        return -120.0
    return -0.691 + 10.0 * np.log10(mean_sq)


# ---------------------------------------------------------------------------
# Sonoridad integrada con puertas (ITU-R BS.1770-4) y limitador de pico con
# anticipacion, para el master de la musica. `lufs_approx` sigue siendo la
# medida rapida del resto del catalogo; la musica se mide y se ajusta con
# `integrated_lufs`, que es la cifra que dan los medidores de referencia.
# ---------------------------------------------------------------------------

_BLOCK_S = 0.4
_BLOCK_HOP_S = 0.1  # solape del 75 %
_ABSOLUTE_GATE_LUFS = -70.0
_RELATIVE_GATE_LU = -10.0


def _block_loudness(mean_sq: np.ndarray) -> np.ndarray:
    with np.errstate(divide="ignore"):
        return -0.691 + 10.0 * np.log10(np.maximum(mean_sq, 1e-20))


def integrated_lufs(x: np.ndarray, sr: int = 48_000) -> float:
    """Sonoridad integrada BS.1770-4: K-weighting, bloques de 400 ms con 75 %
    de solape, SUMA de la potencia de los canales (L y R pesan 1,0), puerta
    absoluta a -70 LUFS y puerta relativa a -10 LU. `x` mono (N,) o (C, N).

    Una señal mas corta que un bloque se mide entera, sin puertas."""
    channels = [x] if x.ndim == 1 else [x[c] for c in range(x.shape[0])]
    n = channels[0].shape[-1]
    if n == 0:
        return -120.0
    weighted_sq = sum(_k_weight(np.asarray(ch, dtype=np.float64)) ** 2 for ch in channels)
    block = int(round(_BLOCK_S * sr))
    hop = int(round(_BLOCK_HOP_S * sr))
    if n < block:
        mean_sq = float(np.mean(weighted_sq))
        return -120.0 if mean_sq <= 1e-12 else float(-0.691 + 10.0 * np.log10(mean_sq))
    csum = np.concatenate([[0.0], np.cumsum(weighted_sq)])
    starts = np.arange(0, n - block + 1, hop)
    block_ms = (csum[starts + block] - csum[starts]) / block
    loud = _block_loudness(block_ms)
    kept = block_ms[loud > _ABSOLUTE_GATE_LUFS]
    if kept.size == 0:
        return -120.0
    relative_gate = float(_block_loudness(np.array([np.mean(kept)]))[0]) + _RELATIVE_GATE_LU
    kept = block_ms[(loud > _ABSOLUTE_GATE_LUFS) & (loud > relative_gate)]
    return float(_block_loudness(np.array([np.mean(kept)]))[0])


def true_peak_envelope(x: np.ndarray, oversample: int = 4) -> np.ndarray:
    """Envolvente de pico por muestra, enlazada entre canales y medida sobre
    la señal sobremuestreada `oversample` veces: capta los picos entre
    muestras que un conversor reconstruye y que el valor de las muestras no
    ve. Devuelve un array (N,)."""
    channels = [x] if x.ndim == 1 else [x[c] for c in range(x.shape[0])]
    n = channels[0].shape[-1]
    env = np.zeros(n)
    for ch in channels:
        env = np.maximum(env, np.abs(ch))
        if oversample > 1 and n > 1:
            up = np.abs(signal.resample_poly(ch.astype(np.float32), oversample, 1))
            env = np.maximum(env, up[: n * oversample].reshape(n, oversample).max(axis=1))
    return env


def lookahead_limiter(
    x: np.ndarray,
    ceiling: float = PEAK_CEILING_LINEAR,
    sr: int = 48_000,
    window_s: float = 0.012,
    circular: bool = False,
) -> np.ndarray:
    """Limitador de pico sin distorsion armonica para el master de la musica.

    Calcula la ganancia que hace falta en cada muestra para que el pico
    (verdadero, ver `true_peak_envelope`) no pase de `ceiling`, la extiende
    con un minimo deslizante de ancho `2h+1` y la suaviza con una media movil
    del mismo ancho. Como toda muestra de la media viene de un minimo cuya
    ventana contiene a la muestra central, la ganancia final nunca supera la
    necesaria: el pico queda garantizado, y la ganancia entra y sale en rampa
    de `window_s` a cada lado (sin el "clic" de un recorte duro ni la
    saturacion de `enforce_peak_ceiling`). Proceso fuera de linea, asi que la
    anticipacion no añade latencia.

    `circular=True` trata la señal como un bucle (la ganancia es continua a
    traves del punto de union)."""
    from scipy.ndimage import minimum_filter1d, uniform_filter1d

    if x.size == 0:
        return x
    target = ceiling * 0.999  # margen para el redondeo de coma flotante
    env = true_peak_envelope(x)
    required = np.minimum(1.0, target / np.maximum(env, 1e-12))
    if float(np.min(required)) >= 1.0:
        return x
    half = max(int(round(window_s * sr)), 1)
    size = 2 * half + 1
    mode = "wrap" if circular else "nearest"
    gain = minimum_filter1d(required, size=size, mode=mode)
    gain = uniform_filter1d(gain, size=size, mode=mode)
    # La media movil de coma flotante puede quedar un ulp por encima.
    gain = np.minimum(gain, required)
    return x * gain


def master_to_lufs(
    x: np.ndarray,
    target_lufs: float,
    ceiling: float = PEAK_CEILING_LINEAR,
    sr: int = 48_000,
    circular: bool = False,
    max_gain_db: float = 24.0,
    iterations: int = 4,
    tolerance_lu: float = 0.1,
) -> np.ndarray:
    """Lleva `x` a `target_lufs` (integrados, BS.1770) con el pico por debajo
    de `ceiling`: ganancia lineal y limitador, repetidos hasta que el
    limitador deja de restar sonoridad apreciable."""
    y = x
    for _ in range(iterations):
        current = integrated_lufs(y, sr)
        if current <= -119.0:
            return y
        gain_db = float(np.clip(target_lufs - current, -max_gain_db, max_gain_db))
        y = lookahead_limiter(y * (10.0 ** (gain_db / 20.0)), ceiling, sr, circular=circular)
        if abs(integrated_lufs(y, sr) - target_lufs) <= tolerance_lu:
            break
    return y
