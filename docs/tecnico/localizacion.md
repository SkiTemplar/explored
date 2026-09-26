# Localización ES/EN

Explored se juega en español (idioma fuente) y en inglés (GDD §15: Ajustes → Juego →
Idioma). No hay diálogos (§12) y todos los textos son de una línea (§3), así que la
localización se reduce a dos cosas:

| Origen | Cómo se traduce | Dónde está el inglés |
|---|---|---|
| Textos del C++ y de los `.ini` (menús, ajustes, avisos, verbos de interacción) | Sistema de localización de Unreal: `NSLOCTEXT`/`LOCTEXT` → `Game.manifest` → `<cultura>/Game.archive` → `Game.locres` | `Tools/Localization/translations/en.json` |
| Textos de `Content/Data/*.json` (objetos, recetas, verbos, plantas, piezas, necesidades, sellos, tesoros, petroglifos) | Campos bilingües en el propio JSON; el juego elige con `ExploredLocalization::Pick` según la cultura activa | El campo `…En` junto al español |

Los datos no pasan por el sistema de Unreal porque se leen en tiempo de ejecución como
JSON (`FText::FromString`), fuera de lo que recopila `GatherText`. Tenerlos en el mismo
fichero que el español hace imposible olvidarse de uno al añadir el otro, y
`Tools/Localization` los valida igual que los del C++.

El informe generado con los recuentos, los literales y los avisos está en
[`localizacion-informe.md`](localizacion-informe.md).

## Cómo escribir textos

### En C++

- Todo texto que vea el jugador es `FText` creado con `NSLOCTEXT` o `LOCTEXT`. Nunca
  `FText::FromString(TEXT("..."))` ni `FString` concatenados para la pantalla.
- Espacios de nombres, uno por módulo y sin puntos (Unreal no los anida):

  | Espacio | Módulo | Claves |
  |---|---|---|
  | `ExploredUI` | `Source/Explored/UI/**` (menús, ajustes, HUD) | `NewGame`, `TabGraphics`, `SectionRemap`… |
  | `Explored` | Juego: `Carry`, `Crafting`, `Items`, `Interaction`… | `<Área>_<Qué>`: `Carry_HandsFull`, `Crafting_NoMatch`, `Verb_PickUp` |
  | `ExploredMap`, `ExploredBuild`, `ExploredMuseum`… | Módulos nuevos (P-MAP, P-BUILD, P-RUINS…) | `<Área>_<Qué>` |

- En un `.cpp` con muchos textos, `LOCTEXT` con el espacio definido arriba y anulado al final:

  ```cpp
  #define LOCTEXT_NAMESPACE "ExploredMap"
  const FText Title = LOCTEXT("Map_Title", "Mapa");
  #undef LOCTEXT_NAMESPACE
  ```

- Misma clave, mismo texto: si dos sitios usan `ExploredUI,Back`, los dos dicen «Volver».
  Una clave con dos textos distintos es un error (Unreal lo marca como conflicto).
- Variables con `FText::Format` y marcadores `{0}` o `{Nombre}`; los números con
  `FText::AsNumber`. Los dos idiomas deben tener los mismos marcadores.
- Lo que no se traduce (números sueltos, `<`, `>`, `·`, resoluciones) va con
  `FText::AsCultureInvariant` o `INVTEXT`. Si un `TEXT("...")` con letras no es para la
  pantalla y la herramienta lo marca, añade `// loc: ignorar` en esa línea.
- Los registros (`UE_LOG`), asserts y errores internos siguen en `TEXT("...")` en español.
- Estilo: tuteo, frases cortas y de una línea (GDD §3), ortografía completa (tildes,
  «¿», «¡»). El inglés es británico (*fibre*, *colour*), cálido y conciso.
- **Dedicatoria:** «Para Almudena, mi Limón» no se traduce; es idéntica en los dos
  idiomas (la herramienta falla si no lo es).
- Los nombres de idioma del selector van en su propia lengua en las dos culturas:
  «Español» y «English».

Al añadir o cambiar un texto del C++, añade su entrada en
`Tools/Localization/translations/en.json` con el español exacto y el inglés, y ejecuta
`uv run l10n export`.

### En `Content/Data`

Cada texto que ve el jugador lleva su pareja en inglés al lado:

| Fichero | Español | Inglés |
|---|---|---|
| `items.json` | `nameEs` | `nameEn` |
| `templates.json` | `nameEs`, `nameTemplate` | `nameEn`, `nameTemplateEn` (mismos `{0}`, `{1}`, `{2}`; el orden de las palabras puede cambiar: «Hacha de {0} con mango de {1}» → «{0} axe with a {1} handle») |
| `verbs.json` | `nameEs`, `description` | `nameEn`, `descriptionEn` |
| `plants.json` | `nameEs` de la planta y de cada etapa | `nameEn` |
| `building_pieces.json` | `nameEs` de niveles y piezas | `nameEn` |
| `survival_needs.json` | `nameEs` | `nameEn` |
| `story_es.json` | `label` de `map_marks` y `artifact_kinds`; `petroglyph_themes` | `labelEn`; `petroglyph_themes_en` (misma longitud y orden) |

