#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "ExploredHUD.generated.h"

class AExploredCharacter;
class UExploredSurvivorComponent;

/** Un aviso flotante (recogida, evento del cuerpo) con el tiempo que le queda en pantalla. */
struct FExploredHUDNotification
{
	FText Text;
	FLinearColor Color = FLinearColor::White;
	float RemainingSeconds = 0.0f;
	float TotalSeconds = 1.0f;
};

/**
 * HUD de Baliza Siete por Canvas (GDD v3 §9.1, sin pulido final hasta F5): 4 barras del cuerpo
 * abajo a la izquierda (sin números; parpadean bajo 25), 3 ranuras rápidas abajo en el centro,
 * aviso de interacción «E · Verbo objeto» bajo la retícula de punto, objetivo arriba a la
 * izquierda, «Día N · HH:MM» y el barómetro (si se lleva) arriba a la derecha, subtítulo de
 * pensamiento o aviso sobre las barras, barra de progreso de acciones largas y el cartel «Día N»
 * al empezar cada día. Las reglas y los textos están en FIslandHudModel. Nada de widgets UMG.
 */
UCLASS()
class EXPLORED_API AExploredHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Aviso flotante breve (recogida, hito). Lo puede llamar cualquier sistema del juego. */
	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void PushNotification(const FText& Text, FLinearColor Color = FLinearColor::White, float DurationSeconds = 2.5f);

	/**
	 * Pensamiento de Iria o aviso de la historia (GDD v3 §9.1): subtítulo abajo, sobre las barras.
	 * Si ya hay uno en pantalla, el nuevo espera su turno.
	 */
	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void ShowSubtitle(const FText& Text, float DurationSeconds = 4.0f);

	/** Página del cuaderno: título, texto de Roig y nota de Iria en un panel central. */
	void ShowPage(const FText& Title, const FText& Body, const FText& Note, float DurationSeconds);

	/** Cartel centrado (inicio de partida, «Día N», desmayo). Si ya hay uno, espera su turno. */
	void ShowCard(const FText& Text, float DurationSeconds);

private:
	AExploredCharacter* GetExploredCharacter() const;
	void DrawReticle();
	FString InteractKeyName() const;
	/** Tecla efectiva (remapeo incluido) de una acción remapeable, para los avisos. */
	FString KeyNameFor(FName ActionName) const;
	void DrawContextPrompt(const AExploredCharacter& Character);
	/** Sed, hambre, energía y calor: icono y barra fina, abajo a la izquierda. */
	void DrawBodyBars(const AExploredCharacter& Character);
	void DrawQuickSlots(const AExploredCharacter& Character);
	/** «Día N · HH:MM» y, si lleva el barómetro, «NNNN hPa ↓», arriba a la derecha. */
	void DrawClock(const AExploredCharacter* Character);
	float CurrentActionProgress(const AExploredCharacter& Character) const;
	void DrawActionBar(const AExploredCharacter& Character);
	/** Cartel «Día N» cuando cambia el día. */
	void UpdateDayCard();
	void EnsureBoundToSurvivor(const AExploredCharacter& Character);
	void DrawNotifications();
	/** Objetivo actual de la historia (objectives.json), una línea arriba a la izquierda (GDD v3 §9.1). */
	void DrawObjective();
	void DrawSubtitle();
	void DrawPage();
	void DrawCard();
	/** Parte un texto en líneas que caben en MaxWidth píxeles con la fuente y escala del HUD. */
	TArray<FString> WrapLines(const FString& Text, UFont* Font, float Scale, float MaxWidth) const;
	/** Resta el tiempo del fotograma y devuelve la opacidad (se desvanece el último cuarto); 0 = caducado. */
	float Age(FExploredHUDNotification& Item) const;
	/** Se conecta una sola vez a UCarryComponent::OnItemPickedUp en cuanto hay personaje (el orden de BeginPlay no está garantizado). */
	void EnsureBoundToCarry(const AExploredCharacter& Character);
	/** Texto centrado en horizontal a la altura Y, con la fuente por defecto del HUD. */
	/** Escala del texto del HUD según la resolución vertical. */
	float TextScale() const;
	void DrawCenteredText(const FText& Text, const FLinearColor& Color, float Y, bool bBacking = false);

	UFUNCTION()
	void HandleItemPickedUp(FText DisplayName);

	UFUNCTION()
	void HandleSurvivorMessage(const FText& Message);

	TArray<FExploredHUDNotification> Notifications;
	/** Subtítulos en cola: se muestra el primero. */
	TArray<FExploredHUDNotification> Subtitles;
	FExploredHUDNotification Page;
	FText PageTitle;
	FText PageNote;
	FExploredHUDNotification Card;
	/** Carteles en espera mientras se ve el actual. */
	TArray<FExploredHUDNotification> PendingCards;
	int32 LastCardDay = 0;
	bool bBoundToCarry = false;
	TWeakObjectPtr<UExploredSurvivorComponent> BoundSurvivor;
};
