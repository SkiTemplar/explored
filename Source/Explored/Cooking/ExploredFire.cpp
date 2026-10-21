#include "Cooking/ExploredFire.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Explored.h"
#include "Cooking/FireActionsModel.h"
#include "Interaction/VerbChoiceModel.h"
#include "Kismet/GameplayStatics.h"
#include "Items/BackpackModel.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Items/IslandDataLoader.h"
#include "Weather/IslandWeatherQuery.h"

#define LOCTEXT_NAMESPACE "ExploredIsland"

namespace ExploredFireDetail
{
	const FName ActCollect = FFireActionsModel::Collect();
	const FName ActLight = FFireActionsModel::Light();
	const FName ActCook = FFireActionsModel::Cook();
	const FName ActBoil = FFireActionsModel::Boil();
	const FName ActFeed = FFireActionsModel::Feed();
	const FName CampfireRecipe = FFireActionsModel::CampfireRecipe();
	const FName GrillRecipe = FFireActionsModel::GrillRecipe();
	const FName OilRecipe = FFireActionsModel::OilRecipe();
	const FName FireType(TEXT("hoguera_fija"));

	FText ActionText(FName Action)
	{
		if (Action == ActCollect) { return LOCTEXT("FireCollect", "Recoger"); }
		if (Action == ActLight) { return LOCTEXT("FireLight", "Encender"); }
		if (Action == ActCook) { return LOCTEXT("FireCook", "Cocinar"); }
		if (Action == ActBoil) { return LOCTEXT("FireBoil", "Hervir"); }
		return LOCTEXT("FireFeed", "Añadir leña");
	}
}

AExploredFire::AExploredFire()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetRelativeLocation(FVector(0.0, 0.0, 80.0));
	Light->SetLightColor(FLinearColor(1.0f, 0.55f, 0.25f));
	Light->SetAttenuationRadius(900.0f);
	Light->SetIntensity(0.0f);
}

void AExploredFire::BeginPlay()
{
	Super::BeginPlay();
	// Encendida con la receta (sin colocación de island_props.json): se viste como la hoguera fija.
	if (!bConfigured)
	{
		ConfigureStandalone(NAME_None, ExploredFireDetail::FireType, FString());
	}
	UpdateShelter();
	Refresh();
}

void AExploredFire::OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type)
{
	bConfigured = true;
	UpdateShelter();
}

void AExploredFire::UpdateShelter()
{
	bSheltered = ExploredIslandWeather::IsSheltered(this, GetActorLocation());
}

void AExploredFire::Refresh()
{
	SetActorTickEnabled(State.bLit || (State.Job.IsActive() && !State.Job.IsDone()));
	if (Light)
	{
		Light->SetIntensity(State.bLit ? MaxLightIntensity : 0.0f);
	}
}

void AExploredFire::Ignite()
{
	State = FCampfireModel::Ignite(State, ExploredIslandData::Get().Campfire);
	UE_LOG(LogExplored, Display, TEXT("[Isla] Hoguera %s encendida (%.0f s)"), *GetName(), State.BurnSeconds);
	Refresh();
}

void AExploredFire::RestoreCampfire(const FCampfireState& InState)
{
	State = InState;
	Refresh();
}

float AExploredFire::GetHeatAt(const FVector& WorldLocation) const
{
	return FCampfireModel::HeatAt(State, ExploredIslandData::Get().Campfire, FVector::Dist(GetActorLocation(), WorldLocation) / 100.0f);
}

void AExploredFire::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SimulationTimer += DeltaSeconds;
	if (SimulationTimer < SimulationIntervalSeconds)
	{
		return;
	}
	const float Step = SimulationTimer;
	SimulationTimer = 0.0f;
	TArray<ECampfireEvent> Events;
	const bool bExposed = !bSheltered && ExploredIslandWeather::IsCycloneWall(this);
	State = FCampfireModel::Tick(State, Step, bExposed, Events);
	for (const ECampfireEvent Event : Events)
	{
		UE_LOG(LogExplored, Display, TEXT("[Isla] Hoguera %s: %s"), *GetName(),
			Event == ECampfireEvent::JobDone ? TEXT("listo para recoger") : Event == ECampfireEvent::Quenched ? TEXT("apagada por el ciclón") : TEXT("consumida"));
	}
	if (Events.Num() > 0)
	{
		Refresh();
	}
}

