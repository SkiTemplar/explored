# Informe de localización

Generado por `Tools/Localization` (`uv run l10n export`); no se edita a mano.
La guía está en [`localizacion.md`](localizacion.md).

## Recuento

| Concepto | Número |
|---|---|
| Textos en el catálogo | 646 |
| … del C++ y los .ini (van al manifiesto de Unreal) | 306 |
| … de `Content/Data` (campos bilingües) | 340 |
| Textos sin inglés | 0 |
| Claves propuestas pendientes de integrar | 0 |
| Literales sin localizar | 0 |
| Literales invariantes | 3 |
| Literales para revisar | 113 |
| Errores / avisos | 0 / 25 |

## Literales del C++

### Literal (0)

Literales con letras que llegan a la pantalla: pasar a `NSLOCTEXT`/`LOCTEXT` (o `FText::Format`).

### Invariante (3)

Sin letras (números, símbolos, separadores): `FText::AsCultureInvariant` o `FText::AsNumber`, sin traducir.

| Fichero:línea | Texto | Código |
|---|---|---|
| `Source/Explored/Debug/PlaytestReportModel.cpp:182` | `== %s (%d) ==\n` | `Out += FString::Printf(TEXT("== %s (%d) ==\n"), Title, Matching.Num());` |
| `Source/Explored/UI/Widgets/SExploredSettingsPanel.cpp:254` | `%d x %d` | `ResolutionLabels.Add(FText::AsCultureInvariant(FString::Printf(TEXT("%d x %d"), R.X, R.Y)));` |
| `Source/Explored/UI/Widgets/SExploredWristWatch.cpp:111` | `%02d:%02d` | `return FText::FromString(FString::Printf(TEXT("%02d:%02d"), R.Hour, R.Minute));` |

### Revisar (113)

Parecen prosa pero no se ve cómo llegan a la UI: comprobar a mano.

