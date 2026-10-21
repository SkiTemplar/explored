#include "UI/ExploredHUD.h"

#include "Carry/CarryComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Interaction/InteractionComponent.h"
#include "Narrative/BeaconSubsystem.h"
#include "Player/ExploredCharacter.h"
#include "UI/ExploredHudPainter.h"
#include "UI/ExploredInputSettingsSubsystem.h"
#include "UI/ExploredPlayerController.h"
#include "UI/ExploredUIPalette.h"
#include "UI/ExploredUIStyle.h"
#include "UI/SettingsLogic.h"
#include "UI/Widgets/SExploredHudOverlay.h"

#include "Cooking/ExploredFire.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Items/IslandDataLoader.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Survival/IslandBodyComponent.h"
#include "UI/IslandHudModel.h"
#include "Weather/ExploredWeatherSubsystem.h"

namespace HUDDetail
{
	using namespace ExploredPalette;

	const FName BarometerItem(TEXT("barometro"));
	constexpr float Margin = 36.0f;
	constexpr float TagHeight = 54.0f;
	constexpr float TagPadding = 16.0f;
	constexpr float TagIcon = 30.0f;
	constexpr float TextPx = 26.0f;
	constexpr float SlotSize = 92.0f;
	constexpr float SlotGap = 16.0f;

	FLinearColor PaperTag(float Alpha = 1.0f) { return Linear(Papel, 0.93f * Alpha); }
	FLinearColor Ink(float Alpha = 1.0f) { return Linear(Tinta, Alpha); }

	/** Etiqueta de papel con icono y texto; devuelve su ancho. Si bRightAligned, Pos.X es el borde derecho. */
	float Tag(FExploredHudPainter& P, FVector2f Pos, EExploredIcon Icon, const FString& Text, bool bRightAligned,
		const FLinearColor& IconColor, EExploredIcon TrailingIcon = EExploredIcon::Count, const FLinearColor& TrailingColor = FLinearColor::White)
	{
		const FSlateFontInfo Font = FExploredUIStyle::Get().Serif(TextPx);
		const FVector2f TextSize = FExploredHudPainter::Measure(Text, Font);
		const bool bTrailing = TrailingIcon != EExploredIcon::Count;
		const float Width = TagPadding * 2.0f + TagIcon + 12.0f + TextSize.X + (bTrailing ? 10.0f + TagIcon * 0.8f : 0.0f);
		const float X = bRightAligned ? Pos.X - Width : Pos.X;
		P.Round(FVector2f(X, Pos.Y), FVector2f(Width, TagHeight), 1, PaperTag());
		P.Icon(FVector2f(X + TagPadding, Pos.Y + (TagHeight - TagIcon) * 0.5f), TagIcon, Icon, IconColor);
		const float TextX = X + TagPadding + TagIcon + 12.0f;
		P.Text(FVector2f(TextX, Pos.Y + (TagHeight - TextSize.Y) * 0.5f), Text, Font, Ink());
		if (bTrailing)
		{
			const float Side = TagIcon * 0.8f;
			P.Icon(FVector2f(TextX + TextSize.X + 10.0f, Pos.Y + (TagHeight - Side) * 0.5f), Side, TrailingIcon, TrailingColor);
		}
		return Width;
	}

	/** Tecla dibujada como una tapa de papel con borde de tinta; devuelve su ancho. */
	float KeyCap(FExploredHudPainter& P, FVector2f Pos, const FString& Key, float Height)
	{
		const FSlateFontInfo Font = FExploredUIStyle::Get().SerifBold(24.0f);
		const FVector2f TextSize = FExploredHudPainter::Measure(Key, Font);
		const float Width = FMath::Max(Height, TextSize.X + 20.0f);
		P.Round(Pos, FVector2f(Width, Height), 0, Linear(Papel));
		P.Outline(Pos, FVector2f(Width, Height), Ink(0.9f));
		P.Text(FVector2f(Pos.X + (Width - TextSize.X) * 0.5f, Pos.Y + (Height - TextSize.Y) * 0.5f), Key, Font, Ink());
		return Width;
	}
}

