#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Subsystems/LocalPlayerSubsystem.h"

#include "ExploredInputSettingsSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnExploredBindingChanged, FName /*ActionName*/);

/** Nombres de las acciones de Enhanced Input que construye AExploredCharacter (Player/ExploredCharacter.cpp). */
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
 * Cómo lo consume el personaje (u otro sistema que construya un
 * UInputMappingContext por código):
 *
 *   1. Al crear cada acción discreta, registrar su tecla por defecto una vez:
 *        Subsystem->RegisterAction(TEXT("IA_Jump"), EKeys::SpaceBar);
 *      y usar la tecla efectiva (override si existe) al mapearla:
 *        MapKey(Context, JumpAction, Subsystem->GetKeyFor(TEXT("IA_Jump"), EKeys::SpaceBar));
 *
 *   2. Suscribirse a OnBindingsChanged y, cuando dispare para una acción que
 *      afecte al contexto propio, desmapear la tecla vieja y mapear la
 *      nueva (Context->UnmapKey / Context->MapKey) y volver a añadir el
 *      contexto en el UEnhancedInputLocalPlayerSubsystem para que Enhanced
 *      Input recalcule los mapeos activos.
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
	 * delegado) si esa tecla ya está en uso por otra acción registrada.
	 * Devuelve true si el cambio se aplicó.
	 */
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Controles")
	bool SetKeyFor(FName ActionName, FKey NewKey);

	/** Deshace el remapeo de una acción, volviendo a su tecla por defecto. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Ajustes|Controles")
	void ResetKeyFor(FName ActionName);

	FOnExploredBindingChanged OnBindingsChanged;

private:
	FKey FindOverride(FName ActionName) const;

	UPROPERTY(Config)
	TArray<FExploredKeyBindingOverride> Overrides;

	/** Teclas por defecto registradas en memoria (no persisten; las registra cada acción al construirse). */
	TMap<FName, FKey> Defaults;
};
