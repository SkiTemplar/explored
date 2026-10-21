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
#include "Narrative/BeaconSubsystem.h"
#include "UI/ExploredPlayerController.h"
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

	// Con el menú, la pausa o el final encima no se dibuja el HUD de juego.
	const AExploredPlayerController* ExploredPC = Cast<AExploredPlayerController>(GetOwningPlayerController());
	if (ExploredPC && (ExploredPC->IsCutsceneActive()
		|| ExploredPC->GetUIMode() != EExploredUIMode::Playing))
	{
		return;
	}

	DrawReticle();
	DrawObjective();

	if (AExploredCharacter* Character = GetExploredCharacter())
	{
		EnsureBoundToCarry(*Character);
		DrawContextPrompt(*Character);
		DrawHandLabels(*Character);
		DrawFishing(*Character);
		DrawBodyNeeds(*Character);
	}

	DrawNotifications();
	DrawSubtitle();
	DrawPage();
	DrawCard();
}

void AExploredHUD::DrawObjective()
{
	const UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(this);
	if (!Beacon)
	{
		return;
	}
	const FText Objective = Beacon->GetObjectiveText();
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (Objective.IsEmpty() || !Font)
	{
		return;
	}
	constexpr float Margin = 24.0f;
	const float Scale = TextScale();
	float Width = 0.0f;
	float Height = 0.0f;
	Canvas->TextSize(Font, Objective.ToString(), Width, Height, Scale, Scale);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.4f), Margin - 12.0f, Margin - 7.0f, Width + 24.0f, Height + 14.0f);
	FCanvasTextItem Item(FVector2D(Margin, Margin), Objective, Font, FLinearColor(1.0f, 0.9f, 0.55f, 0.97f));
	Item.Scale = FVector2D(Scale, Scale);
	Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
	Canvas->DrawItem(Item);
}

void AExploredHUD::ShowSubtitle(const FText& Text, float DurationSeconds)
{
	if (Text.IsEmpty())
	{
		return;
	}
	FExploredHUDNotification& Line = Subtitles.AddDefaulted_GetRef();
	Line.Text = Text;
	Line.Color = FLinearColor(0.96f, 0.94f, 0.86f, 1.0f);
	Line.TotalSeconds = FMath::Max(0.5f, DurationSeconds);
	Line.RemainingSeconds = Line.TotalSeconds;
}

void AExploredHUD::ShowPage(const FText& Title, const FText& Body, const FText& Note, float DurationSeconds)
{
	PageTitle = Title;
	PageNote = Note;
	Page.Text = Body;
	Page.Color = FLinearColor(0.16f, 0.12f, 0.08f, 1.0f);
	Page.TotalSeconds = FMath::Max(1.0f, DurationSeconds);
	Page.RemainingSeconds = Page.TotalSeconds;
}

void AExploredHUD::ShowCard(const FText& Text, float DurationSeconds)
{
	Card.Text = Text;
	Card.Color = FLinearColor::White;
	Card.TotalSeconds = FMath::Max(1.0f, DurationSeconds);
	Card.RemainingSeconds = Card.TotalSeconds;
}

float AExploredHUD::Age(FExploredHUDNotification& Item) const
{
	if (Item.RemainingSeconds <= 0.0f)
	{
		return 0.0f;
	}
	Item.RemainingSeconds -= GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	const float FadeWindow = Item.TotalSeconds * 0.25f;
	return FadeWindow > 0.0f ? FMath::Clamp(Item.RemainingSeconds / FadeWindow, 0.0f, 1.0f) : 1.0f;
}

TArray<FString> AExploredHUD::WrapLines(const FString& Text, UFont* Font, float Scale, float MaxWidth) const
{
	TArray<FString> Words;
	Text.ParseIntoArrayWS(Words);
	TArray<FString> Lines;
	FString Current;
	for (const FString& Word : Words)
	{
		const FString Candidate = Current.IsEmpty() ? Word : Current + TEXT(" ") + Word;
		float Width = 0.0f;
		float Height = 0.0f;
		Canvas->TextSize(Font, Candidate, Width, Height, Scale, Scale);
		if (Width > MaxWidth && !Current.IsEmpty())
		{
			Lines.Add(Current);
			Current = Word;
		}
		else
		{
			Current = Candidate;
		}
	}
	if (!Current.IsEmpty())
	{
		Lines.Add(Current);
	}
	return Lines;
}

void AExploredHUD::DrawSubtitle()
{
	while (Subtitles.Num() > 0)
	{
		const float Alpha = Age(Subtitles[0]);
		if (Alpha <= 0.0f)
		{
			Subtitles.RemoveAt(0);
			continue;
		}
		FLinearColor Color = Subtitles[0].Color;
		Color.A = Alpha;
		// Abajo, por encima de las barras del cuerpo (GDD v3 §9.1).
		DrawCenteredText(Subtitles[0].Text, Color, Canvas->SizeY * 0.80f, true);
		return;
	}
}

