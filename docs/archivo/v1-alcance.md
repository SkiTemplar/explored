> Obsoleto: sustituido por docs/v3/.

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
  llegada, cola en la laguna y motor tierra adentro de la misma isla), construir el «Limón»
  en la grada de la playa y zarpar de noche (20:00–05:00; la grada ofrece «Esperar a la
  noche»). El final muestra la dedicatoria y vuelve al menú. El HUD lleva el objetivo arriba
  a la derecha con la distancia al pecio más cercano, y cada pecio rescatado queda marcado
  en el mapa.

## Fuera (congelado)

- Cooperativo y red (H0–H3), minería y terraformado en tiempo de ejecución (#117, #118, #114),
  barcos por piezas (#90), granja (#81), escalada (#85), museo ampliado, combate, aldeas y piratas.
- Rehorneado del terreno: no se tocan las PR de terreno (#87, #114, #118) en la v1.
- Viajar entre islas: sin barcos jugables, el juego transcurre en la isla de llegada; el resto
  del archipiélago se ve pero no forma parte del objetivo.
- Pendiente de pulido tras la v1: terreno de la isla del manglar, prueba con mando y en otro
  PC, rendimiento de la vista aérea.

## Arte de la v1

- Kit Limón (barco terminado y armazón en grada) y Albatros rehecho, por script
  (`Tools/Blender/props/limon.py`, `albatros.py`; validación con `check_props.py`).
- Vegetación low poly de Quaternius/Poly Pizza (PR #111) conectada al horneado
  (`WorldGenCommandlet::ApplyLowPolySet`), importada con `Tools/Unreal/import_lowpoly_native.py`
  y con colores planos por slot (`Tools/Unreal/fix_lowpoly_materials.py`).
- Ruinas con las mallas de `RuinasTallas` y `BrujulaEstelar`.

## Rehacer el mundo y el paquete

```powershell
powershell -File Tools/unreal_python.ps1 -Script Tools/Unreal/import_lowpoly_native.py
powershell -File Tools/unreal_python.ps1 -Script Tools/Unreal/fix_lowpoly_materials.py
powershell -File Tools/worldgen.ps1 -Mode map; powershell -File Tools/worldgen.ps1 -Mode hlod
powershell -File Tools/package.ps1   # Saved/Package/Windows/Explored.exe
```

## Verificación

- `Tools/test.ps1 -Filter Explored.Narrative` (etapas del viaje).
- `Tools/shots.ps1 -Set voyage`: interactúa con cada pecio por el camino del jugador (el log
  debe decir «foco ExploredSalvageSite, interacción hecha» 4 veces), grada, Limón, salida y final.
- `Tools/test.ps1`: 1460/1462 en verde el 2026-10-17 (0 fallos).
- `Tools/shots.ps1 -Set menu`: menú, ajustes, pausa y mapa con la interfaz en la captura.
