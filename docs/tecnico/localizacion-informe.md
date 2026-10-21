# Informe de localización

Generado por `Tools/Localization` (`uv run l10n export`); no se edita a mano.
La guía está en [`localizacion.md`](localizacion.md).

## Recuento

| Concepto | Número |
|---|---|
| Textos en el catálogo | 350 |
| … del C++ y los .ini (van al manifiesto de Unreal) | 242 |
| … de `Content/Data` (campos bilingües) | 108 |
| Textos sin inglés | 0 |
| Claves propuestas pendientes de integrar | 7 |
| Literales sin localizar | 0 |
| Literales invariantes | 3 |
| Literales para revisar | 179 |
| Errores / avisos | 0 / 0 |

## Literales del C++

### Literal (0)

Literales con letras que llegan a la pantalla: pasar a `NSLOCTEXT`/`LOCTEXT` (o `FText::Format`).

### Invariante (3)

Sin letras (números, símbolos, separadores): `FText::AsCultureInvariant` o `FText::AsNumber`, sin traducir.

| Fichero:línea | Texto | Código |
|---|---|---|
| `Source/Explored/Debug/PlaytestReportModel.cpp:182` | `== %s (%d) ==\n` | `Out += FString::Printf(TEXT("== %s (%d) ==\n"), Title, Matching.Num());` |
| `Source/Explored/UI/Widgets/SExploredSettingsPanel.cpp:254` | `%d x %d` | `ResolutionLabels.Add(FText::AsCultureInvariant(FString::Printf(TEXT("%d x %d"), R.X, R.Y)));` |
| `Source/Explored/WorldGen/ExploredPlaceholder.cpp:95` | `%s\n%s` | `Label->SetText(FText::FromString(FString::Printf(TEXT("%s\n%s"), *Prop.Type.ToString(), *Prop.Id.ToString())));` |

### Revisar (179)

Parecen prosa pero no se ve cómo llegan a la UI: comprobar a mano.

