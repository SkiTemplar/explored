# Informe de localización

Generado por `Tools/Localization` (`uv run l10n export`); no se edita a mano.
La guía está en [`localizacion.md`](localizacion.md).

## Recuento

| Concepto | Número |
|---|---|
| Textos en el catálogo | 1109 |
| … del C++ y los .ini (van al manifiesto de Unreal) | 303 |
| … de `Content/Data` (campos bilingües) | 806 |
| Textos sin inglés | 0 |
| Claves propuestas pendientes de integrar | 7 |
| Literales sin localizar | 0 |
| Literales invariantes | 2 |
| Literales para revisar | 79 |
| Errores / avisos | 0 / 0 |

## Literales del C++

### Literal (0)

Literales con letras que llegan a la pantalla: pasar a `NSLOCTEXT`/`LOCTEXT` (o `FText::Format`).

### Invariante (2)

Sin letras (números, símbolos, separadores): `FText::AsCultureInvariant` o `FText::AsNumber`, sin traducir.

| Fichero:línea | Texto | Código |
|---|---|---|
| `Source/Explored/UI/Widgets/SExploredSettingsPanel.cpp:254` | `%d x %d` | `ResolutionLabels.Add(FText::AsCultureInvariant(FString::Printf(TEXT("%d x %d"), R.X, R.Y)));` |
| `Source/Explored/UI/Widgets/SExploredWristWatch.cpp:111` | `%02d:%02d` | `return FText::FromString(FString::Printf(TEXT("%02d:%02d"), R.Hour, R.Minute));` |

### Revisar (79)

Parecen prosa pero no se ve cómo llegan a la UI: comprobar a mano.

