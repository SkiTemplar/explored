# Localization

Catálogo de textos ES/EN de Explored (sin Unreal). Une en un solo catálogo los
`NSLOCTEXT`/`LOCTEXT` de `Source/Explored` y de `Config/*.ini` con los campos bilingües
de `Content/Data/*.json`, lo valida y exporta el formato fuente de localización de Unreal
para el Localization Dashboard. La guía completa está en `docs/tecnico/localizacion.md`.

```bash
cd Tools/Localization
uv run l10n                  # comprueba; código de salida 1 si hay errores
uv run l10n --strict         # los avisos (longitud, literales sin localizar) también fallan
uv run l10n export           # escribe Content/Localization/Game, el catálogo y el informe
uv run l10n export --check   # no escribe; falla si lo versionado está desfasado
uv run l10n export --utf16   # manifiesto y archivos en UTF-16 LE con BOM, como el editor
uv run pytest                # tests (repo real + regresiones sintéticas)
```

Fuentes de verdad:

- **Español del C++:** el propio código (`NSLOCTEXT("ExploredUI", "Clave", "Texto")`).
- **Inglés del C++:** `translations/en.json` (`espacio → clave → {es, en}`). Guarda también
  el español que traduce, para detectar traducciones desfasadas. Las entradas con
  `pendiente` son claves propuestas que aún no están en el código.
- **Datos:** cada fichero de `Content/Data` lleva sus dos idiomas (`nameEs`/`nameEn`,
  `label`/`labelEn`, `petroglyph_themes`/`petroglyph_themes_en`...). Ver `src/l10n/data.py`.

Qué comprueba:

- Toda clave tiene español e inglés, sin valores vacíos.
- Los marcadores (`{0}`, `{Name}`) coinciden entre idiomas.
- La misma clave no tiene dos textos distintos (Unreal lo trataría como conflicto).
- Traducciones desfasadas (el español cambió) y huérfanas (la clave ya no existe).
- La dedicatoria «Para Almudena, mi Limón» es idéntica en los dos idiomas.
- Avisos de longitud: el inglés más de 1,3 veces el español (desde 12 caracteres).
- Literales `TEXT("...")` que llegan a la pantalla sin pasar por `NSLOCTEXT`/`LOCTEXT`
  (se clasifican en *literal*, *invariante* y *revisar*; `// loc: ignorar` en la línea
  los excluye).
- Modificadores `{Arg}|plural(...)`, `|ordinal(...)`, `|gender(...)`, `|hpp(...)`: sintaxis,
  categorías CLDR de cada idioma y que ES y EN pluralicen los mismos argumentos (`icu.py`).
- Guía anti-IA de biblia 07 §1.2–§1.3: lista negra, emoji, exclamaciones en logros y museo,
  máximos de longitud de logros y etiquetas de vitrina (`estilo.py`).
- Glosario de `docs/tecnico/glosario.md`: términos con traducción fijada (`glosario.py`).
- Que ningún campo `…Es` de `Content/Data` se quede sin registrar en `data.py`, y que un
  mismo nombre no tenga dos traducciones en dos ficheros.

Excepciones revisadas a mano, con su motivo: `translations/excepciones.json`
(`longitud`, `estilo`).

Genera:

- `Content/Localization/Game/Game.manifest`, `es/Game.archive`, `en/Game.archive`.
- `Content/Localization/catalogo.json` (todo el catálogo, también los datos).
- `docs/tecnico/localizacion-informe.md` (recuento, literales, avisos).
