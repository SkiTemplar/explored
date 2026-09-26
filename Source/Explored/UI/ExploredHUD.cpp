#include "UI/ExploredHUD.h"

#include "Carry/CarryComponent.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "InputCoreTypes.h"
#include "Fishing/FishingComponent.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/InteractionComponent.h"
#include "Items/ItemRegistrySubsystem.h"
#include "Player/ExploredCharacter.h"
#include "UI/ExploredInputSettingsSubsystem.h"
#include "UI/SettingsLogic.h"

AExploredCharacter* AExploredHUD::GetExploredCharacter() const
{
	const APlayerController* PC = GetOwningPlayerController();
	return PC ? Cast<AExploredCharacter>(PC->GetPawn()) : nullptr;
}

void AExploredHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	DrawReticle();

	if (const AExploredCharacter* Character = GetExploredCharacter())
	{
		DrawContextPrompt(*Character);
		DrawHandLabels(*Character);
		DrawFishing(*Character);
	}
}

void AExploredHUD::DrawReticle()
{
	const float CenterX = Canvas->SizeX * 0.5f;
	const float CenterY = Canvas->SizeY * 0.5f;
	constexpr float HalfSize = 4.0f;
	const FLinearColor ReticleColor(1.0f, 1.0f, 1.0f, 0.85f);

	DrawLine(CenterX - HalfSize, CenterY, CenterX + HalfSize, CenterY, ReticleColor, 1.5f);
	DrawLine(CenterX, CenterY - HalfSize, CenterX, CenterY + HalfSize, ReticleColor, 1.5f);
}

void AExploredHUD::DrawContextPrompt(const AExploredCharacter& Character)
{
	const UInteractionComponent* InteractionComp = Character.GetInteractionComponent();
	if (!InteractionComp || !InteractionComp->GetFocusedActor())
	{
		return;
	}

	const TArray<FText> Verbs = InteractionComp->GetContextVerbs();
	if (Verbs.Num() == 0)
	{
		return;
	}

	// La tecla es la que tenga asignada Interactuar (se puede remapear), no una «E» fija.
	const FName InteractAction(TEXT("IA_Interact"));
	FKey InteractKey(ExploredSettingsLogic::GetDefaultKeyFor(InteractAction));
	const APlayerController* PC = GetOwningPlayerController();
	if (const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr)
	{
		if (const UExploredInputSettingsSubsystem* InputSettings = LocalPlayer->GetSubsystem<UExploredInputSettingsSubsystem>())
		{
			InteractKey = InputSettings->GetKeyFor(InteractAction, InteractKey);
		}
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("Key"), InteractKey.GetDisplayName(false));
	Args.Add(TEXT("Actions"), FText::Join(INVTEXT(" · "), Verbs));
	const FText Prompt = FText::Format(NSLOCTEXT("ExploredUI", "InteractPrompt", "[{Key}] {Actions}"), Args);
	DrawCenteredText(Prompt, FLinearColor::White, Canvas->SizeY * 0.6f);
}

void AExploredHUD::DrawHandLabels(const AExploredCharacter& Character)
{
	const UCarryComponent* CarryComp = Character.GetCarryComponent();
	const UItemRegistrySubsystem* Registry = UItemRegistrySubsystem::Resolve(&Character);
	if (!CarryComp || !Registry)
	{
		return;
	}

	constexpr float Margin = 24.0f;
	const float Bottom = Canvas->SizeY - Margin - 16.0f;

	if (const FItemInstance* LeftItem = CarryComp->GetHandItemPtr(EHand::Left))
	{
		DrawText(Registry->GetDisplayName(*LeftItem).ToString(), FLinearColor::White, Margin, Bottom);
	}

	if (const FItemInstance* RightItem = CarryComp->GetHandItemPtr(EHand::Right))
	{
		const FString Text = Registry->GetDisplayName(*RightItem).ToString();
		float TextWidth = 0.0f, TextHeight = 0.0f;
		GetTextSize(Text, TextWidth, TextHeight);
		DrawText(Text, FLinearColor::White, Canvas->SizeX - Margin - TextWidth, Bottom);
	}
}

void AExploredHUD::DrawFishing(const AExploredCharacter& Character)
{
	const UFishingComponent* FishingComp = Character.GetFishingComponent();
	if (!FishingComp || FishingComp->GetSessionState() == EFishingSessionState::Idle)
	{
		return;
	}

	const float CenterX = Canvas->SizeX * 0.5f;
	const float Y = Canvas->SizeY * 0.7f;
	if (FishingComp->GetSessionState() == EFishingSessionState::Waiting)
	{
		DrawCenteredText(NSLOCTEXT("ExploredUI", "FishingWaiting", "Esperando la picada… [F] recoger"), FLinearColor(1.0f, 1.0f, 1.0f, 0.8f), Y);
		return;
	}

	// Barra de tensión: verde con el sedal cómodo, roja cerca de la rotura (GDD §8.9).
	constexpr float BarWidth = 240.0f;
	constexpr float BarHeight = 8.0f;
	const float Tension = FMath::Clamp(FishingComp->GetTension01(), 0.0f, 1.0f);
	const FLinearColor Fill = FLinearColor::LerpUsingHSV(FLinearColor(0.2f, 0.8f, 0.3f), FLinearColor(0.9f, 0.15f, 0.1f), Tension);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f), CenterX - BarWidth * 0.5f, Y, BarWidth, BarHeight);
	DrawRect(Fill, CenterX - BarWidth * 0.5f, Y, BarWidth * Tension, BarHeight);

	const FString Label = FText::Format(
		NSLOCTEXT("ExploredUI", "FishingLineOut", "Sedal fuera: {0} m · [R] recoger · [T] soltar"),
		FText::AsNumber(FMath::RoundToInt(FishingComp->GetLineOutM()))).ToString();
	float TextWidth = 0.0f, TextHeight = 0.0f;
	GetTextSize(Label, TextWidth, TextHeight);
	DrawText(Label, FLinearColor::White, CenterX - TextWidth * 0.5f, Y + BarHeight + 6.0f);
}

void AExploredHUD::DrawCenteredText(const FText& Text, const FLinearColor& Color, float Y)
{
	// FText directo al Canvas (sin pasar por FString): se re-traduce solo al cambiar de idioma.
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Canvas || !Font)
	{
		return;
	}
	float TextWidth = 0.0f, TextHeight = 0.0f;
	Canvas->TextSize(Font, Text.ToString(), TextWidth, TextHeight);
	FCanvasTextItem Item(FVector2D((Canvas->SizeX - TextWidth) * 0.5f, Y), Text, Font, Color);
	Canvas->DrawItem(Item);
}