AExploredHUD::AExploredHUD()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AExploredHUD::BeginPlay()
{
	Super::BeginPlay();
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!Viewport)
	{
		return;
	}
	Overlay = SNew(SExploredHudOverlay).HUD(TWeakObjectPtr<AExploredHUD>(this));
	// Debajo de la mochila (400), los menús (500) y el fundido (1000).
	Viewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 10);
}

void AExploredHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UWorld* World = GetWorld();
	if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr; Viewport && Overlay.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	}
	Overlay.Reset();
	Super::EndPlay(EndPlayReason);
}

AExploredCharacter* AExploredHUD::GetExploredCharacter() const
{
	const APlayerController* PC = GetOwningPlayerController();
	return PC ? Cast<AExploredCharacter>(PC->GetPawn()) : nullptr;
}

bool AExploredHUD::ShouldPaint() const
{
	// Con el menú, la pausa, el cuaderno abierto o el final encima no se pinta el HUD de juego.
	const AExploredPlayerController* PC = Cast<AExploredPlayerController>(GetOwningPlayerController());
	return !PC || (!PC->IsCutsceneActive() && !PC->IsNotebookOpen() && PC->GetUIMode() == EExploredUIMode::Playing);
}

void AExploredHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (int32 Index = Notifications.Num() - 1; Index >= 0; --Index)
	{
		if (AgeItem(Notifications[Index], DeltaSeconds) <= 0.0f)
		{
			Notifications.RemoveAt(Index);
		}
	}
	while (Subtitles.Num() > 0 && AgeItem(Subtitles[0], DeltaSeconds) <= 0.0f)
	{
		Subtitles.RemoveAt(0);
	}
	AgeItem(Page, DeltaSeconds);
	if (AgeItem(Card, DeltaSeconds) <= 0.0f && PendingCards.Num() > 0)
	{
		Card = PendingCards[0];
		PendingCards.RemoveAt(0);
	}
	if (ShouldPaint())
	{
		UpdateDayCard();
	}
	if (const AExploredCharacter* Character = GetExploredCharacter())
	{
		EnsureBound(*Character);
	}
}

float AExploredHUD::AgeItem(FExploredHUDNotification& Item, float DeltaSeconds)
{
	if (Item.RemainingSeconds <= 0.0f)
	{
		return 0.0f;
	}
	Item.RemainingSeconds -= DeltaSeconds;
	return AlphaOf(Item);
}

float AExploredHUD::AlphaOf(const FExploredHUDNotification& Item)
{
	// Se desvanece en el último cuarto de su vida en vez de cortarse en seco.
	const float FadeWindow = FMath::Min(Item.TotalSeconds * 0.25f, 1.0f);
	return FadeWindow > 0.0f ? FMath::Clamp(Item.RemainingSeconds / FadeWindow, 0.0f, 1.0f) : 0.0f;
}

void AExploredHUD::ClearTransient()
{
	Notifications.Reset();
	Subtitles.Reset();
	PendingCards.Reset();
	Page = FExploredHUDNotification();
	Card = FExploredHUDNotification();
	LastCardDay = 0;
}

void AExploredHUD::PaintOverlay(FExploredHudPainter& Painter)
{
	if (!ShouldPaint())
	{
		return;
	}
	AExploredCharacter* Character = GetExploredCharacter();
	PaintReticle(Painter);
	PaintObjective(Painter);
	PaintClock(Painter, Character);
	BarsTop = Painter.Size().Y - HUDDetail::Margin;
	if (Character)
	{
		PaintBodyBars(Painter, *Character);
		PaintQuickSlots(Painter, *Character);
		PaintPrompt(Painter, *Character);
		PaintActionBar(Painter, *Character);
	}
	PaintNotifications(Painter);
	PaintSubtitle(Painter);
	PaintPage(Painter);
	PaintCard(Painter);
}

void AExploredHUD::ShowSubtitle(const FText& Text, float DurationSeconds)
{
	if (Text.IsEmpty())
	{
		return;
	}
	FExploredHUDNotification& Line = Subtitles.AddDefaulted_GetRef();
	Line.Text = Text;
	Line.TotalSeconds = FMath::Max(0.5f, DurationSeconds);
	Line.RemainingSeconds = Line.TotalSeconds;
}

