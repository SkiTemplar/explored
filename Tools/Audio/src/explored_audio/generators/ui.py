"""Sonidos de interfaz: tonos limpios y muy cortos, sin ruido, para que se
sientan sinteticos y no se confundan con el mundo.

Todos salen del mismo instrumento, una lamina pulsada tipo kalimba (parciales
armonicos que se apagan cada uno a su ritmo, mas un parcial inarmonico de la
lengueta que muere en pocos milisegundos), y de la misma escala que la musica:
re menor pentatonica (`music.compose.ROOT` es re). Asi los avisos no chocan
con la banda sonora que suena debajo y la interfaz se reconoce como familia.
Ataque de 1-2 ms en coseno y cola que termina en cero exacto: sin clics."""

from __future__ import annotations

import numpy as np

from ..constants import SAMPLE_RATE
from ..rng import rng_for

SR = SAMPLE_RATE

# Re menor pentatonica (re, fa, sol, la, do) en la octava 5-6. La4 = 440 Hz.
_D5, _F5, _G5, _A5, _C6 = (440.0 * 2 ** (s / 12) for s in (5, 8, 10, 12, 15))
_D6, _F6, _G6, _A6 = (2.0 * f for f in (_D5, _F5, _G5, _A5))


def _raised_cosine(n: int) -> np.ndarray:
    return 0.5 - 0.5 * np.cos(np.pi * np.arange(n) / max(n, 1))


def _tine(
    freq: float,
    dur_s: float,
    tau_s: float,
    rng: np.random.Generator,
    brightness: float = 1.0,
    attack_s: float = 0.0015,
    chirp: float = 0.0,
) -> np.ndarray:
    """Una nota de lamina: fundamental, octava y duodecima con decaimientos
    cada vez mas cortos, y el parcial de la lengueta (~5,9x) que da el
    "tic" del ataque y desaparece en ~6 ms. `chirp` sube la afinacion al
    principio y la deja caer en 4 ms: da un punto de contacto mas nitido
    sin meter ruido."""
    n = int(dur_s * SR)
    t = np.arange(n) / SR
    bend = 1.0 + chirp * np.exp(-t / 0.004)
    phase = 2 * np.pi * freq * np.cumsum(bend) / SR
    detune = 1.0 + rng.uniform(-0.002, 0.002)
    out = np.sin(phase)
    out += 0.28 * brightness * np.exp(-t / (tau_s * 0.45)) / np.maximum(np.exp(-t / tau_s), 1e-12) * np.sin(2.0 * phase * detune)
    out += 0.09 * brightness * np.exp(-t / (tau_s * 0.25)) / np.maximum(np.exp(-t / tau_s), 1e-12) * np.sin(3.0 * phase)
    out *= np.exp(-t / tau_s)
    out += 0.18 * brightness * np.exp(-t / 0.006) * np.sin(5.93 * phase)
    a_n = max(int(attack_s * SR), 1)
    out[:a_n] *= _raised_cosine(a_n)
    r_n = min(int(0.006 * SR), n)
    out[n - r_n :] *= _raised_cosine(r_n)[::-1]
    return out


def _mix(dur_s: float, notes: list[tuple[float, np.ndarray]]) -> np.ndarray:
    out = np.zeros(int(dur_s * SR))
    for start_s, note in notes:
        pos = int(start_s * SR)
        end = min(pos + len(note), len(out))
        out[pos:end] += note[: end - pos]
    return out


def ui_click(name: str) -> np.ndarray:
    """Pulsar un boton: un toque seco de re6 con el contacto marcado."""
    rng = rng_for(name)
    return 0.95 * _tine(_D6, 0.06, 0.016, rng, brightness=1.1, attack_s=0.0008, chirp=0.25)


def ui_hover(name: str) -> np.ndarray:
    """Pasar por encima: mas agudo, mas corto y mas flojo que el click (se
    repite mucho al recorrer un menu; no debe cansar)."""
    rng = rng_for(name)
    return 0.75 * _tine(_A6, 0.045, 0.009, rng, brightness=0.6, attack_s=0.002)


def ui_open(name: str) -> np.ndarray:
    """Abrir un panel: quinta ascendente la5 -> re6."""
    rng = rng_for(name)
    return 0.66 * _mix(0.24, [
        (0.0, 0.55 * _tine(_A5, 0.12, 0.05, rng, brightness=0.8)),
        (0.045, 0.75 * _tine(_D6, 0.195, 0.07, rng, brightness=0.9)),
    ])


def ui_close(name: str) -> np.ndarray:
    """Cerrar un panel: la misma quinta, descendente (re6 -> la5), y la
    segunda nota algo mas corta: cierra en vez de quedarse sonando."""
    rng = rng_for(name)
    return 0.75 * _mix(0.22, [
        (0.0, 0.6 * _tine(_D6, 0.12, 0.045, rng, brightness=0.9)),
        (0.045, 0.7 * _tine(_A5, 0.175, 0.055, rng, brightness=0.7)),
    ])


def ui_journal_open(name: str) -> np.ndarray:
    """Abrir el diario: un acorde re-fa-la arpegiado despacio y mas grave y
    largo que `ui_open`, para distinguir "abrir un menu" de "abrir el
    diario"."""
    rng = rng_for(name)
    return 0.75 * _mix(0.42, [
        (0.0, 0.5 * _tine(_D5, 0.42, 0.13, rng, brightness=0.7)),
        (0.05, 0.45 * _tine(_F5, 0.37, 0.12, rng, brightness=0.7)),
        (0.1, 0.5 * _tine(_A5, 0.32, 0.11, rng, brightness=0.8)),
    ])


def ui_page_turn(name: str) -> np.ndarray:
    """Pasar pagina: dos toques rapidos y flojos que bajan (do6 -> la5)."""
    rng = rng_for(name)
    return 0.9 * _mix(0.12, [
        (0.0, 0.5 * _tine(_C6, 0.05, 0.014, rng, brightness=0.7, chirp=0.15)),
        (0.028, 0.7 * _tine(_A5, 0.092, 0.024, rng, brightness=0.6)),
    ])


def ui_discovery_notify(name: str) -> np.ndarray:
    """Notificacion de descubrimiento (PdI, nota o mirador nuevo): arpegio
    ascendente re5-sol5-la5-re6 cuyas notas se solapan y siguen sonando,
    rematado por la octava con un batido lento (dos laminas casi al unisono)
    que deja un brillo de ~0,8 s. Mas festivo que un click, pero abierto
    (sin tercera): suena a hallazgo sin anticipar la musica de descubrimiento."""
    rng = rng_for(name)
    step = 0.075
    notes = [
        (0.0, 0.45 * _tine(_D5, 0.9, 0.22, rng, brightness=0.7)),
        (step, 0.45 * _tine(_G5, 0.8, 0.2, rng, brightness=0.75)),
        (2 * step, 0.5 * _tine(_A5, 0.75, 0.2, rng, brightness=0.8)),
        (3 * step, 0.55 * _tine(_D6, 0.8, 0.26, rng, brightness=0.9)),
        (3 * step + 0.004, 0.3 * _tine(_D6 * 1.0045, 0.8, 0.3, rng, brightness=0.3)),
        (3 * step + 0.02, 0.12 * _tine(_A6, 0.6, 0.15, rng, brightness=0.2)),
    ]
    return 0.8 * _mix(3 * step + 0.82, notes)
