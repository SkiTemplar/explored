#include "Items/ExploredSurvivorComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

#include "Cooking/ExploredFire.h"
#include "Explored.h"
#include "Items/ExploredIslandStateSubsystem.h"
#include "Items/IslandConsumablesModel.h"
#include "Items/IslandDataLoader.h"
#include "Survival/IslandBodyComponent.h"
#include "Survival/SurvivalModel.h"

#define LOCTEXT_NAMESPACE "ExploredIsland"

namespace SurvivorDetail
{
	constexpr float MinActionSeconds = 0.05f;
	constexpr double GroundTraceUpCm = 100.0;
	constexpr double GroundTraceDownCm = 400.0;

	FText NotValid() { return LOCTEXT("NotValid", "No sirve así"); }
	FText Full() { return LOCTEXT("BackpackFull", "No te cabe más"); }
}

UExploredSurvivorComponent::UExploredSurvivorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UExploredSurvivorComponent* UExploredSurvivorComponent::FindOn(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UExploredSurvivorComponent>() : nullptr;
}

void UExploredSurvivorComponent::BeginPlay()
{
	Super::BeginPlay();
	const FIslandData& Data = ExploredIslandData::Get();
	UExploredIslandStateSubsystem* Island = UExploredIslandStateSubsystem::Get(this);
	TOptional<FBackpackState> Saved = Island ? Island->TakePendingBackpack() : TOptional<FBackpackState>();
	Backpack = FBackpackModel::StarterKit(Data.Backpack);
	if (Saved.IsSet())
	{
		RestoreBackpack(Saved.GetValue());
	}
	OnBackpackChanged.Broadcast();
}

void UExploredSurvivorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelTimedAction();
	Super::EndPlay(EndPlayReason);
}

void UExploredSurvivorComponent::ShowMessage(const FText& Message)
{
	UE_LOG(LogExplored, Display, TEXT("[Isla] %s"), *Message.ToString());
	OnMessage.Broadcast(Message);
}

void UExploredSurvivorComponent::ReportResult(EBackpackResult Result)
{
	switch (Result)
	{
	case EBackpackResult::Ok:
		break;
	case EBackpackResult::Full:
		ShowMessage(SurvivorDetail::Full());
		break;
	case EBackpackResult::HandsBusy:
		ShowMessage(LOCTEXT("HandsBusy", "Ya llevas algo pesado en las manos"));
		break;
	default:
		ShowMessage(SurvivorDetail::NotValid());
		break;
	}
}

void UExploredSurvivorComponent::ReportCraft(ECraftResult Result)
{
	switch (Result)
	{
	case ECraftResult::Ok:
		break;
	case ECraftResult::Full:
		ShowMessage(SurvivorDetail::Full());
		break;
	case ECraftResult::MissingInputs:
		ShowMessage(LOCTEXT("MissingInputs", "Te falta material"));
		break;
	default:
		ShowMessage(SurvivorDetail::NotValid());
		break;
	}
}

void UExploredSurvivorComponent::ApplyBackpack(const FBackpackState& NewState)
{
	if (NewState == Backpack)
	{
		return;
	}
	Backpack = NewState;
	OnBackpackChanged.Broadcast();
}

void UExploredSurvivorComponent::RestoreBackpack(const FBackpackState& Saved)
{
	if (!FBackpackModel::IsValid(Saved, ExploredIslandData::Get().Backpack))
	{
		UE_LOG(LogExplored, Warning, TEXT("[Isla] La mochila guardada no es válida; se conserva la actual"));
		return;
	}
	Backpack = Saved;
	OnBackpackChanged.Broadcast();
}

bool UExploredSurvivorComponent::TryAddItem(FName ItemId, int32 Count)
{
	FBackpackState Next;
	const EBackpackResult Result = FBackpackModel::Add(Backpack, ExploredIslandData::Get().Backpack, ItemId, Count, Next);
	ReportResult(Result);
	if (Result != EBackpackResult::Ok)
	{
		return false;
	}
	ApplyBackpack(Next);
	UE_LOG(LogExplored, Display, TEXT("[Isla] Mochila: +%d %s"), Count, *ItemId.ToString());
	return true;
}

