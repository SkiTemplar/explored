#include "Ruins/ExploredRuinElement.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UObject/SoftObjectPath.h"

#include "Carry/CarryComponent.h"
#include "Core/SystemLinks.h"
#include "Explored.h"
#include "Items/ItemTypes.h"
#include "Ruins/RuinsSubsystem.h"

namespace ExploredRuinElementDetail
{
	/** Malla del pueblo navegante por tipo (Art/Export/Props: RuinasTallas, BrujulaEstelar, Petroglifos). */
	void MarkerFor(ERuinElementKind Kind, FName ElementId, const TCHAR*& OutMesh, FVector& OutScale)
	{
		OutScale = FVector::OneVector;
		switch (Kind)
		{
		case ERuinElementKind::StatueAlignment:
			OutMesh = TEXT("/Game/Generated/Meshes/RuinasTallas/SM_Ruin_Statue_Navigator.SM_Ruin_Statue_Navigator");
			break;
		case ERuinElementKind::AltarOffering:
			OutMesh = TEXT("/Game/Generated/Meshes/RuinasTallas/SM_Ruin_Altar_Table.SM_Ruin_Altar_Table");
			break;
		case ERuinElementKind::DoubleCanoe:
			OutMesh = TEXT("/Game/Generated/Meshes/RuinasTallas/SM_Ruin_Canoe_DoubleWreck.SM_Ruin_Canoe_DoubleWreck");
			break;
		case ERuinElementKind::StarCompass:
			OutMesh = TEXT("/Game/Generated/Meshes/BrujulaEstelar/SM_StarCompass_Monument.SM_StarCompass_Monument");
			break;
		case ERuinElementKind::RitualCave:
			OutMesh = TEXT("/Game/Generated/Meshes/RuinasTallas/SM_Ruin_Petroglyph_Star.SM_Ruin_Petroglyph_Star");
			break;
		default:
		{
			// Petroglifos: se reparte el motivo por id para que no sean todos iguales.
			static const TCHAR* const Petroglyphs[] = {
				TEXT("/Game/Generated/Meshes/RuinasTallas/SM_Ruin_Petroglyph_Bird.SM_Ruin_Petroglyph_Bird"),
				TEXT("/Game/Generated/Meshes/RuinasTallas/SM_Ruin_Petroglyph_Canoe.SM_Ruin_Petroglyph_Canoe"),
				TEXT("/Game/Generated/Meshes/RuinasTallas/SM_Ruin_Petroglyph_Honu.SM_Ruin_Petroglyph_Honu"),
				TEXT("/Game/Generated/Meshes/RuinasTallas/SM_Ruin_Petroglyph_Star.SM_Ruin_Petroglyph_Star"),
			};
			OutMesh = Petroglyphs[FCrc::StrCrc32(*ElementId.ToString()) % UE_ARRAY_COUNT(Petroglyphs)];
			break;
		}
		}
	}

	URuinsSubsystem* GetRuins(const UWorld* World)
	{
		return World ? World->GetSubsystem<URuinsSubsystem>() : nullptr;
	}
}

AExploredRuinElement::AExploredRuinElement()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

void AExploredRuinElement::Configure(FName InElementId, ERuinElementKind InKind)
{
	ElementId = InElementId;
	Kind = InKind;
}

void AExploredRuinElement::BeginPlay()
{
	Super::BeginPlay();
	ResolveKind();
	ApplyMarkerMesh();
}

void AExploredRuinElement::ResolveKind()
{
	const URuinsSubsystem* Ruins = ExploredRuinElementDetail::GetRuins(GetWorld());
	if (!Ruins)
	{
		return;
	}
	const FRuinsLayout& Layout = Ruins->GetRuins().GetLayout();
	const int32 SiteIndex = Layout.FindSiteOfElement(ElementId);
	if (!Layout.Sites.IsValidIndex(SiteIndex))
	{
		UE_LOG(LogExplored, Warning, TEXT("%s: el elemento de ruina «%s» no existe en el modelo"), *GetName(), *ElementId.ToString());
		return;
	}
	const int32 ElementIndex = Layout.Sites[SiteIndex].FindElement(ElementId);
	if (Layout.Sites[SiteIndex].Elements.IsValidIndex(ElementIndex))
	{
		Kind = Layout.Sites[SiteIndex].Elements[ElementIndex].Kind;
	}
}

