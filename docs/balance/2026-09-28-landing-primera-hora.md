# Landing: los primeros 60 minutos (porción vertical, GDD v2 §6.1)

Fecha: 2026-09-28. Fuentes: `Content/Data/*.json` (validados con `uv run datacheck --strict`),
`FHarvestModel`/`FFellingModel` (golpes y botín), `FTerrainEditModel` (minería),
`SurvivalModel` vía `survival_needs.json`, `UTimeOfDaySubsystem` (día de 40 min,
arranque a las 07:30). Sin PIE: los tiempos de caminar y de decidir son estimaciones y
van marcados con «≈».

Escala: **1 h de juego = 100 s reales**; 60 min reales = 36 h de juego, es decir, del
día 1 a las 07:30 al día 2 a las 19:30 (una noche entera dentro de la primera hora).
Se supone ~1 golpe/s a mano (el pico va a 0,9 s, `SecondsPerPickaxeHit`).

## 1. Relojes del cuerpo que marcan el ritmo

| Necesidad | Inicial | Horas para vaciarse | Llega a 0 (sin atenderla) |
|---|---|---|---|
| Sed | 70 | 20 h | 14 h de juego ≈ **23 min reales** (día 1, 21:30) |
| Sueño | 90 | 30 h | 27 h ≈ 45 min reales |
| Hambre | 85 | 36 h | 30,6 h ≈ 51 min reales |
| Proteína | 50 | 72 h | fuera de la primera hora |

La sed manda: el primer coco tiene que llegar antes del minuto ~20. Una palmera talada a
mano da 1–3 cocos maduros (media 2), así que la primera tala resuelve la sed del día 1.

## 2. Qué rinde cada acción (medias)

| Acción | Golpes | Rinde (media) | Tiempo real ≈ |
|---|---|---|---|
| Talar palmera a mano | 8 | 4 hojas por golpe (0–1) + 3 al caer, 1 tronco, 2 cocos, 1 fibra | 10 s + recoger 15 s |
| Talar sotobosque a mano | 5 | 2,5 palos por golpe + 1,5 al caer, 1,5 ramas, 1 hoja de platanera, 1 tronco | 8 s + 10 s |
| Arbusto a mano | 1 | 0,5 liana, 0,5 corteza, 0,5 algodón | 2 s |
| Roca a mano (contundente: 3) | 6 | 3 cantos + 1 piedra plana | 8 s |
| Cavar tierra/arena con pala | 6 por m³ | 6 `tierra_suelta` o `arena` por m³ (`mining.json`) | 5,4 s/m³ de golpes |

## 3. Línea de tiempo recomendada (día 1, 07:30 → día 2, 19:30)

| Min. real | Hora de juego | Qué hace | Consume | Obtiene |
|---|---|---|---|---|
| 0–4 | 07:30–10:00 | Fuselaje: kit de arranque, chapa del Albatros | — | chapa, cantimplora… |
| 4–10 | 10:00–13:30 | 2 palmeras y 2 sotobosques a mano, 4 arbustos, 3 rocas | — | ~14 hojas de palma, ~8 palos, ~2 lianas, 4 cocos, 2 hojas de platanera, 9 cantos, 3 piedras planas |
| 10–13 | 13:30–15:20 | Pala: chapa + palo (Atar). Mango atado + canto: **hacha de piedra**. Lasca de pedernal + canto (Tallar): canto aguzado; + mango atado: **pico de piedra** | 4 palos, 2 lianas, 2 cantos, 1 lasca | pala (nivel 1), hacha, pico (nivel 2) |
| 12–16 | 14:40–17:00 | Refugio inclinado (20 min de juego) y fogata (10) | 3 palos, 8 hojas, 1 liana; 6 cantos, 5 ramas, 1 fibra | refugio, fuego |
| 16–20 | 17:00–19:30 | 2 rocas, arena de la playa (2 m³ con pala ≈ 11 s de golpes) | — | 6 cantos (quedan 8), 2 piedras planas, 12 arenas |
| 20–24 | 19:30–22:00 | Arriate del limonero (30 min, pala) y plantar el limón silvestre; beber coco | 8 cantos, 2 arenas, 1 limón | limonero día 1 (fruto el día 13) |
| 24–40 | 22:00–07:30 | Dormir en el refugio (el sueño estaría en ~25) | — | — |
| 40–48 | 07:30–12:00 | **Cueva pequeña**: galería de 1×2×4 m en tierra con pala (8 m³ = 48 golpes ≈ 45 s) y una escalera picada bajando 1,5 m | — | 48 `tierra_suelta` (16,8 kg), agujero que queda al recargar |
| 48–55 | 12:00–16:00 | 1 roca más, 2 sotobosques; piedra de trabajo, bancal (pala), cama de hojas | 1 piedra plana + 2 cantos; 6 palos, 2 arenas; 6 hojas + 4 hojas de platanera | base mínima y huerto |
| 55–60 | 16:00–19:30 | Mirador de la Cresta (boceto del mapa), marae del palmeral | — | primer trazo de mapa, ruina |

