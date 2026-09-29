# Integración del 2026-09-29

## Ejecución 01:00 UTC

Base: `origin/main` en `20ed387` al empezar. Solo #121 es nueva. Las demás PR abiertas no
tienen commits desde su última revisión: #117 (`b819a28`) se revisó a las 23:22 y #118
(`f108840`) a las 23:41.

| PR | Rama | Decisión | Por qué |
|---|---|---|---|
| #121 | `nube/packs-2026-09-29` | **Fusionada** (`5fb68df`) | Lote 11 de packs: clavos y yunque (KayKit RPG Tools Bits, ya declarado en `Tools/Packs/packs.json`). Solo toca el catálogo y la hoja de contacto. Los `gameId` existen (`items.json` y `building_pieces.json`) y la nota del yunque cuadra con su coste (4 basalto y 1 lingote de hierro). Le añadí la fila del lote 11 en `Tools/Packs/README.md`, que faltaba (`f833e26`). |
| #70, #72, #75, #81, #82, #84–#87, #90, #96, #111, #114, #117, #118 | — | Sin cambios | Siguen con `necesita-unreal` y sin commits nuevos. |

### Comprobaciones

| PR | DataCheck `--strict` + pytest | Otros |
|---|---|---|
| #121 (al día con `main`) | 0 errores, 0 avisos · 550 passed | Packs: 64 passed, 1 skipped · ruff sin avisos |

La PR no tiene checks de CI en GitHub (no toca rutas que los disparen), así que las
comprobaciones son las locales.

### 00-TODO.md

- Ninguna casilla nueva en `[x]`. #121 se anota en la nota «en parte» de la casilla de arte
  del GDD §7.1.