void AExploredRuinElement::ApplyMarkerMesh()
{
	if (!Mesh || Mesh->GetStaticMesh())
	{
		return;
	}
	const TCHAR* MeshPath = nullptr;
	FVector Scale = FVector::OneVector;
	ExploredRuinElementDetail::MarkerFor(Kind, ElementId, MeshPath, Scale);
	if (UStaticMesh* Marker = Cast<UStaticMesh>(FSoftObjectPath(MeshPath).TryLoad()))
	{
		Mesh->SetStaticMesh(Marker);
		Mesh->SetRelativeScale3D(Scale);
		ExploredInteraction::AddFocusBox(*this, *Mesh);
	}
}

bool AExploredRuinElement::IsDiscovered() const
{
	const URuinsSubsystem* Ruins = ExploredRuinElementDetail::GetRuins(GetWorld());
	return Ruins && Ruins->GetRuins().IsDiscovered(ElementId);
}

void AExploredRuinElement::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	if (IsDiscovered())
	{
		return;
	}
	switch (Kind)
	{
	case ERuinElementKind::StatueAlignment:
		OutVerbs.Add(NSLOCTEXT("Explored", "Verb_RuinStatue", "Mirar hacia donde mira la estatua"));
		break;
	case ERuinElementKind::AltarOffering:
		OutVerbs.Add(NSLOCTEXT("Explored", "Verb_RuinOffering", "Dejar una ofrenda"));
		break;
	case ERuinElementKind::StarCompass:
		OutVerbs.Add(NSLOCTEXT("Explored", "Verb_RuinCompass", "Estudiar la brújula estelar"));
		break;
	case ERuinElementKind::DoubleCanoe:
		OutVerbs.Add(NSLOCTEXT("Explored", "Verb_RuinCanoe", "Examinar la canoa doble"));
		break;
	case ERuinElementKind::RitualCave:
		OutVerbs.Add(NSLOCTEXT("Explored", "Verb_RuinCave", "Examinar las pinturas"));
		break;
	default:
		OutVerbs.Add(NSLOCTEXT("Explored", "Verb_RuinPetroglyph", "Examinar el petroglifo"));
		break;
	}
}

bool AExploredRuinElement::CanInteract_Implementation(AActor* InInstigator) const
{
	if (!InInstigator || ElementId.IsNone() || IsDiscovered())
	{
		return false;
	}
	if (Kind == ERuinElementKind::AltarOffering)
	{
		// Hace falta algo en la mano para ofrecer.
		const UCarryComponent* Carry = InInstigator->FindComponentByClass<UCarryComponent>();
		return Carry && (!Carry->IsHandEmpty(EHand::Right) || !Carry->IsHandEmpty(EHand::Left));
	}
	return true;
}

void AExploredRuinElement::Interact_Implementation(AActor* InInstigator)
{
	URuinsSubsystem* Ruins = ExploredRuinElementDetail::GetRuins(GetWorld());
	if (!Ruins || !InInstigator || IsDiscovered())
	{
		return;
	}
	switch (Kind)
	{
	case ERuinElementKind::StatueAlignment:
	{
		// La estatua enseña un rumbo: cuenta al mirar hacia donde mira ella (GDD §6.1).
		const APawn* Pawn = Cast<APawn>(InInstigator);
		const float ViewYaw = Pawn ? Pawn->GetControlRotation().Yaw : InInstigator->GetActorRotation().Yaw;
		if (!ExploredLinks::IsFacingAlong(ViewYaw, GetActorRotation().Yaw, StatueAlignToleranceDeg))
		{
			return;
		}
		break;
	}
	case ERuinElementKind::AltarOffering:
	{
		UCarryComponent* Carry = InInstigator->FindComponentByClass<UCarryComponent>();
		FItemInstance Offering;
		if (!Carry || !(Carry->TakeOneFromHand(EHand::Right, Offering) || Carry->TakeOneFromHand(EHand::Left, Offering)))
		{
			return;
		}
		break;
	}
	default:
		break;
	}
	Ruins->NotifyElementDiscovered(ElementId);
}
