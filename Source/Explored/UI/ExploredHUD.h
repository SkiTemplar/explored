#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "ExploredHUD.generated.h"

class AExploredCharacter;

/** Un aviso flotante (recogida, evento del cuerpo) con el tiempo que le queda en pantalla. */
struct FExploredHUDNotification
{
	FText Text;
	FLinearColor Color = FLinearColor::White;
	float RemainingSeconds = 0.0f;
	float TotalSeconds = 1.0f;
};

/**
 * HUD mínimo por Canvas (GDD §7): punto de mira, texto contextual del foco,
 * el nombre de lo que hay en cada mano y las necesidades del cuerpo (solo
 * cuando bajan: discreto por defecto, biblia §5.4 «HUD mínimo», GDD §8.3 «el
 * cuerpo como HUD»). Nada de widgets UMG.
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

	/** Cartel centrado (inicio de partida). */
	void ShowCard(const FText& Text, float DurationSeconds);

private:
	AExploredCharacter* GetExploredCharacter() const;
	void DrawReticle();
	void DrawContextPrompt(const AExploredCharacter& Character);
	void DrawHandLabels(const AExploredCharacter& Character);
	void DrawFishing(const AExploredCharacter& Character);
	/** Anillos/barras discretos de hambre, sed, energía y temperatura, más avisos de heridas, quemaduras y escorbuto. */
	void DrawBodyNeeds(const AExploredCharacter& Character);
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

	TArray<FExploredHUDNotification> Notifications;
	/** Subtítulos en cola: se muestra el primero. */
	TArray<FExploredHUDNotification> Subtitles;
	FExploredHUDNotification Page;
	FText PageTitle;
	FText PageNote;
	FExploredHUDNotification Card;
	bool bBoundToCarry = false;
};
