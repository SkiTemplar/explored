#include "Debug/ExploredIslandSmokeTest.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"

#include "Cooking/ExploredFire.h"
#include "Explored.h"
#include "Fishing/ExploredFishingSpot.h"
#include "Interaction/ExploredInteractable.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Narrative/ExploredSupplyCrate.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Weather/ExploredRainBarrel.h"
#include "WorldGen/ExploredIslandPopulator.h"

namespace IslandSmokeDetail
{
	constexpr float WarmUpSeconds = 3.0f;
	constexpr float FishingTimeoutSeconds = 90.0f;
}

bool UExploredIslandSmokeTest::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && FParse::Param(FCommandLine::Get(), TEXT("ExploredIslandSmoke"));
#endif
}

TStatId UExploredIslandSmokeTest::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExploredIslandSmokeTest, STATGROUP_Tickables);
}

APawn* UExploredIslandSmokeTest::GetPawn() const
{
	return UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
}

AActor* UExploredIslandSmokeTest::FindProp(const TCHAR* PropId) const
{
	for (TActorIterator<AExploredIslandPopulator> It(GetWorld()); It; ++It)
	{
		if (AActor* Found = It->FindSpawned(FName(PropId)))
		{
			return Found;
		}
	}
	return nullptr;
}

int32 UExploredIslandSmokeTest::Count(const TCHAR* ItemId) const
{
	const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(GetPawn());
	return Survivor ? Survivor->GetItemCount(FName(ItemId)) : -1;
}

bool UExploredIslandSmokeTest::Idle() const
{
	const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(GetPawn());
	return Survivor && !Survivor->IsBusy();
}

bool UExploredIslandSmokeTest::Interact(const TCHAR* PropId)
{
	AActor* Target = FindProp(PropId);
	APawn* Pawn = GetPawn();
	if (!Target || !Pawn || !Target->Implements<UExploredInteractable>())
	{
		UE_LOG(LogExplored, Error, TEXT("[IslaSmoke] No hay %s o no es interactuable"), PropId);
		return false;
	}
	// Se coloca a Iria junto al objetivo, como si hubiera caminado hasta él.
	Pawn->SetActorLocation(Target->GetActorLocation() + FVector(-150.0, 0.0, 120.0), false, nullptr, ETeleportType::TeleportPhysics);
	if (!IExploredInteractable::Execute_CanInteract(Target, Pawn))
	{
		UE_LOG(LogExplored, Error, TEXT("[IslaSmoke] %s no admite interacción ahora"), PropId);
		return false;
	}
	IExploredInteractable::Execute_Interact(Target, Pawn);
	return true;
}

