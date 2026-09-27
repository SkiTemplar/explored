# Balance · mobiliario de base ya modelado

2026-09-27 · `building_pieces.json` · Validado con `Tools/DataCheck`
(`uv run datacheck --strict`; 0 errores y 0 avisos).

## Problema

`Tools/Blender/props/mobiliario_base.py` genera ocho mallas `SM_Base_*`. Cinco no
tenían ninguna pieza en `building_pieces.json`, así que el jugador no podía
construirlas: cama, muelle (tramo y final), banco de trabajo, ahumadero y bancal de
piedras. DataCheck solo comprobaba que las mallas usadas existieran, no al revés.

## Cambios

| Pieza | Tier | Coste | Herramientas | Requiere | Trabajo | Integridad / ciclón | Malla |
|---|---|---|---|---|---|---|---|
| `catre_bambu` | bambú | 8 bambús finos, 4 cordeles, 4 hojas de palma | cuchillo | — | 30 min | 35 / 1 | `SM_Base_Bed` |
| `muelle` | madera | 4 troncos pequeños, 6 maderas duras, 2 cuerdas, 2 resinas | hacha | — | 90 min | 70 / 1 | `SM_Base_Dock` |
| `muelle_final` | madera | 2 troncos pequeños, 4 maderas duras, 3 cuerdas | hacha | `muelle` | 60 min | 70 / 1 | `SM_Base_DockEnd` |

- **Catre:** es el paso intermedio de la biblia §3.10 («cama de hojas, catre de
  bambú, hamaca»). Cuesta algo más que el secadero (6 bambús finos, 4 cordeles), y su
  integridad es la del resto del mobiliario de bambú. `SM_Base_Bed` es justo eso: bastidor, lamas de
  bambú y colchón de hojas.
- **Muelle:** es la única estructura del pilar 2 del GDD («huerto, secadero, muelle»)
  que no se podía construir. Aguanta hasta ciclón 1, uno menos que el suelo de madera,
  porque está expuesto al oleaje. La resina impermeabiliza los pilotes, como en
  `pilote_madera`. El final (malla con norays y escalerilla) exige un tramo previo.
- **Encaje:** los tres usan encajes que ya existen. El catre usa `mueble` y el muelle
  `terreno`: la pieza se baja al suelo real, y en la orilla eso es el fondo somero,
  donde los pilotes de 2 m de la malla quedan bien. No hay socket «sobre agua»
  (véase Propuestas).
- **Categoría nueva `exterior`:** viene de la biblia («Exterior: muelle, puente
  colgante…»). El C++ la lee como `FName` libre, sin enumerado.

## Qué sigue sin pieza (nota de DataCheck)

Hay una nota nueva en `check_meshes` que lista las `SM_Base_*` sin pieza. Quedan tres,
y todas dependen de un sistema:

| Malla | Por qué no se da de alta |
|---|---|
| `SM_Base_Smokehouse` | `recipes.json` asigna el ahumado al `secadero` («Secadero y ahumadero»). Separarlo cambia `vessels` de las recetas y hay que regenerar `CookingData.inl` (Source/, fuera del alcance de datos). |
| `SM_Base_GardenPlot_Stones` | `ExploredPlantActor.cpp` solo reconoce la pieza `bancal`. Un bancal de piedra necesitaría que el huerto aceptara varias piezas. |
| `SM_Base_Workbench` | Es una mesa de carpintero con mazo de piedra. No es el «banco de chatarra» del GDD §8.5 (aluminio del Albatros), y no hay plantillas que exijan estación. |

## Progresión

- **Bambú (días 3–5):** el catre sale en cuanto hay cuchillo (los cordeles se trenzan
  a mano). Así la cama mejora a la vez que la cabaña de bambú.
- **Madera (días 5–10):** el muelle completo (tramo + final) cuesta 150 min de juego
  (≈ 4 min reales), 6 troncos pequeños, 10 maderas duras, 5 cuerdas y 2 resinas. Es
  del orden del astillero (120 min), así que llega a la vez que la canoa (GDD §8.10).

## Propuestas (sin tocar C++ en esta rama)

1. Socket `agua` en `EBuildSocket` (pilote y muelle sobre el agua, sin terreno debajo).
2. Efecto del catre: si el sueño leyera la calidad de la cama, el catre recuperaría
   más rápido que la cama de hojas (`sleepRecoveryHours` 6 → 5).
3. Ahumadero propio: pasar `pescado_ahumado` a `vessels: ["ahumadero"]` en una rama
   de cocina que regenere `CookingData.inl`, y dejar el secadero solo para secar.
4. Muelle como punto de amarre de los barcos (`boats.json`), para que la canoa no
   dependa de la playa.
