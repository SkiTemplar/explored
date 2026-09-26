#include "UI/Widgets/SExploredInventoryPanel.h"

#include "GameFramework/PlayerController.h"
#include "Internationalization/Text.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Carry/CarryComponent.h"
#include "Carry/InventoryModel.h"
#include "Items/ItemRegistrySubsystem.h"
#include "Items/ItemTypes.h"
#include "Player/ExploredCharacter.h"
#include "UI/ExploredUIStyle.h"
#include "UI/ScreensLogic.h"

namespace ExploredInventoryPanelDetail
{
	const AExploredCharacter* ResolveCharacter(const TWeakObjectPtr<APlayerController>& Owner)
	{
		const APlayerController* PC = Owner.Get();
		return PC ? Cast<AExploredCharacter>(PC->GetPawn()) : nullptr;
	}

	FText Kilos(float Value)
	{
		FNumberFormattingOptions Options;
		Options.SetMinimumFractionalDigits(0);
		Options.SetMaximumFractionalDigits(1);
		return FText::AsNumber(FMath::IsFinite(Value) ? Value : 0.0f, &Options);
	}

	FText SectionTitle(const ExploredScreens::FInventorySection& Section)
	{
		const FText Used = FText::AsNumber(Section.ItemIds.Num());
		const FText Capacity = FText::AsNumber(Section.Capacity);
		switch (Section.Slot)
		{
		case EInventorySlot::Pockets:
			return FText::Format(NSLOCTEXT("ExploredUI", "InvPockets", "Bolsillos ({0}/{1})"), Used, Capacity);
		case EInventorySlot::Belt:
			return FText::Format(NSLOCTEXT("ExploredUI", "InvBelt", "Cinturón ({0}/{1})"), Used, Capacity);
		case EInventorySlot::Pouch:
			return FText::Format(NSLOCTEXT("ExploredUI", "InvPouch", "Bolsa estanca ({0}/{1})"), Used, Capacity);
		case EInventorySlot::Backpack:
			return FText::Format(NSLOCTEXT("ExploredUI", "InvBackpack", "Mochila ({0} de {1} l)"),
				Kilos(Section.UsedVolumeLiters), Kilos(Section.MaxVolumeLiters));
		default:
			return FText::Format(NSLOCTEXT("ExploredUI", "InvSledge", "Angarillas ({0} kg)"), Kilos(Section.UsedWeightKg));
		}
	}
}

void SExploredInventoryPanel::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	// Etiqueta del mundo, no menú: nunca intercepta el ratón ni el foco.
	SetVisibility(TAttribute<EVisibility>::CreateSP(this, &SExploredInventoryPanel::GetPanelVisibility));

	ChildSlot
	[
		SNew(SBox)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.0f, 0.0f, 48.0f, 0.0f))
		[
			SNew(SBorder)
			.BorderImage(Style.BrushSheet())
			.Padding(FMargin(18.0f, 14.0f))
			[
				SNew(SBox)
				.WidthOverride(300.0f)
				[
					SAssignNew(Content, SBox)
				]
			]
		]
	];
}

EVisibility SExploredInventoryPanel::GetPanelVisibility() const
{
	// Se consulta en cada evaluación y no en Tick: un widget plegado no se pinta ni recibe Tick.
	const AExploredCharacter* Character = ExploredInventoryPanelDetail::ResolveCharacter(Owner);
	const bool bOpen = Character && Character->IsBackpackOpen() && Character->GetCarryComponent() != nullptr;
	return bOpen ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
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
	const AExploredCharacter* Character = ExploredInventoryPanelDetail::ResolveCharacter(Owner);
	const UCarryComponent* Carry = Character ? Character->GetCarryComponent() : nullptr;
	if (!Carry)
	{
		return FString();
	}
	const FInventoryModel& Model = Carry->GetInventoryModel();
	const FInventoryState& State = Model.GetState();
	FString Out = FString::Printf(TEXT("%lld|%lld|%d|%d|%d|%.1f"), State.HandLeft.InstanceId, State.HandRight.InstanceId,
		State.bHasBackpack ? 1 : 0, State.bHasSledge ? 1 : 0, Model.HasPouch() ? 1 : 0, Model.GetBodyWeightKg());
	const FInventoryContainer* Containers[] = { &State.Pockets, &State.Belt, &State.Pouch, &State.Backpack, &State.Sledge };
	for (const FInventoryContainer* Container : Containers)
	{
		Out += TEXT("|");
		for (const FInventoryEntry& Entry : Container->Entries)
		{
			Out += FString::Printf(TEXT("%lld:%d,"), Entry.Item.InstanceId, Entry.SlotIndex);
		}
	}
	return Out;
}