void AExploredHUD::DrawPage()
{
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	const float Alpha = Age(Page);
	if (Alpha <= 0.0f || !Font)
	{
		return;
	}
	const float Scale = TextScale();
	const float PanelWidth = FMath::Min(Canvas->SizeX * 0.6f, 900.0f * Scale);
	const float Padding = 28.0f * Scale;
	const float LineHeight = 28.0f * Scale;
	const TArray<FString> Body = WrapLines(Page.Text.ToString(), Font, Scale, PanelWidth - 2.0f * Padding);
	const TArray<FString> Note = WrapLines(PageNote.ToString(), Font, Scale, PanelWidth - 2.0f * Padding);
	const float PanelHeight = Padding * 2.0f + LineHeight * (Body.Num() + Note.Num() + 2.5f);
	const float X = (Canvas->SizeX - PanelWidth) * 0.5f;
	float Y = (Canvas->SizeY - PanelHeight) * 0.42f;
	// Papel: el panel del cuaderno es claro, con tinta oscura (Roig) y azul (la nota de Iria).
	DrawRect(FLinearColor(0.93f, 0.88f, 0.76f, 0.95f * Alpha), X, Y, PanelWidth, PanelHeight);
	Y += Padding;
	const auto DrawLine = [this, Font, Scale, X, Padding](const FString& Line, float LineY, const FLinearColor& Color)
	{
		FCanvasTextItem Item(FVector2D(X + Padding, LineY), FText::FromString(Line), Font, Color);
		Item.Scale = FVector2D(Scale, Scale);
		Canvas->DrawItem(Item);
	};
	FLinearColor Ink = Page.Color;
	Ink.A = Alpha;
	DrawLine(PageTitle.ToString(), Y, FLinearColor(0.45f, 0.18f, 0.12f, Alpha));
	Y += LineHeight * 1.5f;
	for (const FString& Line : Body)
	{
		DrawLine(Line, Y, Ink);
		Y += LineHeight;
	}
	Y += LineHeight;
	for (const FString& Line : Note)
	{
		DrawLine(Line, Y, FLinearColor(0.10f, 0.22f, 0.50f, Alpha));
		Y += LineHeight;
	}
}

void AExploredHUD::DrawCard()
{
	const float Alpha = Age(Card);
	if (Alpha <= 0.0f)
	{
		return;
	}
	DrawCenteredText(Card.Text, FLinearColor(1.0f, 1.0f, 1.0f, Alpha), Canvas->SizeY * 0.38f, true);
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

	// Tercio inferior, por debajo del texto de interacción (60 %): no tapa la retícula.
	float Y = Canvas->SizeY * 0.70f;
	for (const FExploredHUDNotification& Notification : Notifications)
	{
		// Se desvanece en el último cuarto de su vida en vez de cortarse en seco.
		const float FadeWindow = Notification.TotalSeconds * 0.25f;
		const float Alpha = FadeWindow > 0.0f ? FMath::Clamp(Notification.RemainingSeconds / FadeWindow, 0.0f, 1.0f) : 1.0f;
		FLinearColor Color = Notification.Color;
		Color.A = Alpha;
		DrawCenteredText(Notification.Text, Color, Y, true);
		Y += 30.0f * TextScale();
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
	// Bajo la línea del objetivo, que ocupa la esquina superior izquierda (GDD v3 §9.1) hasta F5.
	constexpr float BelowObjective = 56.0f;

	float Y = Margin + BelowObjective;
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
	const int32 OpenWounds = Algo::CountIf(State.Wounds, [](const FWound& Wound) { return Wound.Healed < 1.0f && !Wound.bBandaged; });
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

float AExploredHUD::TextScale() const
{
	// Tamaño pensado para 1080p; nunca por debajo del de la fuente mediana del motor.
	return Canvas ? FMath::Max(1.0f, Canvas->SizeY / 1080.0f * 1.15f) : 1.0f;
}

void AExploredHUD::DrawCenteredText(const FText& Text, const FLinearColor& Color, float Y, bool bBacking)
{
	// FText directo al Canvas (sin pasar por FString): se re-traduce solo al cambiar de idioma.
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Canvas || !Font)
	{
		return;
	}
	const float Scale = TextScale();
	float TextWidth = 0.0f, TextHeight = 0.0f;
	Canvas->TextSize(Font, Text.ToString(), TextWidth, TextHeight, Scale, Scale);
	if (bBacking)
	{
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.38f * Color.A), (Canvas->SizeX - TextWidth) * 0.5f - 14.0f, Y - 4.0f,
			TextWidth + 28.0f, TextHeight + 8.0f);
	}
	FCanvasTextItem Item(FVector2D((Canvas->SizeX - TextWidth) * 0.5f, Y), Text, Font, Color);
	Item.Scale = FVector2D(Scale, Scale);
	// Sombra: el texto se lee igual sobre arena clara que sobre el cielo.
	Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, Color.A * 0.75f));
	Canvas->DrawItem(Item);
}
