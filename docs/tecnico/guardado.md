# Guardado de partida

Implementa GDD §15 («Guardado») y §17.2 (Save: *semilla + deltas del mundo + jugador +
progreso*). El núcleo es código puro en `Source/Explored/Save/` (solo `CoreMinimal.h`), con
specs que corren en el editor y en `Tools/HostTests` (`Explored.Save.Archive` y
`Explored.Save.Slots`). La capa de Unreal es `UExploredSaveSubsystem`
(`Source/Explored/UI/ExploredSaveSubsystem.*`).

| Fichero | Qué contiene |
|---|---|
| `Save/SaveValue.h` | `FSaveValue` (árbol tipado), `FSaveText` (escritor/lector JSON), `FSaveChecksum` |
| `Save/SaveArchive.h` | `FSaveArchive` (lectura tolerante), `TSaveTraits<T>`, `TSaveEnumNames<E>` |
| `Save/SaveFormat.h` | `FSaveHeader`, `FSaveDocument`, `FSaveCodec`, `FSaveMigrations`, `FSaveSectionRegistry` |
| `Save/SaveSlots.h` | `FSaveSlotPolicy`, `ISaveFileSystem`, `FSaveSlotStore` |
| `Save/SaveWorldDeltas.h` | `FSaveIndexSet`, `FSaveScatterDeltas`, `FSaveWorldDeltas`, `FSavePlayerState` |

## Formato del fichero

Texto JSON legible, UTF-8 (con BOM al escribirlo desde Unreal; el lector la tolera). Ejemplo
generado por el código (con el cuerpo del jugador abreviado):

```json
{
	"checksum": "84cc0f25389de202",
	"format": "explored-save",
	"header": {
		"formatVersion": 1,
		"gameVersion": "0.1.0",
		"playTimeSeconds": 5423.25,
		"seed": 20260926,
		"slotId": "manual1",
		"timestampUnix": 1790409600
	},
	"sections": {
		"player": {
			"body": { "bodyTemperature": 37.0, "energy": 100.0, "health": 100.0, "…": "…" },
			"controlRotation": [-8.5, 271.0, 0.0],
			"location": [152340.5, -80210.25, 412.0],
			"velocity": [0.0, 0.0, 0.0]
		},
		"world": {
			"layers": {
				"harvested": [
					[12, -3, "r:0-39,57"],
					[13, -3, "r:4"]
				]
			},
			"seed": 20260926
		}
	}
}
```

Reglas del texto (`FSaveText`):

- **Claves ordenadas** por unidades de código: el mismo estado produce siempre el mismo texto,
  sin importar el orden en que los sistemas escriban sus campos.
- **Enteros** (`int64`) sin punto decimal. **Reales** siempre con `.` o exponente, con la
  representación más corta que vuelve exactamente al mismo valor: 15–17 cifras para `double`,
  hasta 9 para `float` (`FSaveValue::MakeFloat`). `NaN`, `Infinity` y `-Infinity` se guardan
  como cadenas y se leen como reales. `uint64` se guarda con el patrón de bits de un `int64`.
- **Cadenas** con los caracteres no ASCII tal cual (tildes, eñes, emojis); se escapan comillas,
  barra invertida y controles. El lector acepta `\uXXXX` y parejas sustitutas.
- **Lector robusto**: nunca aborta; ante texto mal formado (truncado, claves duplicadas,
  anidamiento de más de 64 niveles, números fuera de rango…) devuelve `false` y un mensaje con
  la posición. Se prueba con miles de mutaciones deterministas bajo ASan/UBSan.

### Suma de control

`checksum` es FNV-1a de 64 bits sobre los **bytes UTF-8** de la forma compacta canónica de
`{"header": …, "sections": …}`. Al leer se reescribe esa forma desde el árbol leído y se
compara; el escritor es un punto fijo (escribir lo leído da el mismo texto), así que el
espaciado del fichero no importa pero cualquier cambio de un valor sí. Una suma que no cuadra
da `ESaveLoadResult::BadChecksum`.

### Versiones y migraciones

- `ExploredSave::CurrentFormatVersion` (hoy 1) va en `header.formatVersion`.
- Una partida de versión **mayor** que la actual se rechaza con `FutureVersion` (y no se
  sustituye en silencio por su copia de seguridad).
- Una partida **anterior** pasa por `FSaveMigrations`: un paso por versión, `N → N + 1`, sobre
  el objeto `sections`, en orden. Si falta un paso o uno falla, `MigrationFailed`; la partida
  en memoria no queda a medias.

Al cambiar el formato de una sección de forma incompatible:

1. Sube `CurrentFormatVersion`.
2. Registra la migración desde la versión anterior (en `UExploredSaveSubsystem::Initialize`
   o desde el sistema dueño, con `GetMigrations().Register(...)`):

