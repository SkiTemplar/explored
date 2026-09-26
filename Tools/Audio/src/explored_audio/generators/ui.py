"""Sonidos de interfaz: tonos limpios y muy cortos (sintesis aditiva simple),
sin ruido, para que se sientan sinteticos y no se confundan con el mundo."""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..envelopes import ar_envelope, fit_length
from ..rng import rng_for

SR = SAMPLE_RATE


def _tone(n: int, freq_env: np.ndarray, harmonics: tuple[float, ...] = (1.0,)) -> np.ndarray:
    dt = 1.0 / SR
    phase = 2 * np.pi * np.cumsum(freq_env) * dt
    out = np.zeros(n)
    for h, weight in enumerate(harmonics, start=1):
        out += weight * np.sin(h * phase)
    return out / sum(harmonics)


def ui_click(name: str) -> np.ndarray:
    rng = rng_for(name)
    dur = 0.045
    n = int(dur * SR)
    freq = np.full(n, rng.uniform(1400, 1700))
    tone = _tone(n, freq, harmonics=(1.0, 0.3))
    env = fit_length(ar_envelope(SR, 0.002, dur - 0.002, shape=3.0), n)
    return tone * env


def ui_hover(name: str) -> np.ndarray:
    rng = rng_for(name)
    dur = 0.06
    n = int(dur * SR)
    freq = np.full(n, rng.uniform(2000, 2300))
    tone = _tone(n, freq, harmonics=(1.0, 0.2))
    env = fit_length(ar_envelope(SR, 0.008, dur - 0.008, shape=2.0), n)
    return tone * env * 0.6


def ui_open(name: str) -> np.ndarray:
    rng = rng_for(name)
    dur = 0.14
    n = int(dur * SR)
    t = np.linspace(0, 1, n)
    freq = rng.uniform(420, 500) + t * rng.uniform(380, 460)
    tone = _tone(n, freq, harmonics=(1.0, 0.35, 0.15))
    env = fit_length(ar_envelope(SR, 0.01, dur - 0.01, shape=1.4), n)
    return tone * env * 0.8


def ui_close(name: str) -> np.ndarray:
    rng = rng_for(name)
    dur = 0.14
    n = int(dur * SR)
    t = np.linspace(0, 1, n)
    freq = rng.uniform(800, 900) - t * rng.uniform(380, 460)
    tone = _tone(n, freq, harmonics=(1.0, 0.35, 0.15))
    env = fit_length(ar_envelope(SR, 0.01, dur - 0.01, shape=1.4), n)
    return tone * env * 0.8


def ui_journal_open(name: str) -> np.ndarray:
    """Abrir el diario: mas calido y largo que `ui_open` generico (dos
    armonicos mas, barrido mas suave), para distinguir "abrir un menu" de
    "abrir el diario"."""
    rng = rng_for(name)
    dur = 0.22
    n = int(dur * SR)
    t = np.linspace(0, 1, n)
    freq = rng.uniform(500, 560) + t * rng.uniform(260, 320)
    tone = _tone(n, freq, harmonics=(1.0, 0.4, 0.2))
    env = fit_length(ar_envelope(SR, 0.015, dur - 0.015, shape=1.3), n)
    return tone * env * 0.75


def ui_page_turn(name: str) -> np.ndarray:
    """Pasar pagina: un chasquido tonal muy corto y descendente."""
    rng = rng_for(name)
    dur = 0.09
    n = int(dur * SR)
    t = np.linspace(0, 1, n)
    freq = rng.uniform(1100, 1300) - t * rng.uniform(300, 420)
    tone = _tone(n, freq, harmonics=(1.0, 0.25))
    env = fit_length(ar_envelope(SR, 0.004, dur - 0.004, shape=2.2), n)
    return tone * env * 0.65


def ui_discovery_notify(name: str) -> np.ndarray:
    """Notificacion de descubrimiento (PdI, nota o mirador nuevo): fanfarria
    corta de 3 notas ascendentes, mas festiva que un simple click de UI."""
    rng = rng_for(name)
    note_freqs = [rng.uniform(700, 760), rng.uniform(880, 940), rng.uniform(1160, 1240)]
    note_dur = 0.09
    gap_s = 0.03
    pieces = []
    for freq in note_freqs:
        n = int(note_dur * SR)
        tone = _tone(n, np.full(n, freq), harmonics=(1.0, 0.35, 0.15))
        env = fit_length(ar_envelope(SR, 0.006, note_dur - 0.006, shape=1.6), n)
        pieces.append(tone * env * 0.8)
        pieces.append(np.zeros(int(gap_s * SR)))
    return np.concatenate(pieces)
