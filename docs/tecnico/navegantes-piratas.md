# Navegantes y piratas: cómo enganchar los modelos de fase 3 en el motor [F3]

Estado: los cuatro modelos son puros y tienen spec en el host (`Explored.Villages.*` y
`Explored.Raiders.*`). Falta la integración con Unreal, que tiene que hacer una sesión con
el editor. Diseño y números: biblia 05 §1.2-1.6 y §2.1-2.6; red: biblia 08 §1.3, §5.5 y
§5.6. Las decisiones que la biblia no fijaba están en la propia biblia 05, marcadas
`[Decisión]`.

## Qué hay

| Modelo | Fichero | Spec | Qué decide |
|---|---|---|---|
| `FReputationModel` | `Villages/ReputationModel.h` | `Explored.Villages.Reputation` | Reputación 0-100 por asentamiento, tramos, acciones, enfriamiento de 15 días, objetos rituales devueltos, favores (wayfinding ≥ 70, favores altos ≥ 90) y la sección de guardado `reputation` |
| `FBarterModel` | `Villages/BarterModel.h` | `Explored.Villages.Barter` | Si un intercambio se acepta: valor 1-5 por categoría × tasa en cuartos, ventana 8:00-18:00, tramo mínimo de cada oferta, solo básicos en Cauta, trueque gratis del tablón y cuántas unidades cubre lo ofrecido (`AffordableCount`, para el prompt de biblia 06 §2.11) |
| `FPirateThreatModel` | `Raiders/PirateThreatModel.h` | `Explored.Raiders.Threat` | Amenaza 0-100, sus subidas y la bajada de 5 cada 10 días de calma, categoría de asalto, calendario por semilla, aviso (humo la víspera, tambor si es categoría 3), grupos escalados por jugadores y la sección de guardado `raiders` |
| `FRaiderCampsModel` | `Raiders/RaiderCampsModel.h` | `Explored.Raiders.Camps` | Dónde van Cala Rota y Fondeadero Podrido (tienda, hoguera, cofre, barco) y qué hay en cada cofre, fijo por la semilla del mundo |

Los navegantes **nunca** son enemigos: `FReputationModel::IsHostileCombatant` devuelve
siempre false y `StrikeVillager` no quita vida, solo reputación. La IA y el combate tienen
que preguntar ahí, no deducirlo del tramo.

## Enganche propuesto

1. **Subsistema de mundo `UVillagesSubsystem`** (solo servidor). Dueño de un
   `FReputationState`. Registra la sección `reputation` con `FReputationModel::Save` y
   `Load`. Recibe por RPC los intentos de trueque, valida el inventario de quien lo hace
   (biblia 08 §2.4), llama a `FBarterModel::Execute` y aplica los objetos. Replica solo el
   tramo de cada asentamiento: el número no se enseña nunca en pantalla.
2. **Primer contacto.** Un volumen de 80 m (`FReputationModel::FirstContactRadiusM`) en
   el centro de cada asentamiento llama a `Contact` al entrar el primer jugador.
3. **Acciones.** Caza, minería, tala y saqueo cerca de la aldea las detectan los
   sistemas que ya existen (`Fauna`, `TerrainEdit`, `Felling`, contenedores) y llaman a
   `Apply` con la acción. La jornada respetuosa se comprueba una vez al día. Devolver un
   objeto ritual: `URuinsSubsystem` llama a `ReturnRitualObject` con la procedencia del
   artefacto (`EArtifactProvenance`).
4. **Subsistema de mundo `URaidersSubsystem`** (solo servidor). Dueño de un
   `FPirateThreatState`. Registra la sección `raiders`. Al empezar cada día de juego llama
   a `EvaluateDay` con la semilla de la partida y `FRaidConditions` (base marcada = hay
   torre de vigía o bandera; recursos a la vista = objetos sueltos a menos de 40 m de la
   base; aldea Hostil = tramo de la aldea más cercana; jugadores conectados). Con
   `Warning` lanza el humo; con `Raid`, el tambor de madrugada si `bDawnDrum` y el grupo
   de `Party` desde el barco del campamento más cercano.
5. **Campamentos.** Al crear el mundo, `FRaiderCampsModel::Generate(Density, Avoid)` con
   las posiciones de `FPoiLayout::Generate` en `Avoid`. Cada campamento crea su tienda,
   su hoguera, su cofre (con `Loot`) y su barco sobre `FBoatModel`. La geometría no se
   replica: todos los clientes tienen la semilla. Se replica el estado de cada uno
   (cofre abierto, hoguera apagada, barco hundido).

## Qué hay que verificar en local

- Que los cuatro `.cpp` compilan en UE 5.6 (en el host compilan contra el shim de Core).
- Que las specs pasan también en el editor: `Tools/test.ps1 Explored.Villages` y
  `Tools/test.ps1 Explored.Raiders`.
- En PIE, cuando exista el cableado, que Cala Rota queda en una cornisa accesible de Los
  Dientes (con la semilla oficial no hay playa) y que su piragua se ve a flote.