| Fichero:línea | Texto | Código |
|---|---|---|
| `Source/Explored/Achievements/AchievementsModel.cpp:246` | `Estadística sin id` | `return Fail(TEXT("Estadística sin id"));` |
| `Source/Explored/Achievements/AchievementsModel.cpp:250` | `Estadística repetida: %s` | `return Fail(FString::Printf(TEXT("Estadística repetida: %s"), *Def.Id.ToString()));` |
| `Source/Explored/Achievements/AchievementsModel.cpp:259` | `Logro sin id` | `return Fail(TEXT("Logro sin id"));` |
| `Source/Explored/Achievements/AchievementsModel.cpp:263` | `Logro repetido: %s` | `return Fail(FString::Printf(TEXT("Logro repetido: %s"), *Def.Id.ToString()));` |
| `Source/Explored/Carry/InventoryModel.cpp:181` | `Sin error` | `case EInventoryFail::None: return TEXT("Sin error");` |
| `Source/Explored/Carry/InventoryModel.cpp:182` | `Objeto no válido` | `case EInventoryFail::InvalidItem: return TEXT("Objeto no válido");` |
| `Source/Explored/Carry/InventoryModel.cpp:183` | `No se encuentra` | `case EInventoryFail::NotFound: return TEXT("No se encuentra");` |
| `Source/Explored/Carry/InventoryModel.cpp:184` | `Ya está ahí` | `case EInventoryFail::AlreadyThere: return TEXT("Ya está ahí");` |
| `Source/Explored/Carry/InventoryModel.cpp:185` | `Esa mano está ocupada` | `case EInventoryFail::HandOccupied: return TEXT("Esa mano está ocupada");` |
| `Source/Explored/Carry/InventoryModel.cpp:186` | `Hacen falta las dos manos libres` | `case EInventoryFail::NeedBothHands: return TEXT("Hacen falta las dos manos libres");` |
| `Source/Explored/Carry/InventoryModel.cpp:187` | `Las dos manos están ocupadas` | `case EInventoryFail::HandsFull: return TEXT("Las dos manos están ocupadas");` |
| `Source/Explored/Carry/InventoryModel.cpp:188` | `Es un único objeto en las dos manos` | `case EInventoryFail::SameItem: return TEXT("Es un único objeto en las dos manos");` |
| `Source/Explored/Carry/InventoryModel.cpp:189` | `Demasiado grande` | `case EInventoryFail::TooBig: return TEXT("Demasiado grande");` |
| `Source/Explored/Carry/InventoryModel.cpp:190` | `No es de lo que se guarda ahí` | `case EInventoryFail::WrongKind: return TEXT("No es de lo que se guarda ahí");` |
| `Source/Explored/Carry/InventoryModel.cpp:191` | `No quedan huecos` | `case EInventoryFail::ContainerFull: return TEXT("No quedan huecos");` |
| `Source/Explored/Carry/InventoryModel.cpp:192` | `Pesa demasiado` | `case EInventoryFail::TooHeavy: return TEXT("Pesa demasiado");` |
| `Source/Explored/Carry/InventoryModel.cpp:193` | `No cabe` | `case EInventoryFail::NoRoom: return TEXT("No cabe");` |
| `Source/Explored/Carry/InventoryModel.cpp:194` | `Sin mochila` | `case EInventoryFail::NoBackpack: return TEXT("Sin mochila");` |
| `Source/Explored/Carry/InventoryModel.cpp:195` | `Sin bolsa estanca` | `case EInventoryFail::NoPouch: return TEXT("Sin bolsa estanca");` |
| `Source/Explored/Carry/InventoryModel.cpp:196` | `Sin angarillas` | `case EInventoryFail::NoSledge: return TEXT("Sin angarillas");` |
| `Source/Explored/Carry/InventoryModel.cpp:197` | `Ya hay angarillas enganchadas` | `case EInventoryFail::SledgeAttached: return TEXT("Ya hay angarillas enganchadas");` |
| `Source/Explored/Carry/InventoryModel.cpp:198` | `No se puede poner` | `case EInventoryFail::NotEquippable: return TEXT("No se puede poner");` |
| `Source/Explored/Carry/InventoryModel.cpp:199` | `Hay que vaciarlo antes` | `case EInventoryFail::ContainerNotEmpty: return TEXT("Hay que vaciarlo antes");` |
| `Source/Explored/Carry/InventoryModel.cpp:200` | `Demasiado peso encima` | `case EInventoryFail::OverCarryLimit: return TEXT("Demasiado peso encima");` |
| `Source/Explored/Carry/InventoryModel.cpp:201` | `No guarda líquidos` | `case EInventoryFail::NotALiquidContainer: return TEXT("No guarda líquidos");` |
| `Source/Explored/Carry/InventoryModel.cpp:202` | `Id de instancia repetido` | `case EInventoryFail::DuplicateId: return TEXT("Id de instancia repetido");` |
| `Source/Explored/Carry/InventoryModel.cpp:203` | `Estado incoherente` | `case EInventoryFail::CorruptState: return TEXT("Estado incoherente");` |
| `Source/Explored/Cooking/CookingModel.cpp:259` | `No hay nada que cocinar.` | `OutFailReason = TEXT("No hay nada que cocinar.");` |
| `Source/Explored/Cooking/CookingModel.cpp:264` | `Son demasiadas cosas a la vez.` | `OutFailReason = TEXT("Son demasiadas cosas a la vez.");` |
| `Source/Explored/Cooking/CookingModel.cpp:270` | `Eso no sirve para cocinar.` | `OutFailReason = TEXT("Eso no sirve para cocinar.");` |
| `Source/Explored/Cooking/CookingModel.cpp:275` | `No cabe tanto en %s.` | `OutFailReason = FString::Printf(TEXT("No cabe tanto en %s."), *Vessel->NameEs);` |
| `Source/Explored/Cooking/CookingModel.cpp:281` | `Hace falta un recipiente que no pierda agua.` | `OutFailReason = TEXT("Hace falta un recipiente que no pierda agua.");` |
| `Source/Explored/Cooking/CookingModel.cpp:291` | `Así no se puede %s.` | `OutFailReason = FString::Printf(TEXT("Así no se puede %s."), LexToString(Technique));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:411` | `stat unit` | `GEngine->Exec(World, TEXT("stat unit"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:412` | `stat gpu` | `GEngine->Exec(World, TEXT("stat gpu"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:413` | `stat rhi` | `GEngine->Exec(World, TEXT("stat rhi"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:414` | `stat streaming` | `GEngine->Exec(World, TEXT("stat streaming"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:419` | `r.Nanite.ShowStats 1` | `GEngine->Exec(World, TEXT("r.Nanite.ShowStats 1"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:447` | `memreport -full` | `GEngine->Exec(World, TEXT("memreport -full"));` |
| `Source/Explored/Debug/NetBudgetModel.cpp:96` | `%s en el segundo %lld: %.2f kbps supera el tope de %.2f kbps` | `OutReason = FString::Printf(TEXT("%s en el segundo %lld: %.2f kbps supera el tope de %.2f kbps"),` |
| `Source/Explored/Debug/NetBudgetModel.cpp:158` | `%s en el segundo %lld: %.3f kbps en total supera el tope de %s (%.0f kbps)` | `OutViolations.Add(FString::Printf(TEXT("%s en el segundo %lld: %.3f kbps en total supera el tope de %s (%.0f kbps)"),` |
| `Source/Explored/Debug/NetBudgetModel.cpp:167` | `%s en el segundo %lld: el canal %s no tiene un valor finito` | `OutViolations.Add(FString::Printf(TEXT("%s en el segundo %lld: el canal %s no tiene un valor finito"),` |
| `Source/Explored/Debug/NetBudgetModel.cpp:179` | `%s en el segundo %lld: la cola de terreno manda %.3f kbps y su ráfaga es de %.0f kbps` | `OutViolations.Add(FString::Printf(TEXT("%s en el segundo %lld: la cola de terreno manda %.3f kbps y su ráfaga es de %.0f kbps"),` |
| `Source/Explored/Debug/NetBudgetModel.cpp:185` | `%s en el segundo %lld: la cola de terreno lleva más ráfaga de la permitida (%.0f kbps durante %.0f s)` | `OutViolations.Add(FString::Printf(TEXT("%s en el segundo %lld: la cola de terreno lleva más ráfaga de la permitida (%.0f kbps durante %.0f s)"),` |
| `Source/Explored/Debug/NetBudgetModel.cpp:194` | `cliente,segundo,canal,kbps\n` | `FString Csv(TEXT("cliente,segundo,canal,kbps\n"));` |
| `Source/Explored/Debug/NetBudgetModel.cpp:204` | `,total,` | `Csv += Client + TEXT(",") + Second + TEXT(",total,") + NetBudgetDetail::FormatKbps(Row.TotalKbps) + TEXT("\n");` |
| `Source/Explored/Exploration/ExplorationContentModel.cpp:73` | `<sin id>` | `const FString Tag = L.Id.IsNone() ? TEXT("<sin id>") : L.Id.ToString();` |
| `Source/Explored/Fishing/FishingModel.cpp:76` | `Pez loro` | `FFishSpecies S = MakeSpecies(TEXT("pez_loro"), TEXT("Pez loro"), FishBit(EHab::Reef), Rod \| Spear \| Net \| Trap,` |
| `Source/Explored/Fishing/FishingModel.cpp:86` | `Pez cirujano` | `FFishSpecies S = MakeSpecies(TEXT("pez_cirujano"), TEXT("Pez cirujano"), FishBit(EHab::Reef) \| FishBit(EHab::Lagoon),` |
| `Source/Explored/Fishing/FishingModel.cpp:130` | `Pez ballesta` | `FFishSpecies S = MakeSpecies(TEXT("pez_ballesta"), TEXT("Pez ballesta"), FishBit(EHab::Reef), Rod \| Spear \| Trap,` |
| `Source/Explored/Fishing/FishingModel.cpp:188` | `Atún` | `FFishSpecies S = MakeSpecies(TEXT("atun"), TEXT("Atún"), FishBit(EHab::Deep), Rod,` |
| `Source/Explored/Fishing/FishingModel.cpp:202` | `Langosta de arrecife` | `FFishSpecies S = MakeSpecies(TEXT("langosta"), TEXT("Langosta de arrecife"), FishBit(EHab::Reef), Spear \| Trap \| Hand,` |
| `Source/Explored/Fishing/FishingModel.cpp:239` | `El Viejo` | `FLegendaryCatch L = MakeLegend(TEXT("el_viejo"), TEXT("El Viejo"), TEXT("cueva_arenas_blancas"), Rod,` |
| `Source/Explored/Fishing/FishingModel.cpp:260` | `La Manta Negra` | `FLegendaryCatch L = MakeLegend(TEXT("manta_negra"), TEXT("La Manta Negra"), TEXT("bajios_manglar"), Spear,` |
| `Source/Explored/Fishing/FishingModel.cpp:269` | `El Errante` | `FLegendaryCatch L = MakeLegend(TEXT("el_errante"), TEXT("El Errante"), TEXT("arrecife_arenas_blancas"), Spear \| Rod,` |
| `Source/Explored/Fishing/FishingModel.cpp:277` | `El Rey de Plata` | `FLegendaryCatch L = MakeLegend(TEXT("rey_de_plata"), TEXT("El Rey de Plata"), TEXT("mar_abierto"), Rod,` |
| `Source/Explored/Items/ItemTypes.cpp:7` | `Pequeño` | `case EItemSize::Pequeno: return TEXT("Pequeño");` |
| `Source/Explored/Mining/TerrainEditSubsystem.cpp:22` | `Presupuesto del remallado del terreno en el hilo de juego por fotograma (ms).` | `TEXT("Presupuesto del remallado del terreno en el hilo de juego por fotograma (ms)."));` |
| `Source/Explored/Save/SaveValue.cpp:455` | `escape \u incompleto` | `return Fail(TEXT("escape \\u incompleto"));` |
| `Source/Explored/Save/SaveValue.cpp:462` | `cifra hexadecimal no válida` | `else { return Fail(TEXT("cifra hexadecimal no válida")); }` |
| `Source/Explored/Save/SaveValue.cpp:473` | `se esperaba una cadena` | `return Fail(TEXT("se esperaba una cadena"));` |
| `Source/Explored/Save/SaveValue.cpp:480` | `cadena sin cerrar` | `return Fail(TEXT("cadena sin cerrar"));` |
| `Source/Explored/Save/SaveValue.cpp:491` | `carácter de control dentro de una cadena` | `return Fail(TEXT("carácter de control dentro de una cadena"));` |
| `Source/Explored/Save/SaveValue.cpp:502` | `escape incompleto` | `return Fail(TEXT("escape incompleto"));` |
| `Source/Explored/Save/SaveValue.cpp:554` | `escape no válido` | `return Fail(TEXT("escape no válido"));` |
| `Source/Explored/Save/SaveValue.cpp:588` | `número no válido` | `return Fail(TEXT("número no válido"));` |
| `Source/Explored/Save/SaveValue.cpp:597` | `faltan decimales` | `return Fail(TEXT("faltan decimales"));` |
| `Source/Explored/Save/SaveValue.cpp:611` | `falta el exponente` | `return Fail(TEXT("falta el exponente"));` |
| `Source/Explored/Save/SaveValue.cpp:618` | `número demasiado largo` | `return Fail(TEXT("número demasiado largo"));` |
| `Source/Explored/Save/SaveValue.cpp:653` | `número fuera de rango` | `return Fail(TEXT("número fuera de rango"));` |
| `Source/Explored/Save/SaveValue.cpp:664` | `fin inesperado del texto` | `return Fail(TEXT("fin inesperado del texto"));` |
| `Source/Explored/Save/SaveValue.cpp:673` | `anidamiento excesivo` | `return Fail(TEXT("anidamiento excesivo"));` |
| `Source/Explored/Save/SaveValue.cpp:694` | `se esperaba «:»` | `return Fail(TEXT("se esperaba «:»"));` |
| `Source/Explored/Save/SaveValue.cpp:704` | `clave duplicada` | `return Fail(TEXT("clave duplicada"));` |
| `Source/Explored/Save/SaveValue.cpp:718` | `se esperaba «,» o «}»` | `return Fail(TEXT("se esperaba «,» o «}»"));` |
| `Source/Explored/Save/SaveValue.cpp:725` | `anidamiento excesivo` | `return Fail(TEXT("anidamiento excesivo"));` |
| `Source/Explored/Save/SaveValue.cpp:754` | `se esperaba «,» o «]»` | `return Fail(TEXT("se esperaba «,» o «]»"));` |
| `Source/Explored/Save/SaveValue.cpp:781` | `carácter inesperado` | `return Fail(TEXT("carácter inesperado"));` |
| `Source/Explored/Save/SaveValue.cpp:1227` | `texto sobrante tras el valor` | `Parser.Fail(TEXT("texto sobrante tras el valor"));` |