Resultado: a los 60 minutos el jugador cumple el criterio de salida de la porción
vertical salvo el tesoro expuesto (necesita estantería del museo, tier madera; el hacha de
piedra ya está desde el minuto 13). Cabe en la segunda hora.

## 4. Hallazgos

1. **El pico de piedra no tiene qué picar en Landing.** Landing solo tiene tierra y arena
   en superficie y basalto (dureza 3, pico tallado) desde 4 m. Y el pico tallado necesita
   `basalto`, que Landing no da en superficie (biblia 04: «canto rodado, pedernal»). El
   **fondo de la cueva de Landing queda en ~4 m**, lo que acota bien la prueba del pipeline
   de edición (GDD v2 §7.3) y deja el pico para la veta de cobre de Esmeralda. Es coherente
   con el GDD v2 §6.1 («pala en tierra/arena»): no se propone cambiarlo.
2. **Hacha de piedra y pico, sin robarse la cabeza.** *(Corregido tras la integración del
   2026-09-28.)* La primera versión dejaba que canto rodado + mango diera pico y se perdía
   el hacha de piedra que la biblia 01 y 02 §1.2 dan por hecha. Ahora la Cabeza del pico
   exige **Punta ≥ 2** con etiqueta `piedra`, `mineral` o `metal`: canto rodado, piedra
   plana o basalto + mango dan **hacha**, y el pico de piedra pide antes aguzar el canto
   (lasca + canto con Tallar → `canto_aguzado`). Cuesta una lasca y un paso más, que en
   Landing son ~30 s: el pico no hace falta hasta Esmeralda (hallazgo 1).
3. **Hojas de platanera: cuello de botella de la cama.** La cama pide 4 y solo salen del
   sotobosque (0–2 al caer, media 1): hacen falta ~4 sotobosques. Si en PIE se nota, bajar
   la cama a 2 hojas de platanera o dar 1–2 fijas al sotobosque.
4. **Acarreo en minería.** Con 6 unidades por m³ (una por hueco de golpe), una galería de
   1×2×5 m en basalto son 60 basaltos = 60 kg: 2–3 viajes con 25 kg a hombros. Da la
   presión de acarreo que justifica los vagones de fase 2 (GDD v2 §3.5) sin volverlo
   tedioso. La tierra pesa poco (0,35 kg por unidad): cavar la cueva de Landing no satura.
5. **La sed marca el primer cuarto de hora.** Con 70 inicial y 20 h, el coco del minuto
   ~8 llega con margen; sin talar una palmera el jugador ve la visión borrosa hacia el
   minuto 23.

## 5. Qué hay en los datos después de esta rama

- `mining.json`: materiales (espejo del C++), estratos por isla, capa y fase, vetas
  finitas, rendimiento y niveles de herramienta 0–4.
- `items.json`/`templates.json`: `pico`, `canto_aguzado`, `basalto_tallado`,
  `cabeza_pico_rescatada`, `tierra_suelta`; plantillas `pico`, `punta_de_canto_por_tallado`,
  `cabeza_pico_por_tallado` y `cabeza_pico_de_chapa`/`cabeza_pico_de_hierro` (estas dos con
  `"station": "banco_chatarra"`, pieza nueva de `building_pieces.json`).
- `fauna.json`: cangrejo de los cocoteros y gaviota (Landing), cerdo salvaje (Esmeralda),
  sin fauna terrestre en el Humo, fragatas y gaviotas (Los Dientes); cabra montés (fase 2).
- `fases_futuras.json`: borrador de raíles, granja, murallas y trueque (fases 2 y 3).
