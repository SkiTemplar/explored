#pragma once

#include "CoreMinimal.h"

#include "ExploredProgress.generated.h"

/** Piezas del Albatros necesarias para construir el barco «Limón» (GDD §8.10). */
UENUM()
enum class EShipPart : uint8
{
	Fuselage,
	Wing,
	Tail,
	Engine,
	Count UMETA(Hidden)
};

/** Cómo termina (opcionalmente) una partida (GDD §0, §6.3). */
UENUM()
enum class EExploredEnding : uint8
{
	None,
	Departed,
	Stayed
};

/**
 * Progreso de exploración de una partida (mapa, colecciones, barco). Es
 * serializable (se guarda tal cual en la partida) y sus reglas son funciones puras.
 */
USTRUCT()
struct EXPLORED_API FExploredProgress
{
	GENERATED_BODY()

	static constexpr int32 TotalPetroglyphs = 30;
	static constexpr int32 TotalBottles = 8;
	static constexpr int32 TotalViewpoints = 7;

	/** Contenido descubierto por id (petroglifos, botellas, miradores, especies…). */
	UPROPERTY()
	TSet<FName> Discovered;

	UPROPERTY()
	uint8 ShipParts = 0;

	UPROPERTY()
	bool bShipBuilt = false;

	UPROPERTY()
	bool bReachedHiddenIsland = false;

	UPROPERTY()
	TArray<EExploredEnding> EndingsSeen;

	/** Registra un descubrimiento. Devuelve true si es nuevo. */
	bool Discover(FName Id);
	bool IsDiscovered(FName Id) const { return Discovered.Contains(Id); }

	/** Cuenta los descubrimientos cuyo id empieza por el prefijo (p. ej. «petro_»). */
	int32 CountWithPrefix(const FString& Prefix) const;

	void AddShipPart(EShipPart Part) { ShipParts |= 1u << static_cast<uint8>(Part); }
	bool HasShipPart(EShipPart Part) const { return (ShipParts & (1u << static_cast<uint8>(Part))) != 0; }
	bool HasAllShipParts() const;

	bool CanBuildShip() const { return HasAllShipParts() && !bShipBuilt; }

	/** Zarpar hacia la isla oculta guiándose por las estrellas (GDD §6.3). */
	bool CanDepart() const { return bShipBuilt; }

	/** Quedarse (modo libre) es la alternativa a zarpar, una vez construido el barco. */
	bool CanChooseStay() const { return bShipBuilt; }

	/** Porcentaje del mapa completado (petroglifos, botellas, miradores; 0–100). */
	float MapCompletion() const;

	/** El mapa está completo cuando se ha reunido toda la colección. */
	bool IsMapComplete() const;
};
