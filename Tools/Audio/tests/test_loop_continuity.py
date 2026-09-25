"""Continuidad de los bucles: el punto de union no debe dar un salto de valor
ni de derivada mayor que los que ya ocurren en el resto del propio fichero.

Esta es la comparacion correcta para contenido con transitorios (crepitar,
gotas...): comparar el salto de union contra la desviacion tipica GLOBAL del
fichero penaliza injustamente a un sonido cuyo nivel varia mucho en el
tiempo (una desviacion global dominada por los tramos tranquilos hace que
cualquier evento normal parezca "demasiado grande"). Comparando contra el
percentil 99.9 de los saltos que ya aparecen en el propio fichero, un evento
que cae justo en la union solo falla si es mas extremo que cualquier cosa
que ya suene en el resto del bucle.
"""

from __future__ import annotations

import numpy as np


def _seam_ceiling(channel: np.ndarray) -> float:
    interior_diffs = np.abs(np.diff(channel))
    return max(float(np.percentile(interior_diffs, 99.9)) * 1.5, 1e-4)


def test_bucles_sin_clic_en_el_punto_de_union(catalog, rendered):
    loop_specs = [spec for spec in catalog if spec.is_loop]
    assert loop_specs, "no hay ningun bucle en el catalogo: el test no comprobaria nada"

    for spec in loop_specs:
        audio = rendered[spec.name]
        channels = [audio] if audio.ndim == 1 else [audio[c] for c in range(audio.shape[0])]
        for idx, ch in enumerate(channels):
            ceiling = _seam_ceiling(ch)
            value_gap = abs(float(ch[0]) - float(ch[-1]))
            deriv_gap = abs(float(ch[1] - ch[0]) - float(ch[-1] - ch[-2]))
            assert value_gap <= ceiling, (
                f"{spec.name} canal {idx}: salto de valor en la union ({value_gap:.5f}) "
                f"mayor que lo normal en el propio fichero ({ceiling:.5f})"
            )
            assert deriv_gap <= ceiling, (
                f"{spec.name} canal {idx}: salto de derivada en la union ({deriv_gap:.5f}) "
                f"mayor que lo normal en el propio fichero ({ceiling:.5f})"
            )
