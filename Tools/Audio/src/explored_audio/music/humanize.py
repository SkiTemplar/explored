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


class TimingDrift:
    """Deriva lenta de tempo de un interprete: un proceso de Ornstein-Uhlenbeck
    muestreado en el instante de cada nota. El ruido blanco de
    `humanize_timing_s` por si solo suena a "rejilla temblorosa"; un musico real
    adelanta o retrasa frases enteras unos milisegundos y vuelve al pulso. Esta
    deriva correlacionada (media 0, desviacion `sigma_s`, memoria `tau_s`)
    anade ese empuje sin desplazar la pieza respecto al compas.

    Hay que consultarla en orden cronologico (`offset_at` con tiempos no
    decrecientes): un tiempo anterior al ultimo se trata como el mismo instante."""

    def __init__(self, rng: np.random.Generator, sigma_s: float = 0.008, tau_s: float = 2.0) -> None:
        self._rng = rng
        self._sigma = max(sigma_s, 0.0)
        self._tau = max(tau_s, 1e-3)
        self._t: float | None = None
        self._x = 0.0

    def offset_at(self, seconds: float) -> float:
        if self._t is None:
            self._x = float(self._rng.normal(0.0, self._sigma)) if self._sigma > 0.0 else 0.0
        else:
            dt = max(seconds - self._t, 0.0)
            decay = float(np.exp(-dt / self._tau))
            spread = self._sigma * float(np.sqrt(max(1.0 - decay * decay, 0.0)))
            self._x = self._x * decay + (float(self._rng.normal(0.0, spread)) if spread > 0.0 else 0.0)
        self._t = seconds if self._t is None else max(seconds, self._t)
        # Acotada a 3 sigmas: una cola gaussiana extrema no debe sacar una nota
        # de su tiempo.
        limit = 3.0 * self._sigma
        return float(np.clip(self._x, -limit, limit))


def metric_accent(start_beat: float) -> float:
    """Factor de velocidad segun la posicion metrica: la nota en el tiempo
    fuerte suena algo mas, la de contratiempo algo menos. Sin conocer el
    compas, basta con distinguir tiempo entero, media parte y el resto."""
    frac = start_beat % 1.0
    if frac < 1e-6 or frac > 1.0 - 1e-6:
        return 1.05 if round(start_beat) % 2 == 0 else 1.0
    if abs(frac - 0.5) < 1e-6:
        return 0.94
    return 0.9
