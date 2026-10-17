# Explored v1 «El Limón»: alcance cerrado (2026-10-17)

Versión jugable completa con principio, bucle y final. Todo lo que no está aquí queda
fuera de la v1 (sigue en `docs/roadmap.md` y en las PR abiertas, sin fusionar).

## Dentro

- **Frontend:** menú principal sobre el archipiélago, selección de modo, ajustes, cargar,
  guardar (ranuras), pausa, logros, créditos con la dedicatoria.
- **Mundo:** el archipiélago horneado tal como está en `main` (siete islas, océano, cielo,
  ciclo de día y noche de 40 min, clima).
- **Sistemas en juego:** supervivencia (hambre, sed, energía, sueño, temperatura), recolección,
  inventario, fuego y cocina, mapa dibujado a mano (tecla M), ruinas y petroglifos con sus
  mallas, pesca básica.
- **Objetivo y final:** rescatar las cuatro piezas del Albatros (fuselaje y ala en la isla de
  llegada, cola en la laguna, motor en Esmeralda), construir el «Limón» en la grada de la
  playa y zarpar de noche (20:00–05:00). El final muestra la dedicatoria y vuelve al menú.
  El HUD lleva el objetivo arriba a la derecha.

## Fuera (congelado)

- Cooperativo y red (H0–H3), minería y terraformado en tiempo de ejecución (#117, #118, #114),
  barcos por piezas (#90), granja (#81), escalada (#85), museo ampliado, combate, aldeas y piratas.
- Rehorneado del terreno: no se tocan las PR de terreno (#87, #114, #118) en la v1.

## Verificación

- `Tools/test.ps1 -Filter Explored.Narrative` (etapas del viaje).
- `Tools/shots.ps1 -Set voyage`: pecios, grada, Limón construido y pantalla del final.
- `Tools/shots.ps1 -Set menu`: menú, ajustes, pausa y mapa con la interfaz en la captura.
