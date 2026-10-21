# Informe de localización

Generado por `Tools/Localization` (`uv run l10n export`); no se edita a mano.
La guía está en [`localizacion.md`](localizacion.md).

## Recuento

| Concepto | Número |
|---|---|
| Textos en el catálogo | 385 |
| … del C++ y los .ini (van al manifiesto de Unreal) | 260 |
| … de `Content/Data` (campos bilingües) | 125 |
| Textos sin inglés | 0 |
| Claves propuestas pendientes de integrar | 7 |
| Literales sin localizar | 2 |
| Literales invariantes | 4 |
| Literales para revisar | 362 |
| Errores / avisos | 0 / 2 |

## Literales del C++

### Literal (2)

Literales con letras que llegan a la pantalla: pasar a `NSLOCTEXT`/`LOCTEXT` (o `FText::Format`).

| Fichero:línea | Texto | Código |
|---|---|---|
| `Source/Explored/Debug/ExploredPlaytestBotRun.cpp:238` | `ir a ` | `AddStep(TEXT("ir a ") + Label, [this, Target]() { return TeleportNear(Target(), true); }, [this]() { return TravelLeft1x <= 0.0; },` |
| `Source/Explored/Debug/ExploredPlaytestBotRun.cpp:341` | `remar hasta ` | `AddStep(TEXT("remar hasta ") + Label, [Raft]() { return Raft() && Raft()->IsOccupied(); }, [this, Raft, Target, ArriveMeters]()` |

### Invariante (4)

Sin letras (números, símbolos, separadores): `FText::AsCultureInvariant` o `FText::AsNumber`, sin traducir.

| Fichero:línea | Texto | Código |
|---|---|---|
| `Source/Explored/Debug/ExploredPlaytestBotRun.cpp:51` | `%s: %s` | `Errors.Add(FString::Printf(TEXT("%s: %s"), *Category.ToString(), Message));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:184` | `== %s (%d) ==\n` | `Out += FString::Printf(TEXT("== %s (%d) ==\n"), Title, Matching.Num());` |
| `Source/Explored/UI/Widgets/SExploredSettingsPanel.cpp:252` | `%d x %d` | `ResolutionLabels.Add(FText::AsCultureInvariant(FString::Printf(TEXT("%d x %d"), R.X, R.Y)));` |
| `Source/Explored/WorldGen/ExploredPlaceholder.cpp:95` | `%s\n%s` | `Label->SetText(FText::FromString(FString::Printf(TEXT("%s\n%s"), *Prop.Type.ToString(), *Prop.Id.ToString())));` |