TArray<FName> AExploredFire::GetAvailableActions(AActor* InInstigator) const
{
	const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	if (!Survivor)
	{
		return TArray<FName>();
	}
	const FIslandData& Data = ExploredIslandData::Get();
	return FFireActionsModel::Available(State, Survivor->GetBackpack(), Data.Recipes, Data.Campfire);
}

FName AExploredFire::GetChosenAction(AActor* InInstigator) const
{
	return FVerbChoiceModel::Resolve(GetAvailableActions(InInstigator), SelectedAction);
}

void AExploredFire::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	using namespace ExploredFireDetail;
	// Un solo jugador: los verbos que valen para el peón local, el que ejecuta la tecla primero.
	const TArray<FName> Ordered = FVerbChoiceModel::Ordered(GetAvailableActions(UGameplayStatics::GetPlayerPawn(this, 0)), SelectedAction);
	for (const FName Action : Ordered)
	{
		OutVerbs.Add(ActionText(Action));
	}
	if (OutVerbs.Num() > 0)
	{
		return;
	}
	// Sin nada que hacer ahora: lo que pide la hoguera en su estado (Interact explica qué falta).
	OutVerbs.Add(ActionText(State.Job.IsDone() ? ActCollect : !State.bLit ? ActLight : ActFeed));
}

bool AExploredFire::HasVerbChoice_Implementation(AActor* InInstigator) const
{
	return GetAvailableActions(InInstigator).Num() > 1;
}

bool AExploredFire::CycleVerb_Implementation(AActor* InInstigator)
{
	const TArray<FName> Actions = GetAvailableActions(InInstigator);
	if (Actions.Num() < 2)
	{
		return false;
	}
	SelectedAction = FVerbChoiceModel::Next(Actions, FVerbChoiceModel::Resolve(Actions, SelectedAction));
	return true;
}

bool AExploredFire::CanInteract_Implementation(AActor* InInstigator) const
{
	const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	return Survivor && !Survivor->IsBusy();
}

void AExploredFire::Interact_Implementation(AActor* InInstigator)
{
	const FName Action = GetChosenAction(InInstigator);
	if (!Action.IsNone())
	{
		PerformAction(InInstigator, Action);
	}
	else if (UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator))
	{
		Survivor->ShowMessage(State.bLit ? LOCTEXT("FireNothing", "No llevas nada para el fuego")
			: LOCTEXT("FireNeedsWood", "Para encenderla: 3 de leña y el cuchillo"));
	}
}

bool AExploredFire::PerformAction(AActor* InInstigator, FName Action)
{
	using namespace ExploredFireDetail;
	UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	if (!Survivor || Survivor->IsBusy())
	{
		return false;
	}
	if (Action == ActCollect) { return this->Collect(*Survivor); }
	if (Action == ActLight) { return LightWithRecipe(*Survivor); }
	if (Action == ActCook) { return StartCooking(*Survivor, GrillRecipe); }
	if (Action == ActBoil) { return StartCooking(*Survivor, OilRecipe); }
	if (Action == ExploredFireDetail::ActFeed) { return this->Feed(*Survivor); }
	Survivor->ReportCraft(ECraftResult::NotValid);
	return false;
}

bool AExploredFire::LightWithRecipe(UExploredSurvivorComponent& Survivor)
{
	const FIslandData& Data = ExploredIslandData::Get();
	const FRecipeDef* Recipe = Data.Recipes.Find(ExploredFireDetail::CampfireRecipe);
	if (State.bLit || !Recipe)
	{
		return false;
	}
	const ECraftResult Checked = FRecipeBook::Check(*Recipe, Survivor.GetBackpack(), ECraftPlace::Ground, {});
	if (Checked != ECraftResult::Ok)
	{
		Survivor.ReportCraft(Checked);
		return false;
	}
	const TWeakObjectPtr<AExploredFire> WeakThis(this);
	const TWeakObjectPtr<UExploredSurvivorComponent> WeakSurvivor(&Survivor);
	return Survivor.StartTimedAction(Recipe->Seconds, [WeakThis, WeakSurvivor]()
	{
		AExploredFire* Self = WeakThis.Get();
		UExploredSurvivorComponent* Who = WeakSurvivor.Get();
		const FIslandData& Data = ExploredIslandData::Get();
		const FRecipeDef* Recipe = Data.Recipes.Find(ExploredFireDetail::CampfireRecipe);
		if (!Self || !Who || !Recipe || Self->State.bLit)
		{
			return;
		}
		FBackpackState Next;
		const ECraftResult Result = FRecipeBook::Apply(*Recipe, Who->GetBackpack(), Data.Backpack, ECraftPlace::Ground, {}, Next);
		Who->ReportCraft(Result);
		if (Result == ECraftResult::Ok)
		{
			Who->ApplyBackpack(Next);
			Self->Ignite();
		}
	});
}

