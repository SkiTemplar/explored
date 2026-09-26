#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Subsystems/LocalPlayerSubsystem.h"

#include "ExploredInputSettingsSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnExploredBindingChanged, FName /*ActionName*/);

/**
 * Nombres de las acciones de Enhanced Input que construye AExploredCharacter
 * (Player/ExploredCharacter.cpp). La lista completa de acciones remapeables y
 * sus teclas por defecto está en ExploredSettingsLogic::GetRemappableActions().
 */
namespace ExploredInputActionNames
{
	constexpr const TCHAR* Jump = TEXT("IA_Jump");
	constexpr const TCHAR* Sprint = TEXT("IA_Sprint");
}

USTRUCT()
struct FExploredKeyBindingOverride
{
	GENERATED_BODY()

	UPROPERTY(Config)
	FName ActionName;

	UPROPERTY(Config)
	FKey Key;
};

/**
 * Remapeo de teclado/mando persistente (Ajustes > Controles).
 *
 * Todas las acciones remapeables (ExploredSettingsLogic::GetRemappableActions)
 * se registran al inicializar el subsistema, así que la detección de
 * conflictos cubre todas aunque la UI o el personaje aún no las hayan pedido
 * (M12). RegisterAction() sigue sirviendo para acciones extra.
 *
 * Cómo lo consume el personaje:
 *
 *   1. Al mapear cada acción remapeable, usar la tecla efectiva:
 *        MapKey(Context, Action, Subsystem->GetKeyFor(Nombre, TeclaPorDefecto));
 *
 *   2. Suscribirse a OnBindingsChanged y, cuando dispare, reconstruir los
 *      mapeos del contexto y pedir a UEnhancedInputLocalPlayerSubsystem que
 *      recalcule los mapeos activos (RequestRebuildControlMappings).
 *
 * Los ajustes de Ajustes > Controles que no son remapeo de teclas
 * (sensibilidad, invertir Y, FOV, balanceo de cámara, agacharse) viven en
 * UExploredGameUserSettings, no aquí.
 */
UCLASS(Config = Game)
class EXPLORED_API UExploredInputSettingsSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Registra una acción y su tecla por defecto (idempotente). Necesario para poder detectar conflictos. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Controles")
	void RegisterAction(FName ActionName, FKey DefaultKey);

	/** Tecla efectiva: el remapeo guardado, o la tecla por defecto registrada. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Controles")
	FKey GetKeyFor(FName ActionName, FKey DefaultKey) const;

	/**
	 * Cambia la tecla de una acción. Rechaza el cambio (y no dispara el
	 * delegado) si la tecla está reservada, es de mando o ya la usa otra
	 * acción registrada. Devuelve true si el cambio se aplicó.
	 */
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Controles")
	bool SetKeyFor(FName ActionName, FKey NewKey);

	/** Deshace el remapeo de una acción, volviendo a su tecla por defecto. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Controles")
	void ResetKeyFor(FName ActionName);

	/**
	 * Acción que ya usa NewKey (distinta de ActionName), NAME_None si está
	 * libre, o «Reserved» si es una tecla fija. Para mostrar el motivo del
	 * rechazo en la UI antes de llamar a SetKeyFor.
	 */
	FName GetConflictFor(FName ActionName, FKey NewKey) const;

	/**
	 * Solo para tests: parte sin remapeos (ignora los que el CDO leyó del
	 * Game.ini del usuario) y no escribe nada en disco.
	 */
	void UseTransientStorageForTesting();

	FOnExploredBindingChanged OnBindingsChanged;

	/** Valor de GetConflictFor para una tecla reservada. */
	static const FName ReservedConflictName;

private:
	FKey FindOverride(FName ActionName) const;
	void SaveOverrides();

	UPROPERTY(Config)
	TArray<FExploredKeyBindingOverride> Overrides;

	/** Teclas por defecto registradas en memoria (no persisten; las registra cada acción al construirse). */
	TMap<FName, FKey> Defaults;

	/** false en tests: los cambios se quedan en memoria (ver UseTransientStorageForTesting). */
	bool bPersistOverrides = true;
};
