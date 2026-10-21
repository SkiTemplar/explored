#include "UI/ExploredHUD.h"

#include "Carry/CarryComponent.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/InteractionComponent.h"
#include "Narrative/BeaconSubsystem.h"
#include "UI/ExploredPlayerController.h"
#include "Player/ExploredCharacter.h"
#include "UI/ExploredInputSettingsSubsystem.h"
#include "UI/SettingsLogic.h"

#include "Cooking/ExploredFire.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Items/IslandDataLoader.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Survival/IslandBodyComponent.h"
#include "UI/IslandHudModel.h"
#include "Weather/ExploredWeatherSubsystem.h"

namespace HUDDetail
{
	const FName BarometerItem(TEXT("barometro"));
}
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
	UpdateDayCard();

	AExploredCharacter* Character = GetExploredCharacter();
	DrawClock(Character);
	if (Character)
	{
		EnsureBoundToCarry(*Character);
		EnsureBoundToSurvivor(*Character);
		DrawContextPrompt(*Character);
		DrawActionBar(*Character);
		DrawBodyBars(*Character);
		DrawQuickSlots(*Character);
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
	FExploredHUDNotification Next;
	Next.Text = Text;
	Next.Color = FLinearColor::White;
	Next.TotalSeconds = FMath::Max(1.0f, DurationSeconds);
	Next.RemainingSeconds = Next.TotalSeconds;
	if (Card.RemainingSeconds > 0.0f)
	{
		PendingCards.Add(Next);
		return;
	}
	Card = Next;
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
		DrawCenteredText(Subtitles[0].Text, Color, Canvas->SizeY * 0.74f, true);
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
	float Alpha = Age(Card);
	if (Alpha <= 0.0f && PendingCards.Num() > 0)
	{
		Card = PendingCards[0];
		PendingCards.RemoveAt(0);
		Alpha = Age(Card);
	}
	if (Alpha <= 0.0f)
	{
		return;
	}
	DrawCenteredText(Card.Text, FLinearColor(1.0f, 1.0f, 1.0f, Alpha), Canvas->SizeY * 0.38f, true);
}

void AExploredHUD::DrawReticle()
{
	// Retícula de punto (GDD v3 §9.1).
	const float Size = FMath::Max(3.0f, 4.0f * TextScale());
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), Canvas->SizeX * 0.5f - Size * 0.5f - 1.0f, Canvas->SizeY * 0.5f - Size * 0.5f - 1.0f, Size + 2.0f, Size + 2.0f);
	DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.9f), Canvas->SizeX * 0.5f - Size * 0.5f, Canvas->SizeY * 0.5f - Size * 0.5f, Size, Size);
}

FString AExploredHUD::InteractKeyName() const
{
	// La tecla es la que tenga asignada Interactuar (se puede remapear), no una «E» fija.
	return KeyNameFor(FName(TEXT("IA_Interact")));
}

