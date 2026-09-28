# Integración — 2026-09-28

Rutina de integración en la nube (sin Unreal). PR abiertas procesadas de la más antigua a
la más nueva, cada una rebasada sobre `origin/main` (`374d835`) antes de comprobarla.

## Ejecución 01:00 UTC

| PR | Rama | Decisión | Por qué |
|---|---|---|---|
| #44 | `nocturno/revision-2026-09-27c` | No fusionada: `necesita-unreal` | Toca `WorldGenCommandlet.cpp` (editor) y `M_Ocean` vía `build_materials.py`. Todo lo que se puede comprobar aquí está en verde. |
| #45 | `nube/packs-2026-09-27` | **Fusionada** (`9a51c41`) | Solo datos JSON, `Tools/Packs`, `Tools/DataCheck` y una hoja de contacto. |
| #46 | `nube/mundo-2026-09-27` | Cambios pedidos | `FSandModel` puro y con los tests en verde, pero contradice biblia 02 §5.2–5.3 y 08 §2.6 (ver abajo). |
| #48 | `nube/packs-2026-09-28` | **Fusionada** (`45c9f37`) | Incluye #45 más los lotes 2 y 3 y los descartes del kit de construcción. Se fusionó `main` en la rama (sin conflictos) para que GitHub la aceptara. |
| #49 | `nocturno/revision-2026-09-28` | No fusionada: `necesita-unreal` | Toca `M_Ocean` vía `build_materials.py`. El audio de clavar está en verde. |

### Comprobaciones

| PR | HostTests | HostTests + ASan/UBSan | DataCheck `--strict` + pytest | Otros |
|---|---|---|---|---|
| #44 | 653 / 0 fallos | 653 / 0 fallos | 0 errores · 108 passed | Audio: 55 passed (6 ficheros) · Textures: 290 passed |
| #45 | — | — | 0 errores · 115 passed | Packs: 7 passed |
| #46 | 682 / 0 fallos | 682 / 0 fallos | 0 errores · 108 passed | — |
| #48 | — | — | 0 errores · 119 passed | Packs: 7 passed |
| #49 | — | — | 0 errores · 108 passed | Audio: 53 passed (6 ficheros) · Textures: 290 passed |

Audio: se lanzaron `test_garden_cartography`, `test_loudness`, `test_basic_properties`,
`test_determinism`, `test_catalog_contents` y `test_ui_construction`, que cubren lo que
cambian las dos PR. La pasada completa tarda demasiado para esta rutina.
`Tools/Textures` no tiene `pyproject.toml`: se ejecutó con
`uv run --with pytest --with numpy --with pillow python -m pytest`.

### #46: contradicciones con la biblia

- **Oleaje:** la biblia 02 §5.2 rellena un 20 % (35 % en marea viva) por medio ciclo de
  marea, hacia la altura original. La PR difunde un 8 % de la diferencia cada 100 ms
  entre columnas vecinas: un hoyo se cierra en segundos, no en ciclos de marea.
- **Anclaje:** según 02 §5.3, nada se mueve a menos de 1 m de un tablón o un pilote. En la
  PR solo se congela la huella, y una columna (0,25 m) aguanta hasta 60°.
- **Red:** 08 §2.6 fija 80 m de radio, 64 celdas/s por chunk y el oleaje una vez por medio
  ciclo, con un presupuesto de 0,6 kbps. La PR usa 24 m, 4 096 columnas por paso de
  100 ms y oleaje continuo.
- Se piden dos alternativas: ajustar el modelo a la biblia, o cambiar la biblia en la
  misma PR con la aprobación del director.

### Duplicados y choques

- #48 contiene el commit de #45. Se fusionó primero #45 y después #48, y ninguna se
  cerró como duplicado.
- #44 y #49 tocan zonas distintas de `build_materials.py` (`M_Ocean`) y se fusionan
  entre sí sin conflicto (`git merge-tree`).
- #46 no choca con ninguna.

### 00-TODO.md

Sin casillas marcadas. Lo fusionado (#45 y #48) es la curación y normalización de
packs CC0 para herramientas, caza y comida. La casilla de arte de GDD §7.1 pide
vegetación, mobiliario y props seleccionados y retocados, y además importados en
`/Game/Packs`. Eso sigue pendiente en local (ver «Import en Unreal» en
`Tools/Packs/README.md`).