### Revisar (362)

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
| `Source/Explored/Debug/BalizaDebugCommands.cpp:108` | `Acelera el reloj de Baliza Siete: BalizaTimeScale <x>` | `TEXT("Acelera el reloj de Baliza Siete: BalizaTimeScale <x>"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdTimeScale));` |
| `Source/Explored/Debug/BalizaDebugCommands.cpp:110` | `Pone las barras de Iria: BalizaBody <sed> <hambre> <energía> <calor>` | `TEXT("Pone las barras de Iria: BalizaBody <sed> <hambre> <energía> <calor>"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdBody));` |
| `Source/Explored/Debug/BalizaDebugCommands.cpp:112` | `Intenta dormir (refugio, 20:00-05:00)` | `TEXT("Intenta dormir (refugio, 20:00-05:00)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdSleep));` |
| `Source/Explored/Debug/BalizaDebugCommands.cpp:114` | `Desmayo inmediato de Iria` | `TEXT("Desmayo inmediato de Iria"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdFaint));` |
| `Source/Explored/Debug/BalizaDebugCommands.cpp:116` | `Deja la balsa hecha: BalizaRaft [board]` | `TEXT("Deja la balsa hecha: BalizaRaft [board]"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdRaft));` |
| `Source/Explored/Debug/BeaconSoftlockModel.cpp:33` | `%s: faltan %d` | `Missing.Add(FString::Printf(TEXT("%s: faltan %d"), What, Need - Have));` |
| `Source/Explored/Debug/BeaconSoftlockModel.cpp:201` | `lente: el mar ya no vuelve a abrirse` | `Verdict.Missing.Add(TEXT("lente: el mar ya no vuelve a abrirse"));` |
| `Source/Explored/Debug/BeaconSoftlockModel.cpp:229` | `ruta competente: inicio de la partida` | `Out.Add({ TEXT("ruta competente: inicio de la partida"), Snapshot(Kit, Island, 7.0) });` |
| `Source/Explored/Debug/BeaconSoftlockModel.cpp:231` | `orden: mecha y aceite el día 1, balsa después` | `FSoftlockScenario Early{ TEXT("orden: mecha y aceite el día 1, balsa después"),` |
| `Source/Explored/Debug/BeaconSoftlockModel.cpp:238` | `orden: balsa y lente primero, aceite y mecha el día 3` | `FSoftlockScenario LensFirst{ TEXT("orden: balsa y lente primero, aceite y mecha el día 3"),` |
| `Source/Explored/Debug/BeaconSoftlockModel.cpp:245` | `página 1 sin leer y sin lente al cerrarse el mar (D2 19:00)` | `FSoftlockScenario NoPage{ TEXT("página 1 sin leer y sin lente al cerrarse el mar (D2 19:00)"),` |
| `Source/Explored/Debug/BeaconSoftlockModel.cpp:252` | `agotados: toda la leña quemada en hogueras (caja)` | `FSoftlockScenario NoWood{ TEXT("agotados: toda la leña quemada en hogueras (caja)"),` |
| `Source/Explored/Debug/BeaconSoftlockModel.cpp:260` | `agotados: fibras en dos cañas de pescar de más` | `FSoftlockScenario Fibers{ TEXT("agotados: fibras en dos cañas de pescar de más"),` |
| `Source/Explored/Debug/BeaconSoftlockModel.cpp:264` | `agotados: los dos tarros quemados como combustible` | `FSoftlockScenario Burnt{ TEXT("agotados: los dos tarros quemados como combustible"),` |
| `Source/Explored/Debug/BeaconSoftlockModel.cpp:273` | `agotados: cocos de la isla bebidos (vacíos en la mochila)` | `FSoftlockScenario Coconuts{ TEXT("agotados: cocos de la isla bebidos (vacíos en la mochila)"),` |
| `Source/Explored/Debug/BeaconSoftlockModel.cpp:306` | `%s → %s → %s, página 1 %s, lente %s del ciclón` | `Failures.Add(FString::Printf(TEXT("%s → %s → %s, página 1 %s, lente %s del ciclón"), TaskName(Order[0]),` |
| `Source/Explored/Debug/BeaconSoftlockModel.cpp:307` | `leída` | `TaskName(Order[1]), TaskName(Order[2]), bPageOne ? TEXT("leída") : TEXT("sin leer"),` |
| `Source/Explored/Debug/BeaconSoftlockModel.cpp:307` | `sin leer` | `TaskName(Order[1]), TaskName(Order[2]), bPageOne ? TEXT("leída") : TEXT("sin leer"),` |
| `Source/Explored/Debug/BeaconSoftlockModel.cpp:308` | `después` | `bLensEarly ? TEXT("antes") : TEXT("después")));` |
| `Source/Explored/Debug/BeaconSoftlockModel.cpp:323` | `la caja no hace falta` | `const FString Crate = Parts.IsEmpty() ? FString(TEXT("la caja no hace falta")) : TEXT("la caja da ") + FString::Join(Parts, TEXT(", "));` |
| `Source/Explored/Debug/BeaconSoftlockModel.cpp:323` | `la caja da ` | `const FString Crate = Parts.IsEmpty() ? FString(TEXT("la caja no hace falta")) : TEXT("la caja da ") + FString::Join(Parts, TEXT(", "));` |
| `Source/Explored/Debug/BeaconSoftlockModel.cpp:324` | `faltan: ` | `return Verdict.bCompletable ? Crate : TEXT("faltan: ") + FString::Join(Verdict.Missing, TEXT("; ")) + TEXT(" (") + Crate + TEXT(")");` |
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
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:124` | `reloj D1 07:00 con días de 24 min` | `Add(TEXT("reloj D1 07:00 con días de 24 min"), [] { return true; }, [this]()` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:129` | `una barra bajo 25 frena un 20 %` | `Add(TEXT("una barra bajo 25 frena un 20 %"), [this]() { return Console(TEXT("BalizaBody 20 70 70 100")); }, [this]()` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:129` | `BalizaBody 20 70 70 100` | `Add(TEXT("una barra bajo 25 frena un 20 %"), [this]() { return Console(TEXT("BalizaBody 20 70 70 100")); }, [this]()` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:134` | `delante de una palma con cocos en la mochila` | `Add(TEXT("delante de una palma con cocos en la mochila"), [this]()` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:149` | `BalizaGive coco 2` | `return Console(TEXT("BalizaGive coco 2")) && Console(TEXT("BalizaGive fibra 3"));` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:149` | `BalizaGive fibra 3` | `return Console(TEXT("BalizaGive coco 2")) && Console(TEXT("BalizaGive fibra 3"));` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:151` | `captura f3d-hud` | `Add(TEXT("captura f3d-hud"), [this]() { return Shot(TEXT("f3d-hud")); },` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:153` | `chubasco guiado D1 15:00` | `Add(TEXT("chubasco guiado D1 15:00"), [this]() { return Console(TEXT("BalizaBody 70 70 70 100")) && Console(TEXT("BalizaTime 1 15.1")); },` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:153` | `BalizaBody 70 70 70 100` | `Add(TEXT("chubasco guiado D1 15:00"), [this]() { return Console(TEXT("BalizaBody 70 70 70 100")) && Console(TEXT("BalizaTime 1 15.1")); },` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:153` | `BalizaTime 1 15.1` | `Add(TEXT("chubasco guiado D1 15:00"), [this]() { return Console(TEXT("BalizaBody 70 70 70 100")) && Console(TEXT("BalizaTime 1 15.1")); },` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:175` | `balsa atada y a bordo (D2 08:00)` | `Add(TEXT("balsa atada y a bordo (D2 08:00)"), [this]() { return Console(TEXT("BalizaTime 2 8")) && Console(TEXT("BalizaRaft board")); },` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:175` | `BalizaTime 2 8` | `Add(TEXT("balsa atada y a bordo (D2 08:00)"), [this]() { return Console(TEXT("BalizaTime 2 8")) && Console(TEXT("BalizaRaft board")); },` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:175` | `BalizaRaft board` | `Add(TEXT("balsa atada y a bordo (D2 08:00)"), [this]() { return Console(TEXT("BalizaTime 2 8")) && Console(TEXT("BalizaRaft board")); },` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:177` | `remar 6 s: avanza y no vuelca` | `Add(TEXT("remar 6 s: avanza y no vuelca"), [this, Raft]()` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:198` | `girar y bajar de la balsa` | `Add(TEXT("girar y bajar de la balsa"), [Raft]()` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:212` | `a las 18:00 del día 2 no se embarca` | `Add(TEXT("a las 18:00 del día 2 no se embarca"), [this, Raft]()` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:214` | `BalizaTime 2 18.2` | `Console(TEXT("BalizaTime 2 18.2"));` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:218` | `18:45 en el islote: vuelta forzosa a Playa Norte con sed y hambre -20` | `Add(TEXT("18:45 en el islote: vuelta forzosa a Playa Norte con sed y hambre -20"), [this]()` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:226` | `BalizaTime 2 18.6` | `Console(TEXT("BalizaTime 2 18.6"));` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:227` | `BalizaBody 60 60 60 100` | `Console(TEXT("BalizaBody 60 60 60 100"));` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:230` | `BalizaTime 2 18.8` | `return Console(TEXT("BalizaTime 2 18.8"));` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:248` | `hoguera al aire libre antes del ciclón` | `Add(TEXT("hoguera al aire libre antes del ciclón"), [this]()` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:250` | `BalizaTime 2 18.9` | `Console(TEXT("BalizaTime 2 18.9"));` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:251` | `BalizaGive lena 3` | `Console(TEXT("BalizaGive lena 3"));` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:255` | `muro 1: lluvia, olas, niebla y fuego apagado` | `Add(TEXT("muro 1: lluvia, olas, niebla y fuego apagado"), [this]() { return Console(TEXT("BalizaTime 2 19.3")); }, [this, Weather]()` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:255` | `BalizaTime 2 19.3` | `Add(TEXT("muro 1: lluvia, olas, niebla y fuego apagado"), [this]() { return Console(TEXT("BalizaTime 2 19.3")); }, [this, Weather]()` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:262` | `el barril se llena con la lluvia` | `Add(TEXT("el barril se llena con la lluvia"), [this, Barrel]()` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:265` | `BalizaTimeScale 10` | `return MarkCount >= 0 && Console(TEXT("BalizaTimeScale 10"));` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:267` | `captura f3d-ciclon` | `Add(TEXT("captura f3d-ciclon"), [this]()` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:269` | `BalizaTimeScale 1` | `Console(TEXT("BalizaTimeScale 1"));` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:270` | `BalizaTime 2 20.5` | `Console(TEXT("BalizaTime 2 20.5"));` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:280` | `espera de la captura` | `Add(TEXT("espera de la captura"), [] { return true; }, [] { return !FScreenshotRequest::IsScreenshotRequested(); });` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:281` | `ojo: calma sin lluvia` | `Add(TEXT("ojo: calma sin lluvia"), [this]() { return Console(TEXT("BalizaTime 2 22.3")); }, [Weather]()` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:281` | `BalizaTime 2 22.3` | `Add(TEXT("ojo: calma sin lluvia"), [this]() { return Console(TEXT("BalizaTime 2 22.3")); }, [Weather]()` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:287` | `reloj x40: muro 2 y fin del ciclón a las 02:00` | `Add(TEXT("reloj x40: muro 2 y fin del ciclón a las 02:00"), [this]()` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:290` | `BalizaTime 2 23.2` | `return Console(TEXT("BalizaTime 2 23.2")) && Console(TEXT("BalizaTimeScale 40"));` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:290` | `BalizaTimeScale 40` | `return Console(TEXT("BalizaTime 2 23.2")) && Console(TEXT("BalizaTimeScale 40"));` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:306` | `desmayo: 20 s a 0, +3 h y despierta en Cala Varada con 40/40/40/60` | `Add(TEXT("desmayo: 20 s a 0, +3 h y despierta en Cala Varada con 40/40/40/60"), [this]()` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:308` | `BalizaTimeScale 1` | `Console(TEXT("BalizaTimeScale 1"));` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:309` | `BalizaTime 3 10` | `Console(TEXT("BalizaTime 3 10"));` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:311` | `BalizaBody 0 50 50 50` | `return Console(TEXT("BalizaBody 0 50 50 50"));` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:327` | `en el refugio de Cala Varada` | `Add(TEXT("en el refugio de Cala Varada"), [this]()` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:329` | `BalizaTime 3 12` | `Console(TEXT("BalizaTime 3 12"));` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:333` | `a mediodía no se duerme` | `Add(TEXT("a mediodía no se duerme"), [Body]() { return !Body()->TrySleep(); }, [Body]() { return Body()->GetSleepCount() == 0; });` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:335` | `dormir a las 21:00 hasta las 06:00 con energía 100` | `Add(TEXT("dormir a las 21:00 hasta las 06:00 con energía 100"), [this]()` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:337` | `BalizaTime 3 21` | `return Console(TEXT("BalizaTime 3 21")) && Console(TEXT("BalizaBody 50 50 30 80")) && Console(TEXT("BalizaSleep"));` |
| `Source/Explored/Debug/ExploredF3dSmokeTest.cpp:337` | `BalizaBody 50 50 30 80` | `return Console(TEXT("BalizaTime 3 21")) && Console(TEXT("BalizaBody 50 50 30 80")) && Console(TEXT("BalizaSleep"));` |
| `Source/Explored/Debug/ExploredFrontendFlowTest.cpp:104` | `menú principal al arrancar` | `Steps.Add({ TEXT("menú principal al arrancar"), [] { return true; }, [this, Menu] { return InMode(Menu); }, 60.0f });` |
| `Source/Explored/Debug/ExploredFrontendFlowTest.cpp:105` | `con el menú abierto no corren ni el reloj ni las barras` | `Steps.Add({ TEXT("con el menú abierto no corren ni el reloj ni las barras"), [this]` |
| `Source/Explored/Debug/ExploredFrontendFlowTest.cpp:129` | `nueva partida` | `Steps.Add({ TEXT("nueva partida"), [this] { Controller()->StartNewGame(); return true; }, [this, Playing] { return InMode(Playing); } });` |
| `Source/Explored/Debug/ExploredFrontendFlowTest.cpp:130` | `jugar: etapa, página, mochila, reloj y sitio` | `Steps.Add({ TEXT("jugar: etapa, página, mochila, reloj y sitio"), [this]` |
| `Source/Explored/Debug/ExploredFrontendFlowTest.cpp:144` | `guardar en manual2` | `Steps.Add({ TEXT("guardar en manual2"), [this]` |
| `Source/Explored/Debug/ExploredFrontendFlowTest.cpp:156` | `volver al menú (recarga el mapa)` | `Steps.Add({ TEXT("volver al menú (recarga el mapa)"), [this] { Controller()->ExitToMainMenu(); return true; },` |
| `Source/Explored/Debug/ExploredFrontendFlowTest.cpp:159` | `el estado es el guardado` | `Steps.Add({ TEXT("el estado es el guardado"), [] { return true; }, [this]` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:120` | `kit inicial` | `Add(TEXT("kit inicial"), [this]() { return Count(TEXT("cuchillo")) == 1 && Count(TEXT("cantimplora")) == 1 && Count(TEXT("barometro")) == 1; },` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:122` | `recoger leña 1` | `Add(TEXT("recoger leña 1"), [this]() { return Interact(TEXT("lena_cala_1")); }, [this]() { return Idle() && Count(TEXT("lena")) == 2; });` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:123` | `recoger leña 2` | `Add(TEXT("recoger leña 2"), [this]() { return Interact(TEXT("lena_cala_2")); }, [this]() { return Idle() && Count(TEXT("lena")) == 4; });` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:124` | `recoger fibra 1` | `Add(TEXT("recoger fibra 1"), [this]() { return Interact(TEXT("fibra_palmar_1")); }, [this]() { return Idle() && Count(TEXT("fibra")) == 1; });` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:125` | `recoger fibra 2` | `Add(TEXT("recoger fibra 2"), [this]() { return Interact(TEXT("fibra_palmar_2")); }, [this]() { return Idle() && Count(TEXT("fibra")) == 2; });` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:126` | `trenzar cuerda` | `Add(TEXT("trenzar cuerda"), [this, Player]()` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:133` | `recoger caña de bambú` | `Add(TEXT("recoger caña de bambú"), [this]() { return Interact(TEXT("cana_bambu_1")); }, [this]() { return Idle() && Count(TEXT("cana_bambu")) == 1; });` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:134` | `atar caña de pescar` | `Add(TEXT("atar caña de pescar"), [Player]() { return Player() && Player()->CraftFromBackpack(TEXT("cana_pescar")); },` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:136` | `pescar un pez` | `Add(TEXT("pescar un pez"), [this]() { return Interact(TEXT("punto_pesca_1")); },` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:146` | `encender la hoguera del faro` | `Add(TEXT("encender la hoguera del faro"), [FireAction]() { return FireAction(TEXT("encender")); },` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:152` | `asar el pez` | `Add(TEXT("asar el pez"), [FireAction]() { return FireAction(TEXT("cocinar")); },` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:158` | `recoger el pez asado` | `Add(TEXT("recoger el pez asado"), [FireAction]() { return FireAction(TEXT("recoger")); }, [this]() { return Count(TEXT("pez_asado")) == 1; });` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:159` | `comer el pez asado` | `Add(TEXT("comer el pez asado"), [Player]() { return Player() && Player()->UseBackpackItem(TEXT("pez_asado")); },` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:161` | `avivar con leña` | `Add(TEXT("avivar con leña"), [FireAction]() { return FireAction(TEXT("avivar")); }, [this]() { return Count(TEXT("lena")) == 0; });` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:162` | `bajar y beber un coco` | `Add(TEXT("bajar y beber un coco"), [this]() { return Interact(TEXT("palma_cala_1")); },` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:172` | `la caja de suministros no se gasta si no falta nada` | `Add(TEXT("la caja de suministros no se gasta si no falta nada"), [this]() { return Interact(TEXT("caja_suministros_faro")); },` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:178` | `llenar la cantimplora en el barril` | `Add(TEXT("llenar la cantimplora en el barril"), [this]()` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:195` | `beber de la cantimplora con la ranura rápida` | `Add(TEXT("beber de la cantimplora con la ranura rápida"), [this, Player]()` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:215` | `hervir aceite eligiendo el verbo` | `Add(TEXT("hervir aceite eligiendo el verbo"), [this, FireAction]()` |
| `Source/Explored/Debug/ExploredIslandSmokeTest.cpp:238` | `combinación que no es receta` | `Add(TEXT("combinación que no es receta"), [this]()` |
| `Source/Explored/Debug/ExploredPlaytestAuditor.cpp:74` | `-ExploredShots=` | `bStandalone = !FString(FCommandLine::Get()).Contains(TEXT("-ExploredShots="));` |
| `Source/Explored/Debug/ExploredPlaytestAuditor.cpp:455` | `historia: 24 órdenes de lente, aceite y mecha (con y sin página 1)` | `Report.Softlocks.Add({ TEXT("historia: 24 órdenes de lente, aceite y mecha (con y sin página 1)"), StoryFailures.IsEmpty(),` |
| `Source/Explored/Debug/ExploredPlaytestAuditor.cpp:456` | `todos llegan al final` | `StoryFailures.IsEmpty() ? FString(TEXT("todos llegan al final")) : FString::Join(StoryFailures, TEXT(" \| ")) });` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:50` | `beber un coco` | `AddStep(TEXT("beber un coco"), [this]() { return UseItem(TEXT("coco")); },` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:77` | `la hoguera de las rocas` | `AddGoToActor(TEXT("la hoguera de las rocas"), [this]() { return Cast<AActor>(NearestFire()); });` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:80` | `comer un pez asado` | `AddStep(TEXT("comer un pez asado"), [this]() { return UseItem(TEXT("pez_asado")); },` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:89` | `beber el segundo coco (cuenco para el aceite)` | `AddStep(TEXT("beber el segundo coco (cuenco para el aceite)"), [this]() { return Count(TEXT("coco")) == 0 \|\| UseItem(TEXT("coco")); },` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:102` | `fin del chubasco` | `AddWaitUntil(1, 17.1, TEXT("fin del chubasco"));` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:106` | `hora de dormir` | `AddWaitUntil(1, 20.05, TEXT("hora de dormir"));` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:107` | `dormir en el refugio del faro` | `AddStep(TEXT("dormir en el refugio del faro"), [this]() { return Body() && Body()->TrySleep(); },` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:113` | `desayunar el pez asado de ayer` | `AddStep(TEXT("desayunar el pez asado de ayer"), [this]() { return Count(TEXT("pez_asado")) == 0 \|\| UseItem(TEXT("pez_asado")); },` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:139` | `subir a la balsa` | `AddStep(TEXT("subir a la balsa"), [this, RaftActor]() { return InteractWith(RaftActor()); },` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:141` | `el islote del carguero` | `AddRow(TEXT("el islote del carguero"), [this]()` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:146` | `bajar en el islote` | `AddStep(TEXT("bajar en el islote"), [Site]()` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:155` | `la balsa` | `AddGoToActor(TEXT("la balsa"), RaftActor);` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:156` | `volver a subir con la lente` | `AddStep(TEXT("volver a subir con la lente"), [this, RaftActor]() { return InteractWith(RaftActor()); },` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:158` | `Playa Norte` | `AddRow(TEXT("Playa Norte"), [Site]() { return Site()->GetShoreLocation(); }, 12.0);` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:159` | `bajar en Playa Norte` | `AddStep(TEXT("bajar en Playa Norte"), [Site]()` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:165` | `dejar la lente junto al faro` | `AddStep(TEXT("dejar la lente junto al faro"), [this]() { return Survivor() && Survivor()->DropHeavy(); },` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:172` | `noche del ciclón` | `AddWaitUntil(2, 20.1, TEXT("noche del ciclón"));` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:173` | `en el ciclón no se duerme` | `AddStep(TEXT("en el ciclón no se duerme"), [this]() { return Body() && Body()->IsSheltered() && !Body()->TrySleep(); },` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:175` | `ojo del ciclón` | `AddWaitUntil(2, 22.2, TEXT("ojo del ciclón"));` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:179` | `fin del ciclón` | `AddWaitUntil(3, 2.05, TEXT("fin del ciclón"));` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:180` | `dormir tras el ciclón` | `AddStep(TEXT("dormir tras el ciclón"), [this]() { return Body() && Body()->TrySleep(); },` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:186` | `soltar el remo (ya no hace falta)` | `AddStep(TEXT("soltar el remo (ya no hace falta)"), [this]() { return Survivor() && Survivor()->DropItem(TEXT("remo")); },` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:222` | `la lente` | `AddGoToActor(TEXT("la lente"), DroppedLens);` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:223` | `coger la lente` | `AddStep(TEXT("coger la lente"), [this, DroppedLens]() { return InteractWith(DroppedLens()); },` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:226` | `subir a la linterna` | `AddStep(TEXT("subir a la linterna"), [this]() { return InteractWith(Prop(TEXT("faro_torre"))); }, [this]()` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:231` | `leer la página 8` | `AddStep(TEXT("leer la página 8"), [this]() { return InteractWith(Prop(TEXT("pagina_8"))); },` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:233` | `poner la lente` | `for (const TCHAR* Slot : { TEXT("poner la lente"), TEXT("verter los tarros"), TEXT("apretar la mecha") })` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:233` | `verter los tarros` | `for (const TCHAR* Slot : { TEXT("poner la lente"), TEXT("verter los tarros"), TEXT("apretar la mecha") })` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:233` | `apretar la mecha` | `for (const TCHAR* Slot : { TEXT("poner la lente"), TEXT("verter los tarros"), TEXT("apretar la mecha") })` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:238` | `la noche` | `AddWaitUntil(3, 20.05, TEXT("la noche"));` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:239` | `encender la baliza` | `AddStep(TEXT("encender la baliza"), [this]() { return InteractWith(Prop(TEXT("faro_linterna"))); }, [this]()` |
| `Source/Explored/Debug/ExploredPlaytestBotRoute.cpp:244` | `Mistral, bocina y epílogo` | `AddStep(TEXT("Mistral, bocina y epílogo"), [] { return true; }, [this]()` |
| `Source/Explored/Debug/ExploredPlaytestBotRun.cpp:164` | `sin objetivo interactuable (%s)` | `Note(FString::Printf(TEXT("sin objetivo interactuable (%s)"), Target ? *Target->GetName() : TEXT("nulo")));` |
| `Source/Explored/Debug/ExploredPlaytestBotRun.cpp:169` | `%s no admite interacción ahora` | `Note(FString::Printf(TEXT("%s no admite interacción ahora"), *Target->GetName()));` |
| `Source/Explored/Debug/ExploredPlaytestBotRun.cpp:247` | `usar ` | `AddStep(TEXT("usar ") + PropId, [this, PropId, bOptional]() { return InteractWith(Prop(PropId)) \|\| bOptional; },` |
| `Source/Explored/Debug/ExploredPlaytestBotRun.cpp:254` | `craftear ` | `AddStep(TEXT("craftear ") + RecipeId, [this, RecipeId]()` |
| `Source/Explored/Debug/ExploredPlaytestBotRun.cpp:265` | `esperar a D%d %05.2f (%s)` | `AddStep(FString::Printf(TEXT("esperar a D%d %05.2f (%s)"), Day, Hour, *Why), [] { return true; },` |
| `Source/Explored/Debug/ExploredPlaytestBotRun.cpp:271` | `pescar hasta %d peces` | `AddStep(FString::Printf(TEXT("pescar hasta %d peces"), Target), [this]() { return InteractWith(Prop(TEXT("punto_pesca_1"))); },` |
| `Source/Explored/Debug/ExploredPlaytestBotRun.cpp:294` | `hoguera: %s` | `AddStep(FString::Printf(TEXT("hoguera: %s"), *Action), [this, FireGetter, Action, Product]()` |
| `Source/Explored/Debug/ExploredPlaytestBotRun.cpp:356` | `balsa a %.1f m del destino (encallada %d)` | `Note(FString::Printf(TEXT("balsa a %.1f m del destino (encallada %d)"), Distance, R->IsGrounded() ? 1 : 0));` |
| `Source/Explored/Debug/ExploredPlaytestBotRun.cpp:382` | `Partida completa, ruta %s, reloj ×%.0f en esperas` | `Note(FString::Printf(TEXT("Partida completa, ruta %s, reloj ×%.0f en esperas"), *RouteName, FastScale));` |
| `Source/Explored/Debug/ExploredPlaytestBotRun.cpp:473` | ` carga: ` | `bLoaded ? TEXT("") : *(TEXT(" carga: ") + Error), bSameStory ? TEXT("") : TEXT(" historia distinta"),` |
| `Source/Explored/Debug/ExploredPlaytestBotRun.cpp:473` | ` historia distinta` | `bLoaded ? TEXT("") : *(TEXT(" carga: ") + Error), bSameStory ? TEXT("") : TEXT(" historia distinta"),` |
| `Source/Explored/Debug/ExploredPlaytestBotRun.cpp:474` | ` mochila distinta` | `bSamePack ? TEXT("") : TEXT(" mochila distinta"), bSameBody && bSameClock ? TEXT("") : TEXT(" cuerpo o reloj distinto")));` |
| `Source/Explored/Debug/ExploredPlaytestBotRun.cpp:474` | ` cuerpo o reloj distinto` | `bSamePack ? TEXT("") : TEXT(" mochila distinta"), bSameBody && bSameClock ? TEXT("") : TEXT(" cuerpo o reloj distinto")));` |
| `Source/Explored/Debug/ExploredPlaytestBotRun.cpp:491` | `etapa %s (%.1f min a 1×)` | `Note(FString::Printf(TEXT("etapa %s (%.1f min a 1×)"), *Name, Run.Clock.Est1xSeconds / 60.0));` |
| `Source/Explored/Debug/ExploredPlaytestBotRun.cpp:555` | `RESULTADO: %s · %.1f min a 1× · %d desmayos · %d/10 etapas · %d errores de log%s` | `Note(FString::Printf(TEXT("RESULTADO: %s · %.1f min a 1× · %d desmayos · %d/10 etapas · %d errores de log%s"),` |
| `Source/Explored/Debug/ExploredPlaytestBotRun.cpp:557` | ` · falló: ` | `Run.StagesReached.Num(), Run.LogErrors, FailedStep.IsEmpty() ? TEXT("") : *(TEXT(" · falló: ") + FailedStep)));` |
| `Source/Explored/Debug/ExploredPlaytestBotRun.cpp:594` | `tope de tiempo real` | `EndRun(TEXT("tope de tiempo real"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:581` | `stat unit` | `GEngine->Exec(World, TEXT("stat unit"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:582` | `stat gpu` | `GEngine->Exec(World, TEXT("stat gpu"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:583` | `stat rhi` | `GEngine->Exec(World, TEXT("stat rhi"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:584` | `stat streaming` | `GEngine->Exec(World, TEXT("stat streaming"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:589` | `r.Nanite.ShowStats 1` | `GEngine->Exec(World, TEXT("r.Nanite.ShowStats 1"));` |
| `Source/Explored/Debug/ExploredShotSubsystem.cpp:617` | `memreport -full` | `GEngine->Exec(World, TEXT("memreport -full"));` |
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
| `Source/Explored/Debug/PlaytestReportModel.cpp:162` | `  "run": ` | `Out += TEXT(" \"run\": ") + FPlaytestRunModel::ToJson(Report.Run, Report.Softlocks) + TEXT("\n");` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:171` | `Informe de playtest automático — %d defecto(s), %d punto(s) de captura medido(s)\n\n` | `Out += FString::Printf(TEXT("Informe de playtest automático — %d defecto(s), %d punto(s) de captura medido(s)\n\n"),` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:192` | `Materiales nulos o por defecto` | `AppendSection(EPlaytestIssueType::MaterialMissing, TEXT("Materiales nulos o por defecto"));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:193` | `Física a la deriva` | `AppendSection(EPlaytestIssueType::PhysicsDrift, TEXT("Física a la deriva"));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:194` | `Flotando o enterrado` | `AppendSection(EPlaytestIssueType::FloatingOrBuried, TEXT("Flotando o enterrado"));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:195` | `Puntos de aparición bloqueados` | `AppendSection(EPlaytestIssueType::SpawnBlocked, TEXT("Puntos de aparición bloqueados"));` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:197` | `== Rendimiento por punto de captura ==\n` | `Out += TEXT("== Rendimiento por punto de captura ==\n");` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:200` | `- %s: %.1f fps medios, %.1f fps mínimos, %.1f MB VRAM\n` | `Out += FString::Printf(TEXT("- %s: %.1f fps medios, %.1f fps mínimos, %.1f MB VRAM\n"),` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:205` | `== Bot de juego (%d paso(s)) ==\n` | `Out += FString::Printf(TEXT("== Bot de juego (%d paso(s)) ==\n"), Report.BotSteps.Num());` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:208` | `- %s en (%.0f, %.0f, %.0f) m: foco=%s, interactuó=%s, inventario: %s\n` | `Out += FString::Printf(TEXT("- %s en (%.0f, %.0f, %.0f) m: foco=%s, interactuó=%s, inventario: %s\n"),` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:211` | `sí` | `Step.bInteracted ? TEXT("sí") : TEXT("no"),` |
| `Source/Explored/Debug/PlaytestReportModel.cpp:212` | `sin cambios` | `Step.InventoryDelta.IsEmpty() ? TEXT("sin cambios") : *Step.InventoryDelta);` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:12` | `\n` | `return In.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n"));` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:131` | `D%lld %02lld:%02lld` | `return FString::Printf(TEXT("D%lld %02lld:%02lld"), Day, Minute / 60, Minute % 60);` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:139` | `    "route": "%s",\n` | `Out += FString::Printf(TEXT(" \"route\": \"%s\",\n"), *Escape(Summary.Route));` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:140` | `    "ran": %s,\n` | `Out += FString::Printf(TEXT(" \"ran\": %s,\n"), Bool(Summary.bRan));` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:141` | `    "reached_epilogue": %s,\n` | `Out += FString::Printf(TEXT(" \"reached_epilogue\": %s,\n"), Bool(Summary.bReachedEpilogue));` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:142` | `    "failed_step": "%s",\n` | `Out += FString::Printf(TEXT(" \"failed_step\": \"%s\",\n"), *Escape(Summary.FailedStep));` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:143` | `    "real_seconds": %.1f,\n` | `Out += FString::Printf(TEXT(" \"real_seconds\": %.1f,\n"), Summary.Clock.RealSeconds);` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:144` | `    "estimated_1x_seconds": %.1f,\n` | `Out += FString::Printf(TEXT(" \"estimated_1x_seconds\": %.1f,\n"), Summary.Clock.Est1xSeconds);` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:145` | `    "credits_seconds": %.1f,\n` | `Out += FString::Printf(TEXT(" \"credits_seconds\": %.1f,\n"), Summary.CreditsSeconds);` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:146` | `    "estimated_1x_minutes": %.2f,\n` | `Out += FString::Printf(TEXT(" \"estimated_1x_minutes\": %.2f,\n"), EstimatedMinutes(Summary));` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:147` | `    "target_minutes": [%.0f, %.0f],\n` | `Out += FString::Printf(TEXT(" \"target_minutes\": [%.0f, %.0f],\n"), Tuning.MinMinutes, Tuning.MaxMinutes);` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:148` | `    "duration_in_target": %s,\n` | `Out += FString::Printf(TEXT(" \"duration_in_target\": %s,\n"), Bool(IsDurationInTarget(Summary, Tuning)));` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:149` | `    "faints": %d,\n    "sleeps": %d,\n    "pages_read": %d,\n` | `Out += FString::Printf(TEXT(" \"faints\": %d,\n \"sleeps\": %d,\n \"pages_read\": %d,\n"), Summary.Faints, Summary.Sleeps,` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:151` | `    "stages_reached": %s,\n` | `Out += FString::Printf(TEXT(" \"stages_reached\": %s,\n"), *StringArray(Summary.StagesReached));` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:152` | `    "all_stages": %s,\n` | `Out += FString::Printf(TEXT(" \"all_stages\": %s,\n"), Bool(AllStagesReached(Summary)));` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:153` | `    "log_errors": %d,\n    "log_warnings": %d,\n` | `Out += FString::Printf(TEXT(" \"log_errors\": %d,\n \"log_warnings\": %d,\n"), Summary.LogErrors, Summary.LogWarnings);` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:154` | `    "error_samples": %s,\n` | `Out += FString::Printf(TEXT(" \"error_samples\": %s,\n"), *StringArray(Summary.ErrorSamples));` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:155` | `    "save_load_checks": %d,\n    "save_load_failures": %d,\n` | `Out += FString::Printf(TEXT(" \"save_load_checks\": %d,\n \"save_load_failures\": %d,\n"), Summary.SaveLoadChecks,` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:157` | `    "save_load_notes": %s,\n` | `Out += FString::Printf(TEXT(" \"save_load_notes\": %s,\n"), *StringArray(Summary.SaveLoadNotes));` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:158` | `    "steps": [\n` | `Out += TEXT(" \"steps\": [\n");` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:162` | `      {"name": "%s", "ok": %s, "t_1x_s": %.1f, "clock": "%s", "stage": "%s"}%s\n` | `Out += FString::Printf(TEXT(" {\"name\": \"%s\", \"ok\": %s, \"t_1x_s\": %.1f, \"clock\": \"%s\", \"stage\": \"%s\"}%s\n"),` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:167` | `    "softlocks": [\n` | `Out += TEXT(" \"softlocks\": [\n");` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:171` | `      {"scenario": "%s", "completable": %s, "detail": "%s"}%s\n` | `Out += FString::Printf(TEXT(" {\"scenario\": \"%s\", \"completable\": %s, \"detail\": \"%s\"}%s\n"), *Escape(Check.Name),` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:175` | `    "softlocks_found": %d,\n` | `Out += FString::Printf(TEXT(" \"softlocks_found\": %d,\n"),` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:177` | `    "passed": %s\n` | `Out += FString::Printf(TEXT(" \"passed\": %s\n"), Bool(Passed(Summary, Tuning) && AllCompletable(Softlocks)));` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:186` | `== Partida completa del bot (ruta «%s») ==\n` | `FString Out = FString::Printf(TEXT("== Partida completa del bot (ruta «%s») ==\n"), *Summary.Route);` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:187` | `- Epílogo alcanzado: %s%s\n` | `Out += FString::Printf(TEXT("- Epílogo alcanzado: %s%s\n"), Summary.bReachedEpilogue ? TEXT("sí") : TEXT("no"),` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:187` | `sí` | `Out += FString::Printf(TEXT("- Epílogo alcanzado: %s%s\n"), Summary.bReachedEpilogue ? TEXT("sí") : TEXT("no"),` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:188` | ` (falló: ` | `Summary.FailedStep.IsEmpty() ? TEXT("") : *(TEXT(" (falló: ") + Summary.FailedStep + TEXT(")")));` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:189` | `- Duración estimada a 1×: %.1f min (%.1f de partida + %.0f s de créditos), objetivo %.0f-%.0f: %s\n` | `Out += FString::Printf(TEXT("- Duración estimada a 1×: %.1f min (%.1f de partida + %.0f s de créditos), objetivo %.0f-%.0f: %s\n"),` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:192` | `- Tiempo real de la prueba: %.1f min\n` | `Out += FString::Printf(TEXT("- Tiempo real de la prueba: %.1f min\n"), Summary.Clock.RealSeconds / 60.0);` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:193` | `- Etapas: %d/%d (%s)\n` | `Out += FString::Printf(TEXT("- Etapas: %d/%d (%s)\n"), Summary.StagesReached.Num(), StageCount,` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:195` | `- Desmayos: %d · sueños: %d · páginas leídas: %d\n` | `Out += FString::Printf(TEXT("- Desmayos: %d · sueños: %d · páginas leídas: %d\n"), Summary.Faints, Summary.Sleeps, Summary.PagesRead);` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:196` | `- Guardar/cargar: %d comprobaciones, %d fallos\n` | `Out += FString::Printf(TEXT("- Guardar/cargar: %d comprobaciones, %d fallos\n"), Summary.SaveLoadChecks, Summary.SaveLoadFailures);` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:201` | `- Log: %d errores, %d avisos\n` | `Out += FString::Printf(TEXT("- Log: %d errores, %d avisos\n"), Summary.LogErrors, Summary.LogWarnings);` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:206` | `- Pasos:\n` | `Out += TEXT("- Pasos:\n");` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:209` | `    %s %6.1f min  %s  [%s] %s\n` | `Out += FString::Printf(TEXT(" %s %6.1f min %s [%s] %s\n"), Step.bOk ? TEXT("ok ") : TEXT("FALLO"), Step.Est1xSeconds / 60.0,` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:209` | `ok   ` | `Out += FString::Printf(TEXT(" %s %6.1f min %s [%s] %s\n"), Step.bOk ? TEXT("ok ") : TEXT("FALLO"), Step.Est1xSeconds / 60.0,` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:212` | `\n== Softlocks (%d escenarios) ==\n` | `Out += FString::Printf(TEXT("\n== Softlocks (%d escenarios) ==\n"), Softlocks.Num());` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:215` | `se puede terminar` | `Out += FString::Printf(TEXT("- %s: %s%s\n"), *Check.Name, Check.bCompletable ? TEXT("se puede terminar") : TEXT("SOFTLOCK"),` |
| `Source/Explored/Debug/PlaytestRunModel.cpp:218` | `\nRESULTADO: %s\n` | `Out += FString::Printf(TEXT("\nRESULTADO: %s\n"), Verdict(Passed(Summary, Tuning) && AllCompletable(Softlocks)));` |
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
| `Source/Explored/Items/IslandDataLoader.cpp:159` | `recipes.json: falta la lista «recipes»` | `Errors.Add(TEXT("recipes.json: falta la lista «recipes»"));` |
| `Source/Explored/Items/IslandDataLoader.cpp:171` | `recipes.json: receta incompleta (id, verb, place y output)` | `Errors.Add(TEXT("recipes.json: receta incompleta (id, verb, place y output)"));` |
| `Source/Explored/Items/IslandDataLoader.cpp:178` | `recipes.json: sin recetas` | `Errors.Add(TEXT("recipes.json: sin recetas"));` |
| `Source/Explored/Items/IslandDataLoader.cpp:183` | `recipes.json: %s` | `Errors.Add(FString::Printf(TEXT("recipes.json: %s"), *Error));` |
| `Source/Explored/Items/IslandDataLoader.cpp:288` | `survival_needs.json: valores del cuerpo no válidos` | `Errors.Add(TEXT("survival_needs.json: valores del cuerpo no válidos"));` |
| `Source/Explored/Items/IslandDataLoader.cpp:471` | `storm_curve.json: ventana sin estado conocido` | `Errors.Add(TEXT("storm_curve.json: ventana sin estado conocido"));` |
| `Source/Explored/Items/IslandDataLoader.cpp:499` | `storm_curve.json: curva o ventanas no válidas` | `Errors.Add(TEXT("storm_curve.json: curva o ventanas no válidas"));` |
| `Source/Explored/Items/ItemTypes.cpp:7` | `Pequeño` | `case EItemSize::Pequeno: return TEXT("Pequeño");` |
| `Source/Explored/Narrative/BeaconSubsystem.cpp:154` | `Pone la historia de Baliza Siete en una etapa: BalizaStage <nombre o 0-9>` | `TEXT("Pone la historia de Baliza Siete en una etapa: BalizaStage <nombre o 0-9>"),` |
| `Source/Explored/Narrative/BeaconSubsystem.cpp:157` | `Mete objetos en la mochila de Iria: BalizaGive <id> <n>` | `TEXT("Mete objetos en la mochila de Iria: BalizaGive <id> <n>"),` |
| `Source/Explored/Narrative/BeaconSubsystem.cpp:160` | `Pone el reloj: BalizaTime <día> <hora>` | `TEXT("Pone el reloj: BalizaTime <día> <hora>"),` |
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
| `Source/Explored/UI/IslandHudModel.cpp:12` | `Día %d · %02d:%02d` | `return FString::Printf(TEXT("Día %d · %02d:%02d"), FMath::Max(1, Day), Minutes / 60, Minutes % 60);` |
| `Source/Explored/UI/IslandHudModel.cpp:17` | `Día %d` | `return FString::Printf(TEXT("Día %d"), FMath::Max(1, Day));` |
| `Source/Explored/UI/IslandHudModel.cpp:24` | `%d hPa %s` | `return FString::Printf(TEXT("%d hPa %s"), Rounded, Arrow);` |
| `Source/Explored/UI/IslandHudModel.cpp:29` | `%d hPa` | `return FString::Printf(TEXT("%d hPa"), FMath::IsFinite(Hpa) ? FMath::RoundToInt32(Hpa) : 0);` |
| `Source/Explored/UI/Widgets/SExploredCredits.cpp:54` | `Clear Air` | `TEXT("Morning"), TEXT("Clear Air"), TEXT("Evening"), TEXT("Plaint"), TEXT("Gymnopedie No. 3"),` |
| `Source/Explored/UI/Widgets/SExploredCredits.cpp:54` | `Gymnopedie No. 3` | `TEXT("Morning"), TEXT("Clear Air"), TEXT("Evening"), TEXT("Plaint"), TEXT("Gymnopedie No. 3"),` |
| `Source/Explored/UI/Widgets/SExploredCredits.cpp:55` | `Enchanted Journey` | `TEXT("Enchanted Journey"), TEXT("Evening Fall (Harp)"), TEXT("Lamentation"), TEXT("Canon in D Major"), TEXT("Danse Morialta"),` |
| `Source/Explored/UI/Widgets/SExploredCredits.cpp:55` | `Evening Fall (Harp)` | `TEXT("Enchanted Journey"), TEXT("Evening Fall (Harp)"), TEXT("Lamentation"), TEXT("Canon in D Major"), TEXT("Danse Morialta"),` |
| `Source/Explored/UI/Widgets/SExploredCredits.cpp:55` | `Canon in D Major` | `TEXT("Enchanted Journey"), TEXT("Evening Fall (Harp)"), TEXT("Lamentation"), TEXT("Canon in D Major"), TEXT("Danse Morialta"),` |
| `Source/Explored/UI/Widgets/SExploredCredits.cpp:55` | `Danse Morialta` | `TEXT("Enchanted Journey"), TEXT("Evening Fall (Harp)"), TEXT("Lamentation"), TEXT("Canon in D Major"), TEXT("Danse Morialta"),` |
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

## Avisos (2)

- Source/Explored/Debug/ExploredPlaytestBotRun.cpp:238: literal sin localizar «ir a »
- Source/Explored/Debug/ExploredPlaytestBotRun.cpp:341: literal sin localizar «remar hasta »