FString AExploredHUD::KeyNameFor(FName ActionName) const
{
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

void AExploredHUD::DrawContextPrompt(const AExploredCharacter& Character)
{
	const UInteractionComponent* InteractionComp = Character.GetInteractionComponent();
	const UIslandBodyComponent* Body = Character.GetIslandBodyComponent();
	FString Action;
	if (InteractionComp && InteractionComp->GetFocusedActor())
	{
		const TArray<FText> Verbs = InteractionComp->GetContextVerbs();
		if (InteractionComp->FocusHasVerbChoice() && Verbs.Num() > 1)
		{
			// Hoguera con varios verbos: E ejecuta el primero y la tecla de combinar elige el siguiente.
			TArray<FString> Names;
			for (const FText& Verb : Verbs)
			{
				Names.Add(Verb.ToString());
			}
			const FString Choice = FIslandHudModel::ChoicePromptText(InteractKeyName(), KeyNameFor(FName(TEXT("IA_Combine"))), Names);
			DrawCenteredText(FText::FromString(Choice), FLinearColor::White, Canvas->SizeY * 0.55f, true);
			return;
		}
		if (Verbs.Num() > 0)
		{
			Action = FText::Join(INVTEXT(" · "), Verbs).ToString();
		}
	}
	else if (Body && Body->IsSheltered())
	{
		// Sin foco, dentro de un refugio: E es «Dormir» (solo a su hora).
		Action = Body->CanSleepNow() ? NSLOCTEXT("ExploredUI", "PromptSleep", "Dormir").ToString() : FString();
	}
	const FString Prompt = FIslandHudModel::PromptText(InteractKeyName(), Action);
	if (!Prompt.IsEmpty())
	{
		DrawCenteredText(FText::FromString(Prompt), FLinearColor::White, Canvas->SizeY * 0.55f, true);
	}
}

void AExploredHUD::DrawBodyBars(const AExploredCharacter& Character)
{
	const UIslandBodyComponent* Body = Character.GetIslandBodyComponent();
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Body || !Font)
	{
		return;
	}
	struct FBarStyle
	{
		EBodyBar Bar;
		const TCHAR* Glyph;
		FLinearColor Color;
	};
	// Iconos planos de un color (provisionales hasta F5): la inicial de la barra sobre su color.
	const FBarStyle Styles[] = {
		{ EBodyBar::Thirst, TEXT("S"), FLinearColor(0.36f, 0.62f, 0.86f) },
		{ EBodyBar::Hunger, TEXT("H"), FLinearColor(0.80f, 0.55f, 0.24f) },
		{ EBodyBar::Energy, TEXT("E"), FLinearColor(0.85f, 0.78f, 0.30f) },
		{ EBodyBar::Warmth, TEXT("C"), FLinearColor(0.85f, 0.33f, 0.22f) },
	};
	const float Scale = TextScale();
	const float Icon = 22.0f * Scale;
	const float BarWidth = 150.0f * Scale;
	const float BarHeight = 6.0f * Scale;
	const float Gap = 8.0f * Scale;
	const float Margin = 28.0f * Scale;
	const double Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	const float Low = Body->GetTuning().LowThreshold;
	float Y = Canvas->SizeY - Margin - 4.0f * (Icon + Gap);
	for (const FBarStyle& Style : Styles)
	{
		const float Value = FIslandBodyModel::Get(Body->GetBars(), Style.Bar);
		if (FIslandHudModel::IsBarVisible(Value, Low, Now))
		{
			DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f), Margin - 4.0f, Y - 4.0f, Icon + BarWidth + Gap + 12.0f, Icon + 8.0f);
			DrawRect(Style.Color, Margin, Y, Icon, Icon);
			FCanvasTextItem Glyph(FVector2D(Margin + Icon * 0.28f, Y + Icon * 0.05f), FText::FromString(Style.Glyph), Font, FLinearColor(0.08f, 0.08f, 0.08f));
			Glyph.Scale = FVector2D(Scale, Scale);
			Canvas->DrawItem(Glyph);
			const float BarY = Y + (Icon - BarHeight) * 0.5f;
			DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.18f), Margin + Icon + Gap, BarY, BarWidth, BarHeight);
			DrawRect(Style.Color, Margin + Icon + Gap, BarY, BarWidth * FMath::Clamp(Value / 100.0f, 0.0f, 1.0f), BarHeight);
		}
		Y += Icon + Gap;
	}
}

