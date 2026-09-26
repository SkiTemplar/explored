"""Humanizacion: la diferencia entre "sintetizado" y "MIDI barato" esta casi
toda aqui. Nada suena a maquina si el tiempo, la velocidad y la afinacion de
cada nota se apartan un poco -y de forma distinta cada vez- de la rejilla
exacta."""

from __future__ import annotations

import numpy as np


def humanize_timing_s(seconds: float, rng: np.random.Generator, amount_s: float = 0.018) -> float:
    """Adelanta o retrasa una nota una pizca (tipicamente 10-20 ms)."""
    return max(0.0, seconds + rng.uniform(-amount_s, amount_s))


def humanize_velocity(velocity: float, rng: np.random.Generator, amount: float = 0.15) -> float:
    """Variacion de intensidad nota a nota, como un interprete real."""
    return float(np.clip(velocity * (1.0 + rng.uniform(-amount, amount)), 0.05, 1.15))


def micro_detune_ratio(rng: np.random.Generator, max_cents: float = 6.0) -> float:
    """Factor multiplicativo de frecuencia para una micro-desafinacion de
    hasta `max_cents` centesimas (100 centesimas = un semitono)."""
    cents = rng.uniform(-max_cents, max_cents)
    return float(2.0 ** (cents / 1200.0))