void SExploredInventoryPanel::Rebuild()
{
	using namespace ExploredInventoryPanelDetail;

	const AExploredCharacter* Character = ResolveCharacter(Owner);
	const UCarryComponent* Carry = Character ? Character->GetCarryComponent() : nullptr;
	if (!Carry || !Content.IsValid())
	{
		return;
	}
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	const UItemRegistrySubsystem* Registry = UItemRegistrySubsystem::Resolve(Character);
	const FInventoryModel& Model = Carry->GetInventoryModel();
	const FInventoryState& State = Model.GetState();

	auto ItemName = [Carry, Registry, &Model](int64 InstanceId) -> FText
	{
		const FItemInstance* Instance = Carry->FindInstance(InstanceId);
		if (Instance && Registry)
		{
			return Registry->GetDisplayName(*Instance);
		}
		const FInventoryItem* Item = Model.FindItemById(InstanceId);
		return Item ? FText::FromName(Item->DefinitionId) : FText::GetEmpty();
	};
	auto Line = [&Style](const FText& Text, bool bHeading = false, const FLinearColor* Color = nullptr) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(bHeading ? Style.FontBody() : Style.FontSmall())
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(Color ? *Color : Style.ColorSheetInk()));
	};

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
	[
		Line(NSLOCTEXT("ExploredUI", "InvTitle", "Lo que llevas"), true)
	];

	// Manos: un objeto de dos manos se cuenta una vez.
	const FText Empty = NSLOCTEXT("ExploredUI", "InvEmptyHand", "vacía");
	if (State.bHandsHoldTwoHanded && State.HandLeft.IsValid())
	{
		Box->AddSlot().AutoHeight()
		[
			Line(FText::Format(NSLOCTEXT("ExploredUI", "InvBothHands", "En las dos manos: {0}"), ItemName(State.HandLeft.InstanceId)))
		];
	}
	else
	{
		Box->AddSlot().AutoHeight()
		[
			Line(FText::Format(NSLOCTEXT("ExploredUI", "InvLeftHand", "Mano izquierda: {0}"),
				State.HandLeft.IsValid() ? ItemName(State.HandLeft.InstanceId) : Empty))
		];
		Box->AddSlot().AutoHeight()
		[
			Line(FText::Format(NSLOCTEXT("ExploredUI", "InvRightHand", "Mano derecha: {0}"),
				State.HandRight.IsValid() ? ItemName(State.HandRight.InstanceId) : Empty))
		];
	}

	for (const ExploredScreens::FInventorySection& Section : ExploredScreens::BuildInventorySections(State, Model.HasPouch()))
	{
		Box->AddSlot().AutoHeight().Padding(FMargin(0.0f, 8.0f, 0.0f, 2.0f))
		[
			Line(SectionTitle(Section), true)
		];
		if (Section.ItemIds.Num() == 0)
		{
			const FLinearColor Dim = Style.ColorSheetInk() * FLinearColor(1.0f, 1.0f, 1.0f, 0.55f);
			Box->AddSlot().AutoHeight().Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
			[
				Line(NSLOCTEXT("ExploredUI", "InvNothing", "nada"), false, &Dim)
			];
		}
		for (const int64 InstanceId : Section.ItemIds)
		{
			Box->AddSlot().AutoHeight().Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
			[
				Line(ItemName(InstanceId))
			];
		}
	}

	// Peso frente a la capacidad cómoda (GDD §8.2: afecta a la energía, al nado y al ruido).
	const ExploredScreens::ELoadBand Band = ExploredScreens::LoadBandFor(Model.GetCarriedWeightRatio());
	const FLinearColor LoadColor = Band == ExploredScreens::ELoadBand::Comfortable ? Style.ColorSheetInk()
		: Band == ExploredScreens::ELoadBand::Heavy ? Style.ColorAccentDim() : Style.ColorWarning();
	Box->AddSlot().AutoHeight().Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
	[
		Line(FText::Format(NSLOCTEXT("ExploredUI", "InvWeight", "Carga: {0} kg de {1} kg cómodos"),
			Kilos(Model.GetBodyWeightKg()), Kilos(Model.GetComfortableCapacityKg())), true, &LoadColor)
	];
	if (Band != ExploredScreens::ELoadBand::Comfortable)
	{
		Box->AddSlot().AutoHeight()
		[
			Line(Band == ExploredScreens::ELoadBand::Heavy
				? NSLOCTEXT("ExploredUI", "InvHeavy", "Vas cargado: te cansas antes y haces más ruido.")
				: NSLOCTEXT("ExploredUI", "InvOverloaded", "No puedes cargar más."), false, &LoadColor)
		];
	}

	Content->SetContent(Box);
}
