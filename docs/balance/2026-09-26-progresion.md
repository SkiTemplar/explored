# Balance · progresión de fabricación, base y huerto

2026-09-26 · Datos en `Content/Data/*.json` · Validado con `Tools/DataCheck`
(`uv run datacheck`; 0 errores y 0 avisos).

Escala de tiempo: 1 día de juego = 24 h de juego = 40 min reales, así que 1 h de
juego ≈ 100 s reales. Todas las cifras en horas o minutos son de juego.

## 1. Herramientas: todas salen de materiales en bruto en ≤ 2 combinaciones

| Herramienta | Pasos | Cadena mínima de referencia |
|---|---|---|
| Lasca, estaca, cordel, cesta, martillo, pala, antorcha, flecha, cuchillo | 1 | p. ej. canto rodado + pedernal (Golpear) → lasca |
| Hacha | 1–2 | palo + liana (Atar) → mango atado; + lasca de pedernal (Atar) → hacha |
| Lanza | 2 | bambú grueso + liana → asta atada; + hueso largo → lanza |
| Arco | 1 (+2 para la cuerda) | fibra ×2 → cordel ×2 → cuerda; vara flexible + cuerda (Atar) → arco |

**Corregido hoy:** el arco no se podía fabricar. Empataba en número de slots con
`atado_generico`, que iba antes en `templates.json`, así que siempre salía un
«atado». `CraftingSpec` no lo veía porque solo comprueba que `Apply` devuelva true.

## 2. Tiers de construcción: coste de una cabaña básica

Suelo + 4 paredes + techo + puerta (`building_pieces.json`):

| Tier | Herramienta | Materiales | Trabajo | Aguanta ciclón |
|---|---|---|---|---|
| Palma | manos | 54 hojas de palma, 22 palos, 14 lianas | 140 min (≈ 4 min reales) | cat. 0 |
| Bambú | cuchillo | 22 bambús gruesos, 36 finos, 20 cordeles (= 40 fibras), 8 hojas | 205 min | cat. 1 |
| Madera | hacha | 14 troncos pequeños (112 kg), 11 maderas duras, 13 cuerdas (= 52 fibras), 4 resinas, 10 hojas | 305 min | cat. 2 |
| Piedra (muros) | martillo + pala | 38 basaltos, 19 arcillas, 10 arenas + techo y puerta de madera | 530 min | cat. 3 |

Tiempos estimados si el jugador sigue la curva de la biblia (§5.1):

- **Días 1–2:** refugio inclinado + fogata + cama de hojas (sin herramientas).
- **Días 3–5:** cuchillo → cabaña de bambú en la isla del Amaraje, secadero y
  recolector de lluvia. El cuello de botella es la fibra: unas 40 fibras para 20
  cordeles.
- **Días 5–10:** hacha → madera. El cuello de botella son 112 kg de troncos: no
  es viable sin las **angarillas** del GDD §8.2 (ver §5).
- **Semanas 2–3:** piedra. Requiere el Humo o Los Dientes para el basalto y el
  manglar para la arcilla, así que llega de forma natural con la balsa.

## 3. Huerto y limonero (`plants.json`)

| Cultivo | Días hasta la primera cosecha | Rinde |
|---|---|---|
| Limonero | 12 | 2–4 limones cada 3 días |
| Batata | 6 | 2–4, se arranca |
| Taro | 8 | 2–3, se arranca |
| Platanera | 8 | 4–8 cada 6 días |
| Maracuyá | 8 (necesita espaldera) | 3–6 cada 3 días |
| Piña | 10 | 1, se arranca |

Si se planta el día 2, el limonero da fruto el día 14, justo cuando el GDD dice
que aparece el escorbuto («semanas sin fruta»). Mientras tanto, la lima silvestre
cubre el hueco en Esmeralda.

## 4. Desajustes detectados (propuestas, sin tocar C++)

1. **La vitamina C se vacía en 3 días, no en semanas.** `SurvivalModel.cpp` usa
   `NutrientHours = 72` para proteína, hidratos y vitaminas. Con el valor inicial
   de 50, las vitaminas llegan a 0 en unas 36 h. *Propuesta:* separar
   `VitaminHours ≈ 336` (14 días) y añadir la condición `Scurvy` (hoy no existe
   en `ECondition`). `survival_needs.json` ya lo marca como «pendiente en C++».
2. **La nutrición de `items.json` (escala 0–5) no llega a `FConsumable` (0–100).**
   No hay conversión. *Propuesta:* 1 punto de objeto = 10 puntos de necesidad, de
   modo que un limón (5) = 50 puntos de vitaminas ≈ 7 días con `VitaminHours`
   = 336.
3. **Plantillas que se disparan con una sola pieza.** Como cada slot lo puede
   cubrir cualquiera de las dos piezas, «fibra de coco + cualquier cosa» da
   cordel y «palo + cualquier cosa» da pala (DataCheck lo lista como INFO).
   *Propuesta:* en `TemplateMatches`, exigir que cada pieza cubra al menos un
   slot con requisitos.
4. **Empates por orden de fichero.** Hay 28 empates, con el mismo número de slots,
   que decide la posición en `templates.json` (p. ej. cordel frente a cuerda y
   cesta). Mientras la regla siga así, las plantillas específicas deben ir antes
   que las genéricas. DataCheck falla si alguna queda inalcanzable.
5. **Restos de sistemas eliminados fuera de los datos:** `Tools/Blender/props/small_items.py`
   sigue generando `ItemFlightLog` e `ItemHaldenPage`, y `Tools/Blender/animals/`
   incluye `quadrupeds.py`, `serpent.py`, `bat.py`, `reptiles.py` y `rig.py`, en
   contra del GDD §10 y §12 (sin fauna terrestre ni esqueletos). Ningún JSON los
   referencia.
6. **Tendón:** sin fauna terrestre no tiene fuente clara. Se propone que salga del
   despiece de pescado grande o que se sustituya por fibra de pita en la sutura.
7. **Mallas:** 82 objetos usan un marcador de `/Engine/BasicShapes`, igual que
   26 piezas y 19 etapas de planta (`meshes_pendientes.json`). Las primeras que
   convendría generar son el limonero (4 etapas) y los kits de bambú.

## 5. Angarillas (añadidas en datos)

- **Objeto** `angarillas` (`items.json`): 4 kg, `DosManos`, 60 L, durabilidad 60,
  etiquetas `contenedor` y `arrastre`. La malla es un marcador y está en
  `meshes_pendientes.json`.
- **Plantilla** `angarillas` (`templates.json`, verbo Atar, 3 slots): varales
  (Largo ≥ 4 y Rígido ≥ 3), lecho (Fibroso ≥ 2) y unión (Ata ≥ 3).
- **Cadena de referencia:** bambú grueso + liana → atado; atado + hoja de palma →
  angarillas. Son 2 pasos y no hacen falta herramientas, así que las angarillas
  están disponibles desde el día 1.
- **Orden en el fichero:** va antes que `hacha` y `lanza`, porque empata con
  ellas en slots. Las cadenas de CraftingSpec no llevan nada con Fibroso ≥ 2, así
  que siguen dando hacha y lanza. DataCheck tiene un test de regresión para esto.
  Con un tronco atado + hoja de palma sale una angarilla en vez de un hacha, y
  eso es coherente.
- **Pendiente en C++:** el arrastre no existe. *Propuesta:* capacidad de
  4 objetos `DosManos` (4 troncos = 32 kg), a velocidad ×0,6 y sin poder nadar.
  Así la cabaña de madera pasa de 14 viajes con troncos en brazos a 4.
