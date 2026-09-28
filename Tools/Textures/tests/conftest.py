import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from texgen.materials import MATERIALS, generate

# Resolución de test: los rasgos se definen en unidades de tile, así que 256 px conserva
# la estructura de 1024/2048 y mantiene la batería en segundos.
TEST_SIZE = 256


@pytest.fixture(scope="session")
def generated():
    """Mapas de todos los materiales a TEST_SIZE con su semilla por defecto (cacheados)."""
    return {name: generate(name, TEST_SIZE) for name in MATERIALS}