void UExploredSurvivorComponent::ApplyBodyEffect(float Thirst, float Hunger, float Energy) const
{
	// Las cuatro barras del GDD v3 §5.1 viven en UIslandBodyComponent.
	if (UIslandBodyComponent* Body = UIslandBodyComponent::FindOn(GetOwner()))
	{
		Body->ApplyDelta(Thirst, Hunger, Energy);
	}
}

bool UExploredSurvivorComponent::ConsumeItem(FName ItemId)
{
	const FIslandData& Data = ExploredIslandData::Get();
	FBackpackState Next;
	const FConsumeOutcome Outcome = FIslandConsumables::Consume(Data.Consumables, Backpack, Data.Backpack, ItemId, Next);
	switch (Outcome.Result)
	{
	case EConsumeResult::Ok:
		break;
	case EConsumeResult::Empty:
		ShowMessage(LOCTEXT("CanteenEmpty", "La cantimplora está vacía"));
		return false;
	default:
		ShowMessage(SurvivorDetail::NotValid());
		return false;
	}
	ApplyBackpack(Next);
	ApplyBodyEffect(Outcome.Effect.Thirst, Outcome.Effect.Hunger, Outcome.Effect.Energy);
	UE_LOG(LogExplored, Display, TEXT("[Isla] Consume %s: sed %+.0f, hambre %+.0f, energía %+.0f"), *ItemId.ToString(),
		Outcome.Effect.Thirst, Outcome.Effect.Hunger, Outcome.Effect.Energy);
	if (!Outcome.Dropped.IsNone() && GetOwner())
	{
		if (UExploredIslandStateSubsystem* Island = UExploredIslandStateSubsystem::Get(this))
		{
			Island->SpawnDropped(Outcome.Dropped, 1, GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 60.0);
		}
	}
	return true;
}

TArray<FName> UExploredSurvivorComponent::GetCraftOptions(FName ItemA, FName ItemB) const
{
	return ExploredIslandData::Get().Recipes.FindOptions(ItemA, ItemB);
}

bool UExploredSurvivorComponent::StartTimedAction(float Seconds, TFunction<void()> OnDone)
{
	UWorld* World = GetWorld();
	if (bBusy || !World)
	{
		return false;
	}
	bBusy = true;
	PendingAction = MoveTemp(OnDone);
	ActionTotalSeconds = FMath::Max(Seconds, SurvivorDetail::MinActionSeconds);
	const TWeakObjectPtr<UExploredSurvivorComponent> WeakThis(this);
	World->GetTimerManager().SetTimer(ActionTimer, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
	{
		UExploredSurvivorComponent* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		Self->bBusy = false;
		TFunction<void()> Action = MoveTemp(Self->PendingAction);
		Self->PendingAction = nullptr;
		if (Action)
		{
			Action();
		}
	}), ActionTotalSeconds, false);
	return true;
}

void UExploredSurvivorComponent::CancelTimedAction()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActionTimer);
	}
	PendingAction = nullptr;
	bBusy = false;
}

bool UExploredSurvivorComponent::FindCampfireSpot(FVector& OutLocation) const
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return false;
	}
	FVector Forward = Owner->GetActorForwardVector();
	Forward.Z = 0.0;
	const FVector Probe = Owner->GetActorLocation() + Forward.GetSafeNormal() * CampfireDistanceCm;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SurvivorCampfireSpot), false, Owner);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Probe + FVector(0.0, 0.0, SurvivorDetail::GroundTraceUpCm),
		Probe - FVector(0.0, 0.0, SurvivorDetail::GroundTraceDownCm), ECC_Visibility, Params))
	{
		return false;
	}
	// Suelo despejado: casi horizontal y sin agua (el mar está a cota 0).
	if (Hit.ImpactNormal.Z < 0.8 || Hit.ImpactPoint.Z <= 10.0)
	{
		return false;
	}
	OutLocation = Hit.ImpactPoint;
	return true;
}