void AExploredHUD::ShowPage(const FText& Title, const FText& Body, const FText& Note, float DurationSeconds)
{
	PageTitle = Title;
	PageNote = Note;
	Page.Text = Body;
	Page.TotalSeconds = FMath::Max(1.0f, DurationSeconds);
	Page.RemainingSeconds = Page.TotalSeconds;
}

void AExploredHUD::ShowBlackout(const FText& Text, float DurationSeconds)
{
	// Va delante de cualquier cartel en cola: es lo que se ve durante el desmayo.
	Card.Text = Text;
	Card.bVeil = true;
	Card.TotalSeconds = FMath::Max(1.0f, DurationSeconds);
	Card.RemainingSeconds = Card.TotalSeconds;
}

void AExploredHUD::ShowCard(const FText& Text, float DurationSeconds)
{
	FExploredHUDNotification Next;
	Next.Text = Text;
	Next.TotalSeconds = FMath::Max(1.0f, DurationSeconds);
	Next.RemainingSeconds = Next.TotalSeconds;
	if (Card.RemainingSeconds > 0.0f)
	{
		PendingCards.Add(Next);
		return;
	}
	Card = Next;
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

void AExploredHUD::PaintReticle(FExploredHudPainter& P)
{
	// Retícula de punto (GDD v3 §9.1): papel con un aro de tinta, se ve sobre arena y sobre cielo.
	const FVector2f Center = P.Size() * 0.5f;
	P.Round(Center - FVector2f(5.0f, 5.0f), FVector2f(10.0f, 10.0f), 2, HUDDetail::Ink(0.7f));
	P.Round(Center - FVector2f(3.0f, 3.0f), FVector2f(6.0f, 6.0f), 2, ExploredPalette::Linear(ExploredPalette::Papel));
}

void AExploredHUD::PaintObjective(FExploredHudPainter& P)
{
	const UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(this);
	const FText Objective = Beacon ? Beacon->GetObjectiveText() : FText::GetEmpty();
	if (Objective.IsEmpty())
	{
		return;
	}
	HUDDetail::Tag(P, FVector2f(HUDDetail::Margin, HUDDetail::Margin), EExploredIcon::Faro, Objective.ToString(), false,
		ExploredPalette::Linear(ExploredPalette::RojoFaro));
}

void AExploredHUD::PaintClock(FExploredHudPainter& P, const AExploredCharacter* Character)
{
	using namespace ExploredPalette;
	const UWorld* World = GetWorld();
	const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	if (!Time)
	{
		return;
	}
	const float Right = P.Size().X - HUDDetail::Margin;
	const bool bNight = Time->IsNight();
	HUDDetail::Tag(P, FVector2f(Right, HUDDetail::Margin), bNight ? EExploredIcon::Luna : EExploredIcon::Sol,
		FIslandHudModel::ClockText(Time->GetDay(), Time->GetHours()), true, Linear(bNight ? CieloNoche : Fuego));
	const UExploredSurvivorComponent* Survivor = Character ? Character->GetSurvivorComponent() : nullptr;
	const UExploredWeatherSubsystem* Weather = World->GetSubsystem<UExploredWeatherSubsystem>();
	if (!Survivor || !Weather || Survivor->GetItemCount(HUDDetail::BarometerItem) <= 0)
	{
		return;
	}
	const EBarometerTrend Trend = Weather->GetPressureTrend();
	const EExploredIcon Arrow = Trend == EBarometerTrend::Rising ? EExploredIcon::FlechaSube
		: Trend == EBarometerTrend::Falling ? EExploredIcon::FlechaBaja : EExploredIcon::Estable;
	const FLinearColor ArrowColor = Linear(Trend == EBarometerTrend::Falling ? RojoFaro : Trend == EBarometerTrend::Rising ? HierbaOscura : Tinta);
	HUDDetail::Tag(P, FVector2f(Right, HUDDetail::Margin + HUDDetail::TagHeight + 12.0f), EExploredIcon::Barometro,
		FIslandHudModel::BarometerValueText(Weather->GetPressureHpa()), true, Linear(Laton), Arrow, ArrowColor);
}

FString AExploredHUD::KeyNameFor(FName ActionName) const
{
	// La tecla es la que tenga asignada la acción (se puede remapear), no una letra fija.
	FKey Key(ExploredSettingsLogic::GetDefaultKeyFor(ActionName));
	const APlayerController* PC = GetOwningPlayerController();
	if (const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr)
	{
		if (const UExploredInputSettingsSubsystem* InputSettings = LocalPlayer->GetSubsystem<UExploredInputSettingsSubsystem>())
		{
			Key = InputSettings->GetKeyFor(ActionName, Key);
		}
	}
	return Key.GetDisplayName(false).ToString();
}

void AExploredHUD::PaintPrompt(FExploredHudPainter& P, const AExploredCharacter& Character)
{
	const UInteractionComponent* InteractionComp = Character.GetInteractionComponent();
	const UIslandBodyComponent* Body = Character.GetIslandBodyComponent();
	TArray<TPair<FString, FString>> Segments;
	const FString InteractKey = KeyNameFor(FName(TEXT("IA_Interact")));
	if (InteractionComp && InteractionComp->GetFocusedActor())
	{
		const TArray<FText> Verbs = InteractionComp->GetContextVerbs();
		if (InteractionComp->FocusHasVerbChoice() && Verbs.Num() > 1)
		{
			// Hoguera con varios verbos: E ejecuta el primero y la tecla de combinar elige el siguiente.
			Segments.Emplace(InteractKey, Verbs[0].ToString());
			Segments.Emplace(KeyNameFor(FName(TEXT("IA_Combine"))), Verbs[1].ToString());
		}
		else if (Verbs.Num() > 0)
		{
			Segments.Emplace(InteractKey, FText::Join(INVTEXT(" · "), Verbs).ToString());
		}
	}
	else if (Body && Body->IsSheltered() && Body->CanSleepNow())
	{
		// Sin foco, dentro de un refugio: E es «Dormir» (solo a su hora).
		Segments.Emplace(InteractKey, NSLOCTEXT("ExploredUI", "PromptSleep", "Dormir").ToString());
	}
	if (Segments.Num() == 0 || InteractKey.IsEmpty())
	{
		return;
	}
	const FSlateFontInfo Font = FExploredUIStyle::Get().Serif(HUDDetail::TextPx);
	constexpr float Height = 50.0f;
	constexpr float KeyHeight = 38.0f;
	float Width = 0.0f;
	for (const TPair<FString, FString>& Segment : Segments)
	{
		Width += FMath::Max(KeyHeight, FExploredHudPainter::Measure(Segment.Key, FExploredUIStyle::Get().SerifBold(24.0f)).X + 20.0f)
			+ 12.0f + FExploredHudPainter::Measure(Segment.Value, Font).X + 22.0f;
	}
	Width += 12.0f;
	float X = (P.Size().X - Width) * 0.5f;
	const float Y = P.Size().Y * 0.5f + 40.0f;
	P.Round(FVector2f(X, Y), FVector2f(Width, Height), 2, HUDDetail::PaperTag());
	X += 12.0f;
	for (const TPair<FString, FString>& Segment : Segments)
	{
		X += HUDDetail::KeyCap(P, FVector2f(X, Y + (Height - KeyHeight) * 0.5f), Segment.Key, KeyHeight) + 12.0f;
		const FVector2f TextSize = FExploredHudPainter::Measure(Segment.Value, Font);
		P.Text(FVector2f(X, Y + (Height - TextSize.Y) * 0.5f), Segment.Value, Font, HUDDetail::Ink());
		X += TextSize.X + 22.0f;
	}
}

void AExploredHUD::PaintBodyBars(FExploredHudPainter& P, const AExploredCharacter& Character)
{
	using namespace ExploredPalette;
	const UIslandBodyComponent* Body = Character.GetIslandBodyComponent();
	if (!Body)
	{
		return;
	}
	struct FBarStyle
	{
		EBodyBar Bar;
		EExploredIcon Icon;
		FColor Color;
	};
	const FBarStyle Styles[] = {
		{ EBodyBar::Thirst, EExploredIcon::Sed, AguaMedia },
		{ EBodyBar::Hunger, EExploredIcon::Hambre, MaderaOscura },
		{ EBodyBar::Energy, EExploredIcon::Energia, HierbaOscura },
		{ EBodyBar::Warmth, EExploredIcon::Calor, Fuego },
	};
	constexpr float Icon = 34.0f;
	constexpr float Row = 44.0f;
	constexpr float BarWidth = 200.0f;
	constexpr float BarHeight = 12.0f;
	constexpr float Pad = 14.0f;
	const FVector2f CardSize(Pad * 2.0f + Icon + 14.0f + BarWidth, Pad * 2.0f + Row * 4.0f - (Row - Icon));
	const FVector2f CardPos(HUDDetail::Margin, P.Size().Y - HUDDetail::Margin - CardSize.Y);
	BarsTop = CardPos.Y;
	P.Round(CardPos, CardSize, 1, HUDDetail::PaperTag());
	const double Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	const float Low = Body->GetTuning().LowThreshold;
	float Y = CardPos.Y + Pad;
	for (const FBarStyle& Style : Styles)
	{
		const float Value = FIslandBodyModel::Get(Body->GetBars(), Style.Bar);
		const bool bLow = Value < Low;
		const bool bVisible = FIslandHudModel::IsBarVisible(Value, Low, Now);
		P.Icon(FVector2f(CardPos.X + Pad, Y), Icon, Style.Icon, Linear(bLow && !bVisible ? RojoFaro : Style.Color));
		const FVector2f BarPos(CardPos.X + Pad + Icon + 14.0f, Y + (Icon - BarHeight) * 0.5f);
		P.Round(BarPos, FVector2f(BarWidth, BarHeight), 2, Linear(ArenaOscura, 0.55f));
		const float Fill = BarWidth * FMath::Clamp(Value / 100.0f, 0.0f, 1.0f);
		if (bVisible && Fill > 1.0f)
		{
			P.Round(BarPos, FVector2f(FMath::Max(Fill, BarHeight), BarHeight), 2, Linear(bLow ? RojoFaro : Style.Color));
		}
		Y += Row;
	}
}

void AExploredHUD::PaintQuickSlots(FExploredHudPainter& P, const AExploredCharacter& Character)
{
	using namespace ExploredPalette;
	const UExploredSurvivorComponent* Survivor = Character.GetSurvivorComponent();
	if (!Survivor)
	{
		return;
	}
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	const TArray<FHudQuickSlot> Slots = FIslandHudModel::QuickSlots(Survivor->GetBackpack(), Survivor->GetSelectedQuickSlot());
	constexpr float Size = HUDDetail::SlotSize;
	const float Total = Slots.Num() * Size + (Slots.Num() - 1) * HUDDetail::SlotGap;
	const float Bottom = P.Size().Y - HUDDetail::Margin;
	float X = (P.Size().X - Total) * 0.5f;
	FString SelectedName;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const FHudQuickSlot& Slot = Slots[Index];
		const float Lift = Slot.bSelected ? 8.0f : 0.0f;
		const FVector2f Pos(X, Bottom - Size - Lift);
		P.Round(Pos, FVector2f(Size, Size), 1, HUDDetail::PaperTag(Slot.IsEmpty() ? 0.6f : 1.0f));
		if (Slot.bSelected)
		{
			P.Outline(Pos, FVector2f(Size, Size), Linear(Laton));
		}
		if (!Slot.IsEmpty())
		{
			P.Icon(Pos + FVector2f(14.0f, 14.0f), Size - 28.0f, ExploredIconSheet::FromId(Slot.ItemId), HUDDetail::Ink());
			if (Slot.Count > 1)
			{
				const FString Count = FString::Printf(TEXT("×%d"), Slot.Count);
				const FSlateFontInfo CountFont = Style.SerifBold(HUDDetail::TextPx - 2.0f);
				const FVector2f CountSize = FExploredHudPainter::Measure(Count, CountFont);
				P.Text(Pos + FVector2f(Size - CountSize.X - 8.0f, Size - CountSize.Y - 2.0f), Count, CountFont, Linear(RojoFaro));
			}
			if (Slot.bSelected)
			{
				SelectedName = ExploredIslandData::ItemName(Slot.ItemId).ToString();
			}
		}
		// Número de la tecla en un círculo de tinta sobre la esquina.
		const FString Key = FString::FromInt(Index + 1);
		const FSlateFontInfo KeyFont = Style.SerifBold(HUDDetail::TextPx - 3.0f);
		const FVector2f KeySize = FExploredHudPainter::Measure(Key, KeyFont);
		const FVector2f Badge = Pos - FVector2f(10.0f, 10.0f);
		P.Round(Badge, FVector2f(34.0f, 34.0f), 2, HUDDetail::Ink(0.92f));
		P.Text(Badge + (FVector2f(34.0f, 34.0f) - KeySize) * 0.5f, Key, KeyFont, Linear(Papel));
		X += Size + HUDDetail::SlotGap;
	}
	if (!SelectedName.IsEmpty())
	{
		const FSlateFontInfo Font = Style.SerifItalic(HUDDetail::TextPx - 2.0f);
		const FVector2f NameSize = FExploredHudPainter::Measure(SelectedName, Font);
		P.Text(FVector2f((P.Size().X - NameSize.X) * 0.5f, Bottom - Size - 10.0f - NameSize.Y - 6.0f), SelectedName, Font, Linear(Papel), true);
	}
}

