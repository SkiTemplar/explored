#include "UI/Widgets/SExploredInventoryPanel.h"

#include "GameFramework/PlayerController.h"
#include "Internationalization/Text.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include "Items/ExploredSurvivorComponent.h"
#include "Items/IslandDataLoader.h"
#include "Player/ExploredCharacter.h"
#include "UI/BackpackMenuModel.h"
#include "UI/ExploredUIPalette.h"
#include "UI/ExploredUIStyle.h"
#include "UI/Widgets/ExploredUIWidgets.h"

#define LOCTEXT_NAMESPACE "ExploredInventory"

namespace ExploredInventoryPanelDetail
{
	FText VerbText(FName Verb)
	{
		if (Verb == FName(TEXT("encender"))) { return LOCTEXT("VerbLight", "Encender"); }
		if (Verb == FName(TEXT("trenzar"))) { return LOCTEXT("VerbBraid", "Trenzar"); }
		if (Verb == FName(TEXT("atar"))) { return LOCTEXT("VerbTie", "Atar"); }
		if (Verb == FName(TEXT("cocinar"))) { return LOCTEXT("VerbCook", "Cocinar"); }
		if (Verb == FName(TEXT("hervir"))) { return LOCTEXT("VerbBoil", "Hervir"); }
		if (Verb == FName(TEXT("montar"))) { return LOCTEXT("VerbMount", "Montar"); }
		return FText::FromName(Verb);
	}

	FText EntryText(const FBackpackMenuEntry& Entry)
	{
		const FText Name = ExploredIslandData::ItemName(Entry.ItemId);
		if (Entry.Sips != INDEX_NONE)
		{
			return FText::Format(LOCTEXT("EntrySips", "{0} ({1}/{2} sorbos)"), Name, FText::AsNumber(Entry.Sips),
				FText::AsNumber(FBackpackModel::CanteenMaxSips));
		}
		return Entry.Count > 1 ? FText::Format(LOCTEXT("EntryCount", "{0} ×{1}"), Name, FText::AsNumber(Entry.Count)) : Name;
	}

	TSharedRef<SWidget> MakeButton(const FText& Label, bool bEnabled, TFunction<void()> OnClick)
	{
		const FExploredUIStyle& Style = FExploredUIStyle::Get();
		return SNew(SButton)
			.ButtonStyle(&Style.ButtonStyle())
			// No enfocable: un clic no se lleva el teclado y Tab sigue cerrando la mochila.
			.IsFocusable(false)
			.IsEnabled(bEnabled)
			.OnClicked_Lambda([OnClick]()
			{
				OnClick();
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(Style.SerifBold(24.0f))
				.ColorAndOpacity(FSlateColor(bEnabled ? Style.ColorAccent() : Style.ColorDisabled()))
			];
	}

	TSharedRef<SWidget> Icon(FName ItemId, float Side, const FLinearColor& Color)
	{
		return SNew(SBox)
			.WidthOverride(Side)
			.HeightOverride(Side)
			[
				SNew(SImage)
				.Image(FExploredUIStyle::Get().IconBrush(ExploredIconSheet::FromId(ItemId)))
				.ColorAndOpacity(FSlateColor(Color))
			];
	}

	TSharedRef<SWidget> Label(const FText& Value, const FSlateFontInfo& Font, const FLinearColor& Color, bool bWrap = false)
	{
		return SNew(STextBlock).Text(Value).Font(Font).ColorAndOpacity(FSlateColor(Color)).AutoWrapText(bWrap);
	}
}

void SExploredInventoryPanel::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	SetVisibility(TAttribute<EVisibility>::CreateSP(this, &SExploredInventoryPanel::GetPanelVisibility));

	ChildSlot
	[
		SNew(SBox)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		// Debajo del reloj y del barómetro (arriba a la derecha) y a la derecha de las ranuras.
		.Padding(FMargin(0.0f, 176.0f, 36.0f, 0.0f))
		[
			SNew(SBorder)
			.BorderImage(Style.BrushSheet())
			.Padding(FMargin(32.0f, 26.0f))
			[
				SNew(SBox)
				.WidthOverride(560.0f)
				[
					SAssignNew(Content, SBox)
				]
			]
		]
	];
}

AExploredCharacter* SExploredInventoryPanel::ResolveCharacter() const
{
	const APlayerController* PC = Owner.Get();
	return PC ? Cast<AExploredCharacter>(PC->GetPawn()) : nullptr;
}

EVisibility SExploredInventoryPanel::GetPanelVisibility() const
{
	// Se consulta en cada evaluación y no en Tick: un widget plegado no se pinta ni recibe Tick.
	// SelfHitTestInvisible: los botones reciben el clic y el resto de la pantalla sigue siendo del juego.
	const AExploredCharacter* Character = ResolveCharacter();
	const bool bOpen = Character && Character->IsBackpackOpen() && Character->GetSurvivorComponent() != nullptr;
	return bOpen ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
}

void SExploredInventoryPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	// Solo se llama con el panel visible (mochila abierta).
	const FString Signature = ComputeSignature();
	if (Signature != LastSignature)
	{
		LastSignature = Signature;
		Rebuild();
	}
}

FString SExploredInventoryPanel::ComputeSignature() const
{
	const AExploredCharacter* Character = ResolveCharacter();
	const UExploredSurvivorComponent* Survivor = Character ? Character->GetSurvivorComponent() : nullptr;
	if (!Survivor)
	{
		return FString();
	}
	const FBackpackState& Pack = Survivor->GetBackpack();
	FString Out = FString::Printf(TEXT("%s|%d|%d|%s|"), *Pack.HeldHeavy.ToString(), Pack.CanteenSips, Survivor->IsBusy() ? 1 : 0,
		*SelectedItem.ToString());
	for (const FBackpackStack& Stack : Pack.Stacks)
	{
		Out += FString::Printf(TEXT("%s:%d,"), *Stack.Id.ToString(), Stack.Count);
	}
	return Out;
}

TSharedRef<SWidget> SExploredInventoryPanel::MakeSlot(int32 Index, const FBackpackMenuEntry* Entry)
{
	using namespace ExploredInventoryPanelDetail;
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	const bool bSelected = Entry && Entry->ItemId == SelectedItem;
	const FName ItemId = Entry ? Entry->ItemId : NAME_None;
	const TSharedRef<SOverlay> Cell = SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(Style.BrushRound(1))
			.ColorAndOpacity(FSlateColor(ExploredPalette::Linear(bSelected ? ExploredPalette::CieloOcaso : ExploredPalette::ArenaClara,
				Entry ? 1.0f : 0.5f)))
		];
	if (Entry)
	{
		Cell->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			Icon(ItemId, 64.0f, Style.ColorInk())
		];
		const FString Badge = Entry->Sips != INDEX_NONE ? FString::Printf(TEXT("%d/%d"), Entry->Sips, FBackpackModel::CanteenMaxSips)
			: Entry->Count > 1 ? FString::Printf(TEXT("×%d"), Entry->Count) : FString();
		Cell->AddSlot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
		[
			Label(FText::FromString(Badge), Style.SerifBold(22.0f), Style.ColorAccent())
		];
	}
	if (bSelected)
	{
		Cell->AddSlot()[SNew(SImage).Image(Style.BrushOutline()).ColorAndOpacity(FSlateColor(Style.ColorInk()))];
	}
	const TWeakPtr<SExploredInventoryPanel> WeakSelf = SharedThis(this);
	return SNew(SBox)
		.WidthOverride(112.0f)
		.HeightOverride(112.0f)
		[
			SNew(SButton)
			.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
			.IsFocusable(false)
			.ContentPadding(FMargin(0.0f))
			.OnClicked_Lambda([WeakSelf, ItemId]()
			{
				if (const TSharedPtr<SExploredInventoryPanel> Self = WeakSelf.Pin(); Self.IsValid() && !ItemId.IsNone())
				{
					Self->SelectedItem = ItemId;
				}
				return FReply::Handled();
			})
			[
				Cell
			]
		];
}

TSharedRef<SWidget> SExploredInventoryPanel::MakeDetails(const FBackpackMenuEntry* Entry, bool bFree)
{
	using namespace ExploredInventoryPanelDetail;
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	if (!Entry)
	{
		return Label(LOCTEXT("Empty", "La mochila está vacía."), Style.SerifItalic(24.0f), Style.ColorInkDim());
	}
	const FIslandData& Data = ExploredIslandData::Get();
	const FName ItemId = Entry->ItemId;
	const TWeakObjectPtr<AExploredCharacter> WeakCharacter(ResolveCharacter());
	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
	if (Data.Consumables.Find(ItemId))
	{
		Buttons->AddSlot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
		[
			MakeButton(LOCTEXT("Use", "Usar"), bFree && Entry->bCanUse, [WeakCharacter, ItemId]()
			{
				if (AExploredCharacter* Target = WeakCharacter.Get())
				{
					Target->UseBackpackItem(ItemId);
				}
			})
		];
	}
	if (Entry->bCanDrop)
	{
		Buttons->AddSlot().AutoWidth()
		[
			MakeButton(LOCTEXT("Drop", "Soltar"), bFree, [WeakCharacter, ItemId]()
			{
				if (AExploredCharacter* Target = WeakCharacter.Get())
				{
					Target->DropBackpackItem(ItemId);
				}
			})
		];
	}
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(FMargin(0.0f, 4.0f, 16.0f, 0.0f))
		[
			Icon(ItemId, 72.0f, Style.ColorInk())
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Label(EntryText(*Entry), Style.SerifBold(27.0f), Style.ColorInk())]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 2.0f, 0.0f, 6.0f))
			[
				Label(ExploredIslandData::ItemDescription(ItemId), Style.Serif(23.0f), Style.ColorInkDim(), true)
			]
			+ SVerticalBox::Slot().AutoHeight()[Buttons]
		];
}

