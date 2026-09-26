# Informe de localización

Generado por `Tools/Localization` (`uv run l10n export`); no se edita a mano.
La guía está en [`localizacion.md`](localizacion.md).

## Recuento

| Concepto | Número |
|---|---|
| Textos en el catálogo | 361 |
| … del C++ y los .ini (van al manifiesto de Unreal) | 114 |
| … de `Content/Data` (campos bilingües) | 247 |
| Textos sin inglés | 0 |
| Claves propuestas pendientes de integrar | 3 |
| Literales sin localizar | 2 |
| Literales invariantes | 8 |
| Literales para revisar | 1 |
| Errores / avisos | 0 / 12 |

## Literales del C++

### Literal (2)

Literales con letras que llegan a la pantalla: pasar a `NSLOCTEXT`/`LOCTEXT` (o `FText::Format`).

| Fichero:línea | Texto | Código |
|---|---|---|
| `Source/Explored/UI/ExploredHUD.cpp:59` | `[E] ` | `FString Prompt = TEXT("[E] ");` |
| `Source/Explored/UI/Widgets/SExploredSettingsPanel.cpp:359` | `%d min` | `DayLengthLabels.Add(FText::FromString(FString::Printf(TEXT("%d min"), static_cast<int32>(Minutes))));` |

### Invariante (8)

Sin letras (números, símbolos, separadores): `FText::AsCultureInvariant` o `FText::AsNumber`, sin traducir.

| Fichero:línea | Texto | Código |
|---|---|---|
| `Source/Explored/UI/ExploredHUD.cpp:65` | ` · ` | `Prompt += TEXT(" · ");` |
| `Source/Explored/UI/Widgets/SExploredSettingsPanel.cpp:67` | `<` | `.Text(FText::FromString(TEXT("<")))` |
| `Source/Explored/UI/Widgets/SExploredSettingsPanel.cpp:77` | `>` | `.Text(FText::FromString(TEXT(">")))` |
| `Source/Explored/UI/Widgets/SExploredSettingsPanel.cpp:184` | `%d x %d` | `ResolutionLabels.Add(FText::FromString(FString::Printf(TEXT("%d x %d"), R.X, R.Y)));` |
| `Source/Explored/UI/Widgets/SExploredSettingsPanel.cpp:195` | `30` | `FText::FromString(TEXT("30")), FText::FromString(TEXT("60")), FText::FromString(TEXT("120")),` |
| `Source/Explored/UI/Widgets/SExploredSettingsPanel.cpp:195` | `60` | `FText::FromString(TEXT("30")), FText::FromString(TEXT("60")), FText::FromString(TEXT("120")),` |
| `Source/Explored/UI/Widgets/SExploredSettingsPanel.cpp:195` | `120` | `FText::FromString(TEXT("30")), FText::FromString(TEXT("60")), FText::FromString(TEXT("120")),` |
| `Source/Explored/UI/Widgets/SExploredSettingsPanel.cpp:196` | `144` | `FText::FromString(TEXT("144")), NSLOCTEXT("ExploredUI", "Unlimited", "Sin límite")` |

### Revisar (1)

Parecen prosa pero no se ve cómo llegan a la UI: comprobar a mano.

| Fichero:línea | Texto | Código |
|---|---|---|
| `Source/Explored/Items/ItemTypes.cpp:7` | `Pequeño` | `case EItemSize::Pequeno: return TEXT("Pequeño");` |

## Claves propuestas pendientes de integrar

Ya traducidas en `Tools/Localization/translations/en.json`; el paquete que toque el fichero
las usa con exactamente este espacio de nombres, clave y texto.

| Espacio,clave | ES | EN | Dónde |
|---|---|---|---|
| `ExploredUI,DayLengthMinutes` | {0} min | {0} min | SExploredSettingsPanel.cpp:359, en lugar de FString::Printf(TEXT("%d min")) |
| `ExploredUI,Dedication` | Para Almudena, mi Limón | Para Almudena, mi Limón | SExploredMainMenu (bajo el subtítulo) y SExploredCredits (GDD §15); no se traduce |
| `ExploredUI,InteractPrompt` | [{Key}] {Actions} | [{Key}] {Actions} | ExploredHUD.cpp:59-66, en lugar de TEXT("[E] ") y la concatenación con TEXT(" · ") |

## Errores (0)

Ninguno.

## Avisos (12)

- Source/Explored/UI/Widgets/SExploredSettingsPanel.cpp:247: ExploredUI,ViewDistance: el inglés (13 car.) es más de 1.3× el español (5); comprueba que cabe
- Source/Explored/UI/Widgets/SExploredSettingsPanel.cpp:251: ExploredUI,PostProcess: el inglés (15 car.) es más de 1.3× el español (11); comprueba que cabe
- Content/Data/items.json «palo_recto» nameEs/nameEn: Data.items.items,palo_recto.nameEs: el inglés (14 car.) es más de 1.3× el español (10); comprueba que cabe
- Content/Data/items.json «batata» nameEs/nameEn: Data.items.items,batata.nameEs: el inglés (12 car.) es más de 1.3× el español (6); comprueba que cabe
- Content/Data/items.json «maracuya» nameEs/nameEn: Data.items.items,maracuya.nameEs: el inglés (13 car.) es más de 1.3× el español (8); comprueba que cabe
- Content/Data/items.json «lasca_tallada» nameEs/nameEn: Data.items.items,lasca_tallada.nameEs: el inglés (13 car.) es más de 1.3× el español (5); comprueba que cabe
- Content/Data/plants.json «platanera» nameEs/nameEn: Data.plants.plants,platanera.nameEs: el inglés (12 car.) es más de 1.3× el español (9); comprueba que cabe
- Content/Data/plants.json «batata» nameEs/nameEn: Data.plants.plants,batata.nameEs: el inglés (12 car.) es más de 1.3× el español (6); comprueba que cabe
- Content/Data/plants.json «maracuya» nameEs/nameEn: Data.plants.plants,maracuya.nameEs: el inglés (13 car.) es más de 1.3× el español (8); comprueba que cabe
- Content/Data/story_es.json «6» petroglyph_themes/petroglyph_themes_en: Data.story_es.petroglyph_themes,06: el inglés (21 car.) es más de 1.3× el español (16); comprueba que cabe
- Source/Explored/UI/ExploredHUD.cpp:59: literal sin localizar «[E] »
- Source/Explored/UI/Widgets/SExploredSettingsPanel.cpp:359: literal sin localizar «%d min»