## Claves propuestas pendientes de integrar

Ya traducidas en `Tools/Localization/translations/en.json`; el paquete que toque el fichero
las usa con exactamente este espacio de nombres, clave y texto.

| Espacio,clave | ES | EN | Dónde |
|---|---|---|---|
| `ExploredCoop,PlayerBackUp` | {Name} vuelve a estar en pie. | {Name} is back up. | UI de cooperativo (biblia 08 §6.6 y §6.8), con ExploredLinks::DecideGroupSleep y el derribado de SystemLinks |
| `ExploredCoop,PlayerDown` | {Name} está en el suelo. | {Name} is down. | UI de cooperativo (biblia 08 §6.6 y §6.8), con ExploredLinks::DecideGroupSleep y el derribado de SystemLinks |
| `ExploredCoop,ReviveVerb` | Levantar a {Name} | Help {Name} up | UI de cooperativo (biblia 08 §6.6 y §6.8), con ExploredLinks::DecideGroupSleep y el derribado de SystemLinks |
| `ExploredCoop,ReviveVerbMedicine` | Atender a {Name} | Patch {Name} up | UI de cooperativo (biblia 08 §6.6 y §6.8), con ExploredLinks::DecideGroupSleep y el derribado de SystemLinks |
| `ExploredCoop,SleepBlockedByDowned` | No se duerme con alguien en el suelo. | Nobody sleeps with someone down. | UI de cooperativo (biblia 08 §6.6 y §6.8), con ExploredLinks::DecideGroupSleep y el derribado de SystemLinks |
| `ExploredCoop,SleepOneStillUp` | Queda uno en pie. | One still up. | UI de cooperativo (biblia 08 §6.6 y §6.8), con ExploredLinks::DecideGroupSleep y el derribado de SystemLinks |
| `ExploredCoop,SleepStillUp` | Hay {Count} en pie todavía. | {Count} still up. | UI de cooperativo (biblia 08 §6.6 y §6.8), con ExploredLinks::DecideGroupSleep y el derribado de SystemLinks |

## Errores (0)

Ninguno.

## Avisos (0)

Ninguno.