```cpp
SaveSubsystem->GetMigrations().Register(1, [](FSaveValue& Sections, FString& OutError)
{
	// v1 → v2: la sección «carta» pasa a llamarse «cartography».
	if (const FSaveValue* Old = Sections.Find(TEXT("carta")))
	{
		const FSaveValue Moved = *Old;
		Sections.Remove(TEXT("carta"));
		Sections.Set(TEXT("cartography"), Moved);
	}
	return true;
});
```

Añadir un campo nuevo **no** necesita migración: basta con leerlo con `Read` (ver abajo).

## Cómo añade un sistema su sección

Cada sistema expone su estado como datos planos y registra dos callbacks en el subsistema:

```cpp
// En el componente o subsistema del sistema (p. ej. el huerto):
void UFarmSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UExploredSaveSubsystem* Save = Collection.InitializeDependency<UExploredSaveSubsystem>();
	TWeakObjectPtr<UFarmSubsystem> WeakThis(this);
	Save->RegisterSection(TEXT("farm"),
		[WeakThis](FSaveArchive& Ar)
		{
			if (const UFarmSubsystem* Self = WeakThis.Get())
			{
				Ar.Write(TEXT("lemonTreeStage"), Self->State.LemonTreeStage); // enum por nombre
				Ar.Write(TEXT("plots"), Self->State.Plots);                   // TArray<FPlot> con TSaveTraits
			}
		},
		[WeakThis](const FSaveArchive& Ar)
		{
			if (UFarmSubsystem* Self = WeakThis.Get())
			{
				Self->State = FFarmState();                               // 1) estado por defecto
				Ar.Read(TEXT("lemonTreeStage"), Self->State.LemonTreeStage); // 2) lo que haya
				Ar.Read(TEXT("plots"), Self->State.Plots);
			}
		});
}

void UFarmSubsystem::Deinitialize()
{
	if (UExploredSaveSubsystem* Save = GetGameInstance()->GetSubsystem<UExploredSaveSubsystem>())
	{
		Save->UnregisterSection(TEXT("farm"));
	}
	Super::Deinitialize();
}
```

Contrato:

- **Nombre** único en minúsculas y en inglés, como las claves (`"farm"`, `"building"`,
  `"cartography"`, `"museum"`, `"cooking"`, `"events"`, `"world"`).
- **Save** recibe un archivo vacío y escribe su estado.
- **Load** recibe el archivo de la partida, o uno **vacío** si la partida no trae la sección
  (partidas anteriores al sistema): debe partir del estado por defecto y leer con `Read`, que
  no toca el valor si la clave falta o no encaja (tipo equivocado, fuera de rango, nombre de
  enum desconocido). Así, una sección o un campo nuevos nunca rompen partidas viejas.
- Las secciones que la partida trae y nadie registra se **conservan** y se reescriben al
  guardar (un sistema desactivado no borra sus datos).
- Las secciones se cargan en orden de registro.
- Los callbacks corren en el hilo de juego, dentro de `SaveToSlot`/`LoadFromSlot`. Si
  capturan un `UObject`, que sea con `TWeakObjectPtr`.

### Tipos que se guardan solos

`FSaveArchive::Write/Read/ReadOr` aceptan cualquier tipo con `TSaveTraits<T>`:

| Tipo | En el texto |
|---|---|
| `bool`, `int8`…`int64`, `uint8`…`uint32` | booleano / entero (al leer se comprueba el rango) |
| `uint64` | entero con el patrón de bits de `int64` |
| `float`, `double` | real (precisión exacta de ida y vuelta) |
| `FString`, `FName` | cadena |
| `FVector`, `FVector2D`, `FRotator` | `[X, Y, Z]`, `[X, Y]`, `[Pitch, Yaw, Roll]` |
| `FIntPoint` | `[X, Y]` |
| `enum` con `TSaveEnumNames<E>` | nombre del enumerador |
| `TArray<T>` | lista (todo o nada) |
| `TMap<FString/FName, V>` | objeto con claves ordenadas |
| `TMap<K, V>` (otra clave) | lista de pares `[clave, valor]` |
| `FSaveArchive`, `FSaveValue` | objeto / valor tal cual |

Un enum se guarda por nombre para que reordenarlo no rompa partidas:

```cpp
template <>
struct TSaveEnumNames<EPlantStage>
{
	static constexpr const TCHAR* Names[] = { TEXT("Seed"), TEXT("Sprout"), TEXT("Flower"), TEXT("Fruit") };
};
```

Para un struct propio hay dos opciones: escribir un `FSaveArchive` hijo
(`Ar.Write(TEXT("plot"), PlotArchive)`) o especializar `TSaveTraits<FMyStruct>` con
`ToValue`/`FromValue`, y entonces también funcionan `TArray<FMyStruct>` y los mapas.

