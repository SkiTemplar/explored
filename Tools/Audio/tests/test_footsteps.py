"""Pasos: cuerpo grave presente (no un siseo), nivel coherente entre
materiales y dos contactos (talon y punta) separados en el tiempo."""

from __future__ import annotations

import numpy as np
from scipy import signal

from explored_audio.constants import SAMPLE_RATE
from explored_audio.levels import k_weighted_momentary_max

MATERIALS = ("sand", "grass", "rock", "wood", "water")


def _footsteps(rendered):
    return {name: audio for name, audio in rendered.items() if name.startswith("sfx_footstep_")}


def test_pasos_tienen_cuerpo_grave(rendered):
    """Al menos un 5 % de la energia por debajo de 400 Hz: el peso del
    jugador sobre el suelo, que la primera version no tenia."""
    for name, audio in _footsteps(rendered).items():
        spectrum = np.abs(np.fft.rfft(audio)) ** 2
        freqs = np.fft.rfftfreq(len(audio), 1.0 / SAMPLE_RATE)
        low_share = spectrum[freqs < 400.0].sum() / spectrum.sum()
        assert low_share >= 0.05, f"{name}: solo {low_share:.1%} de energia bajo 400 Hz"


def test_pasos_nivel_coherente_entre_materiales(rendered):
    """Sonoridad momentanea media por material dentro de 3 dB entre si."""
    means = []
    for material in MATERIALS:
        values = [k_weighted_momentary_max(audio) for name, audio in _footsteps(rendered).items() if f"_{material}_" in name]
        assert len(values) == 4
        means.append(float(np.mean(values)))
    assert max(means) - min(means) <= 3.0, f"medias por material demasiado dispares: {means}"


def test_pasos_tienen_talon_y_punta(rendered):
    """Dos contactos: la envolvente de la banda grave (< 300 Hz, el golpe del
    peso; la textura del material, como el roce de la hierba, puede tapar el
    hueco entre contactos en agudos) tiene un segundo maximo 50-160 ms
    despues del primero, de al menos -20 dB."""
    win = int(0.005 * SAMPLE_RATE)
    sos = signal.butter(4, 300.0, btype="lowpass", fs=SAMPLE_RATE, output="sos")
    for name, audio in _footsteps(rendered).items():
        low = signal.sosfiltfilt(sos, audio)
        env = np.convolve(np.abs(low), np.ones(win) / win, mode="same")
        first = int(np.argmax(env[: int(0.03 * SAMPLE_RATE)]))
        lo, hi = first + int(0.05 * SAMPLE_RATE), first + int(0.16 * SAMPLE_RATE)
        segment = env[lo:hi]
        # Un repunte: el maximo del tramo supera claramente su minimo previo.
        rise = segment.max() / max(segment[: int(np.argmax(segment)) + 1].min(), 1e-9)
        assert segment.max() >= env[first] * 0.1, f"{name}: la punta es demasiado debil"
        assert rise >= 1.5, f"{name}: no se aprecia el segundo contacto (repunte x{rise:.2f})"