float AExploredHUD::CurrentActionProgress(const AExploredCharacter& Character) const
{
	if (const UExploredSurvivorComponent* Survivor = Character.GetSurvivorComponent(); Survivor && Survivor->IsBusy())
	{
		return FMath::Max(0.001f, Survivor->GetActionProgress01());
	}
	// Cocinar o hervir en la hoguera enfocada (aceite 40 s, pescado 20 s).
	const UInteractionComponent* InteractionComp = Character.GetInteractionComponent();
	const AExploredFire* Fire = InteractionComp ? Cast<AExploredFire>(InteractionComp->GetFocusedActor()) : nullptr;
	if (!Fire || !Fire->HasJob() || Fire->IsJobReady())
	{
		return 0.0f;
	}
	const FCampfireJob& Job = Fire->GetCampfireState().Job;
	const FRecipeDef* Recipe = ExploredIslandData::Get().Recipes.Find(Job.RecipeId);
	const float Total = Recipe ? Recipe->Seconds : 0.0f;
	return FMath::Max(0.001f, FIslandHudModel::Progress01(Total - Job.RemainingSeconds, Total));
}

void AExploredHUD::PaintActionBar(FExploredHudPainter& P, const AExploredCharacter& Character)
{
	const float Progress = CurrentActionProgress(Character);
	if (Progress <= 0.0f)
	{
		return;
	}
	constexpr float Width = 360.0f;
	constexpr float Height = 14.0f;
	const FVector2f Pos((P.Size().X - Width) * 0.5f, P.Size().Y * 0.5f + 108.0f);
	P.Round(Pos - FVector2f(4.0f, 4.0f), FVector2f(Width + 8.0f, Height + 8.0f), 2, HUDDetail::Ink(0.6f));
	P.Round(Pos, FVector2f(FMath::Max(Width * Progress, Height), Height), 2, ExploredPalette::Linear(ExploredPalette::Laton));
}

