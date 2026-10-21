# Investigación: césped continuo y arena estilizada (2026-10-22)

Encargo de Rodrigo: el césped actual se lee como matas sueltas y la arena como una superficie lisa.
Esta nota resume las técnicas que usan otros juegos y fija la decisión para Baliza Siete (UE 5.6, terreno
`ProceduralMeshComponent`, sin Landscape, objetivo 60 fps a 1080p). Solo recoge lo que dicen las fuentes.

## Césped

### Qué hacen otros juegos

| Juego o proyecto | Técnica | Cifras |
|---|---|---|
| Ghost of Tsushima (Sucker Punch, GDC) | Hojas generadas en la GPU, cada una una Bézier cúbica de 4 puntos; sin *grass cards* por el overdraw; clumps por Voronoi (altura, dirección, color y curvatura comunes) | ~83 000 hojas en pantalla, ~2,5 ms por frame; 15 vértices por hoja de cerca y 7 de lejos |
| Ghost of Tsushima, LOD | Tiles lejanos con el doble de tamaño y las mismas hojas; la hierba cercana descarta progresivamente 3 de cada 4 hojas al acercarse al límite, y la forma se mezcla con la de baja resolución. No hay pop | Hierba corta: 7 vértices, con los otros 8 se hace una segunda hoja |
| Zelda BotW (análisis de aficionados, sin confirmar) | Hojas de geometría opaca (un triángulo cada una), viento moviendo el vértice superior, degradado de color base→punta por color de vértice, hierba generada solo alrededor del jugador | — |
| PixelAnt (UE + Niagara) | Compute shader en Niagara: rejilla con jitter por hash de posición (no por ID de partícula), Bézier de 3 puntos, altura del terreno por RVT, interacción con render target | Sin cifras; 3 días de desarrollo |
| PCG + Bézier en UE5 (notas de estudio) | Hojas con Bézier sembradas por PCG | 75 puntos/m²: 60 fps sin sombras y 50 con sombras (GTX 1660 Super, en el editor). El culling en el material no mejoró nada; hace falta a nivel de malla |
| Nanite Foliage | Vóxeles de Nanite sin pop ni fundidos | Según las fuentes es de UE 5.7, no de 5.6 |

### Conclusiones para Explored

1. El aspecto de alfombra viene de la **densidad de hojas** (decenas por m²) con geometría opaca, no de una técnica
   distinta. El diseño actual (~15 hojas/m²: 2 matas/m² de 5-9 hojas) queda muy por debajo.
2. Nuestra opción (instancias de parches de 12-18 hojas, 5-6 por m²: ~70-100 hojas/m²) cae en el mismo orden que
   la prueba de PCG (75/m²) y no exige un sistema nuevo. Descartado para hoy: Niagara/compute (requiere heightmap
   por RVT, que no encaja con una malla procedural, y no hay cifras de rendimiento) y Nanite Foliage (5.7).
3. Sin pop: el recorte por distancia es un **adelgazamiento continuo**, no un corte. Cada hoja decide en el material si
   se queda según una fracción `f(d)` que baja con la distancia (rango por instancia con `PerInstanceRandom`) y las
   que sobreviven se agrandan para compensar; las descartadas se encogen a 0 antes de que el componente las quite.
   `f(d)` es la única fuente de verdad: la comparte el modelo de siembra (`FGrassFieldModel`).
4. Coste: sin sombras en el césped (en la prueba de PCG costaron 10 fps), sin Nanite ni alfa en hojas, sin LODs
   de malla (una LOD por malla), corte de distancia a nivel de instancia y desactivar el WPO de viento lejos.
5. Aspecto: gradiente base→punta, variación por parche (color y altura por instancia, a la manera de los clumps de
   Voronoi), normales de la hoja redondeadas hacia arriba y el color del terreno bajo la hierba igualado al verde
   de la base para que no se vea el suelo entre hojas ni en la transición.
6. Presupuesto: ≤ ~1,5 M de triángulos de césped a la vista, ≤ 1 ms por frame de siembra, bench ≥ 60 fps de media
   y ≥ 45 de mínimo.

## Arena

Referencia: el shader de arena de Journey (Alan Zucconi, 2019). Componentes y qué aportan:

- **Normal con bump** de granos pequeños sobre una duna lisa.
- **Ripples** (marcas de viento) cuya forma depende de la pendiente y la orientación de la duna: mezcla de texturas tipo
  triplanar según orientación y pendiente.
- **Glitter**: microfacetas con dirección aleatoria por grano; `G = normalize(2·ruido − 1)`, `R = reflect(L, G)`,
  brilla solo si `R·V` es pequeño (umbral); «raro y muy brillante», sumado aparte del albedo. Con luz móvil se usa
  `reflect` para que sea coherente entre frames; en algunos juegos (Astroneer, Slime Rancher) solo de noche.
- **Rim lighting** (Fresnel) para separar dunas con una paleta limitada.

Propuesta para `M_TerrainToon` (sin texturas fotográficas, paleta limitada):

1. **Ripples estilizados** en espacio de mundo: bandas finas perpendiculares a la dirección del viento, moduladas por
   pendiente y por la máscara `T_TerrainStrokes`, alternando dos tonos de arena.
2. **Glitter** escaso: celdas de ruido de pocos cm, umbral de `R·V` y suma emisiva pequeña; más fuerte de cara al sol.
3. **Orilla húmeda** por altura sobre el nivel del mar (no por distancia a superficie: `DistanceToNearestSurface` no ve el
   terreno): oscurecimiento, borde claro y banda de espuma.
4. **Grano** de alta frecuencia que se desvanece con la distancia.
5. Revisar las manchas blancas planas de la playa (aparecen en los bordes de `f6-relieve-cala.png`): parecen
   sobreexposición del tono de arena clara y no tienen textura.

## Fuentes

- Procedural Grass in Ghost of Tsushima (GDC Vault): <https://gdcvault.com/play/1027033/Advanced-Graphics-Summit-Procedural-Grass>
- Resumen de la técnica: <https://tigerabrodi.blog/what-we-can-learn-from-grass-in-ghost-of-tsushima-renders>
- Niagara GPU grass en UE (PixelAnt Games): <https://pixelantgames.com/blog/rendering-millions-of-grass-blades-using-ue-and-niagara/>
- Césped procedural con PCG en UE5 (notas de estudio): <https://immersivenick.wordpress.com/2025/10/20/making-a-procedural-grass-material-with-unreal-engine-5-and-the-pcg-framework-study-notes/>
- Nanite Foliage (guía): <https://www.strayspark.studio/blog/nanite-foliage-ue5-complete-guide>
- Journey sand shader, parte 1: <https://www.alanzucconi.com/2019/10/08/journey-sand-shader-1/>
- Journey sand shader, glitter (parte 5): <https://www.alanzucconi.com/2019/10/08/journey-sand-shader-5/>
- Hilo sobre el césped de BotW (aficionados): <https://forums.unrealengine.com/t/botw-grass-style/437647>