| Fichero:línea | Texto | Código |
|---|---|---|
| `Source/Explored/Achievements/AchievementsModel.cpp:186` | `Estadística sin id` | `return Fail(TEXT("Estadística sin id"));` |
| `Source/Explored/Achievements/AchievementsModel.cpp:190` | `Estadística repetida: %s` | `return Fail(FString::Printf(TEXT("Estadística repetida: %s"), *Def.Id.ToString()));` |
| `Source/Explored/Achievements/AchievementsModel.cpp:199` | `Logro sin id` | `return Fail(TEXT("Logro sin id"));` |
| `Source/Explored/Achievements/AchievementsModel.cpp:203` | `Logro repetido: %s` | `return Fail(FString::Printf(TEXT("Logro repetido: %s"), *Def.Id.ToString()));` |
| `Source/Explored/Carry/InventoryModel.cpp:164` | `Sin error` | `case EInventoryFail::None: return TEXT("Sin error");` |
| `Source/Explored/Carry/InventoryModel.cpp:165` | `Objeto no válido` | `case EInventoryFail::InvalidItem: return TEXT("Objeto no válido");` |
| `Source/Explored/Carry/InventoryModel.cpp:166` | `No se encuentra` | `case EInventoryFail::NotFound: return TEXT("No se encuentra");` |
| `Source/Explored/Carry/InventoryModel.cpp:167` | `Ya está ahí` | `case EInventoryFail::AlreadyThere: return TEXT("Ya está ahí");` |
| `Source/Explored/Carry/InventoryModel.cpp:168` | `Esa mano está ocupada` | `case EInventoryFail::HandOccupied: return TEXT("Esa mano está ocupada");` |
| `Source/Explored/Carry/InventoryModel.cpp:169` | `Hacen falta las dos manos libres` | `case EInventoryFail::NeedBothHands: return TEXT("Hacen falta las dos manos libres");` |
| `Source/Explored/Carry/InventoryModel.cpp:170` | `Las dos manos están ocupadas` | `case EInventoryFail::HandsFull: return TEXT("Las dos manos están ocupadas");` |
| `Source/Explored/Carry/InventoryModel.cpp:171` | `Es un único objeto en las dos manos` | `case EInventoryFail::SameItem: return TEXT("Es un único objeto en las dos manos");` |
| `Source/Explored/Carry/InventoryModel.cpp:172` | `Demasiado grande` | `case EInventoryFail::TooBig: return TEXT("Demasiado grande");` |
| `Source/Explored/Carry/InventoryModel.cpp:173` | `No es de lo que se guarda ahí` | `case EInventoryFail::WrongKind: return TEXT("No es de lo que se guarda ahí");` |
| `Source/Explored/Carry/InventoryModel.cpp:174` | `No quedan huecos` | `case EInventoryFail::ContainerFull: return TEXT("No quedan huecos");` |
| `Source/Explored/Carry/InventoryModel.cpp:175` | `Pesa demasiado` | `case EInventoryFail::TooHeavy: return TEXT("Pesa demasiado");` |
| `Source/Explored/Carry/InventoryModel.cpp:176` | `No cabe` | `case EInventoryFail::NoRoom: return TEXT("No cabe");` |
| `Source/Explored/Carry/InventoryModel.cpp:177` | `Sin mochila` | `case EInventoryFail::NoBackpack: return TEXT("Sin mochila");` |
| `Source/Explored/Carry/InventoryModel.cpp:178` | `Sin bolsa estanca` | `case EInventoryFail::NoPouch: return TEXT("Sin bolsa estanca");` |
| `Source/Explored/Carry/InventoryModel.cpp:179` | `Sin angarillas` | `case EInventoryFail::NoSledge: return TEXT("Sin angarillas");` |
| `Source/Explored/Carry/InventoryModel.cpp:180` | `Ya hay angarillas enganchadas` | `case EInventoryFail::SledgeAttached: return TEXT("Ya hay angarillas enganchadas");` |
| `Source/Explored/Carry/InventoryModel.cpp:181` | `No se puede poner` | `case EInventoryFail::NotEquippable: return TEXT("No se puede poner");` |
| `Source/Explored/Carry/InventoryModel.cpp:182` | `Hay que vaciarlo antes` | `case EInventoryFail::ContainerNotEmpty: return TEXT("Hay que vaciarlo antes");` |
| `Source/Explored/Carry/InventoryModel.cpp:183` | `Demasiado peso encima` | `case EInventoryFail::OverCarryLimit: return TEXT("Demasiado peso encima");` |
| `Source/Explored/Carry/InventoryModel.cpp:184` | `No guarda líquidos` | `case EInventoryFail::NotALiquidContainer: return TEXT("No guarda líquidos");` |
| `Source/Explored/Carry/InventoryModel.cpp:185` | `Id de instancia repetido` | `case EInventoryFail::DuplicateId: return TEXT("Id de instancia repetido");` |
| `Source/Explored/Carry/InventoryModel.cpp:186` | `Estado incoherente` | `case EInventoryFail::CorruptState: return TEXT("Estado incoherente");` |
| `Source/Explored/Cooking/CookingModel.cpp:248` | `No hay nada que cocinar.` | `OutFailReason = TEXT("No hay nada que cocinar.");` |
| `Source/Explored/Cooking/CookingModel.cpp:253` | `Son demasiadas cosas a la vez.` | `OutFailReason = TEXT("Son demasiadas cosas a la vez.");` |
| `Source/Explored/Cooking/CookingModel.cpp:259` | `Eso no sirve para cocinar.` | `OutFailReason = TEXT("Eso no sirve para cocinar.");` |
| `Source/Explored/Cooking/CookingModel.cpp:264` | `No cabe tanto en %s.` | `OutFailReason = FString::Printf(TEXT("No cabe tanto en %s."), *Vessel->NameEs);` |
| `Source/Explored/Cooking/CookingModel.cpp:270` | `Hace falta un recipiente que no pierda agua.` | `OutFailReason = TEXT("Hace falta un recipiente que no pierda agua.");` |
| `Source/Explored/Cooking/CookingModel.cpp:280` | `Así no se puede %s.` | `OutFailReason = FString::Printf(TEXT("Así no se puede %s."), LexToString(Technique));` |
| `Source/Explored/Debug/ExploredPlaytestAuditor.cpp:71` | `-ExploredShots=` | `bStandalone = !FString(FCommandLine::Get()).Contains(TEXT("-ExploredShots="));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:614` | `stat unit` | `GEngine->Exec(World, TEXT("stat unit"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:615` | `stat gpu` | `GEngine->Exec(World, TEXT("stat gpu"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:616` | `stat rhi` | `GEngine->Exec(World, TEXT("stat rhi"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:617` | `stat streaming` | `GEngine->Exec(World, TEXT("stat streaming"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:622` | `r.Nanite.ShowStats 1` | `GEngine->Exec(World, TEXT("r.Nanite.ShowStats 1"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:650` | `memreport -full` | `GEngine->Exec(World, TEXT("memreport -full"));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:33` | `slot %s` | `Issue.Detail = FString::Printf(TEXT("slot %s"), *SlotName);` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:36` | `%s (slot %s): material nulo o por defecto en %d instancia(s), cerca de (%.0f, %.0f, %.0f) m` | `Issue.Summary = FString::Printf(TEXT("%s (slot %s): material nulo o por defecto en %d instancia(s), cerca de (%.0f, %.0f, %.0f) m"),` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:51` | `por debajo del terreno` | `Flags.Add(TEXT("por debajo del terreno"));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:55` | `por debajo del agua` | `Flags.Add(TEXT("por debajo del agua"));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:57` | `inicio (%.2f, %.2f, %.2f) m -> %.1f s (%.2f, %.2f, %.2f) m` | `Issue.Detail = FString::Printf(TEXT("inicio (%.2f, %.2f, %.2f) m -> %.1f s (%.2f, %.2f, %.2f) m"),` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:65` | `%s se movió solo %.1f m en %.0f s%s%s` | `Issue.Summary = FString::Printf(TEXT("%s se movió solo %.1f m en %.0f s%s%s"), *ActorName,` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:78` | `clearance %.1f cm` | `Issue.Detail = FString::Printf(TEXT("clearance %.1f cm"), ClearanceCm);` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:79` | `%s %s (%.1f cm) en (%.0f, %.0f, %.0f) m` | `Issue.Summary = FString::Printf(TEXT("%s %s (%.1f cm) en (%.0f, %.0f, %.0f) m"), *SubjectName,` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:80` | `flotando sobre el terreno` | `bFloating ? TEXT("flotando sobre el terreno") : TEXT("enterrado bajo el terreno"),` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:80` | `enterrado bajo el terreno` | `bFloating ? TEXT("flotando sobre el terreno") : TEXT("enterrado bajo el terreno"),` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:93` | `%s bloqueado por: %s` | `Issue.Summary = FString::Printf(TEXT("%s bloqueado por: %s"), *SpawnName, *BlockingActorsCsv);` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:111` | `\n` | `return In.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n"));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:118` | `  "issues": [\n` | `Out += TEXT(" \"issues\": [\n");` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:123` | `      "type": "%s",\n` | `Out += FString::Printf(TEXT(" \"type\": \"%s\",\n"), LexToString(Issue.Type));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:124` | `      "subject": "%s",\n` | `Out += FString::Printf(TEXT(" \"subject\": \"%s\",\n"), *EscapeJsonString(Issue.SubjectName));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:125` | `      "detail": "%s",\n` | `Out += FString::Printf(TEXT(" \"detail\": \"%s\",\n"), *EscapeJsonString(Issue.Detail));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:126` | `      "summary": "%s",\n` | `Out += FString::Printf(TEXT(" \"summary\": \"%s\",\n"), *EscapeJsonString(Issue.Summary));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:127` | `      "location_m": [%.3f, %.3f, %.3f],\n` | `Out += FString::Printf(TEXT(" \"location_m\": [%.3f, %.3f, %.3f],\n"),` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:129` | `      "instance_count": %d\n` | `Out += FString::Printf(TEXT(" \"instance_count\": %d\n"), Issue.InstanceCount);` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:134` | `  "frame_samples": [\n` | `Out += TEXT(" \"frame_samples\": [\n");` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:139` | `      "shot": "%s",\n` | `Out += FString::Printf(TEXT(" \"shot\": \"%s\",\n"), *EscapeJsonString(Sample.ShotName));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:140` | `      "avg_fps": %.1f,\n` | `Out += FString::Printf(TEXT(" \"avg_fps\": %.1f,\n"), Sample.AvgFPS);` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:141` | `      "min_fps": %.1f,\n` | `Out += FString::Printf(TEXT(" \"min_fps\": %.1f,\n"), Sample.MinFPS);` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:142` | `      "vram_mb": %.1f\n` | `Out += FString::Printf(TEXT(" \"vram_mb\": %.1f\n"), Sample.VRAMUsedMB);` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:147` | `  "bot_steps": [\n` | `Out += TEXT(" \"bot_steps\": [\n");` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:152` | `      "waypoint": "%s",\n` | `Out += FString::Printf(TEXT(" \"waypoint\": \"%s\",\n"), *EscapeJsonString(Step.WaypointName));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:153` | `      "location_m": [%.3f, %.3f, %.3f],\n` | `Out += FString::Printf(TEXT(" \"location_m\": [%.3f, %.3f, %.3f],\n"),` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:155` | `      "focused_actor": "%s",\n` | `Out += FString::Printf(TEXT(" \"focused_actor\": \"%s\",\n"), *EscapeJsonString(Step.FocusedActorName));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:156` | `      "interacted": %s,\n` | `Out += FString::Printf(TEXT(" \"interacted\": %s,\n"), Step.bInteracted ? TEXT("true") : TEXT("false"));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:157` | `      "inventory_delta": "%s"\n` | `Out += FString::Printf(TEXT(" \"inventory_delta\": \"%s\"\n"), *EscapeJsonString(Step.InventoryDelta));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:169` | `Informe de playtest automático — %d defecto(s), %d punto(s) de captura medido(s)\n\n` | `Out += FString::Printf(TEXT("Informe de playtest automático — %d defecto(s), %d punto(s) de captura medido(s)\n\n"),` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:190` | `Materiales nulos o por defecto` | `AppendSection(EPlaytestIssueType::MaterialMissing, TEXT("Materiales nulos o por defecto"));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:191` | `Física a la deriva` | `AppendSection(EPlaytestIssueType::PhysicsDrift, TEXT("Física a la deriva"));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:192` | `Flotando o enterrado` | `AppendSection(EPlaytestIssueType::FloatingOrBuried, TEXT("Flotando o enterrado"));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:193` | `Puntos de aparición bloqueados` | `AppendSection(EPlaytestIssueType::SpawnBlocked, TEXT("Puntos de aparición bloqueados"));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:195` | `== Rendimiento por punto de captura ==\n` | `Out += TEXT("== Rendimiento por punto de captura ==\n");` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:198` | `- %s: %.1f fps medios, %.1f fps mínimos, %.1f MB VRAM\n` | `Out += FString::Printf(TEXT("- %s: %.1f fps medios, %.1f fps mínimos, %.1f MB VRAM\n"),` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:203` | `== Bot de juego (%d paso(s)) ==\n` | `Out += FString::Printf(TEXT("== Bot de juego (%d paso(s)) ==\n"), Report.BotSteps.Num());` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:206` | `- %s en (%.0f, %.0f, %.0f) m: foco=%s, interactuó=%s, inventario: %s\n` | `Out += FString::Printf(TEXT("- %s en (%.0f, %.0f, %.0f) m: foco=%s, interactuó=%s, inventario: %s\n"),` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:209` | `sí` | `Step.bInteracted ? TEXT("sí") : TEXT("no"),` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:210` | `sin cambios` | `Step.InventoryDelta.IsEmpty() ? TEXT("sin cambios") : *Step.InventoryDelta);` |
| `Source/Explored/Exploration/ExplorationContentModel.cpp:73` | `<sin id>` | `const FString Tag = L.Id.IsNone() ? TEXT("<sin id>") : L.Id.ToString();` |
| `Source/Explored/Fishing/FishingModel.cpp:70` | `Pez loro` | `FFishSpecies S = MakeSpecies(TEXT("pez_loro"), TEXT("Pez loro"), FishBit(EHab::Reef), Rod \| Spear \| Net \| Trap,` |
| `Source/Explored/Fishing/FishingModel.cpp:80` | `Pez cirujano` | `FFishSpecies S = MakeSpecies(TEXT("pez_cirujano"), TEXT("Pez cirujano"), FishBit(EHab::Reef) \| FishBit(EHab::Lagoon),` |
| `Source/Explored/Fishing/FishingModel.cpp:124` | `Pez ballesta` | `FFishSpecies S = MakeSpecies(TEXT("pez_ballesta"), TEXT("Pez ballesta"), FishBit(EHab::Reef), Rod \| Spear \| Trap,` |
| `Source/Explored/Fishing/FishingModel.cpp:182` | `Atún` | `FFishSpecies S = MakeSpecies(TEXT("atun"), TEXT("Atún"), FishBit(EHab::Deep), Rod,` |
| `Source/Explored/Fishing/FishingModel.cpp:196` | `Langosta de arrecife` | `FFishSpecies S = MakeSpecies(TEXT("langosta"), TEXT("Langosta de arrecife"), FishBit(EHab::Reef), Spear \| Trap \| Hand,` |
| `Source/Explored/Fishing/FishingModel.cpp:233` | `El Viejo` | `FLegendaryCatch L = MakeLegend(TEXT("el_viejo"), TEXT("El Viejo"), TEXT("cueva_arenas_blancas"), Rod,` |
| `Source/Explored/Fishing/FishingModel.cpp:254` | `La Manta Negra` | `FLegendaryCatch L = MakeLegend(TEXT("manta_negra"), TEXT("La Manta Negra"), TEXT("bajios_manglar"), Spear,` |
| `Source/Explored/Fishing/FishingModel.cpp:263` | `El Errante` | `FLegendaryCatch L = MakeLegend(TEXT("el_errante"), TEXT("El Errante"), TEXT("arrecife_arenas_blancas"), Spear \| Rod,` |
| `Source/Explored/Fishing/FishingModel.cpp:271` | `El Rey de Plata` | `FLegendaryCatch L = MakeLegend(TEXT("rey_de_plata"), TEXT("El Rey de Plata"), TEXT("mar_abierto"), Rod,` |
| `Source/Explored/Items/ItemTypes.cpp:7` | `Pequeño` | `case EItemSize::Pequeno: return TEXT("Pequeño");` |
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

## Errores (0)

Ninguno.

## Avisos (25)

- translations/en.json: Explored,Carry_NoRegistry no aparece en el código (¿clave renombrada o borrada?)
- Source/Explored/Building/BuildingSubsystem.cpp:277: ExploredBuilding,Occupied: el inglés (26 car.) es más de 1.3× el español (15); comprueba que cabe
- Source/Explored/Building/BuildingSubsystem.cpp:281: ExploredBuilding,MissingRequiredPiece: el inglés (36 car.) es más de 1.3× el español (27); comprueba que cabe
- Source/Explored/Carry/CarryComponent.cpp:111: Explored,Carry_AlreadyThere: el inglés (19 car.) es más de 1.3× el español (12); comprueba que cabe
- Source/Explored/Carry/CarryComponent.cpp:153: Explored,Carry_NoRoom: el inglés (13 car.) es más de 1.3× el español (9); comprueba que cabe
- Source/Explored/Carry/CarryComponent.cpp:160: Explored,Carry_OverCarryLimit: el inglés (32 car.) es más de 1.3× el español (23); comprueba que cabe
- Source/Explored/Carry/CarryComponent.cpp:162: Explored,Carry_Generic: el inglés (18 car.) es más de 1.3× el español (12); comprueba que cabe
- Source/Explored/Fishing/ExploredTrap.cpp:132: Explored,Verb_TidePool: el inglés (15 car.) es más de 1.3× el español (10); comprueba que cabe
- Source/Explored/UI/Widgets/SExploredAchievements.cpp:105: ExploredUI,AchievementsTitle: el inglés (12 car.) es más de 1.3× el español (6); comprueba que cabe
- Source/Explored/UI/Widgets/SExploredMainMenu.cpp:79: ExploredUI,Achievements: el inglés (12 car.) es más de 1.3× el español (6); comprueba que cabe
- Source/Explored/UI/Widgets/SExploredSettingsPanel.cpp:312: ExploredUI,ViewDistance: el inglés (13 car.) es más de 1.3× el español (5); comprueba que cabe
- Source/Explored/UI/Widgets/SExploredSettingsPanel.cpp:316: ExploredUI,PostProcess: el inglés (15 car.) es más de 1.3× el español (11); comprueba que cabe
- Content/Data/items.json «palo_recto» nameEs/nameEn: Data.items.items,palo_recto.nameEs: el inglés (14 car.) es más de 1.3× el español (10); comprueba que cabe
- Content/Data/items.json «bonito» nameEs/nameEn: Data.items.items,bonito.nameEs: el inglés (13 car.) es más de 1.3× el español (6); comprueba que cabe
- Content/Data/items.json «atun» nameEs/nameEn: Data.items.items,atun.nameEs: el inglés (14 car.) es más de 1.3× el español (4); comprueba que cabe
- Content/Data/items.json «batata» nameEs/nameEn: Data.items.items,batata.nameEs: el inglés (12 car.) es más de 1.3× el español (6); comprueba que cabe
- Content/Data/items.json «maracuya» nameEs/nameEn: Data.items.items,maracuya.nameEs: el inglés (13 car.) es más de 1.3× el español (8); comprueba que cabe
- Content/Data/items.json «yuca» nameEs/nameEn: Data.items.items,yuca.nameEs: el inglés (12 car.) es más de 1.3× el español (4); comprueba que cabe
- Content/Data/items.json «batata_asada» nameEs/nameEn: Data.items.items,batata_asada.nameEs: el inglés (18 car.) es más de 1.3× el español (12); comprueba que cabe
- Content/Data/items.json «lasca_tallada» nameEs/nameEn: Data.items.items,lasca_tallada.nameEs: el inglés (13 car.) es más de 1.3× el español (5); comprueba que cabe
- Content/Data/plants.json «platanera» nameEs/nameEn: Data.plants.plants,platanera.nameEs: el inglés (12 car.) es más de 1.3× el español (9); comprueba que cabe
- Content/Data/plants.json «batata» nameEs/nameEn: Data.plants.plants,batata.nameEs: el inglés (12 car.) es más de 1.3× el español (6); comprueba que cabe
- Content/Data/plants.json «maracuya» nameEs/nameEn: Data.plants.plants,maracuya.nameEs: el inglés (13 car.) es más de 1.3× el español (8); comprueba que cabe
- Content/Data/building_pieces.json «vitrina_museo» nameEs/nameEn: Data.building_pieces.pieces,vitrina_museo.nameEs: el inglés (12 car.) es más de 1.3× el español (7); comprueba que cabe
- Content/Data/story_es.json «6» petroglyph_themes/petroglyph_themes_en: Data.story_es.petroglyph_themes,06: el inglés (21 car.) es más de 1.3× el español (16); comprueba que cabe
