#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Ruins/MuseumModel.h"
#include "Ruins/RuinsModel.h"
#include "WorldGen/PointsOfInterest.h"

#include "RuinsSubsystem.generated.h"

/** Un descubrimiento en una ruina (el resultado trae si la completa y qué enseña). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRuinDiscovery, FName /*ElementId*/, const FRuinDiscovery& /*Result*/);
/** Cambió algo del catálogo o de lo expuesto: los muebles del museo se redibujan. */
DECLARE_MULTICAST_DELEGATE(FOnMuseumChanged);

/**
 * Capa fina de Unreal sobre FRuinsModel y FMuseumModel: genera las ruinas y el
 * reparto de tesoros a partir de la semilla, carga artifacts.json y ruins.json y
 * recibe los eventos de descubrimiento de los puntos de interés y de la interacción
 * (AExploredRuinElement). Registra la sección «ruins» de la partida (ruinas y museo).
 * Toda la lógica vive en los modelos puros (ver RuinsSpec y MuseumSpec).
 */
UCLASS()
class EXPLORED_API URuinsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Genera ruinas, tesoros y museo para una semilla (en la partida, la oficial). */
	void BuildForSeed(uint32 Seed);

	/** Evento de un punto de interés o de la interacción: petroglifo visto, estatua alineada, ofrenda… */
	FRuinDiscovery NotifyElementDiscovered(FName ElementId);
	/** El jugador recoge un tesoro; la procedencia sale del reparto. Devuelve true si es nuevo. */
	bool NotifyArtifactFound(FName ArtifactId);
	/** La cámara desechable fotografía un tesoro (GDD §8.11). */
	bool NotifyArtifactPhotographed(FName ArtifactId);

	/** Operaciones del museo que avisan a los muebles. */
	EMuseumResult RegisterDisplay(int32 Key, FName DisplayId);
	EMuseumResult UnregisterDisplay(int32 Key, TArray<FName>& OutReturned);
	EMuseumResult PlaceArtifact(FName ArtifactId, int32 Key, int32 Slot);
	EMuseumResult RemoveArtifact(int32 Key, int32 Slot, FName& OutArtifact);

	const FRuinsModel& GetRuins() const { return Ruins; }
	const FMuseumModel& GetMuseum() const { return Museum; }
	const TArray<FArtifactPlacement>& GetArtifactPlacements() const { return Placements; }
	const TArray<FPointOfInterest>& GetPointsOfInterest() const { return Pois; }

	/** Anotaciones de wayfinding para la cartografía (P-MAP). */
	TArray<FWayfindingAnnotation> GetMapAnnotations() const { return Ruins.BuildAnnotations(); }
	/** Caminos de estrellas suficientes; la noche y el barco los comprueba quien llama. */
	bool CanSailToHiddenIsland() const { return Ruins.CanSailToHiddenIsland(); }

	/** Nombre de ruina, técnica, elemento, tesoro o mueble (ruins.json / artifacts.json). */
	FString GetDisplayName(FName Id, bool bEnglish = false) const;

	/** Estado plano para el guardado (P-SAVE). */
	const FRuinsState& GetRuinsState() const { return Ruins.GetState(); }
	const FMuseumState& GetMuseumState() const { return Museum.GetState(); }
	void LoadSavedState(const FRuinsState& RuinsState, const FMuseumState& MuseumState);

	FOnRuinDiscovery OnRuinDiscovery;
	FOnMuseumChanged OnMuseumChanged;

	/** Parseo de los JSON en funciones estáticas para poder validarlos sin mundo. */
	static bool ParseArtifactsJson(const FString& JsonText, FTreasureCatalog& OutCatalog, TMap<FName, FString>& OutNamesEs,
		TMap<FName, FString>& OutNamesEn, FString& OutError);
	static bool ParseRuinsJson(const FString& JsonText, TMap<FName, FString>& OutNamesEs, TMap<FName, FString>& OutNamesEn,
		FString& OutError);

private:
	void LoadDataFiles();
	/**
	 * Crea un AExploredRuinElement por cada elemento del modelo que no tenga ya un
	 * actor en el mapa, asentado en el suelo con una traza vertical (P-WIRE).
	 */
	void SpawnMissingElementActors(UWorld& World);

	FRuinsModel Ruins;
	FMuseumModel Museum;
	FTreasureCatalog Catalog;
	TArray<FArtifactPlacement> Placements;
	TArray<FPointOfInterest> Pois;
	TMap<FName, FString> NamesEs;
	TMap<FName, FString> NamesEn;
};
