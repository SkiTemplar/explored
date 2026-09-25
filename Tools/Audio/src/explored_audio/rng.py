"""Generacion determinista de semillas a partir del nombre de cada sonido.

Cada fichero del catalogo se deriva de una semilla estable (CRC32 del nombre),
de modo que volver a ejecutar la generacion produce exactamente los mismos
bytes: es el requisito de determinismo que comprueban los tests.
"""

from __future__ import annotations

import zlib

import numpy as np


def seed_for(name: str) -> int:
    """Semilla entera de 32 bits, estable para un nombre de sonido dado."""
    return zlib.crc32(name.encode("utf-8")) & 0xFFFFFFFF


def rng_for(name: str) -> np.random.Generator:
    """Generador de numpy determinista asociado a un nombre de sonido."""
    return np.random.default_rng(seed_for(name))