void AExploredHUD::UpdateDayCard()
{
	const UWorld* World = GetWorld();
	const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	const UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(this);
	if (!Time || !Beacon || Beacon->GetStage() == EBeaconStage::Final)
	{
		return;
	}
	const int32 Day = Time->GetDay();
	if (Day != LastCardDay)
	{
		// El primero (día 1) va detrás del cartel de inicio, que ya está en cola.
		LastCardDay = Day;
		ShowCard(FText::FromString(FIslandHudModel::DayCardText(Day)), 3.5f);
	}
}

void AExploredHUD::PaintNotifications(FExploredHudPainter& P)
{
	const FSlateFontInfo Font = FExploredUIStyle::Get().Serif(HUDDetail::TextPx - 2.0f);
	// Encima de la tarjeta de las barras, apiladas hacia arriba: no tapan la retícula ni las ranuras.
	float Y = BarsTop - 14.0f;
	for (const FExploredHUDNotification& Notification : Notifications)
	{
		const float Alpha = AlphaOf(Notification);
		const FString Line = Notification.Text.ToString();
		const FVector2f TextSize = FExploredHudPainter::Measure(Line, Font);
		const FVector2f Box(TextSize.X + 32.0f, 44.0f);
		Y -= Box.Y;
		P.Round(FVector2f(HUDDetail::Margin, Y), Box, 1, HUDDetail::PaperTag(Alpha));
		P.Text(FVector2f(HUDDetail::Margin + 16.0f, Y + (Box.Y - TextSize.Y) * 0.5f), Line, Font, HUDDetail::Ink(Alpha));
		Y -= 8.0f;
	}
}

