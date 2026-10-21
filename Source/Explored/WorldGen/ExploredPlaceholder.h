#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ExploredPlaceholder.generated.h"

class UMaterialInterface;
class UStaticMeshComponent;
class UTextRenderComponent;
struct FIslandProp;
struct FIslandPropType;

/**
 * Reserva de una colocación de island_props.json cuyo actor de juego aún no existe (F3a). Si la
 * malla definitiva del GDD §11.1 ya está importada (/Game/Art/<Kit>/SM_<Nombre>_01) la usa a escala
 * 1; si no, pone la primitiva del motor escalada a su tamaño y coloreada con su celda de paleta
 * (M_Toon con PaletteIndex). La etiqueta de depuración (tipo e id) no existe en Shipping.
 *
 * Los tramos siguientes de F3 sustituyen cada tipo de juego por su actor registrando la clase en
 * AExploredIslandPopulator::ClassRegistry; el decorado puede quedarse como reserva hasta F4.
 */
UCLASS()
class EXPLORED_API AExploredPlaceholder : public AActor
{
	GENERATED_BODY()

public:
	AExploredPlaceholder();

	/** Configura malla, escala, color, colisión y etiqueta. ToonMaterial puede ser nulo (material por defecto). */
	void Configure(const FIslandProp& Prop, const FIslandPropType& Type, const FString& MeshPath, UMaterialInterface* ToonMaterial);

	/**
	 * Configura un actor creado en partida (sin colocación de island_props.json): usa el tipo TypeName
	 * de island_props.json si existe (si no, un cubo pequeño) y M_Toon. Id puede ser NAME_None.
	 */
	void ConfigureStandalone(FName Id, FName TypeName, const FString& MeshPath);

	/** Material de las primitivas de reserva (M_Toon). */
	static const TCHAR* ToonMaterialPath();

	UFUNCTION(BlueprintPure, Category = "Explored|Isla")
	FName GetPropId() const { return PropId; }

	UFUNCTION(BlueprintPure, Category = "Explored|Isla")
	FName GetPropType() const { return PropType; }

	/** true si usa la malla definitiva (no la primitiva de reserva). */
	UFUNCTION(BlueprintPure, Category = "Explored|Isla")
	bool UsesFinalMesh() const { return bFinalMesh; }

protected:
	/** Lo llama Configure al terminar: los actores de juego leen aquí cantidades, cocos o ids. */
	virtual void OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type) {}

	UPROPERTY(VisibleAnywhere, Category = "Explored|Isla")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Isla")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Etiqueta de depuración (tipo e id), orientada al sur. */
	UPROPERTY(VisibleAnywhere, Category = "Explored|Isla")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Isla")
	FName PropId;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Isla")
	FName PropType;

	UPROPERTY(VisibleAnywhere, Category = "Explored|Isla")
	bool bFinalMesh = false;
};
