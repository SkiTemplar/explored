#include "UI/ExploredHUD.h"

#include "Carry/CarryComponent.h"
#include "Engine/Canvas.h"
#include "Fishing/FishingComponent.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/InteractionComponent.h"
#include "Items/ItemRegistrySubsystem.h"
#include "Player/ExploredCharacter.h"

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

	FString Prompt = TEXT("[E] ");
	for (int32 Index = 0; Index < Verbs.Num(); ++Index)
	{
		Prompt += Verbs[Index].ToString();
		if (Index + 1 < Verbs.Num())
		{
			Prompt += TEXT(" · ");
		}
	}

	float TextWidth = 0.0f, TextHeight = 0.0f;
	GetTextSize(Prompt, TextWidth, TextHeight);
	DrawText(Prompt, FLinearColor::White, (Canvas->SizeX - TextWidth) * 0.5f, Canvas->SizeY * 0.6f);
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
		const FString Waiting = TEXT("Esperando la picada… [F] recoger");
		float TextWidth = 0.0f, TextHeight = 0.0f;
		GetTextSize(Waiting, TextWidth, TextHeight);
		DrawText(Waiting, FLinearColor(1.0f, 1.0f, 1.0f, 0.8f), CenterX - TextWidth * 0.5f, Y);
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