void AExploredHUD::PaintSubtitle(FExploredHudPainter& P)
{
	if (Subtitles.Num() == 0)
	{
		return;
	}
	const float Alpha = AlphaOf(Subtitles[0]);
	const FSlateFontInfo Font = FExploredUIStyle::Get().SerifItalic(28.0f);
	const TArray<FString> Lines = FExploredHudPainter::Wrap(Subtitles[0].Text.ToString(), Font, 1040.0f);
	constexpr float LineHeight = 38.0f;
	float Width = 0.0f;
	for (const FString& Line : Lines)
	{
		Width = FMath::Max(Width, FExploredHudPainter::Measure(Line, Font).X);
	}
	// Abajo, sobre las ranuras rápidas (GDD v3 §9.1).
	const FVector2f Box(Width + 48.0f, LineHeight * Lines.Num() + 20.0f);
	const FVector2f Pos((P.Size().X - Box.X) * 0.5f, P.Size().Y - HUDDetail::Margin - HUDDetail::SlotSize - 64.0f - Box.Y);
	P.Round(Pos, Box, 1, HUDDetail::Ink(0.74f * Alpha));
	float Y = Pos.Y + 10.0f;
	for (const FString& Line : Lines)
	{
		const float LineWidth = FExploredHudPainter::Measure(Line, Font).X;
		P.Text(FVector2f((P.Size().X - LineWidth) * 0.5f, Y), Line, Font, ExploredPalette::Linear(ExploredPalette::Papel, Alpha));
		Y += LineHeight;
	}
}

