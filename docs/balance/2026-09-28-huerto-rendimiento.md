# Huerto: rendimiento por bancal y reglas del modelo (2026-09-28)

Datos: `Content/Data/plants.json` (cultivos y el bloque nuevo `rules`) y
`recipes.json/foods` (puntos de hambre por unidad). Modelo: `FFarmModel`
(`Source/Explored/Farming/FarmModel.h`). Hambre: 100 puntos en 36 h de juego
(`survival_needs.json`), así que **un jugador gasta ≈ 67 puntos al día**.

## 1. Hallazgo: la piña no producía nada

La piña se plantaba con una `pina`, daba 1 (min = max = 1) y se arrancaba al cosechar
(`everyDays: 0`). Cada ciclo de 10 días devolvía exactamente la piña que costó: **cosecha
neta cero**. El resto de cultivos que se replantan con su cosecha sí ganan (taro y batata dan
como mínimo 2).

**Propuesta aplicada:** `everyDays: 8`. La mata vuelve a dar fruto (retoño) cada 8 días
buenos en su estación, como la platanera y la maracuyá. No hace falta un objeto nuevo
(`corona_pina`) ni tocar el C++, porque `FFarmModel` ya admite cosechas repetidas.
Alternativa si el director prefiere que se arranque: `min: 2`.

DataCheck ahora exige que un cultivo que se planta con su propia cosecha y se arranca al
cosechar dé **como mínimo 2**.

**Pendiente en local (no se toca `Source/`):** `Source/Explored/Tests/FarmSpec.cpp`
lleva una copia de `plants.json` en `DataPlants()`. Hay que cambiar la piña de
`TEXT("pina"), 1, 1, 0` a `TEXT("pina"), 1, 1, 8`. Ningún `It` actual falla por esto: la
primera cosecha sigue llegando a los 10 días.

## 2. Rendimiento por bancal en su estación

Régimen estable, regado todos los días. Un año son 32 días (4 estaciones de 8).
«Neto» descuenta lo que se replanta. Las aves, sin espantapájaros, se llevan el 25 % de la
cosecha la mitad de las veces (`birdChance` × `birdShare`): de media, un 12,5 %.

| Cultivo | Primera cosecha | Neto medio | Unidades/día | Puntos/unidad | Puntos/día por bancal | Días que produce al año | Riegos/día |
|---|---|---|---|---|---|---|---|
| Platanera | 8 d | 6 cada 6 d | 1,0 | 12 crudo | **12** (10,5 sin espantapájaros) | 16 | 1 |
| Maracuyá (espaldera) | 8 d | 4,5 cada 3 d | 1,5 | 5 | 7,5 (6,6) | 16 | 1 |
| Batata | 6 d | 2 cada 6 d | 0,33 | 18 asada | 6 | 24 | 1 |
| Taro | 8 d | 1,5 cada 8 d | 0,19 | 16 hervido | 3 | 16 | **2** |
| Piña (propuesta) | 10 d | 1 cada 8 d | 0,125 | 14 | 1,75 | 16 | 0 |
| Limonero | 12 d | 3 cada 3 d | 1,0 | 2 | 2 (su valor es el escorbuto) | 32 | 1 |

Lectura:

- **Alimentar a un jugador solo con el huerto** en primeras lluvias o monzón pide unos
  **6 bancales de platanera**, o 4 de platanera y 3 de maracuyá. En la seca solo producen
  batata, piña y limonero: haría falta una docena de bancales de batata. El huerto
  complementa la pesca y la recolección y no las sustituye. Encaja con que la
  supervivencia deje de ser protagonista hacia la semana 2–3 (GDD v2 §3.1).
- **El taro es el peor cultivo**: la mitad de comida que la batata y el doble de riego.
  Su papel es la estación húmeda, donde la lluvia riega sola (2 h de lluvia plena = 1
  riego, `rainHoursPerWatering`). Si en PIE nadie lo planta, subir la cosecha a 3–4.
- **La piña ya no es un error**, pero sigue siendo marginal. Justo por eso no pide riego:
  es el cultivo de «plantar y olvidarse».

## 3. Contradicciones abiertas entre el modelo y la biblia 02 §10.1

Ahora `plants.json/rules` copia las constantes de `FFarmModel`, y DataCheck falla si se
separan. Dos de ellas chocan con la biblia y **las tiene que decidir el director**:

| Punto | Biblia 02 §10.1 | `FFarmModel` | Recomendación |
|---|---|---|---|
| Días sin riego | «deja de avanzar (no muere) hasta que se riega»: solo pausa | Sedienta a 1 día, marchita a 2 y **muerta a 4** (el limonero nunca muere) | Seguir la biblia: que se quede marchita sin morir. Encaja con la regla anti-microgestión (biblia §8.3) y con el cooperativo, porque un invitado que no se conecta en 4 días no pierde el huerto del anfitrión. |
| Radio del espantapájaros | 4 m | 15 m (`ScarecrowRadius = 1500` cm) | 4 m pide un espantapájaros cada ~3 bancales, y 15 m cubre un huerto entero. Con el rendimiento de §2, la pérdida sin él es del 12,5 %: con 4 m, a nadie le compensaría construir más de uno. Recomiendo **mantener 15 m** y corregir la biblia. |

## 4. Fuera de este análisis: la carne de caza

`fauna.json` ya da hueso y grasa al despiezar el cerdo salvaje de Esmeralda (fase 1), pero
**no da carne ni piel**: quedan en `lootPendiente`. Añadir la carne exige una entrada de
comida en `recipes.json` y regenerar `Source/Explored/Cooking/CookingData.inl` con
`uv run datacheck --write-cooking`. El encargo nocturno no toca `Source/`, así que tiene que
hacerlo una sesión con permiso para regenerar ese `.inl`. Es el hueco más visible de la
fauna del acceso anticipado: cazar un cerdo y no sacar comida.
