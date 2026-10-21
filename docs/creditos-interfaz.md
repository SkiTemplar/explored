# Créditos de la interfaz

## Tipografía

- **Lora** (Regular, Bold, Italic), Copyright 2011 The Lora Project Authors
  (https://github.com/cyrealtype/Lora-Cyrillic), nombre reservado «Lora».
- Licencia: SIL Open Font License 1.1. El texto completo va junto a las fuentes en
  `Content/Slate/Fonts/OFL.txt` y se empaqueta con el juego (`DefaultGame.ini`,
  `DirectoriesToAlwaysStageAsUFS=Slate`).
- Uso: toda la interfaz (`UI/ExploredUIStyle.cpp`), texto mínimo de 22 px a 1080p (GDD v3 §9).
- Aparece en la pantalla de créditos (`SExploredCredits`, apartado «Tipografía»).

## Iconos

- `Content/Slate/T_IconosUI.png` es la **única textura de interfaz** (excepción a «0 texturas» del
  GDD §11, que se refiere a las mallas). Iconos planos y blancos, dibujados por código y teñidos
  en Slate con los colores de la paleta.
- Generada por `cd Tools/Style && uv run style-ui` a partir de `explored_style/ui_icons.py` y
  `paleta.json`; el mismo comando escribe `Source/Explored/UI/ExploredUIPalette.h` (colores y enum
  `EExploredIcon`) y la lámina `docs/v3/laminas/f5-iconos.png`. `uv run style-ui --check` falla si
  lo versionado está desfasado (lo comprueban los tests de `Tools/Style`).
- Obra propia del proyecto; sin recursos de terceros.

## Música

Ver `docs/creditos-musica.md` (generado por `Tools/Audio`).