Quedan fuera, por ser notas de diseño que el jugador no ve: `units`, `note`, `source`,
`atZero` de `survival_needs.json` y los ids. Una plantilla con `nameTemplate` vacío usa
el nombre del objeto resultante y no necesita `nameTemplateEn`. El nombre
`story_es.json` se mantiene (lo leen `NarrativeSpec` y `Tools/DataCheck`) aunque ya es
bilingüe; cuando se separe en `petroglyphs.json`/`artifacts.json`/`map_marks.json`
(biblia §10) conviene llevar los dos idiomas igual.

En C++, `Core/ExploredLocalization.h` (modelo puro, con `LocalizationSpec` en el host):

```cpp
const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
const FString& Name = ExploredLocalization::Pick(NameEs, NameEn, Culture); // cae al español si falta
FExploredLocalizedString Label(Es, En); Label.Get(Culture);
```

## La herramienta

```bash
cd Tools/Localization
uv run l10n                  # comprueba (código 1 si hay errores)
uv run l10n --strict         # también fallan los avisos y los literales sin localizar
uv run l10n export           # regenera Content/Localization/Game, catálogo e informe
uv run l10n export --check   # falla si lo generado está desfasado (ignora números de línea)
uv run pytest
```

Comprueba que toda clave tiene español e inglés sin valores vacíos, que los marcadores
coinciden, que no hay claves en conflicto, traducciones desfasadas (cambió el español) ni
huérfanas, la dedicatoria, y avisa si el inglés supera 1,3 veces el español (desde 12
caracteres) por si no cabe. Además lista los `TEXT("...")` que llegan a la pantalla sin
localizar. Más detalle en `Tools/Localization/README.md`.

Genera `Content/Localization/Game/Game.manifest`, `Content/Localization/Game/es/Game.archive`,
`Content/Localization/Game/en/Game.archive`, `Content/Localization/catalogo.json` y
[`localizacion-informe.md`](localizacion-informe.md).

## Compilar en el editor (local, UE 5.6)

Los `.locres` que carga el juego son binarios y solo los genera el editor:

1. Regenera las fuentes: `cd Tools/Localization && uv run l10n export`.
2. Compila sin abrir el panel:

   ```powershell
   & "$env:UE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$PWD\Explored.uproject" `
     -run=GatherText -config=Config/Localization/Game_Compile.ini -unattended -nopause
   ```

   Produce `Content/Localization/Game/es/Game.locres` y `en/Game.locres`. Versiónalos.
3. Para comprobar que la recopilación de Unreal coincide con la nuestra, ejecuta antes
   `-config=Config/Localization/Game_Gather.ini`. Reescribe el manifiesto y los archivos
   (en UTF-16 y con las rutas a su manera) conservando las traducciones; después
   `uv run l10n export` los deja otra vez como en el repositorio. Si el panel o la
   recopilación encuentran textos que la herramienta no ve, es un fallo de la herramienta.
4. Con el panel (*Tools → Localization Dashboard*): destino **Game**, cultura nativa
   `es`, culturas `es` y `en`; *Gather from Text Files* activado con las rutas
   `Source/Explored` y `Config` y extensiones `*.h`, `*.cpp`, `*.ini`, excluyendo
   `Source/Explored/Tests/*` y `Config/Localization/*`. El panel reescribe
   `Config/Localization/Game_*.ini` con esos ajustes. **Las traducciones se editan en
   `translations/en.json`, no en el Translation Editor del panel**: lo que se cambie allí
   se pierde con el siguiente `uv run l10n export`.
5. Prueba: `UnrealEditor.exe Explored.uproject -game -culture=en`, o Ajustes → Juego →
   Idioma.

`Config/DefaultGame.ini` empaqueta las culturas `es` y `en` (`CulturesToStage`), con
`es` por defecto y los datos ICU del preset `EFIGS` (incluye español).

### Supuestos sobre el formato de Unreal

Deducidos del pipeline de UE 4.14–5.x (`FJsonInternationalizationManifestSerializer` y
`FJsonInternationalizationArchiveSerializer`); a verificar la primera vez en local:

- Manifiesto y archivo son JSON con tabulaciones. Raíz: `FormatVersion`, `Namespace` vacío,
  `Children` (textos sin espacio de nombres) y `Subnamespaces` (uno por espacio de nombres
  con su `Namespace` y `Children`).
- Manifiesto con `FormatVersion` 1: un hijo por texto fuente dentro del espacio,
  `{"Source": {"Text"}, "Keys": [{"Key", "Path"}]}`; las claves que comparten texto van en
  el mismo hijo. `Path` sigue el estilo de `GatherTextFromSource`: `Fichero - line N`.
- Archivo con `FormatVersion` 2: un hijo por clave, `{"Source": {"Text"}, "Translation":
  {"Text"}, "Key"}`; en la cultura nativa (`es`) la traducción es el propio texto fuente.
- Unreal escribe estos ficheros en UTF-16 LE con BOM y lee también UTF-8; los versionamos
  en UTF-8 para que el diff sea legible (`--utf16` imita al editor).
- `%LOCPROJECTROOT%` en los `.ini` es la raíz del proyecto, como en los que genera el panel.
- Sin metadatos ni claves opcionales (`MetaData`, `Optional`): no se usan.

## Integración pendiente

Otros paquetes están tocando `Source/Explored/UI/**` y `ExploredGameUserSettings.*`, así
que esta rama no los modifica. Lo que falta, por fichero:

### Literales de la UI que hay que convertir (paquete P-UI)

| Fichero:línea | Hoy | Cambio |
|---|---|---|
| `UI/ExploredHUD.cpp:59` | `FString Prompt = TEXT("[E] ");` | `FText::Format(NSLOCTEXT("ExploredUI", "InteractPrompt", "[{Key}] {Actions}"), Args)` con `Key` = nombre de la tecla asignada a `IA_Interact` (hoy la «E» está fija aunque se reasigne) |
| `UI/ExploredHUD.cpp:65` | `Prompt += TEXT(" · ");` | `Actions` = `FText::Join(INVTEXT(" · "), Verbs)`; dibujar el `FText` sin pasar por `FString` |
| `UI/Widgets/SExploredSettingsPanel.cpp:359` | `FText::FromString(FString::Printf(TEXT("%d min"), …))` | `FText::Format(NSLOCTEXT("ExploredUI", "DayLengthMinutes", "{0} min"), FText::AsNumber(Minutes))` |
| `UI/Widgets/SExploredSettingsPanel.cpp:67` y `:77` | `FText::FromString(TEXT("<"))`, `TEXT(">")` | `INVTEXT("<")`, `INVTEXT(">")` |
| `UI/Widgets/SExploredSettingsPanel.cpp:184` | `FText::FromString(FString::Printf(TEXT("%d x %d"), …))` | `FText::AsCultureInvariant(FString::Printf(…))` (sin `AsNumber`: pondría separador de millares) |
| `UI/Widgets/SExploredSettingsPanel.cpp:195-196` | `FText::FromString(TEXT("30"))`… `TEXT("144")` | `FText::AsNumber(30)`… o `INVTEXT("30")` |
| `UI/Widgets/SExploredMainMenu.cpp` y `SExploredCredits.cpp` | Falta la dedicatoria (GDD §15) | `NSLOCTEXT("ExploredUI", "Dedication", "Para Almudena, mi Limón")` bajo el subtítulo y en los créditos |

Las claves `InteractPrompt`, `DayLengthMinutes` y `Dedication` ya están traducidas en
`translations/en.json` como *pendientes*; al usarlas con ese texto exacto, la herramienta
avisa de que se quite la marca `pendiente`.

`Items/ItemTypes.cpp:7` (`LexToString(EItemSize)` → «Pequeño»…) sale como *revisar*: hoy
solo se usa en depuración; si llega a la UI, que sea con `NSLOCTEXT`.

### Idioma activo (paquete de ajustes)

- `UExploredGameUserSettings::SetLanguage` ya llama a
  `FInternationalization::Get().SetCurrentCulture("en"/"es")`, pero el idioma guardado no
  se aplica al arrancar ni al restaurar valores por defecto: hay que llamar a
  `SetCurrentCulture` también tras cargar los ajustes (p. ej. en
  `ApplyNonResolutionSettings`).
- Los `FText` de `NSLOCTEXT` se actualizan solos al cambiar de cultura; los construidos
  con `FText::FromString` no (por eso los literales de arriba).

### Nombres de los datos (paquetes de objetos, fabricación y futuros cargadores)

- `ItemEffective::GetDisplayName` y `FindDominantLeafName` (`Items/ItemTypes.cpp`)
  devuelven siempre `NameEs`: deben usar `ExploredLocalization::Pick` con la cultura activa
  (`NameEn` ya se carga).
- `UItemRegistrySubsystem` no carga `nameEn` de plantillas ni `nameTemplateEn`, `nameEn` y
  `descriptionEn` de verbos: añadirlos a `FCraftingTemplateDef`/`FCraftingVerbDef` y elegir
  en `UCraftingLibrary::BuildGeneratedName`.
- `BuildGeneratedName` guarda el nombre ya formateado en `FItemInstance::GeneratedName`:
  al cambiar de idioma los objetos fabricados conservarían el nombre antiguo. Propuesta:
  guardar el id de la plantilla y generar el nombre al mostrarlo.
- Los cargadores futuros de `plants.json`, `building_pieces.json`, `survival_needs.json` y
  `story_es.json` leen los dos campos y eligen con `ExploredLocalization::Pick`.
