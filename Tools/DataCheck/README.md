# DataCheck

Validador de `Content/Data/*.json` de Baliza Siete (GDD v3) sin Unreal. Comprueba la lista
cerrada de objetos, verbos y recetas, y que los números que también viven en C++ (valores por
defecto de los modelos puros) coinciden con el JSON.

```bash
cd Tools/DataCheck
uv run datacheck            # informe; código de salida 1 si hay errores
uv run datacheck --strict   # los avisos también fallan
uv run datacheck --root RUTA # valida otro checkout (los tests lo usan con repos temporales)
uv run pytest               # tests (datos reales + regresiones sintéticas)
uv run ruff check . && uv run basedpyright   # lint y tipos (CI)
uv run pytest -q --cov --cov-report=term-missing   # cobertura (mínimo 80 %)
```

Un JSON mal formado se informa como error (`JSON mal formado`) en vez de abortar con una
excepción, y los campos de tipo equivocado (texto donde va un número, ids nulos…) también.

Qué comprueba:

- **`items.json`**: exactamente los 17 objetos del GDD §7, con la pila y el `heavy` del GDD
  (tronco y lente pesados, `size: DosManos`), malla `/Game/Art/K3_Objetos/SM_<Nombre>_01`,
  nombres en los dos idiomas, pesos y volúmenes positivos.
- **`verbs.json`**: los 6 verbos del GDD §5.6 en orden, con nombre y ayuda bilingües.
- **`recipes.json`**: las 10 recetas del GDD §5.6 (verbo, lugar y segundos del GDD), entradas,
  herramientas y salidas que existen, cantidades enteras positivas, una sola salida (objeto o
  mundo) y que todo objeto se pueda recoger o fabricar.
- **`fuels.json`**: hoguera y combustibles (leña y tarro de aceite), espejo de
  `FCampfireConfig` (`Cooking/CampfireModel.h`).
- **`survival_needs.json`**: las 4 barras del GDD §5.1, consumibles (espejo de
  `FIslandConsumables`), kit inicial, huecos de la mochila (`FBackpackRules`) y barril
  (`FRainBarrelModel`).
- **`fish.json`**: pesca simplificada, espejo de `FCastFishingConfig`.
- **`music_layers.json`**: papeles, islas, bucles y tempo frente a `MusicDirectorModel`.
- **Textos que ve el jugador**: guía anti-IA (lista negra, exclamaciones, emoji) y términos
  eliminados del diseño.
