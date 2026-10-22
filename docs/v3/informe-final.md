# Baliza Siete: informe de entrega (2026-10-22)

Rama `reset/v2`, etiqueta local `v3-baliza-siete` (sin push). Detalle y cifras completas en
[`estado-f7.md`](estado-f7.md) y [`pendiente-motor.md`](pendiente-motor.md).

## El juego y el paquete

Supervivencia corta en primera persona (≈ 52 min medidos) en la Isla de Sal: Iria Valdés tiene que
encender el faro (lente del carguero, aceite de pescado y mecha) antes de que el correo «Mistral»
pase la noche del día 3. Sin combate ni muerte; la penalización es el desmayo. Teclado y ratón;
español con inglés en Ajustes → Idioma.

| Paquete | Ruta | Tamaño | Commit |
|---|---|---|---|
| Shipping (para jugar) | `Saved/Package/Windows/Explored.exe` | 604 MB | `HEAD` (rehecho en el cierre) |
| Development (bot y bench) | `Saved/Package-Development/Windows/Explored.exe` | 1.193 MB (385 MB de `.pdb`) | binarios de `6ac833f9` |

`Saved/Package-Shipping.zip` es el Shipping anterior (cocinado 5 min antes del commit que versiona
`M_TerrainToon`); el bueno es el de `Saved/Package/Windows`.

## Qué se hizo esta noche

72 commits desde las 20:00 del 21 (328 ficheros, +18.567 / −5.740 líneas), entre ellos:

- **Césped continuo y arena (F8):** alfombra de parches sin LOD que se aclara de 25 a 100 m sin saltos,
  sin sombras; arena con ondas, grano y brillos; espuma pintada por el terreno según su altura real.
- **Diseño P1-P10:** textos de objetivos, gradación, terreno, haz del faro, llegada del Mistral,
  créditos, nubes, bocina, mezcla de efectos del jugador y menús con foco y confirmación de salida.
- **Revisión:** 19 hallazgos confirmados en dos rondas, todos corregidos (tabla de abajo).
- **Huecos:** pistas contextuales, créditos y licencias (EULA de Unreal, MIT del soundfont, CC BY),
  icono y versión 1.0.0, fuera el barco y los datos de v1, guardado endurecido, commandlet de estilo.
- **Motor:** bot ×3 en el editor y sobre el paquete, bench en editor y paquete, dos tirones
  eliminados (crecimiento de ISM del césped y recolección de basura), paquetes Shipping y Development.

## Evidencias

| Prueba | Resultado |
|---|---|
| Bot competente (paquete Dev, `-Clock 6`) | VERDE: 52,4 min a 1×, 0 desmayos, 8/8 páginas, 140 pasos `ok` |
| Bot sin_paginas (paquete Dev) | VERDE: 52,4 min, 0 desmayos, 1/8 páginas, epílogo parcial, 127 pasos |
| Bot lenta (paquete Dev) | VERDE: 52,4 min, 0 desmayos, 8/8 páginas, 188 pasos |
| En las tres | 10/10 etapas, 10/10 guardar/cargar, 9/9 softlocks con salida, 0 `Error` en el log, créditos → menú |
| Bench paquete (1080p, media / mínimo fps) | cala 208,6 / 114,3 · palmar 163,3 / 98,4 · faro 196,5 / 106,6 · galería 160,2 / 103,1 · ciclón 230,1 / 130,7 · **resumen 191,7 / 98,4 OK** (objetivo ≥ 60 / ≥ 45) |
| Triángulos por vista | RHI en el bench: 433.000-454.000 de media con el césped; host `GrassRealData` (peor dirección): césped 1,02-1,18 M (tope 1,5 M) |
| Recolección de basura en el paquete | 0,34-0,96 ms (16,4 ms en el editor) |
| Build editor y Shipping | `Succeeded` (cierre, 2026-10-22) |
| Host tests (Windows, clang) | 838 casos, 0 fallos; ASan + UBSan y `-ffast-math` en WSL: 836/836 (antes del cierre) |
| Cobertura C++ de modelos | 91,8 % de líneas; modelos v3 92-100 % |
| Automation tests del editor | 884/885 verdes, 0 fallos (1 con avisos de `GConfig`) |
| DataCheck `--strict` / `l10n export --check` | 0 errores / 379 textos, 0 sin inglés, 0 errores |
| Python (ruff, basedpyright, pytest) | 0 errores; cobertura Island 99 %, Textures 100 %, Audio 94 %, DataCheck 95 %, Localization 96 %, Style 99 %, Blender 86 %, Packs 91 % |
| Commandlet de estilo | 56 mallas, 0 fallos; 2 texturas en `/Game`, 0 fuera de la lista blanca |
| Arranque Shipping | Menú principal a los 40 s, 0 volcados (`Saved/Shots/pkg-shipping-final-menu.png`) |
| Crítica de arte (ronda 2) | 7/10, sin bloqueantes |