void SExploredInventoryPanel::Rebuild()
{
	using namespace ExploredInventoryPanelDetail;

	AExploredCharacter* Character = ResolveCharacter();
	const UExploredSurvivorComponent* Survivor = Character ? Character->GetSurvivorComponent() : nullptr;
	if (!Survivor || !Content.IsValid())
	{
		return;
	}
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	const FIslandData& Data = ExploredIslandData::Get();
	const FBackpackState& Pack = Survivor->GetBackpack();
	const bool bFree = !Survivor->IsBusy();
	const TWeakObjectPtr<AExploredCharacter> WeakCharacter(Character);
	const TArray<FBackpackMenuEntry> Entries = FBackpackMenuModel::Entries(Pack, Data.Consumables);
	const FBackpackMenuEntry* Selected = Entries.FindByPredicate([this](const FBackpackMenuEntry& E) { return E.ItemId == SelectedItem; });
	if (!Selected && Entries.Num() > 0)
	{
		Selected = &Entries[0];
		SelectedItem = Selected->ItemId;
	}

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
	[
		Label(FText::Format(LOCTEXT("Title", "Mochila · {0}/{1}"), FText::AsNumber(FBackpackModel::UsedSlots(Pack)),
			FText::AsNumber(Data.Backpack.Slots)), Style.SerifBold(34.0f), Style.ColorInk())
	];
	if (!Pack.HeldHeavy.IsNone())
	{
		Box->AddSlot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
		[
			Label(FText::Format(LOCTEXT("Heavy", "En las manos: {0}"), ExploredIslandData::ItemName(Pack.HeldHeavy)), Style.SerifItalic(24.0f),
				Style.ColorInkDim())
		];
	}
	const TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(4.0f));
	const int32 SlotCount = FMath::Max(Data.Backpack.Slots, Entries.Num());
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		Grid->AddSlot(Index % 4, Index / 4)[MakeSlot(Index, Entries.IsValidIndex(Index) ? &Entries[Index] : nullptr)];
	}
	Box->AddSlot().AutoHeight().HAlign(HAlign_Left)[Grid];
	Box->AddSlot().AutoHeight().Padding(FMargin(0.0f, 14.0f, 0.0f, 0.0f))[MakeDetails(Selected, bFree)];

	Box->AddSlot().AutoHeight().Padding(FMargin(0.0f, 18.0f, 0.0f, 4.0f))
	[
		Label(LOCTEXT("Recipes", "Recetas"), Style.SerifBold(28.0f), Style.ColorInk())
	];
	for (const FBackpackMenuRecipe& Recipe : FBackpackMenuModel::HandRecipes(Data.Recipes, Pack))
	{
		const FRecipeDef* Def = Data.Recipes.Find(Recipe.RecipeId);
		if (!Def)
		{
			continue;
		}
		const FName RecipeId = Recipe.RecipeId;
		const bool bCan = bFree && Recipe.CanCraft();
		const TSharedRef<SHorizontalBox> Inputs = SNew(SHorizontalBox);
		for (const FRecipeAmount& Input : Def->Inputs)
		{
			Inputs->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.0f, 0.0f, 2.0f, 0.0f))
			[
				Icon(Input.Item, 32.0f, Style.ColorInkDim())
			];
			Inputs->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
			[
				Label(FText::Format(LOCTEXT("InputCount", "×{0}"), FText::AsNumber(Input.Count)), Style.Serif(22.0f), Style.ColorInkDim())
			];
		}
		Box->AddSlot().AutoHeight().Padding(FMargin(0.0f, 2.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).MinDesiredWidth(150.0f)
				[
					MakeButton(VerbText(Recipe.Verb), bCan, [WeakCharacter, RecipeId]()
					{
						if (AExploredCharacter* Target = WeakCharacter.Get())
						{
							Target->CraftFromBackpack(RecipeId);
						}
					})
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6.0f, 0.0f, 14.0f, 0.0f))
			[
				Label(ExploredUIWidgets::PickLocalized(Def->NameEs, Def->NameEn), Style.Serif(24.0f),
					bCan ? Style.ColorInk() : Style.ColorDisabled())
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[Inputs]
		];
	}
	Box->AddSlot().AutoHeight().Padding(FMargin(0.0f, 14.0f, 0.0f, 0.0f))
	[
		Label(LOCTEXT("Hint", "Clic: elegir, usar o hacer · Tab: cerrar"), Style.SerifItalic(22.0f), Style.ColorInkDim())
	];

	Content->SetContent(Box);
}

#undef LOCTEXT_NAMESPACE
