#include "UI/ExploredHUD.h"

#include "Carry/CarryComponent.h"
#include "Algo/Count.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Fishing/FishingComponent.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/InteractionComponent.h"
#include "Items/ItemRegistrySubsystem.h"
#include "Player/ExploredCharacter.h"
#include "Survival/BodySignalsComponent.h"
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

	if (AExploredCharacter* Character = GetExploredCharacter())
	{
		EnsureBoundToCarry(*Character);
		DrawContextPrompt(*Character);
		DrawHandLabels(*Character);
		DrawFishing(*Character);
		DrawBodyNeeds(*Character);
	}

	DrawNotifications();
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

void AExploredHUD::EnsureBoundToCarry(const AExploredCharacter& Character)
{
	if (bBoundToCarry)
	{
		return;
	}
	// GetCarryComponent es const; el delegado no lo es, así que hace falta el puntero mutable.
	if (UCarryComponent* Carry = Character.GetCarryComponent())
	{
		Carry->OnItemPickedUp.AddDynamic(this, &AExploredHUD::HandleItemPickedUp);
		bBoundToCarry = true;
	}
}

void AExploredHUD::HandleItemPickedUp(FText DisplayName)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("Item"), DisplayName);
	PushNotification(FText::Format(NSLOCTEXT("ExploredUI", "PickedUpItem", "Recogido: {Item}"), Args));
}

void AExploredHUD::PushNotification(const FText& Text, FLinearColor Color, float DurationSeconds)
{
	FExploredHUDNotification Notification;
	Notification.Text = Text;
	Notification.Color = Color;
	Notification.RemainingSeconds = DurationSeconds;
	Notification.TotalSeconds = FMath::Max(DurationSeconds, KINDA_SMALL_NUMBER);
	Notifications.Insert(Notification, 0);
	constexpr int32 MaxVisibleNotifications = 4;
	if (Notifications.Num() > MaxVisibleNotifications)
	{
		Notifications.SetNum(MaxVisibleNotifications);
	}
}

void AExploredHUD::DrawNotifications()
{
	if (Notifications.Num() == 0)
	{
		return;
	}

	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	for (int32 Index = Notifications.Num() - 1; Index >= 0; --Index)
	{
		Notifications[Index].RemainingSeconds -= DeltaSeconds;
		if (Notifications[Index].RemainingSeconds <= 0.0f)
		{
			Notifications.RemoveAt(Index);
		}
	}

	float Y = Canvas->SizeY * 0.42f;
	for (const FExploredHUDNotification& Notification : Notifications)
	{
		// Se desvanece en el último cuarto de su vida en vez de cortarse en seco.
		const float FadeWindow = Notification.TotalSeconds * 0.25f;
		const float Alpha = FadeWindow > 0.0f ? FMath::Clamp(Notification.RemainingSeconds / FadeWindow, 0.0f, 1.0f) : 1.0f;
		FLinearColor Color = Notification.Color;
		Color.A = Alpha;
		DrawCenteredText(Notification.Text, Color, Y);
		Y += 22.0f;
	}
}