void AExploredHUD::PaintPage(FExploredHudPainter& P)
{
	using namespace ExploredPalette;
	const float Alpha = AlphaOf(Page);
	if (Alpha <= 0.0f)
	{
		return;
	}
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	constexpr float Width = 940.0f;
	constexpr float Pad = 44.0f;
	constexpr float MarginLine = 72.0f;
	constexpr float LineHeight = 36.0f;
	const FSlateFontInfo BodyFont = Style.Serif(25.0f);
	const FSlateFontInfo NoteFont = Style.SerifItalic(25.0f);
	const TArray<FString> Body = FExploredHudPainter::Wrap(Page.Text.ToString(), BodyFont, Width - MarginLine - Pad);
	const TArray<FString> Note = FExploredHudPainter::Wrap(PageNote.ToString(), NoteFont, Width - MarginLine - Pad);
	const float Height = Pad * 2.0f + 52.0f + LineHeight * (Body.Num() + Note.Num()) + (Note.Num() > 0 ? 24.0f : 0.0f);
	const FVector2f Pos((P.Size().X - Width) * 0.5f, FMath::Max(40.0f, (P.Size().Y - Height) * 0.42f));
	// Hoja de cuaderno: papel, margen rojo, texto de Roig en tinta y la nota de Iria en azul.
	P.Round(Pos + FVector2f(6.0f, 8.0f), FVector2f(Width, Height), 0, Linear(Tinta, 0.35f * Alpha));
	P.Round(Pos, FVector2f(Width, Height), 0, Linear(Papel, Alpha));
	P.Round(Pos + FVector2f(MarginLine - 16.0f, 0.0f), FVector2f(2.0f, Height), 0, Linear(RojoFaro, 0.7f * Alpha));
	const float TextX = Pos.X + MarginLine;
	float Y = Pos.Y + Pad;
	P.Text(FVector2f(TextX, Y), PageTitle.ToString(), Style.SerifBold(27.0f), Linear(MaderaOscura, Alpha));
	Y += 52.0f;
	for (const FString& Line : Body)
	{
		P.Text(FVector2f(TextX, Y), Line, BodyFont, Linear(Tinta, Alpha));
		Y += LineHeight;
	}
	Y += 24.0f;
	for (const FString& Line : Note)
	{
		P.Text(FVector2f(TextX, Y), Line, NoteFont, Linear(AguaProfunda, Alpha));
		Y += LineHeight;
	}
}

