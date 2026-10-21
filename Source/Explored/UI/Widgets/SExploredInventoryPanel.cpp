#include "UI/Widgets/SExploredInventoryPanel.h"

#include "GameFramework/PlayerController.h"
#include "Internationalization/Text.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Items/ExploredSurvivorComponent.h"
#include "Items/IslandDataLoader.h"
#include "Player/ExploredCharacter.h"
#include "UI/BackpackMenuModel.h"
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

	FText InputsText(const FRecipeDef& Recipe)
	{
		TArray<FText> Parts;
		for (const FRecipeAmount& Input : Recipe.Inputs)
		{
			Parts.Add(FText::Format(LOCTEXT("InputAmount", "{0} {1}"), FText::AsNumber(Input.Count), ExploredIslandData::ItemName(Input.Item)));
		}
		for (const FName Tool : Recipe.Tools)
		{
			Parts.Add(ExploredIslandData::ItemName(Tool));
		}
		return FText::Join(LOCTEXT("InputSeparator", " + "), Parts);
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
				.Font(Style.FontSmall())
				.ColorAndOpacity(FSlateColor(bEnabled ? Style.ColorAccent() : Style.ColorDisabled()))
			];
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
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.0f, 0.0f, 48.0f, 0.0f))
		[
			SNew(SBorder)
			.BorderImage(Style.BrushPanel())
			.Padding(FMargin(18.0f, 14.0f))
			[
				SNew(SBox)
				.WidthOverride(400.0f)
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
	FString Out = FString::Printf(TEXT("%s|%d|%d|"), *Pack.HeldHeavy.ToString(), Pack.CanteenSips, Survivor->IsBusy() ? 1 : 0);
	for (const FBackpackStack& Stack : Pack.Stacks)
	{
		Out += FString::Printf(TEXT("%s:%d,"), *Stack.Id.ToString(), Stack.Count);
	}
	return Out;
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

	auto Line = [&Style](const FText& Text, bool bHeading = false, bool bDim = false) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(bHeading ? Style.FontBody() : Style.FontSmall())
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(bDim ? Style.ColorInkDim() : Style.ColorInk()));
	};

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
	[
		Line(FText::Format(LOCTEXT("Title", "Mochila ({0}/{1})"), FText::AsNumber(FBackpackModel::UsedSlots(Pack)),
			FText::AsNumber(Data.Backpack.Slots)), true)
	];
	if (!Pack.HeldHeavy.IsNone())
	{
		Box->AddSlot().AutoHeight()
		[
			Line(FText::Format(LOCTEXT("Heavy", "En las manos: {0}"), ExploredIslandData::ItemName(Pack.HeldHeavy)))
		];
	}

	for (const FBackpackMenuEntry& Entry : FBackpackMenuModel::Entries(Pack, Data.Consumables))
	{
		const FName ItemId = Entry.ItemId;
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		Row->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center)[Line(EntryText(Entry))];
		if (Data.Consumables.Find(ItemId))
		{
			Row->AddSlot().AutoWidth()
			[
				MakeButton(LOCTEXT("Use", "Usar"), bFree && Entry.bCanUse, [WeakCharacter, ItemId]()
				{
					if (AExploredCharacter* Target = WeakCharacter.Get())
					{
						Target->UseBackpackItem(ItemId);
					}
				})
			];
		}
		if (Entry.bCanDrop)
		{
			Row->AddSlot().AutoWidth()
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
		Box->AddSlot().AutoHeight()[Row];
	}

	Box->AddSlot().AutoHeight().Padding(FMargin(0.0f, 10.0f, 0.0f, 2.0f))
	[
		Line(LOCTEXT("Recipes", "Recetas"), true)
	];
	for (const FBackpackMenuRecipe& Recipe : FBackpackMenuModel::HandRecipes(Data.Recipes, Pack))
	{
		const FRecipeDef* Def = Data.Recipes.Find(Recipe.RecipeId);
		if (!Def)
		{
			continue;
		}
		const FName RecipeId = Recipe.RecipeId;
		const FText Label = FText::Format(LOCTEXT("RecipeButton", "{0}: {1}"), VerbText(Recipe.Verb),
			ExploredUIWidgets::PickLocalized(Def->NameEs, Def->NameEn));
		Box->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				MakeButton(Label, bFree && Recipe.CanCraft(), [WeakCharacter, RecipeId]()
				{
					if (AExploredCharacter* Target = WeakCharacter.Get())
					{
						Target->CraftFromBackpack(RecipeId);
					}
				})
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				Line(InputsText(*Def), false, true)
			]
		];
	}
	Box->AddSlot().AutoHeight().Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
	[
		Line(LOCTEXT("Hint", "Clic: usar, soltar o hacer · Tab: cerrar"), false, true)
	];

	Content->SetContent(Box);
}

#undef LOCTEXT_NAMESPACE
