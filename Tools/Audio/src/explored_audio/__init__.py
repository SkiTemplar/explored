"""explored_audio: sintesis procedural de efectos y ambientes para Explored.

Todo el catalogo se genera por codigo (ruido filtrado, sintesis modal, FM,
aditiva y granular). No se usan muestras externas. Punto de entrada:
`explored_audio.build.main()` (tambien expuesto como script `explored-audio`).
"""

from .constants import BIT_DEPTH, SAMPLE_RATE

__all__ = ["SAMPLE_RATE", "BIT_DEPTH"]