| Fichero:línea | Texto | Código |
|---|---|---|
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
| `Source/Explored/Crafting/RecipeBookModel.cpp:94` | `Caña de pescar` | `Book.Recipes.Add(Make(TEXT("cana_pescar"), TEXT("Caña de pescar"), TEXT("Fishing rod"), TEXT("atar"),` |
| `Source/Explored/Crafting/RecipeBookModel.cpp:94` | `Fishing rod` | `Book.Recipes.Add(Make(TEXT("cana_pescar"), TEXT("Caña de pescar"), TEXT("Fishing rod"), TEXT("atar"),` |
| `Source/Explored/Crafting/RecipeBookModel.cpp:96` | `Pez asado` | `Book.Recipes.Add(Make(TEXT("pez_asado"), TEXT("Pez asado"), TEXT("Cooked fish"), TEXT("cocinar"), { { TEXT("pez_crudo"), 1 } },` |
| `Source/Explored/Crafting/RecipeBookModel.cpp:96` | `Cooked fish` | `Book.Recipes.Add(Make(TEXT("pez_asado"), TEXT("Pez asado"), TEXT("Cooked fish"), TEXT("cocinar"), { { TEXT("pez_crudo"), 1 } },` |
| `Source/Explored/Crafting/RecipeBookModel.cpp:100` | `Tarro de aceite` | `Book.Recipes.Add(Make(TEXT("tarro_aceite"), TEXT("Tarro de aceite"), TEXT("Oil jar"), TEXT("hervir"),` |
| `Source/Explored/Crafting/RecipeBookModel.cpp:100` | `Oil jar` | `Book.Recipes.Add(Make(TEXT("tarro_aceite"), TEXT("Tarro de aceite"), TEXT("Oil jar"), TEXT("hervir"),` |
| `Source/Explored/Crafting/RecipeBookModel.cpp:109` | `Linterna montada` | `FRecipeDef Lantern = Make(TEXT("linterna"), TEXT("Linterna montada"), TEXT("Mounted lantern"), TEXT("montar"),` |
| `Source/Explored/Crafting/RecipeBookModel.cpp:109` | `Mounted lantern` | `FRecipeDef Lantern = Make(TEXT("linterna"), TEXT("Linterna montada"), TEXT("Mounted lantern"), TEXT("montar"),` |
| `Source/Explored/Crafting/RecipeBookModel.cpp:114` | `Baliza encendida` | `FRecipeDef Beacon = Make(TEXT("baliza"), TEXT("Baliza encendida"), TEXT("Beacon lit"), TEXT("encender"), {},` |
| `Source/Explored/Crafting/RecipeBookModel.cpp:114` | `Beacon lit` | `FRecipeDef Beacon = Make(TEXT("baliza"), TEXT("Baliza encendida"), TEXT("Beacon lit"), TEXT("encender"), {},` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:115` | `empieza en el prólogo con objetivo` | `Add(TEXT("empieza en el prólogo con objetivo"), [] { return true; },` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:117` | `leer la página 1` | `Add(TEXT("leer la página 1"), [this]() { return Interact(TEXT("pagina_1")); },` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:119` | `BalizaStage Preparacion` | `Add(TEXT("BalizaStage Preparacion"), [this]() { return Console(TEXT("BalizaStage Preparacion")); },` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:119` | `BalizaStage Preparacion` | `Add(TEXT("BalizaStage Preparacion"), [this]() { return Console(TEXT("BalizaStage Preparacion")); },` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:121` | `BalizaGive tarros, mecha y lente` | `Add(TEXT("BalizaGive tarros, mecha y lente"), [this]()` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:123` | `BalizaGive tarro_aceite 2` | `return Console(TEXT("BalizaGive tarro_aceite 2")) && Console(TEXT("BalizaGive mecha 1")) && Console(TEXT("BalizaGive lente 1"));` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:123` | `BalizaGive mecha 1` | `return Console(TEXT("BalizaGive tarro_aceite 2")) && Console(TEXT("BalizaGive mecha 1")) && Console(TEXT("BalizaGive lente 1"));` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:123` | `BalizaGive lente 1` | `return Console(TEXT("BalizaGive tarro_aceite 2")) && Console(TEXT("BalizaGive mecha 1")) && Console(TEXT("BalizaGive lente 1"));` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:130` | `subir a la linterna` | `Add(TEXT("subir a la linterna"), [this]() { return Interact(TEXT("faro_torre"), false); },` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:136` | `poner la lente` | `for (const TCHAR* SlotName : { TEXT("poner la lente"), TEXT("verter los tarros"), TEXT("apretar la mecha") })` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:136` | `verter los tarros` | `for (const TCHAR* SlotName : { TEXT("poner la lente"), TEXT("verter los tarros"), TEXT("apretar la mecha") })` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:136` | `apretar la mecha` | `for (const TCHAR* SlotName : { TEXT("poner la lente"), TEXT("verter los tarros"), TEXT("apretar la mecha") })` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:140` | `linterna montada` | `Add(TEXT("linterna montada"), [] { return true; },` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:142` | `guardar en manual3` | `Add(TEXT("guardar en manual3"), [this]()` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:149` | `cargar manual3 tras volver al prólogo` | `Add(TEXT("cargar manual3 tras volver al prólogo"), [this, Stage]()` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:151` | `BalizaStage Prologo` | `Console(TEXT("BalizaStage Prologo"));` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:161` | `partida del formato 1 rechazada sin romper nada` | `Add(TEXT("partida del formato 1 rechazada sin romper nada"), [this]()` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:183` | `leer la página 8` | `Add(TEXT("leer la página 8"), [this]() { return Interact(TEXT("pagina_8"), false); },` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:185` | `BalizaTime 3 21.5` | `Add(TEXT("BalizaTime 3 21.5"), [this]() { return Console(TEXT("BalizaTime 3 21.5")); }, [] { return true; });` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:185` | `BalizaTime 3 21.5` | `Add(TEXT("BalizaTime 3 21.5"), [this]() { return Console(TEXT("BalizaTime 3 21.5")); }, [] { return true; });` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:186` | `encender la baliza` | `Add(TEXT("encender la baliza"), [this]() { return Interact(TEXT("faro_linterna"), false); },` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:188` | `luces del Mistral` | `Add(TEXT("luces del Mistral"), [] { return true; },` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:190` | `pantalla de epílogo` | `Add(TEXT("pantalla de epílogo"), [] { return true; },` |
| `Source/Explored/Debug/ExploredBeaconSmokeTest.cpp:196` | `créditos del final` | `Add(TEXT("créditos del final"), [this]()` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:101` | `kit inicial` | `Add(TEXT("kit inicial"), [this]() { return Count(TEXT("cuchillo")) == 1 && Count(TEXT("cantimplora")) == 1 && Count(TEXT("barometro")) == 1; },` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:103` | `recoger leña 1` | `Add(TEXT("recoger leña 1"), [this]() { return Interact(TEXT("lena_cala_1")); }, [this]() { return Idle() && Count(TEXT("lena")) == 2; });` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:104` | `recoger leña 2` | `Add(TEXT("recoger leña 2"), [this]() { return Interact(TEXT("lena_cala_2")); }, [this]() { return Idle() && Count(TEXT("lena")) == 4; });` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:105` | `recoger fibra 1` | `Add(TEXT("recoger fibra 1"), [this]() { return Interact(TEXT("fibra_palmar_1")); }, [this]() { return Idle() && Count(TEXT("fibra")) == 1; });` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:106` | `recoger fibra 2` | `Add(TEXT("recoger fibra 2"), [this]() { return Interact(TEXT("fibra_palmar_2")); }, [this]() { return Idle() && Count(TEXT("fibra")) == 2; });` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:107` | `trenzar cuerda` | `Add(TEXT("trenzar cuerda"), [this]()` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:113` | `recoger caña de bambú` | `Add(TEXT("recoger caña de bambú"), [this]() { return Interact(TEXT("cana_bambu_1")); }, [this]() { return Idle() && Count(TEXT("cana_bambu")) == 1; });` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:114` | `atar caña de pescar` | `Add(TEXT("atar caña de pescar"), [this]()` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:120` | `pescar un pez` | `Add(TEXT("pescar un pez"), [this]() { return Interact(TEXT("punto_pesca_1")); },` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:130` | `encender la hoguera del faro` | `Add(TEXT("encender la hoguera del faro"), [FireAction]() { return FireAction(TEXT("encender")); },` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:136` | `asar el pez` | `Add(TEXT("asar el pez"), [FireAction]() { return FireAction(TEXT("cocinar")); },` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:142` | `recoger el pez asado` | `Add(TEXT("recoger el pez asado"), [FireAction]() { return FireAction(TEXT("recoger")); }, [this]() { return Count(TEXT("pez_asado")) == 1; });` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:143` | `comer el pez asado` | `Add(TEXT("comer el pez asado"), [this]()` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:149` | `avivar con leña` | `Add(TEXT("avivar con leña"), [FireAction]() { return FireAction(TEXT("avivar")); }, [this]() { return Count(TEXT("lena")) == 0; });` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:150` | `bajar y beber un coco` | `Add(TEXT("bajar y beber un coco"), [this]() { return Interact(TEXT("palma_cala_1")); },` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:160` | `abrir la caja de suministros` | `Add(TEXT("abrir la caja de suministros"), [this]() { return Interact(TEXT("caja_suministros_faro")); },` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:166` | `llenar la cantimplora en el barril` | `Add(TEXT("llenar la cantimplora en el barril"), [this]()` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:182` | `beber de la cantimplora` | `Add(TEXT("beber de la cantimplora"), [this]()` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:192` | `combinación que no es receta` | `Add(TEXT("combinación que no es receta"), [this]()` |
| `Source/Explored/Debug/ExploredPlaytestAuditor.cpp:71` | `-ExploredShots=` | `bStandalone = !FString(FCommandLine::Get()).Contains(TEXT("-ExploredShots="));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:483` | `stat unit` | `GEngine->Exec(World, TEXT("stat unit"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:484` | `stat gpu` | `GEngine->Exec(World, TEXT("stat gpu"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:485` | `stat rhi` | `GEngine->Exec(World, TEXT("stat rhi"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:486` | `stat streaming` | `GEngine->Exec(World, TEXT("stat streaming"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:491` | `r.Nanite.ShowStats 1` | `GEngine->Exec(World, TEXT("r.Nanite.ShowStats 1"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:519` | `memreport -full` | `GEngine->Exec(World, TEXT("memreport -full"));` |
| `Source/Explored/Debug/ExploredWalkProbe.cpp:160` | `{\n  "status": "%s",\n  "start_zone": "%s",\n  "start_m": [%.2f, %.2f, %.2f],\n  "end_m": [%.2f, %.2f, %.2f],\n` | `TEXT("{\n \"status\": \"%s\",\n \"start_zone\": \"%s\",\n \"start_m\": [%.2f, %.2f, %.2f],\n \"end_m\": [%.2f, %.2f, %.2f],\n")` |
| `Source/Explored/Debug/ExploredWalkProbe.cpp:161` | `  "waypoints_reached": %d,\n  "waypoints": %d,\n  "elapsed_s": %.1f,\n  "travelled_m": %.1f,\n  "max_height_m": %.2f\n}\n` | `TEXT(" \"waypoints_reached\": %d,\n \"waypoints\": %d,\n \"elapsed_s\": %.1f,\n \"travelled_m\": %.1f,\n \"max_height_m\": %.2f\n}\n"),` |
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
| `Source/Explored/Fishing/FishingModel.cpp:76` | `Pez loro` | `FFishSpecies S = MakeSpecies(TEXT("pez_loro"), TEXT("Pez loro"), FishBit(EHab::Reef), Rod \| Spear \| Net \| Trap,` |
| `Source/Explored/Fishing/FishingModel.cpp:86` | `Pez cirujano` | `FFishSpecies S = MakeSpecies(TEXT("pez_cirujano"), TEXT("Pez cirujano"), FishBit(EHab::Reef) \| FishBit(EHab::Lagoon),` |
| `Source/Explored/Fishing/FishingModel.cpp:130` | `Pez ballesta` | `FFishSpecies S = MakeSpecies(TEXT("pez_ballesta"), TEXT("Pez ballesta"), FishBit(EHab::Reef), Rod \| Spear \| Trap,` |
| `Source/Explored/Fishing/FishingModel.cpp:188` | `Atún` | `FFishSpecies S = MakeSpecies(TEXT("atun"), TEXT("Atún"), FishBit(EHab::Deep), Rod,` |
| `Source/Explored/Fishing/FishingModel.cpp:202` | `Langosta de arrecife` | `FFishSpecies S = MakeSpecies(TEXT("langosta"), TEXT("Langosta de arrecife"), FishBit(EHab::Reef), Spear \| Trap \| Hand,` |
| `Source/Explored/Fishing/FishingModel.cpp:239` | `El Viejo` | `FLegendaryCatch L = MakeLegend(TEXT("el_viejo"), TEXT("El Viejo"), TEXT("cueva_arenas_blancas"), Rod,` |
| `Source/Explored/Fishing/FishingModel.cpp:260` | `La Manta Negra` | `FLegendaryCatch L = MakeLegend(TEXT("manta_negra"), TEXT("La Manta Negra"), TEXT("bajios_manglar"), Spear,` |
| `Source/Explored/Fishing/FishingModel.cpp:269` | `El Errante` | `FLegendaryCatch L = MakeLegend(TEXT("el_errante"), TEXT("El Errante"), TEXT("arrecife_arenas_blancas"), Spear \| Rod,` |
| `Source/Explored/Fishing/FishingModel.cpp:277` | `El Rey de Plata` | `FLegendaryCatch L = MakeLegend(TEXT("rey_de_plata"), TEXT("El Rey de Plata"), TEXT("mar_abierto"), Rod,` |
| `Source/Explored/Items/IslandDataLoader.cpp:30` | `%s: JSON mal formado o sin objeto raíz` | `Errors.Add(FString::Printf(TEXT("%s: JSON mal formado o sin objeto raíz"), File));` |
| `Source/Explored/Items/IslandDataLoader.cpp:67` | `items.json: JSON mal formado o sin lista raíz` | `Errors.Add(TEXT("items.json: JSON mal formado o sin lista raíz"));` |
| `Source/Explored/Items/IslandDataLoader.cpp:78` | `items.json: objeto sin id` | `Errors.Add(TEXT("items.json: objeto sin id"));` |
| `Source/Explored/Items/IslandDataLoader.cpp:155` | `recipes.json: falta la lista «recipes»` | `Errors.Add(TEXT("recipes.json: falta la lista «recipes»"));` |
| `Source/Explored/Items/IslandDataLoader.cpp:167` | `recipes.json: receta incompleta (id, verb, place y output)` | `Errors.Add(TEXT("recipes.json: receta incompleta (id, verb, place y output)"));` |
| `Source/Explored/Items/IslandDataLoader.cpp:174` | `recipes.json: sin recetas` | `Errors.Add(TEXT("recipes.json: sin recetas"));` |
| `Source/Explored/Items/IslandDataLoader.cpp:179` | `recipes.json: %s` | `Errors.Add(FString::Printf(TEXT("recipes.json: %s"), *Error));` |
| `Source/Explored/Items/ItemTypes.cpp:7` | `Pequeño` | `case EItemSize::Pequeno: return TEXT("Pequeño");` |
| `Source/Explored/Narrative/BeaconSubsystem.cpp:148` | `Pone la historia de Baliza Siete en una etapa: BalizaStage <nombre o 0-9>` | `TEXT("Pone la historia de Baliza Siete en una etapa: BalizaStage <nombre o 0-9>"),` |
| `Source/Explored/Narrative/BeaconSubsystem.cpp:151` | `Mete objetos en la mochila de Iria: BalizaGive <id> <n>` | `TEXT("Mete objetos en la mochila de Iria: BalizaGive <id> <n>"),` |
| `Source/Explored/Narrative/BeaconSubsystem.cpp:154` | `Pone el reloj: BalizaTime <día> <hora>` | `TEXT("Pone el reloj: BalizaTime <día> <hora>"),` |
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
| `Source/Explored/WorldGen/IslandLayout.cpp:168` | `Cala Varada` | `AddZone(TEXT("cala_varada"), TEXT("Cala Varada"), EIslandArchetype::Landing, FVector2D(-100.0f, 10.0f), 35.0f, 0.0f);` |
| `Source/Explored/WorldGen/IslandLayout.cpp:170` | `Rocas del Pescador` | `AddZone(TEXT("rocas_pescador"), TEXT("Rocas del Pescador"), EIslandArchetype::Teeth, FVector2D(-15.0f, -140.0f), 30.0f, 2.0f);` |
| `Source/Explored/WorldGen/IslandLayout.cpp:171` | `Bosque de Bambú` | `AddZone(TEXT("bosque_bambu"), TEXT("Bosque de Bambú"), EIslandArchetype::Emerald, FVector2D(55.0f, -85.0f), 42.0f, 5.0f);` |
| `Source/Explored/WorldGen/IslandLayout.cpp:172` | `Playa Norte` | `AddZone(TEXT("playa_norte"), TEXT("Playa Norte"), EIslandArchetype::WhiteSands, FVector2D(110.0f, 55.0f), 28.0f, 0.0f);` |
| `Source/Explored/WorldGen/IslandLayout.cpp:173` | `Colina del Faro` | `AddZone(TEXT("colina_faro"), TEXT("Colina del Faro"), EIslandArchetype::Mesa, FVector2D(45.0f, 15.0f), 47.0f, 32.5f, 42.0f);` |
| `Source/Explored/WorldGen/IslandLayout.cpp:174` | `Isla del Carguero` | `AddZone(TEXT("isla_carguero"), TEXT("Isla del Carguero"), EIslandArchetype::Teeth, FVector2D(232.0f, 150.0f), 32.0f, 0.0f);` |

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