void UExploredIslandSmokeTest::BuildSteps()
{
	auto Add = [this](const TCHAR* Name, TFunction<bool()> Start, TFunction<bool()> IsDone, float Timeout = 10.0f)
	{
		Steps.Add({ Name, MoveTemp(Start), MoveTemp(IsDone), Timeout });
	};
	auto FireAction = [this](const TCHAR* Action)
	{
		AExploredFire* Fire = Cast<AExploredFire>(FindProp(TEXT("hoguera_faro")));
		return Fire && Fire->PerformAction(GetPawn(), FName(Action));
	};

	Add(TEXT("kit inicial"), [this]() { return Count(TEXT("cuchillo")) == 1 && Count(TEXT("cantimplora")) == 1 && Count(TEXT("barometro")) == 1; },
		[]() { return true; });
	Add(TEXT("recoger leña 1"), [this]() { return Interact(TEXT("lena_cala_1")); }, [this]() { return Idle() && Count(TEXT("lena")) == 2; });
	Add(TEXT("recoger leña 2"), [this]() { return Interact(TEXT("lena_cala_2")); }, [this]() { return Idle() && Count(TEXT("lena")) == 4; });
	Add(TEXT("recoger fibra 1"), [this]() { return Interact(TEXT("fibra_palmar_1")); }, [this]() { return Idle() && Count(TEXT("fibra")) == 1; });
	Add(TEXT("recoger fibra 2"), [this]() { return Interact(TEXT("fibra_palmar_2")); }, [this]() { return Idle() && Count(TEXT("fibra")) == 2; });
	Add(TEXT("trenzar cuerda"), [this]()
		{
			UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(GetPawn());
			return Survivor && Survivor->GetCraftOptions(TEXT("fibra"), TEXT("fibra")).Contains(FName(TEXT("cuerda"))) && Survivor->Craft(TEXT("cuerda"));
		},
		[this]() { return Idle() && Count(TEXT("cuerda")) == 1 && Count(TEXT("fibra")) == 0; });
	Add(TEXT("recoger caña de bambú"), [this]() { return Interact(TEXT("cana_bambu_1")); }, [this]() { return Idle() && Count(TEXT("cana_bambu")) == 1; });
	Add(TEXT("atar caña de pescar"), [this]()
		{
			UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(GetPawn());
			return Survivor && Survivor->Craft(TEXT("cana_pescar"));
		},
		[this]() { return Idle() && Count(TEXT("cana_pescar")) == 1 && Count(TEXT("cuerda")) == 0; });
	Add(TEXT("pescar un pez"), [this]() { return Interact(TEXT("punto_pesca_1")); },
		[this]()
		{
			AExploredFishingSpot* Spot = Cast<AExploredFishingSpot>(FindProp(TEXT("punto_pesca_1")));
			if (Spot && Spot->GetPhase() == static_cast<int32>(ECastFishingPhase::Biting))
			{
				IExploredInteractable::Execute_Interact(Spot, GetPawn());
			}
			return Count(TEXT("pez_crudo")) == 1;
		}, IslandSmokeDetail::FishingTimeoutSeconds);
	Add(TEXT("encender la hoguera del faro"), [FireAction]() { return FireAction(TEXT("encender")); },
		[this]()
		{
			const AExploredFire* Fire = Cast<AExploredFire>(FindProp(TEXT("hoguera_faro")));
			return Idle() && Fire && Fire->IsBurning() && Count(TEXT("lena")) == 1 && Count(TEXT("cuchillo")) == 1;
		});
	Add(TEXT("asar el pez"), [FireAction]() { return FireAction(TEXT("cocinar")); },
		[this]()
		{
			const AExploredFire* Fire = Cast<AExploredFire>(FindProp(TEXT("hoguera_faro")));
			return Fire && Fire->IsJobReady() && Count(TEXT("pez_crudo")) == 0;
		}, 30.0f);
	Add(TEXT("recoger el pez asado"), [FireAction]() { return FireAction(TEXT("recoger")); }, [this]() { return Count(TEXT("pez_asado")) == 1; });
	Add(TEXT("comer el pez asado"), [this]()
		{
			UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(GetPawn());
			return Survivor && Survivor->ConsumeItem(TEXT("pez_asado"));
		},
		[this]() { return Count(TEXT("pez_asado")) == 0; });
	Add(TEXT("avivar con leña"), [FireAction]() { return FireAction(TEXT("avivar")); }, [this]() { return Count(TEXT("lena")) == 0; });
	Add(TEXT("bajar y beber un coco"), [this]() { return Interact(TEXT("palma_cala_1")); },
		[this]()
		{
			UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(GetPawn());
			if (Idle() && Count(TEXT("coco")) == 1)
			{
				Survivor->ConsumeItem(TEXT("coco"));
			}
			return Count(TEXT("coco_vacio")) == 1 && Count(TEXT("coco")) == 0;
		});
	// Con la isla llena y caña en la mochila no falta nada: la caja no se gasta (F3e).
	Add(TEXT("la caja de suministros no se gasta si no falta nada"), [this]() { return Interact(TEXT("caja_suministros_faro")); },
		[this]()
		{
			const AExploredSupplyCrate* Crate = Cast<AExploredSupplyCrate>(FindProp(TEXT("caja_suministros_faro")));
			return Crate && !Crate->IsDelivered() && Count(TEXT("pez_crudo")) == 0;
		});
	Add(TEXT("llenar la cantimplora en el barril"), [this]()
		{
			AExploredRainBarrel* Barrel = Cast<AExploredRainBarrel>(FindProp(TEXT("barril_lluvia_faro")));
			if (!Barrel)
			{
				return false;
			}
			Barrel->AddCharges(FRainBarrelModel::ShowerCharges);
			return Interact(TEXT("barril_lluvia_faro"));
		},
		[this]()
		{
			const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(GetPawn());
			const AExploredRainBarrel* Barrel = Cast<AExploredRainBarrel>(FindProp(TEXT("barril_lluvia_faro")));
			return Survivor && Barrel && Survivor->GetCanteenSips() == 4 && Barrel->GetCharges() == 1;
		});
	Add(TEXT("beber de la cantimplora"), [this]()
		{
			UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(GetPawn());
			return Survivor && Survivor->ConsumeItem(TEXT("cantimplora"));
		},
		[this]()
		{
			const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(GetPawn());
			return Survivor && Survivor->GetCanteenSips() == 3;
		});
	Add(TEXT("combinación que no es receta"), [this]()
		{
			const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(GetPawn());
			return Survivor && Survivor->GetCraftOptions(TEXT("coco_vacio"), TEXT("cuchillo")).Num() == 0;
		},
		[]() { return true; });
}