Capturas clave en `docs/v3/capturas/`: `f8-cesped-cerca`, `f8-cesped-palmar`, `f8-cesped-paseo-1…8`
(sin saltos), `f8-arena-cala`, `f8-arena-orilla`, `f5-hud`, `f8-menu-pausa`, `f6-final-haz`,
`f6-final-mistral`, `f6-final-epilogo`, `f6-final-creditos`.

## Decisiones de diseño tomadas por mí

- **Aceptadas (P1-P10):** las de más impacto en los primeros 15 min y en el final, con riesgo bajo.
- **Rechazadas:** balances sin playtest (pesca, barómetro, energía, sed), sistemas nuevos (cuenta
  atrás, HUD de ingredientes, autoasignar ranuras, pausar el reloj al leer), arte que necesita tu ojo
  (refugio, rocas, realce en `M_Toon`) y audio que hay que escuchar (renormalizar música, −4 dB de
  ambiente, regenerar SFX). Lista completa en `estado-f7.md`, «Pasada de diseño».
- La duración la marca el reloj (el Mistral pasa el D3 a las 20:05): las tres rutas duran lo mismo.
- «Salir del juego» confirma y **no** autoguarda: un guardado silencioso la noche del D3 podría dejar
  al jugador peor que su último guardado manual.
- Césped sin sombras y sin recorte de densidad: el bench va con 50 fps de margen sobre el mínimo.
- Luna sin luz hasta que el sol se apaga del todo, con fundido corto (sin Lumen, evita dos VSM).
- Bocina a 0,95 con la música agachada 8 s (a 1,6 recortaba).

## Revisión final

| Ronda | Hallazgos confirmados | Estado |
|---|---|---|
| 1 | 4 HIGH (leña de deriva enterrada, nubes que recreaban el proxy, sin `.locres`, README sin aviso de audio local) y 10 MEDIUM | Todos corregidos, con spec donde había lógica |
| 2 | 5 MEDIUM (sombras de la luna, orden de `package.ps1`, tres tests de regresión que faltaban) | Todos corregidos; los 7 casos nuevos fallan con el código anterior |
| Adversarial (`/codex:adversarial-review`) | No ejecutada: `codex exec` no lanza procesos en el sandbox de Windows | **Pendiente** |

## Límites conocidos (sin ojo ni oído humano)

- **Mezcla de audio:** nada se ha escuchado. Bocina, lente, transiciones de música y lluvia al entrar
  en el faro están medidos por nivel, no a oído (paso 9 de `pendiente-motor.md`).
- **Tacto:** velocidad, cámara, balsa y menús con teclado real no se han probado a mano; el bot juega
  por guion. La balsa no responde al mando (entrega declarada: teclado y ratón).
- **Césped y arena:** la crítica ve el verde de la colina plano a media distancia, calvas junto a los
  caminos, una franja de arena mojada entre el agua y la espuma y ondas visibles a contraluz. ΔE frente
  a la paleta: hierba 14,5 y cénit 9,8 (objetivo < 8).
- **Sombras:** la sombra del terreno está quitada (dibujaba triángulos con el sol bajo); no se ha
  revisado de noche ni en interiores más allá de las capturas.
- **Rocas** con techo verde junto al faro (siguen igual desde f6).
- El auditor del bot marca 276-283 instancias de decorado «flotando o enterradas»; no es criterio y
  no se ha revisado una por una.
- Tras un teletransporte (cargar partida) el primer fotograma tarda 6-8 ms.
- **Cortafuegos:** en el primer arranque del Shipping Windows pregunta si permite el acceso a la red.
  El juego no usa red; no se ha averiguado qué abre el socket.

## Criterios de F7

1 rendimiento OK · 2 paquete y bot ×3 OK · 5 tamaño OK · 3 revisión adversarial **no cumplido** ·
4 cobertura y sanitizers OK en local, **CI sin ejecutar** (rama sin empujar). Por eso la tarjeta de F7
sigue en curso en el kanban; la del césped y la arena pasa a hecha.

## Decisiones que son tuyas

1. **Jugar una partida entera** con sonido y validar mezcla, tacto y arte (pasos 7 y 9).
2. **Empujar `reset/v2`** para que corra CI (`host-tests.yml` con sanitizers) y **fusionar con `main`**.
3. **Revisión adversarial** desde tu sesión (`/codex:adversarial-review`).
4. **Aprobar por escrito** las láminas de F2 y K1-K3 (`docs/v3/laminas`).
5. **`Content/Audio` (411 MB) y `Content/Generated` (274 MB) no están en git:** el paquete solo se
   puede generar en este PC. Decide si van por LFS o a una copia externa.
6. **Publicar** (itch.io u otro) y con qué ficha; el aviso del cortafuegos conviene resolverlo antes.
7. Borrar `Saved/Package-Shipping.zip` si ya no lo quieres.
