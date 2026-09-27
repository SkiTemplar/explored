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

private:
	AExploredCharacter* GetExploredCharacter() const;
	void DrawReticle();
	void DrawContextPrompt(const AExploredCharacter& Character);
	void DrawHandLabels(const AExploredCharacter& Character);
	void DrawFishing(const AExploredCharacter& Character);
	/** Anillos/barras discretos de hambre, sed, energía y temperatura, más avisos de heridas, quemaduras y escorbuto. */
	void DrawBodyNeeds(const AExploredCharacter& Character);
	void DrawNotifications();
	/** Se conecta una sola vez a UCarryComponent::OnItemPickedUp en cuanto hay personaje (el orden de BeginPlay no está garantizado). */
	void EnsureBoundToCarry(const AExploredCharacter& Character);
	/** Texto centrado en horizontal a la altura Y, con la fuente por defecto del HUD. */
	void DrawCenteredText(const FText& Text, const FLinearColor& Color, float Y);

	UFUNCTION()
	void HandleItemPickedUp(FText DisplayName);

	TArray<FExploredHUDNotification> Notifications;
	bool bBoundToCarry = false;
};