void UExploredIslandSmokeTest::Tick(float DeltaTime)
{
	if (bFinished)
	{
		return;
	}
	if (!bStarted)
	{
		// Espera a que exista el personaje y la isla esté poblada.
		WarmUp += DeltaTime;
		if (WarmUp < IslandSmokeDetail::WarmUpSeconds || !GetPawn() || !FindProp(TEXT("hoguera_faro")))
		{
			return;
		}
		if (UTimeOfDaySubsystem* Time = GetWorld()->GetSubsystem<UTimeOfDaySubsystem>())
		{
			Time->SetTime(1, 10.0f);
		}
		BuildSteps();
		bStarted = true;
		Current = INDEX_NONE;
	}
	if (Current == INDEX_NONE || Current >= Steps.Num())
	{
		Current = Current == INDEX_NONE ? 0 : Current;
		if (Current >= Steps.Num())
		{
			Finish(bAllOk);
			return;
		}
		Elapsed = 0.0f;
		if (!Steps[Current].Start())
		{
			UE_LOG(LogExplored, Error, TEXT("[IslaSmoke] FALLO al empezar: %s"), *Steps[Current].Name);
			Finish(false);
		}
		return;
	}
	FStep& Step = Steps[Current];
	Elapsed += DeltaTime;
	if (Step.IsDone())
	{
		UE_LOG(LogExplored, Display, TEXT("[IslaSmoke] %s ok (%.1f s)"), *Step.Name, Elapsed);
		++Current;
		if (Current >= Steps.Num())
		{
			Finish(bAllOk);
			return;
		}
		Elapsed = 0.0f;
		if (!Steps[Current].Start())
		{
			UE_LOG(LogExplored, Error, TEXT("[IslaSmoke] FALLO al empezar: %s"), *Steps[Current].Name);
			Finish(false);
		}
		return;
	}
	if (Elapsed > Step.TimeoutSeconds)
	{
		UE_LOG(LogExplored, Error, TEXT("[IslaSmoke] FALLO por tiempo: %s"), *Step.Name);
		Finish(false);
	}
}

void UExploredIslandSmokeTest::Finish(bool bOk)
{
	bFinished = true;
	bAllOk = bOk;
	UE_LOG(LogExplored, Display, TEXT("[IslaSmoke] RESULTADO: %s (%d/%d pasos)"), bOk ? TEXT("OK") : TEXT("FALLO"),
		bOk ? Steps.Num() : FMath::Max(0, Current), Steps.Num());
	FGenericPlatformMisc::RequestExit(false);
}