## Mundo: semilla + deltas

El archipiélago se regenera desde la semilla (`header.seed`) y la partida solo guarda lo que
el jugador cambió. `FSaveScatterDeltas` guarda instancias procedurales retiradas por
**(celda, índice dentro de la celda)**, con un mapa de bits por celda:

- en el texto, cada celda es `[X, Y, "<índices>"]`, con las celdas ordenadas por (Y, X);
- los índices usan la forma más corta de dos: rangos `"r:0-39,57"` (claros talados) o mapa de
  bits en base64 `"b:…"` (recolección dispersa); el peor caso cuesta 6 bits por instancia;
- el índice máximo por celda es 2²⁰ − 1: un fichero manipulado no puede reservar más de
  128 KiB por celda y el lector exige rangos ascendentes (trabajo acotado);
- `Merge` une dos conjuntos de deltas; `ForEach` los recorre en orden determinista para
  aplicarlos al regenerar una celda.

`FSaveWorldDeltas` agrupa capas por nombre (`"harvested"`, `"destroyed"`…) y la semilla.
La sección `"world"` todavía no está registrada: la registrará el scatter de vegetación cuando
tenga recolección persistente.

## Ranuras

- 3 ranuras manuales (`manual1`…`manual3`) y una automática (`auto`), en
  `Saved/SaveGames/<ranura>.sav`, cada una con **una copia** `<ranura>.bak` (la versión
  anterior).
- **Escritura atómica** (`FSaveSlotPolicy::PlanWrite`): se escribe `<ranura>.tmp`, la
  principal pasa a `.bak` y el temporal pasa a principal. Si el proceso muere en cualquier
  paso, la principal o la copia siguen siendo una partida completa.
- **Lectura**: si la principal falla (ilegible, suma que no cuadra, migración imposible) se
  lee la copia; `FSaveReadOutcome::bFromBackup` lo indica. Una versión futura no cae a la copia.
- **Continuar**: la ranura legible más reciente (`timestampUnix`); a igualdad, más tiempo
  jugado y después el orden fijo `auto, manual1, manual2, manual3`. Las ilegibles van al final.
- **Autoguardado** (`ESaveTrigger`): siempre al dormir; en hogueras, como mucho cada
  `MinSecondsBetweenCampfireSaves` (300 s reales). Los autoguardados van a `auto`.
  `UExploredSaveSubsystem::RequestAutosave(ESaveTrigger::Sleep)` es lo que deben llamar la cama
  y la hoguera cuando existan.

`FSaveSlotStore` implementa todo esto sobre `ISaveFileSystem`; los tests usan un sistema de
ficheros en memoria que puede fallar en cualquier paso, y el subsistema usa el disco
(`FFileHelper::SaveStringToFile`/`LoadFileToString` en UTF-8 e `IFileManager::Move`).

## API del subsistema

| Llamada | Uso |
|---|---|
| `RequestSave(FName Slot = "Auto")` | Pausa → «Guardar» → selector de ranura (`SExploredSaveSlots`, ranuras manuales). Dispara `OnSaveCompleted` u `OnSaveFailed`. |
| `RequestAutosave(ESaveTrigger)` | Cama y hoguera. |
| `LoadContinueGame()` | Menú → «Continuar» (lo llama `AExploredPlayerController::ContinueGame`). |
| `SaveToSlot` / `LoadFromSlot` | Guardar y cargar una ranura concreta (menú → «Cargar» usa `LoadFromSlot`); `OnLoadCompleted` tras cargar. |
| `HasSaveGame()` / `GetSlotNames()` / `ListSlots()` | Menús (ranuras legibles, en orden de «Continuar»). |
| `GetSlotsWithBackup()` | Selector de ranura (P-UI2): ranuras con copia `.bak` en disco. |
| `RegisterSection` / `UnregisterSection` | Sistemas (ver arriba). |
| `GetMigrations()` | Migraciones del formato. |
| `SetWorldSeed` / `GetPlayTimeSeconds` | Cabecera. |

La sección `"player"` la registra el propio subsistema: guarda posición, rotación del control
y velocidad del personaje y, al cargar, lo teletransporta (ahora o, si aún no existe, al
terminar de cargar el mapa). Los valores del cuerpo (`FSavePlayerState::Health`…) son datos
planos a la espera de un componente de supervivencia en el personaje.

## Pendiente

- Secciones de cada sistema: mundo (recolección del scatter), progreso (`FExploredProgress`),
  hora y clima, construcción, huerto, cartografía, museo, cocina y eventos.
- Llamar a `RequestAutosave` desde la cama y las hogueras cuando existan.
- Selector de ranura en la UI (hoy «Guardar partida» usa `auto`).
- La capa de Unreal está **sin compilar**: verificar en local con `Tools/build.ps1` y
  `Tools/test.ps1`.