bool UExploredSurvivorComponent::Craft(FName RecipeId)
{
	const FIslandData& Data = ExploredIslandData::Get();
	const FRecipeDef* Recipe = Data.Recipes.Find(RecipeId);
	if (!Recipe)
	{
		ShowMessage(SurvivorDetail::NotValid());
		return false;
	}
	const ECraftResult Checked = FRecipeBook::Check(*Recipe, Backpack, ECraftPlace::Anywhere, TMap<FName, int32>());
	if (Checked != ECraftResult::Ok)
	{
		ReportCraft(Checked);
		return false;
	}
	FVector Spot;
	if (Recipe->OutputWorld == FName(TEXT("hoguera")) && !FindCampfireSpot(Spot))
	{
		ShowMessage(LOCTEXT("NoGround", "Aquí no hay suelo despejado"));
		return false;
	}
	const TWeakObjectPtr<UExploredSurvivorComponent> WeakThis(this);
	return StartTimedAction(Recipe->Seconds, [WeakThis, RecipeId]()
	{
		if (UExploredSurvivorComponent* Self = WeakThis.Get())
		{
			Self->FinishCraft(RecipeId);
		}
	});
}

void UExploredSurvivorComponent::FinishCraft(FName RecipeId)
{
	const FIslandData& Data = ExploredIslandData::Get();
	const FRecipeDef* Recipe = Data.Recipes.Find(RecipeId);
	if (!Recipe)
	{
		return;
	}
	FVector Spot;
	const bool bCampfire = Recipe->OutputWorld == FName(TEXT("hoguera"));
	if (bCampfire && !FindCampfireSpot(Spot))
	{
		ShowMessage(LOCTEXT("NoGround", "Aquí no hay suelo despejado"));
		return;
	}
	FBackpackState Next;
	const ECraftResult Result = FRecipeBook::Apply(*Recipe, Backpack, Data.Backpack, ECraftPlace::Anywhere, TMap<FName, int32>(), Next);
	if (Result != ECraftResult::Ok)
	{
		ReportCraft(Result);
		return;
	}
	ApplyBackpack(Next);
	UE_LOG(LogExplored, Display, TEXT("[Isla] Receta %s hecha"), *RecipeId.ToString());
	if (bCampfire)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AExploredFire* Fire = GetWorld()->SpawnActor<AExploredFire>(AExploredFire::StaticClass(), FTransform(Spot), Params))
		{
			Fire->Ignite();
		}
	}
}

bool UExploredSurvivorComponent::DropHeavy()
{
	const AActor* Owner = GetOwner();
	UExploredIslandStateSubsystem* Island = UExploredIslandStateSubsystem::Get(this);
	if (Backpack.HeldHeavy.IsNone() || !Owner || !Island)
	{
		return false;
	}
	const FName Item = Backpack.HeldHeavy;
	FBackpackState Next;
	if (FBackpackModel::Remove(Backpack, Item, 1, Next) != EBackpackResult::Ok)
	{
		return false;
	}
	FVector Forward = Owner->GetActorForwardVector();
	Forward.Z = 0.0;
	FVector Location = Owner->GetActorLocation() + Forward.GetSafeNormal() * 120.0;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SurvivorDrop), false, Owner);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Location + FVector(0.0, 0.0, 100.0), Location - FVector(0.0, 0.0, 500.0), ECC_Visibility, Params))
	{
		Location = Hit.ImpactPoint;
	}
	ApplyBackpack(Next);
	Island->SpawnDropped(Item, 1, Location);
	return true;
}

#undef LOCTEXT_NAMESPACE

float UExploredSurvivorComponent::GetActionProgress01() const
{
	const UWorld* World = GetWorld();
	if (!bBusy || !World || ActionTotalSeconds <= 0.0f)
	{
		return 0.0f;
	}
	const float Elapsed = World->GetTimerManager().GetTimerElapsed(ActionTimer);
	return Elapsed < 0.0f ? 0.0f : FMath::Clamp(Elapsed / ActionTotalSeconds, 0.0f, 1.0f);
}

void UExploredSurvivorComponent::SelectQuickSlot(int32 Index)
{
	SelectedQuickSlot = FMath::Clamp(Index, 0, 2);
}
