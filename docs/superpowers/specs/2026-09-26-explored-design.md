# Explored — Documento de diseño

Fecha: 2026-09-26 · Motor: Unreal Engine 5.6 · Plataforma: Windows (Steam como objetivo)

## 1. Visión

Juego de supervivencia y exploración en primera persona. El avión en el que viajaba el
jugador cae sobre un archipiélago tropical generado proceduralmente. Hay que sobrevivir
(comida, agua, refugio, fuego, clima, noche), explorar las islas y reunir los restos del
avión necesarios para enviar una señal de rescate. Al completarla, el jugador decide
marcharse (créditos) o quedarse y seguir en modo libre.

Pilares:

1. **Exploración con recompensa**: cada isla tiene lugares singulares que merece la pena
   encontrar.
2. **Supervivencia diegética**: sin cuadrícula de inventario. Lo que llevas está en tus
   manos o en la mochila, y se ve.
3. **Mundo vivo**: ciclo día/noche, clima, fauna con comportamiento natural.
4. **Todo generado por código**: mallas, texturas, audio y música se producen mediante
   scripts versionados del repositorio. Ningún asset externo.

## 2. Dirección de arte

- Low-poly facetado (flat shading), color por vértice y paletas por bioma; sin texturas
  fotográficas.
- Iluminación Lumen, niebla volumétrica, SkyAtmosphere, postproceso estilizado (color
  grading cálido, bloom suave, viñeta leve).
- Agua: malla de océano propia con desplazamiento de vértices en el material, espuma en la
  orilla por profundidad y transparencia en aguas someras.

## 3. Arquitectura técnica

- **C++ como núcleo** (módulo `Explored`). Los Blueprints solo se usan como datos
  generados por scripts de Python del editor; ninguna lógica vive en Blueprint.
- **UI** en C++ (`UUserWidget` construidos por código y Slate donde convenga).
- **Subsistemas** (`UWorldSubsystem` / `UGameInstanceSubsystem`) con una responsabilidad
  cada uno.

| Módulo / subsistema | Responsabilidad |
|---|---|
| `WorldGen` | Semilla → mapa de islas, alturas, biomas, ríos; genera chunks de malla facetada |
| `Scatter` | Colocación determinista de vegetación, rocas y recursos en HISM, por reglas de bioma |
| `TimeOfDay` / `Weather` | Rotación solar, luna, estrellas, lluvia, tormentas, viento |
| `Interaction` | Trazas desde la cámara, foco, acciones contextuales |
| `Hands` / `Backpack` | Mano izquierda/derecha, mochila con volumen y peso |
| `Crafting` | Recetas por combinación de lo que sostienes en ambas manos |
| `Building` | Colocación con fantasma de refugio, fogata, recolector de agua, balsa |
| `Survival` | Hambre, sed, energía, salud, temperatura corporal |
| `Fauna` | Máquinas de estados por especie y animación procedural |
| `Progression` | Piezas del avión, señal de rescate, final, descubrimientos |
| `Audio` | Ambiente por bioma y hora, música adaptativa, efectos |
| `Save` | Semilla + deltas del mundo + estado del jugador |
| `Settings` | Gráficos, audio, controles, accesibilidad, idioma |

### Pipeline de assets (100 % por código)

```
Tools/Blender/*.py  --(blender -b -P)-->  Art/Export/*.fbx
Tools/Audio/*.py    --(uv run)-------->  Art/Export/Audio/*.wav
Tools/Unreal/import_*.py --(UnrealEditor-Cmd -run=pythonscript)--> Content/
```

- Mallas low-poly generadas con `bpy` en modo headless: reproducibles y deterministas.
- El MCP de Blender se usa para inspección visual e iteración, no como fuente de verdad.
- Audio: síntesis con numpy (olas, viento, lluvia, pasos por superficie, fuego, animales,
  UI). Música: composición procedural (pads, marimba, percusión suave) con capas
  día/noche/tensión/descubrimiento.

## 4. Mundo

