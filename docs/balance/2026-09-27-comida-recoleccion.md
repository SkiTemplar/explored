# Comida de recolección pendiente (GDD §8.8)

Fecha: 2026-09-27. Fuente: `uv run datacheck` (nota `check_gdd_food_coverage`).

## Hueco detectado

El GDD §8.8 enumera la recolección y el marisqueo. `items.json` cubre coco, plátano,
mango, taro, yuca, batata, huevos, cangrejo, lapas, erizo, pulpo y langosta, pero
**faltan**:

- papaya, carambola, guayaba, fruta del pan, miel y algas comestibles
  (`alga_fibra` es material de trenzado, no comida);
- setas: hay 1 comestible de las 5 especies (2 comestibles, 2 tóxicas y
  1 alucinógena).

## Por qué no se añaden en esta pasada

Toda comida de `items.json` debe darse de alta en `recipes.json/foods`, y esa
tabla se genera en `Source/Explored/Cooking/CookingData.inl`
(`uv run datacheck --write-cooking`). El agente de datos solo puede tocar `.json`
bajo `Source/`, así que añadir comida lo tiene que hacer una rama que pueda
regenerar el `.inl` (p. ej. `nube/cocina-*`). DataCheck deja el hueco como nota,
no como error, para que el CI siga en verde.

## Propuesta lista para aplicar

Se usa la escala de `items.json` (0–5, × `nutrientPointsPerUnit` = 8) y la de
`recipes.json/foods` (puntos de `FConsumable`). Las mallas serían marcadores
`/Engine/BasicShapes/Sphere.Sphere` y entrarían en `meshes_pendientes.json`
(`uv run datacheck --write-pending`).

| id | kg | E/P/V | food | water | morale | toxicity | familia | notas |
|---|---|---|---|---|---|---|---|---|
| `papaya` | 0,8 | 2/0/4 | 14 | 8 | 2 | 0 | fruta | Mediano; alternativa al limón para vitaminas |
| `carambola` | 0,1 | 1/0/3 | 5 | 4 | 1 | 0 | fruta | |
| `guayaba` | 0,1 | 1/0/5 | 5 | 3 | 1 | 0 | fruta | vitamina C alta: segundo recurso contra el escorbuto |
| `fruta_pan` | 1,2 | 4/0/1 | 10 | 0 | −1 | 0 | tuberculo | cruda es pobre; `fruta_pan_asada` (biblia §3.6) da food 25, warmth 3 |
| `miel` | 0,3 | 3/0/0 | 12 | 0 | 4 | 0 | mineral (no se estropea) | propiedad Medicinal 2 (heridas, biblia §2.1) |
| `alga_comestible` | 0,1 | 0/1/2 | 3 | 1 | −1 | 0 | marisco | poza de marea, sin herramientas |
| `seta_parda` | 0,05 | 0/1/1 | 3 | 0 | 0 | 0 | seta | 2.ª comestible |
| `seta_blanca_toxica` | 0,05 | 0/0/0 | 2 | 0 | 0 | 0,6 | seta | Toxico 3; no se quita al cocinar |
| `seta_roja_toxica` | 0,05 | 0/0/0 | 2 | 0 | 0 | 0,9 | seta | Toxico 4; se parece a la parda |
| `seta_alucinogena` | 0,05 | 0/0/0 | 1 | 0 | 3 | 0,2 | seta | etiqueta `alucinogena` |

Criterios:

- **Vitaminas:** la guayaba (5) iguala al limón y la papaya (4) a la lima. Así, si
  el limonero tarda, hay una alternativa silvestre. Siguen siendo estacionales
  (biblia §9), así que el huerto conserva su valor.
- **Energía:** la fruta del pan y la miel son las calorías densas sin fuego. La miel
  llega con la colmena (biblia §10), después del tier de madera.
- **Setas:** mismo tamaño y peso para que distinguirlas sea cosa de observación y
  del cuaderno, no del inventario.

## Pendiente en C++

- La seta alucinógena necesita un efecto (`ECondition::Hallucinating` o similar)
  que hoy no existe. Hasta entonces bastaría con su toxicidad baja y la moral.
- `miel` como Medicinal para heridas depende de que el cuidado de heridas lea esa
  propiedad.
