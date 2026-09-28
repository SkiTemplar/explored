# Lote6-fondomarino: hoja de créditos

Sin hoja de contacto en PNG todavía: `contact_sheet.py` compone el original y el
normalizado a partir de lo que exporta `normalize.py`, y ese paso necesita Blender +
`--lote lote6-fondomarino` (ver `Content/Data/seabed_scatter.json`, campo `pipeline`), que
esta sesión no ejecuta (worktree sin Unreal). Esta tabla es el crédito exacto mientras
tanto; en cuanto se genere `docs/art/packs/lote6-fondomarino.png`, sustituye a este fichero
como hoja de contacto y esta tabla se mueve a su pie.

## CC0 (Tools/Packs/packs.json)

| Id | Modelo | Autor | Página |
|---|---|---|---|
| `minipoly_coral_reef_set` | Coral Reef Set | MiniPoly | <https://poly.pizza/m/74GL45Fvdh> |
| `quaternius_underwater_rock` | Rock | Quaternius | <https://poly.pizza/m/34W5ymEePk> |
| `quaternius_pebble_square` | Pebble Square | Quaternius | <https://poly.pizza/m/2YtLzwgsWp> |

CC0 1.0: sin obligación de crédito. Se lista igualmente por trazabilidad.

## CC-BY 3.0 (Tools/Packs/packs_cc_by.json) — crédito obligatorio

GDD v2 §7.1: el crédito exacto de cada obra derivada va en los créditos del juego. Texto
tal cual (`license.attribution` de cada entrada):

- «Coral» de Poly by Google (poly.pizza/m/4KUXdtDdgHR), CC BY 3.0.
- «Orange Coral» de Device Lab (poly.pizza/m/3HEc6LvqCJd), CC BY 3.0.
- «underwater_enviro_coral» de God Appeasers (poly.pizza/m/0CrDBiCnI5e), CC BY 3.0.
- «Sponge» de Poly by Google (poly.pizza/m/0ICnrmv06-S), CC BY 3.0.
- «Sea anemone» de Poly by Google (poly.pizza/m/5mWGVL6VgMd), CC BY 3.0.
- «Seaweed» de Laney XR Labs (poly.pizza/m/461xlaa6SZW), CC BY 3.0.
- «kelp» de M Smith Jonn (poly.pizza/m/5ECC8rIOZJl), CC BY 3.0.
- «Clam» de Poly by Google (poly.pizza/m/65ObbG0O7Ze), CC BY 3.0.
- «Seashell» de dook (poly.pizza/m/5ovn4mnRejL), CC BY 3.0.
- «Sea Urchin» de Poly by Google (poly.pizza/m/f4iwEhEP3-d), CC BY 3.0.
- «Starfish» de Poly by Google (poly.pizza/m/6H-0K9IEr56), CC BY 3.0.

Todas verificadas el 2026-09-28 contra la insignia de licencia de la ficha del modelo en
Poly Pizza (agregador de modelos CC0/CC-BY del antiguo Google Poly, Quaternius, Kenney y
autores independientes; redistribuye con la licencia que declaró quien subió el modelo).
Descargadas y comprobado el sha256 con `uv run python Tools/Packs/fetch_packs.py --manifest packs_cc_by.json`.

## Qué cubre cada asset

| Especie (`Content/Data/seabed_scatter.json`) | Categoría | Pack(s) |
|---|---|---|
| CoralCerebro | Arrecife somero | `minipoly_coral_reef_set` (2 de sus 8 variantes) |
| CoralCuernoCiervo | Arrecife somero | `minipoly_coral_reef_set` (2 variantes) + `polybygoogle_coral_staghorn` |
| CoralMesa | Arrecife somero | `minipoly_coral_reef_set` (4 variantes) + `devicelab_orange_coral` |
| CoralAbanico | Borde del arrecife | `godappeasers_coral_fan` |
| EsponjaBarril | Borde del arrecife | `polybygoogle_sponge` |
| Anemona | Borde del arrecife | `polybygoogle_anemone` |
| PraderaMarina | Arena | `laneyxrlabs_seaweed` |
| ConchaGrande | Arena | `polybygoogle_clam` |
| ConchaPequena | Arena | `dook_seashell` |
| AlgaKelp | Roca | `msmithjonn_kelp` |
| ErizoMar | Roca | `polybygoogle_seaurchin` |
| EstrellaMar | Roca | `polybygoogle_starfish` |
| RocaSubmarina | Roca (estructura, con colisión) | `quaternius_underwater_rock` + `quaternius_pebble_square` |

Los 8 nodos (`A_CoralReef`..`A_CoralReef.007`) de `minipoly_coral_reef_set` son formas de
coral sin clasificar visualmente por esta sesión (sin Blender ni Unreal a mano): el reparto
de arriba entre cerebro/cuerno de ciervo/mesa es una primera pasada por número de nodo, no
por su forma real. Reasignar según la forma al verlos en Blender es parte del paso de
`normalize.py` de la sesión con Unreal (ver `pipeline` en `seabed_scatter.json`).
