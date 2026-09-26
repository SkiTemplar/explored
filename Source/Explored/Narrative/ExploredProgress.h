#pragma once

#include "CoreMinimal.h"

#include "ExploredProgress.generated.h"

/** Piezas del Albatros necesarias para la baliza (GDD §7). */
UENUM()
enum class EBeaconPart : uint8
{
	Radio,
	Battery,
	Antenna,
	Flare,
	Count UMETA(Hidden)
};

/** Finales posibles (GDD §2.4). */
UENUM()
enum class EExploredEnding : uint8
{
	None,
	Rescue,
	Voyage,
	Stay,
	SecretEpilogue
};

/**
 * Progreso narrativo y de exploración de una partida. Es serializable
 * (se guarda tal cual en la partida) y sus reglas son funciones puras.
 */
USTRUCT()
struct EXPLORED_API FExploredProgress
{
	GENERATED_BODY()

	static constexpr int32 TotalInesNotes = 18;
	static constexpr int32 TotalHaldenPages = 24;
	static constexpr int32 TotalPetroglyphs = 30;
	static constexpr int32 TotalBottles = 8;
	static constexpr int32 TotalMorse = 6;
	static constexpr int32 TotalViewpoints = 7;

	/** Contenido descubierto por id (notas, páginas, petroglifos, botellas, morse, miradores, especies…). */
	UPROPERTY()
	TSet<FName> Discovered;

	UPROPERTY()
	uint8 BeaconParts = 0;

	UPROPERTY()
	bool bBeaconBuilt = false;

	UPROPERTY()
	bool bBeaconActivated = false;

	UPROPERTY()
	bool bStarCompassDeciphered = false;

	UPROPERTY()
	bool bOutriggerCanoeBuilt = false;

	UPROPERTY()
	bool bCanelaBefriended = false;

	UPROPERTY()
	bool bReachedEmergedIsland = false;

	UPROPERTY()
	TArray<EExploredEnding> EndingsSeen;

	/** Registra un descubrimiento. Devuelve true si es nuevo. */
	bool Discover(FName Id);
	bool IsDiscovered(FName Id) const { return Discovered.Contains(Id); }

	/** Cuenta los descubrimientos cuyo id empieza por el prefijo (p. ej. «ines_»). */
	int32 CountWithPrefix(const FString& Prefix) const;

	void AddBeaconPart(EBeaconPart Part) { BeaconParts |= 1u << static_cast<uint8>(Part); }
	bool HasBeaconPart(EBeaconPart Part) const { return (BeaconParts & (1u << static_cast<uint8>(Part))) != 0; }
	bool HasAllBeaconParts() const;

	bool CanBuildBeacon() const { return HasAllBeaconParts() && !bBeaconBuilt; }

	/** El final Rescate exige la baliza montada y encendida (de noche, lo comprueba el juego). */
	bool CanTriggerRescue() const { return bBeaconBuilt && bBeaconActivated; }

	/** La Travesía exige descifrar la brújula estelar y la canoa de balancín, y llegar a la isla emergida. */
	bool CanTriggerVoyage() const { return bStarCompassDeciphered && bOutriggerCanoeBuilt && bReachedEmergedIsland; }

	/** «El que se queda»: rechazar el rescate con el diario Halden completo. */
	bool CanChooseStay() const { return CanTriggerRescue() && CountWithPrefix(TEXT("halden_")) >= TotalHaldenPages; }

	/** Epílogo secreto: diario al 100 %. */
	bool IsJournalComplete() const;

	/** Porcentaje del diario completado (0–100). */
	float JournalCompletion() const;
};