bool AExploredFire::StartCooking(UExploredSurvivorComponent& Survivor, FName RecipeId)
{
	const FIslandData& Data = ExploredIslandData::Get();
	const FRecipeDef* Recipe = Data.Recipes.Find(RecipeId);
	if (!Recipe)
	{
		return false;
	}
	ECraftResult Checked = FRecipeBook::Check(*Recipe, Survivor.GetBackpack(), State.bLit ? ECraftPlace::Fire : ECraftPlace::Anywhere, {});
	if (Checked == ECraftResult::Ok && State.Job.IsActive())
	{
		Checked = ECraftResult::StationNotReady;
	}
	if (Checked != ECraftResult::Ok)
	{
		Survivor.ReportCraft(Checked);
		return false;
	}
	// Las entradas se gastan al ponerlas al fuego; la salida se queda en la hoguera hasta recogerla.
	TMap<FName, int32> Inputs;
	for (const FRecipeAmount& Input : Recipe->Inputs)
	{
		Inputs.FindOrAdd(Input.Item) += Input.Count;
	}
	FBackpackState Next;
	FCampfireState NextFire;
	if (FBackpackModel::Exchange(Survivor.GetBackpack(), Data.Backpack, Inputs, {}, Next) != EBackpackResult::Ok
		|| !FCampfireModel::StartJob(State, Recipe->Id, Recipe->OutputItem, Recipe->OutputCount, Recipe->Seconds, NextFire))
	{
		Survivor.ReportCraft(ECraftResult::NotValid);
		return false;
	}
	Survivor.ApplyBackpack(Next);
	State = NextFire;
	UE_LOG(LogExplored, Display, TEXT("[Isla] Hoguera %s: %s en marcha (%.0f s)"), *GetName(), *RecipeId.ToString(), Recipe->Seconds);
	Refresh();
	return true;
}

bool AExploredFire::Feed(UExploredSurvivorComponent& Survivor)
{
	const FIslandData& Data = ExploredIslandData::Get();
	for (const TPair<FName, float>& Fuel : Data.Campfire.FuelSeconds)
	{
		if (Survivor.GetItemCount(Fuel.Key) <= 0)
		{
			continue;
		}
		FCampfireState NextFire;
		FBackpackState Next;
		if (FCampfireModel::AddFuel(State, Data.Campfire, Fuel.Key, NextFire)
			&& FBackpackModel::Remove(Survivor.GetBackpack(), Fuel.Key, 1, Next) == EBackpackResult::Ok)
		{
			Survivor.ApplyBackpack(Next);
			State = NextFire;
			UE_LOG(LogExplored, Display, TEXT("[Isla] Hoguera %s: +%s (%.0f s)"), *GetName(), *Fuel.Key.ToString(), State.BurnSeconds);
			Refresh();
			return true;
		}
	}
	Survivor.ReportCraft(ECraftResult::MissingInputs);
	return false;
}

bool AExploredFire::Collect(UExploredSurvivorComponent& Survivor)
{
	FCampfireState NextFire;
	FName Item;
	int32 Count = 0;
	if (!FCampfireModel::CollectJob(State, NextFire, Item, Count))
	{
		return false;
	}
	if (!Survivor.TryAddItem(Item, Count))
	{
		return false;
	}
	State = NextFire;
	Refresh();
	return true;
}

#undef LOCTEXT_NAMESPACE
