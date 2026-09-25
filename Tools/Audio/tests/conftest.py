"""Fixtures compartidas: catalogo completo y cache de renderizado (generar
cada sonido una sola vez por sesion de tests, no una vez por test)."""

from __future__ import annotations

import numpy as np
import pytest

from explored_audio.build import render_sound
from explored_audio.catalog import SoundSpec, build_catalog
from explored_audio.constants import SAMPLE_RATE


@pytest.fixture(scope="session")
def catalog() -> list[SoundSpec]:
    return build_catalog()


@pytest.fixture(scope="session")
def rendered(catalog: list[SoundSpec]) -> dict[str, np.ndarray]:
    return {spec.name: render_sound(spec) for spec in catalog}


@pytest.fixture(scope="session")
def specs_by_name(catalog: list[SoundSpec]) -> dict[str, SoundSpec]:
    return {spec.name: spec for spec in catalog}


@pytest.fixture(scope="session")
def sample_rate() -> int:
    return SAMPLE_RATE
