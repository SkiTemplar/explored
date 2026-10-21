#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class APlayerController;
class AExploredCharacter;
class SBox;
struct FBackpackMenuEntry;

/**
 * Inventario de Baliza Siete (Tab, GDD v3 §9.2): los huecos de la mochila (FBackpackState de
 * UExploredSurvivorComponent), la carga pesada de las manos y las recetas de mano.
 *
 * Aspecto de hoja de cuaderno: 8 huecos con el icono de cada objeto; un clic elige uno y enseña su
 * descripción con «Usar» (comer o beber) y «Soltar»; cada receta, un botón con su verbo («Trenzar»)
 * que se activa cuando hay material, con los iconos de lo que pide. Los botones llaman a AExploredCharacter
 * (UseBackpackItem, DropBackpackItem, CraftFromBackpack), que valida con FBackpackMenuModel. Con el
 * panel abierto AExploredPlayerController enseña el cursor; los botones no toman el foco, así que el
 * teclado sigue en el juego (Tab cierra). Se reconstruye cuando cambia la mochila (firma barata en Tick).
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
	/** Resumen barato de la mochila para saber si hay que reconstruir. */
	FString ComputeSignature() const;
	AExploredCharacter* ResolveCharacter() const;

	TSharedRef<SWidget> MakeSlot(int32 Index, const struct FBackpackMenuEntry* Entry);
	TSharedRef<SWidget> MakeDetails(const struct FBackpackMenuEntry* Entry, bool bFree);

	TWeakObjectPtr<APlayerController> Owner;
	TSharedPtr<SBox> Content;
	FString LastSignature;
	/** Objeto elegido con un clic (su descripción y sus botones); None = el primero. */
	FName SelectedItem;
};