void AExploredHUD::DrawBodyNeeds(const AExploredCharacter& Character)
{
	const UBodySignalsComponent* Body = Character.GetBodySignalsComponent();
	if (!Body)
	{
		return;
	}
	const FSurvivalState& State = Body->GetSurvivalState();

	struct FNeedBar
	{
		FText Label;
		float Value01 = 1.0f;
		FLinearColor Color;
	};

	// Temperatura corporal: 37 °C es cómodo; el aviso crece al alejarse en cualquier sentido
	// (frío o fiebre), en línea con FBodySignalsModel (mismo criterio: desviación, no umbral fijo).
	const float TempDeviation = FMath::Abs(State.BodyTemperature - 37.0f);
	const float Warmth01 = FMath::Clamp(1.0f - TempDeviation / 3.5f, 0.0f, 1.0f);

	const FNeedBar Bars[] = {
		{ NSLOCTEXT("ExploredUI", "NeedHunger", "Hambre"), State.Hunger / 100.0f, FLinearColor(0.80f, 0.55f, 0.20f, 1.0f) },
		{ NSLOCTEXT("ExploredUI", "NeedThirst", "Sed"), State.Thirst / 100.0f, FLinearColor(0.30f, 0.55f, 0.90f, 1.0f) },
		{ NSLOCTEXT("ExploredUI", "NeedEnergy", "Energía"), State.Energy / 100.0f, FLinearColor(0.85f, 0.80f, 0.30f, 1.0f) },
		{ NSLOCTEXT("ExploredUI", "NeedRest", "Sueño"), State.Rest / 100.0f, FLinearColor(0.55f, 0.45f, 0.85f, 1.0f) },
		{ NSLOCTEXT("ExploredUI", "NeedWarmth", "Temperatura"), Warmth01, FLinearColor(0.90f, 0.35f, 0.20f, 1.0f) },
	};

	// Discreto a propósito (biblia §5.4): por debajo de este umbral empieza a asomar
	// y se pone más opaco cuanto peor está, en vez de mostrar siempre las cinco barras.
	constexpr float VisibleThreshold = 0.55f;
	constexpr float BarWidth = 92.0f;
	constexpr float BarHeight = 5.0f;
	constexpr float RowGap = 15.0f;
	constexpr float Margin = 24.0f;

	float Y = Margin;
	for (const FNeedBar& Bar : Bars)
	{
		if (Bar.Value01 >= VisibleThreshold)
		{
			continue;
		}
		const float Urgency = FMath::Clamp(1.0f - (Bar.Value01 / VisibleThreshold), 0.0f, 1.0f);
		const float Alpha = FMath::Lerp(0.35f, 1.0f, Urgency);
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f * Alpha), Margin, Y, BarWidth, BarHeight);
		DrawRect(FLinearColor(Bar.Color.R, Bar.Color.G, Bar.Color.B, Alpha), Margin, Y, BarWidth * FMath::Max(Bar.Value01, 0.03f), BarHeight);
		DrawText(Bar.Label.ToString(), FLinearColor(1.0f, 1.0f, 1.0f, Alpha), Margin, Y - 14.0f);
		Y += BarHeight + RowGap;
	}

	// Heridas, quemaduras y escorbuto: texto solo si están activos (sin iconos, GDD §8.3).
	TArray<FText> Conditions;
	if (State.HasCondition(ECondition::Bleeding))
	{
		Conditions.Add(NSLOCTEXT("ExploredUI", "CondBleeding", "Sangrando"));
	}
	if (State.HasCondition(ECondition::SunBurn))
	{
		Conditions.Add(NSLOCTEXT("ExploredUI", "CondSunBurn", "Quemadura solar"));
	}
	if (State.HasCondition(ECondition::ContactBurn))
	{
		Conditions.Add(NSLOCTEXT("ExploredUI", "CondContactBurn", "Quemadura"));
	}
	if (State.HasCondition(ECondition::Fever))
	{
		Conditions.Add(NSLOCTEXT("ExploredUI", "CondFever", "Fiebre"));
	}
	if (State.HasCondition(ECondition::Infection))
	{
		Conditions.Add(NSLOCTEXT("ExploredUI", "CondInfection", "Herida infectada"));
	}
	if (State.HasCondition(ECondition::Sprain))
	{
		Conditions.Add(NSLOCTEXT("ExploredUI", "CondSprain", "Esguince"));
	}
	// Umbral tomado de FBodyModel::ScurvyStage (0.15 = primera etapa, encías); repetirlo aquí
	// evitaría el acoplamiento, pero el HUD solo necesita saber "ya hay algo que avisar".
	if (State.ScurvySeverity >= 0.15f)
	{
		Conditions.Add(NSLOCTEXT("ExploredUI", "CondScurvy", "Escorbuto"));
	}
	const int32 OpenWounds = Algo::CountIf(State.Wounds, [](const FWound& Wound) { return Wound.Healed < 1.0f && !Wound.bBandaged && !Wound.bBurn; });
	if (OpenWounds > 0)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Count"), OpenWounds);
		Conditions.Add(FText::Format(NSLOCTEXT("ExploredUI", "CondOpenWounds", "{Count} {Count}|plural(one=herida,other=heridas) sin vendar"), Args));
	}

	if (Conditions.Num() > 0)
	{
		const FText Joined = FText::Join(INVTEXT(" · "), Conditions);
		DrawText(Joined.ToString(), FLinearColor(0.95f, 0.30f, 0.22f, 0.95f), Margin, Y + 4.0f);
	}
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
