#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class APlayerController;
class SBox;

/**
 * Vista mínima del inventario diegético (GDD §8.2): mientras la mochila está
 * abierta (Tab / cruceta derecha → AExploredCharacter::IsBackpackOpen), una
 * etiqueta de papel al lado derecho enseña lo que hay en cada mano, los
 * bolsillos, el cinturón, la bolsa estanca, la mochila y las angarillas (lo
 * que se lleve), con huecos, volumen y el peso frente a la capacidad cómoda.
 *
 * Es solo lectura y no toma el foco: el juego sigue y el personaje se puede
 * mover con la mochila abierta. La visibilidad se consulta al personaje en
 * cada evaluación; mientras se ve, se reconstruye cuando cambia lo que se
 * lleva (una firma barata en cada Tick). TODO(P-UI3): el interior 3D de la mochila
 * y sacar las cosas con la mano (GDD §8.2).
 *
 * Qué secciones se ven y el color de la carga: ExploredScreens (ScreensLogicSpec).
 */
class EXPLORED_API SExploredInventoryPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredInventoryPanel) {}
		/** Controlador local: el personaje se busca en cada Tick (puede cambiar de peón). */
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	EVisibility GetPanelVisibility() const;
	void Rebuild();
	/** Resumen barato de lo que se lleva para saber si hay que reconstruir. */
	FString ComputeSignature() const;

	TWeakObjectPtr<APlayerController> Owner;
	TSharedPtr<SBox> Content;
	FString LastSignature;
};