void AExploredHUD::PaintCard(FExploredHudPainter& P)
{
	using namespace ExploredPalette;
	const float Alpha = AlphaOf(Card);
	if (Alpha <= 0.0f)
	{
		return;
	}
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	const FString Text = Card.Text.ToString();
	if (Card.bVeil)
	{
		P.Round(FVector2f::ZeroVector, P.Size(), 0, Linear(Tinta, FMath::Min(1.0f, Alpha * 1.5f)));
	}
	const FSlateFontInfo Big = Style.SerifBold(84.0f);
	const FVector2f BigSize = FExploredHudPainter::Measure(Text, Big);
	const float Y = P.Size().Y * 0.26f;
	if (BigSize.X <= 900.0f)
	{
		// «Día 2»: grande, en papel con sombra y una raya de latón debajo.
		P.Text(FVector2f((P.Size().X - BigSize.X) * 0.5f, Y), Text, Big, Linear(Papel, Alpha), true);
		const float Rule = FMath::Max(160.0f, BigSize.X * 0.6f);
		P.Round(FVector2f((P.Size().X - Rule) * 0.5f + 2.0f, Y + BigSize.Y + 6.0f), FVector2f(Rule, 4.0f), 2, Linear(Tinta, 0.5f * Alpha));
		P.Round(FVector2f((P.Size().X - Rule) * 0.5f, Y + BigSize.Y + 4.0f), FVector2f(Rule, 4.0f), 2, Linear(Laton, Alpha));
		return;
	}
	// Carteles largos (inicio, desmayo): cursiva sobre una banda de tinta.
	const FSlateFontInfo Font = Style.SerifItalic(34.0f);
	const TArray<FString> Lines = FExploredHudPainter::Wrap(Text, Font, 1100.0f);
	constexpr float LineHeight = 46.0f;
	const FVector2f Box(1180.0f, LineHeight * Lines.Num() + 36.0f);
	// El desmayo va centrado en el negro; el resto de carteles largos, en el tercio superior.
	const FVector2f Pos((P.Size().X - Box.X) * 0.5f, Card.bVeil ? (P.Size().Y - Box.Y) * 0.5f : Y);
	P.Round(Pos, Box, 1, Linear(Tinta, 0.72f * Alpha));
	float LineY = Pos.Y + 18.0f;
	for (const FString& Line : Lines)
	{
		const float LineWidth = FExploredHudPainter::Measure(Line, Font).X;
		P.Text(FVector2f((P.Size().X - LineWidth) * 0.5f, LineY), Line, Font, Linear(Papel, Alpha));
		LineY += LineHeight;
	}
}

void AExploredHUD::EnsureBound(const AExploredCharacter& Character)
{
	UExploredSurvivorComponent* Survivor = Character.GetSurvivorComponent();
	if (Survivor && BoundSurvivor.Get() != Survivor)
	{
		Survivor->OnMessage.AddDynamic(this, &AExploredHUD::HandleSurvivorMessage);
		BoundSurvivor = Survivor;
	}
	if (bBoundToCarry)
	{
		return;
	}
	// El orden de BeginPlay no está garantizado: se conecta en cuanto hay personaje.
	if (UCarryComponent* Carry = Character.GetCarryComponent())
	{
		Carry->OnItemPickedUp.AddDynamic(this, &AExploredHUD::HandleItemPickedUp);
		bBoundToCarry = true;
	}
}

void AExploredHUD::HandleSurvivorMessage(const FText& Message)
{
	ShowSubtitle(Message, 3.0f);
}

void AExploredHUD::HandleItemPickedUp(FText DisplayName)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("Item"), DisplayName);
	PushNotification(FText::Format(NSLOCTEXT("ExploredUI", "PickedUpItem", "Recogido: {Item}"), Args));
}
