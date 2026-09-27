# El cuerpo como HUD (P-BODY)

GDD §8.3 y biblia §5: el estado del náufrago no se enseña con iconos, se siente.
Este documento describe cómo se reparte el trabajo entre los modelos puros y la capa
de Unreal, y fija los **nombres de los parámetros del material de postproceso** para
que `Tools/Unreal/build_materials.py` pueda crear el material más adelante.

## Piezas

| Pieza | Tipo | Qué hace |
|---|---|---|
| `Survival/SurvivalModel.{h,cpp}` | modelo puro | Necesidades, temperatura, estados. Nuevo: `FSurvivalModeSettings` (modo Personalizado con multiplicador de necesidades), `VitaminHours` = 336 |
| `Survival/BodyModel.{h,cpp}` | modelo puro | Escorbuto, cortes e infección, `FallDamage`, picaduras y antídotos, fuentes de intoxicación, quemadura solar por dosis, dieta monótona, torpeza y alucinaciones por falta de sueño, sucesos de ánimo |
| `Survival/BodySignals.{h,cpp}` | modelo puro | `FBodySignalsModel`: estado → señales perceptivas (0–1) + suavizado + lectura del reloj |
| `Survival/BodySignalsComponent.{h,cpp}` | capa UE | Tickea el modelo, lee el entorno, detecta caídas, aplica postproceso, tiritona, temblor de manos, audio y reloj |
| `UI/Widgets/SExploredWristWatch.{h,cpp}` | Slate | Reloj de pulsera: hora y cuatro indicadores en palabras |
| `Tests/BodySpec.cpp` | spec | «Explored.Body», corre en el editor y en `Tools/HostTests` |

Las constantes del cuerpo están reflejadas en `Content/Data/survival_needs.json`
(sección `body`); `Tools/DataCheck` falla si no coinciden con `BodyModel.cpp`.

## Señales corporales

`FBodySignalsModel::Evaluate(State, Context)` devuelve el objetivo sin memoria;
`Smooth` lo persigue con una constante de tiempo por canal (el pulso y el latido
reaccionan en segundos; el color, en decenas). Todas las señales van de 0 a 1 y son
monótonas en su causa (lo comprueba `BodySpec`).

| Señal | Causas | Salida en juego |
|---|---|---|
| `Breathing` | esfuerzo, apnea, calor, poca salud, sangrado | sonido de respiración (`BreathsPerMinute` 12–40) |
| `Heartbeat` | esfuerzo, poca salud, sangrado, fiebre, dolor, apnea, sed | latido en bucle (`HeartRateBpm` 60–160); audible por encima de 0.3 |
| `Shivering` | frío corporal, escalofríos de fiebre | tiritona de cámara (`GetShiverRotation`) |
| `StomachGrowl` | hambre, dieta monótona, intoxicación | probabilidad por minuto real de que suene el estómago |
| `Vignette` + `VignetteTint` | la causa más intensa (frío, calor, sed, hambre, sangrado, dolor, veneno, agotamiento, escorbuto) | borde de pantalla teñido; nunca pasa de 0.75 |
| `Blur` | sed, intoxicación, agotamiento | visión borrosa |
| `HandTremor` | frío, hambre, agotamiento, fiebre, veneno, poca salud | temblor de las manos en primer plano (`GetHandTremorOffset`) |
| `Desaturation` | escorbuto (etapa de visión), poca salud | pérdida de color |
| `BleedingPulse` | cortes abiertos, sangrado | pulso rojo del borde al ritmo del latido |
| `Hallucination` | seta alucinógena (1), falta de sueño extrema (≤ 0.35) | deformación y deriva de color |

## Material de postproceso `M_PP_Body`

Ruta que carga el componente: `/Game/Materials/M_PP_Body` (dominio *Post Process*,
*Blendable Location: Before Tonemapping*). Si no existe, el componente usa los ajustes
de postproceso de la cámara (viñeta, saturación y un leve tinte) como respaldo.

| Parámetro | Tipo | Rango | Uso sugerido |
|---|---|---|---|
| `BodyVignette` | escalar | 0–0.75 | Intensidad del borde: `lerp(SceneColor, Tint, Vignette × máscara radial)` |
| `BodyVignetteTint` | vector (RGB lineal) | 0–1 | Color del borde según la causa dominante |
| `BodyBlur` | escalar | 0–1 | Desenfoque de la escena (muestreo en cruz de `SceneTexture:PostProcessInput0`), más fuerte hacia los bordes |
| `BodyDesaturation` | escalar | 0–1 | `lerp(color, luminancia, Desaturation)` |
| `BodyBleedPulse` | escalar | 0–1 | Amplitud de un pulso rojo en el borde: `BleedPulse × pow(0.5 + 0.5 sin(2π · HeartRateHz · Time), 4)` |
| `BodyHeartRateHz` | escalar | 1–2.67 | Latidos por segundo para sincronizar el pulso |
| `BodyHallucination` | escalar | 0–1 | Deriva de tono (rotación de matiz) y leve ondulación de UV |

Accesibilidad (GDD §15): con «reducir movimiento» conviene escalar `BodyHallucination`
y la tiritona; con «desactivar destellos», `BodyBleedPulse`. Aún no hay ajuste propio.

## Audio

| Sonido | Ruta por defecto | Estado |
|---|---|---|
| Latido (bucle) | `/Game/Generated/Audio/Efectos/sfx_heartbeat_low_loop` | existe en `Tools/Audio` |
| Respiración | `/Game/Generated/Audio/Efectos/sfx_breath_tired` | existe en `Tools/Audio` |
| Estómago | `/Game/Generated/Audio/Efectos/sfx_stomach_growl` | existe en `Tools/Audio` |

El componente los crea como sonidos 2D propios (no pasa por
`UExploredAmbienceSubsystem`, que solo mezcla capas de entorno).

## Reloj de pulsera

Mantener **T** (o la cruceta arriba del mando) levanta la muñeca: se ve la hora
(`HH:MM`) y, si `SetShowWatchNeeds(true)` (por defecto), agua, comida, sueño y calor en
cuatro palabras: *bien*, *regular*, *mal*, *muy mal*. Sin números.

## Entorno que lee el componente

- Temperatura, viento, lluvia, nubes y temporal de `UExploredWeatherSubsystem`.
- Sol: dirección de `ExploredSky::SunDirection` y una traza hacia el Sol (sombra real).
- Refugio: una traza de 4 m hacia arriba (bajo un árbol frondoso también cuenta).
- Agua y oxígeno de `USwimComponent`; actividad por velocidad y estado de nado.
- Caídas: altura desde el punto más alto del salto hasta el aterrizaje; al agua cuentan
  como `ELandingSurface::Water`. El vuelo de depuración no cuenta.
- Pendiente: fuego, sombrero, carga y dormir no tienen aún fuente en el mundo
  (`FireHeat`, `bHasHat`, `CarriedWeightRatio`, actividad `Sleeping`).