- Archipiélago de 5–7 islas en unos 4×4 km, con la semilla elegida en «Nueva partida».
- Biomas: playa, selva densa, manglar, pradera alta, pico volcánico, arrecife somero.
- Terreno: ruido fractal + máscara radial por isla + erosión simplificada; ríos desde el
  pico hasta el mar; chunks de malla facetada con LOD y colisión.
- Puntos de interés garantizados por la generación: restos del avión (4 zonas),
  campamento abandonado, cascada con cueva, faro en ruinas, poza de marea, mirador, bahía
  de tortugas.

## 5. Jugador y juego

- Controlador en primera persona: andar, correr, agacharse, nadar (limitado por energía),
  saltar y trepar salientes bajos.
- **Manos**: cada mano sostiene un objeto; con dos manos se llevan objetos grandes
  (troncos). **Mochila**: se encuentra en los restos del avión; guardar y sacar tiene una
  animación breve; capacidad limitada por volumen y peso.
- **Fabricación**: con un objeto en cada mano se combinan (piedra + palo → hacha tosca,
  liana + palos → lanza, etc.). Las recetas se descubren y quedan anotadas en el diario.
- **Construcción**: refugio de hojas, fogata, secadero, recolector de lluvia, balsa (para
  llegar a otras islas).
- **Supervivencia**: comer (fruta, cocos, pescado, carne cocinada), beber (coco, lluvia,
  agua hervida), dormir en refugio para saltar la noche, frío nocturno y lluvia que bajan
  la temperatura.
- **Diario** (objeto físico): mapa dibujado de lo explorado, descubrimientos y recetas.

## 6. Fauna

Cangrejos, jabalíes, aves (bandadas), peces, monos, tortugas, tiburón en aguas profundas.
Cada especie: máquina de estados en C++ (vagar, pastar, huir, alerta, atacar si aplica),
animación procedural (patas por ciclo senoidal, balanceo del cuerpo, aleteo) y ciclo
diario de actividad.

## 7. Progresión y final

- Piezas: radio, batería, antena y bengala en restos repartidos por 3–4 islas.
- Construir la baliza en el pico más alto → activarla de noche → rescate al amanecer
  siguiente.
- Final: secuencia de helicóptero; elección «Marcharse» (créditos) o «Quedarse» (modo
  libre, partida intacta).

## 8. Frontend

- Menú principal con la isla de fondo en tiempo real. Opciones: Nueva partida (semilla),
  Continuar, Ajustes, Créditos, Salir.
- Pausa, pantalla de carga con la generación en curso, HUD mínimo contextual (sin barras
  permanentes; el estado del jugador se transmite con efectos de pantalla y sonidos).
- Ajustes: resolución, modo de ventana, calidad (escalabilidad), límite de FPS, VSync,
  DLSS/TSR, FOV, sensibilidad, invertir Y, remapeo de teclas (Enhanced Input user
  settings), volúmenes (maestro, música, efectos, ambiente), subtítulos, idioma ES/EN.
- Guardado automático al dormir y manual en la pausa.

## 9. Calidad y pruebas

- Automation Tests de UE (C++): determinismo de la generación por semilla, validez de
  biomas y PdI, manos/mochila, recetas, supervivencia, guardado y carga.
- Verificación visual: capturas automáticas (`HighResShot`) de vistas fijas, revisadas
  en cada hito.
- Rendimiento objetivo: 60 FPS a 1080p en calidad Alta sobre una RTX 4060 Laptop.

## 10. Hitos

| Hito | Contenido |
|---|---|
| M0 | Proyecto UE 5.6 C++, build por CLI, pipeline Blender/audio/importación funcionando |
| M1 | Archipiélago procedural, océano, cielo, día/noche, vegetación |
| M2 | Jugador, manos, mochila, interacción, recolección |
| M3 | Supervivencia, fabricación, construcción, fuego, clima |
| M4 | Fauna |
| M5 | Audio y música adaptativa |
| M6 | Menús, ajustes, guardado, localización |
| M7 | Progresión, puntos de interés, final |
| M8 | Pulido, rendimiento, empaquetado Win64 |

## 11. Fuera de alcance

- Multijugador.
- Publicación en Steam: el alta de Steamworks (cuenta y tasa de 100 USD) depende del
  titular; el proyecto entrega el build empaquetado y listo para subir.
