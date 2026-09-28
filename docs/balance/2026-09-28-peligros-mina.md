# Peligros de la mina: vigas, luz y aire (GDD v2 §3.4, biblia 02 §2.4)

Fecha: 2026-09-28. Fuentes: `Content/Data/mining.json` (`hazards`, `places`),
`building_pieces.json` (`viga_apoyo`), `templates.json` (cuerda y cordel), rendimientos de
`FHarvestModel` y la línea de tiempo de
[`2026-09-28-landing-primera-hora.md`](2026-09-28-landing-primera-hora.md). No hay PIE ni
modelo C++ de galerías: son cuentas sobre los números de la biblia.

Escala: 1 h de juego = 100 s reales.

## 1. La cueva de Landing se derrumba tal como está en la línea de tiempo

La primera hora cava una galería de **1 × 2 × 4 m**. Con la regla de la biblia (luz sin
apoyo > 3 m → colapso a los 8 s), **4 m de largo se hunden**. Hay dos salidas:

| Opción | Coste | Tiempo real extra |
|---|---|---|
| Galería de 3 m (6 m³, 36 golpes de pala) | Nada | −10 s |
| Galería de 4 m + 1 `viga_apoyo` en el centro | 2 `tronco_pequeno` + 1 `cuerda` (1 liana + cordel de fibra de coco y corteza), hacha | ≈ 25 s de obra (15 min de juego) + 2 arbustos más (≈ 4 s) |

**Recomendación: mantener los 4 m y la viga.** Los troncos sobran en la primera hora
(4 al talar 2 palmeras y 2 sotobosques, y ninguna receta del minuto 0–40 los gasta) y el
hacha ya está desde el minuto 13. La cuerda sale de una liana y un cordel, pero las lianas
ya van justas (pala, hacha y refugio gastan 3, y 4 arbustos dan unas 2). Dos arbustos más
lo arreglan. A cambio, el jugador ve el crujido y el polvo antes del colapso, y el
logro oculto «Viga a tiempo» (biblia 07) cae de forma natural en la primera hora.

La oscuridad (6 m) y el aire viciado (15 m) **no aparecen** en Landing: la cueva mide 4 m y
entra la luz del día. Así Landing enseña un solo peligro, el derrumbe, y es lo que pide la
porción vertical (GDD v2 §6.1).

## 2. Vigas por galería

Con una viga que apoya 1,5 m a cada lado, cada tramo sin apoyo mide como mucho 3 m:
**vigas = ⌈L / 3⌉ − 1** para una galería recta de largo L (y una más en cada cruce de más de
3 m de ancho).

| Galería | Material | Golpes (herramienta mínima) | Vigas | Troncos | Cuerdas |
|---|---|---|---|---|---|
| 1 × 2 × 4 m, Landing | tierra | 48 (pala) | 1 | 2 | 1 |
| 1 × 2 × 5 m, veta de cobre de Esmeralda | caliza | 120 (pico de piedra) | 1 | 2 | 1 |
| 1 × 2 × 10 m, basalto del Humo | basalto | 360 (pico tallado) | 3 | 6 | 3 |
| 1 × 2 × 20 m, hacia el cristal | basalto | 720 (pico tallado) | 6 | 12 | 6 |

En basalto, cada 3 m de galería cuesta ~108 golpes (≈ 97 s) más una viga (≈ 25 s de obra).
La madera pasa a ser **el 20 % del tiempo** de una galería larga. Esa presión es buena:
hace que la mina dependa de la tala (GDD v2 §2.1, pilar 2), sin que la domine.

## 3. Luz: la antorcha no tiene tiempo de quema

`antorcha` tiene `maxDurability: 20`, pero ningún dato ni ningún modelo dice **a qué
ritmo** se consume la durabilidad de una luz. Sin eso no se puede medir el «turno de mina
con antorchas de sobra» del GDD v2 §2.2. Propuesta para validar (ya en
`mining.json/hazards/oscuridad/lightBurn`, marcada `propuesta`):

- **1 punto por cada 6 min de juego** → una antorcha dura 2 h de juego (≈ 3 min 20 s
  reales).
- Una galería de 20 m en basalto (720 golpes ≈ 11 min reales, más vigas y acarreo ≈ 16 min)
  pide **5 antorchas**. Cada antorcha es una rama seca más algo que arda. Es una presión
  parecida a la de las vigas.
- La `lampara_aceite` (queda en `lightItemsPendientes`, falta el objeto) debería durar
  el triple con `aceite_pescado`. Así el aceite, que hoy solo sale del despiece de pescado
  (`fish.json`), tiene un segundo uso.

## 4. Aire viciado: en qué minutos se mide

La biblia da «4 %/min tras 2 minutos de gracia» sin decir si son minutos de juego o reales.
Con minutos de juego, pasar de 100 % a 20 % llevaría 20 min de juego, **33 s reales**, que
es casi un temporizador de muerte súbita, justo lo que el GDD v2 §3.4 descarta. Con
minutos reales son 2 min de gracia y 20 min hasta el mareo: presión suave, como el resto
del cuerpo. `mining.json` usa **minutos reales** y lo marca como interpretación.

Solo afecta a bolsas cerradas a más de 15 m: en el acceso anticipado, las galerías largas
del Humo y las cavernas de cristal (25 m o más). Una chimenea al exterior cuesta 1 m² de
sección hasta la superficie. En basalto, a 25 m, son 25 m³ = **450 golpes**, así que lo
normal será salir y no airear. Es coherente con «empuja a salir, nunca mata».

## 5. Qué queda para el C++

- Modelo de galerías con luz sin apoyo, `viga_apoyo` como apoyo y el aviso de 2 s (biblia
  02 §2.4). La pieza ya está en los datos.
- Tasa de quema de las luces (punto 3) y el objeto `lampara_aceite`.
- Indicador de aire con los números de `mining.json/hazards/aire_viciado`, leído en el
  cuerpo.
- Lugares de `mining.json/places` como `FCaveDesc` por semilla. La cueva de Landing no es
  un carving: la cava el jugador.
