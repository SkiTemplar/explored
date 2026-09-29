# Huerto medicinal: aloe y cúrcuma silvestre (2026-09-28)

Datos: `Content/Data/plants.json` (cultivos `aloe` y `curcuma`), `items.json`
(`planta_medicinal_aloe`, ya existente, y `rizoma_curcuma`, nuevo) y `templates.json`
(`pasta_medicinal_por_machacado`: mazo + planta con `Medicinal ≥ 1`).

## 1. Por qué

El GDD v3 §8.7 y la biblia de contenido §7.2 piden un huerto con «plátano, taro, batata,
piña, maracuyá, **especias y plantas medicinales**». `plants.json` solo tenía los cinco
primeros y el limonero, así que las curas del cuerpo (biblia 01: gel de aloe para
quemaduras, cataplasma y pasta para heridas) dependían de encontrar la planta silvestre.
En una base asentada, el jugador no podía producir medicina.

## 2. Números (propuesta pendiente de PIE)

Un año son 32 días (4 estaciones de 8). Régimen estable, regado cuando toca.

| Cultivo | Se planta con | Estaciones | Riego/día | Primera cosecha | Cosecha | Neto medio por bancal |
|---|---|---|---|---|---|---|
| Aloe | `planta_medicinal_aloe` | seca, primeras lluvias, ciclones | 0 | 8 d | 1–2 hojas cada 4 d, la mata sigue | 1,5 cada 4 d ≈ **0,37/día**, 24 días al año |
| Cúrcuma silvestre | `rizoma_curcuma` | primeras lluvias, monzón | 1 | 9 d | 3–5 rizomas, se arranca | 4 − 1 replantado = **3 cada 9 d ≈ 0,33/día**, 16 días al año |

Lectura:

- **Uno de cada basta.** Un corte profundo pide una cura; una quemadura, otra. Con
  ≈ 0,35 unidades al día por bancal, un bancal de cada planta cubre más de una herida
  cada tres días, que es mucho más de lo que se hiere un jugador asentado. El huerto
  medicinal es seguro de vida, no una fuente de comida: no le quita sitio a la platanera.
- **Se complementan por estación.** El aloe se da en la seca y en los ciclones
  (cuando el sol y las quemaduras aprietan) y descansa en el monzón. La cúrcuma es al
  revés: la estación húmeda es la de las heridas infectadas y es la suya. Ninguna
  estación se queda sin cultivo medicinal.
- **Ninguno lo picotean las aves** (`birdsEat` ausente): no exigen espantapájaros.
- **El aloe no pide riego**, igual que la piña: es el cultivo de «plantar y olvidarse»
  para quien pasa semanas fuera navegando.
- **La cúrcuma es también especia** (`tags: especia`), para cuando la cocina tenga
  condimentos. Hoy, machacada con el mazo, da `pasta_medicinal` por la plantilla
  genérica; la «pasta de cúrcuma silvestre» propia (infección, biblia 01) llega con el
  modelo de medicina del PR #84.

## 3. Validación nueva en DataCheck

`farm.py` exige ahora que:

1. Todo cultivo coseche **comida** (`recipes.json/foods`) o un objeto con
   **`Medicinal ≥ 1`**. Un cultivo que no sirve para nada es un error.
2. Haya **al menos un cultivo medicinal que no sea comida** (el limón cura el escorbuto,
   pero es fruta y no cuenta).

## 4. Pendiente

- **Mallas:** las seis etapas nuevas (`aloe.*`, `curcuma.*`) y `rizoma_curcuma` están
  en `meshes_pendientes.json`. El objeto usa la esfera provisional, como el resto de
  objetos sin malla.
- **Dónde se consigue el primer rizoma:** la dispersión de vegetación está en C++
  (`VegetationScatter`) y no en datos. Propuesta: cúrcuma silvestre en el sotobosque
  húmedo de Esmeralda (fase 1), igual que el aloe crece en las laderas secas de Landing.
- **`FarmSpec.cpp`** lleva su propia copia de las plantas en `DataPlants()`. Ningún `It`
  falla porque haya dos cultivos más, pero conviene añadirlos en local cuando se toque ese
  spec (el encargo nocturno no toca `Source/`).
- **Noni** (hojas para la cataplasma, biblia de contenido §3.7): se deja fuera a
  propósito. Su fruto es comida y darlo de alta obliga a regenerar `CookingData.inl`
  (`uv run datacheck --write-cooking`), que es C++ y no entra en este encargo.
