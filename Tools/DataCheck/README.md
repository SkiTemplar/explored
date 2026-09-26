# DataCheck

Validador de `Content/Data/*.json` (sin Unreal). Replica en Python la regla de
combinación de `UCraftingLibrary` (cada slot lo cubre al menos una de las dos
piezas; las propiedades de un compuesto son el máximo de sus piezas; gana la
plantilla con más slots y, a igualdad, la primera del fichero).

```bash
cd Tools/DataCheck
uv run datacheck            # informe; código de salida 1 si hay errores
uv run datacheck --strict   # los avisos también fallan
uv run pytest               # tests (datos reales + regresiones sintéticas)
```

Qué comprueba:

- **Esquema** de `items.json`, `templates.json`, `verbs.json`, `story_es.json`,
  `plants.json`, `building_pieces.json`, `survival_needs.json`, `artifacts.json`,
  `ruins.json` y `meshes_pendientes.json` (campos, tipos, rangos: propiedades 0-5,
  pesos > 0, ids ASCII sin tildes…).
- **Referencias cruzadas**: resultados de plantillas, verbos, ingredientes y
  herramientas de construcción, objetos de siembra/cosecha, piezas requeridas.
- **Progresión**: simula la fabricación desde los materiales en bruto y exige que
  toda plantilla sea alcanzable (no sombreada por otra) y que cada herramienta y
  pieza tenga una cadena finita desde el inicio (sin ciclos de requisitos).
- **Mallas**: `meshPath` de `/Engine/BasicShapes` o `SM_*` generados por
  `Tools/Blender/props`; todo marcador o `null` debe estar en
  `meshes_pendientes.json`, y nada obsoleto puede quedarse allí.
- **Espejo del C++**: `survival_needs.json` contra las constantes de
  `Source/Explored/Survival/SurvivalModel.{h,cpp}`; `ruins.json` y `artifacts.json`
  contra los ids de `LexToString` y las constantes de `Source/Explored/Ruins/*Model.{h,cpp}`
  (técnicas, elementos, ruinas por isla, caminos de estrellas, umbral de «Coleccionista»).
- **Museo**: cada tesoro tiene un tipo de `story_es.json`, una malla de `tesoros.py` y
  algún hueco de mueble donde cabe; los muebles apuntan a piezas `museo` existentes.
- **Reglas del GDD §12**: sin fauna terrestre ni narrativa eliminada en los datos.