void AExploredHUD::DrawQuickSlots(const AExploredCharacter& Character)
{
	const UExploredSurvivorComponent* Survivor = Character.GetSurvivorComponent();
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Survivor || !Font)
	{
		return;
	}
	const TArray<FHudQuickSlot> Slots = FIslandHudModel::QuickSlots(Survivor->GetBackpack(), Survivor->GetSelectedQuickSlot());
	const float Scale = TextScale();
	const float Size = 72.0f * Scale;
	const float Gap = 10.0f * Scale;
	const float Total = Slots.Num() * Size + (Slots.Num() - 1) * Gap;
	const float Top = Canvas->SizeY - 28.0f * Scale - Size;
	float X = (Canvas->SizeX - Total) * 0.5f;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const FHudQuickSlot& Slot = Slots[Index];
		const FLinearColor Border = Slot.bSelected ? FLinearColor(0.85f, 0.64f, 0.25f, 1.0f) : FLinearColor(1.0f, 1.0f, 1.0f, 0.25f);
		const float Thick = Slot.bSelected ? 3.0f : 1.5f;
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.4f), X, Top, Size, Size);
		DrawRect(Border, X, Top, Size, Thick);
		DrawRect(Border, X, Top + Size - Thick, Size, Thick);
		DrawRect(Border, X, Top, Thick, Size);
		DrawRect(Border, X + Size - Thick, Top, Thick, Size);
		const auto Text = [this, Font, Scale](const FString& Line, float TX, float TY, const FLinearColor& Color)
		{
			FCanvasTextItem Item(FVector2D(TX, TY), FText::FromString(Line), Font, Color);
			Item.Scale = FVector2D(Scale, Scale);
			Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
			Canvas->DrawItem(Item);
		};
		Text(FString::FromInt(Index + 1), X + 5.0f * Scale, Top + 3.0f * Scale, FLinearColor(1.0f, 1.0f, 1.0f, 0.7f));
		if (!Slot.IsEmpty())
		{
			const TArray<FString> Lines = WrapLines(ExploredIslandData::ItemName(Slot.ItemId).ToString(), Font, Scale, Size - 8.0f * Scale);
			float LineY = Top + Size * 0.36f;
			for (int32 Line = 0; Line < FMath::Min(2, Lines.Num()); ++Line)
			{
				Text(Lines[Line], X + 5.0f * Scale, LineY, FLinearColor::White);
				LineY += 14.0f * Scale;
			}
			if (Slot.Count > 1)
			{
				Text(FString::Printf(TEXT("×%d"), Slot.Count), X + Size - 26.0f * Scale, Top + 3.0f * Scale, FLinearColor(1.0f, 0.9f, 0.6f));
			}
		}
		X += Size + Gap;
	}
}

void AExploredHUD::DrawClock(const AExploredCharacter* Character)
{
	const UWorld* World = GetWorld();
	const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Time || !Font)
	{
		return;
	}
	const float Scale = TextScale();
	const float Margin = 24.0f;
	TArray<FString> Lines;
	Lines.Add(FIslandHudModel::ClockText(Time->GetDay(), Time->GetHours()));
	const UExploredSurvivorComponent* Survivor = Character ? Character->GetSurvivorComponent() : nullptr;
	const UExploredWeatherSubsystem* Weather = World->GetSubsystem<UExploredWeatherSubsystem>();
	if (Survivor && Weather && Survivor->GetItemCount(HUDDetail::BarometerItem) > 0)
	{
		Lines.Add(FIslandHudModel::BarometerText(Weather->GetPressureHpa(), Weather->GetPressureTrend()));
	}
	float Y = Margin;
	for (const FString& Line : Lines)
	{
		float Width = 0.0f;
		float Height = 0.0f;
		Canvas->TextSize(Font, Line, Width, Height, Scale, Scale);
		const float X = Canvas->SizeX - Margin - Width;
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.4f), X - 12.0f, Y - 7.0f, Width + 24.0f, Height + 14.0f);
		FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Line), Font, FLinearColor(0.97f, 0.95f, 0.88f, 0.97f));
		Item.Scale = FVector2D(Scale, Scale);
		Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
		Canvas->DrawItem(Item);
		Y += Height + 18.0f;
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

void AExploredHUD::DrawActionBar(const AExploredCharacter& Character)
{
	const float Progress = CurrentActionProgress(Character);
	if (Progress <= 0.0f)
	{
		return;
	}
	const float Scale = TextScale();
	const float Width = 320.0f * Scale;
	const float Height = 8.0f * Scale;
	const float X = (Canvas->SizeX - Width) * 0.5f;
	const float Y = Canvas->SizeY * 0.62f;
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), X - 2.0f, Y - 2.0f, Width + 4.0f, Height + 4.0f);
	DrawRect(FLinearColor(0.85f, 0.64f, 0.25f, 0.95f), X, Y, Width * Progress, Height);
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
		ShowCard(FText::FromString(FIslandHudModel::DayCardText(Day)), 3.0f);
	}
}

void AExploredHUD::EnsureBoundToSurvivor(const AExploredCharacter& Character)
{
	UExploredSurvivorComponent* Survivor = Character.GetSurvivorComponent();
	if (!Survivor || BoundSurvivor.Get() == Survivor)
	{
		return;
	}
	Survivor->OnMessage.AddDynamic(this, &AExploredHUD::HandleSurvivorMessage);
	BoundSurvivor = Survivor;
}

void AExploredHUD::HandleSurvivorMessage(const FText& Message)
{
	ShowSubtitle(Message, 3.0f);
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
