#include "Debug/ExploredBeaconSmokeTest.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"

#include "Explored.h"
#include "Interaction/ExploredInteractable.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Narrative/BeaconSubsystem.h"
#include "Narrative/ExploredBeaconActors.h"
#include "Narrative/LighthouseLamp.h"
#include "UI/ExploredPlayerController.h"
#include "Save/SaveFormat.h"
#include "Save/SaveSlots.h"
#include "UI/ExploredSaveSubsystem.h"
#include "WorldGen/ExploredIslandPopulator.h"

namespace BeaconSmokeDetail
{
	constexpr float WarmUpSeconds = 3.0f;
	/** Acercamiento del Mistral (12 s), bocina, fundido y epílogo, con margen. */
	constexpr float EndingTimeoutSeconds = 45.0f;
	const TCHAR* const SaveSlot = TEXT("manual3");
	const TCHAR* const ObsoleteSlot = TEXT("manual1");
}

bool UExploredBeaconSmokeTest::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && FParse::Param(FCommandLine::Get(), TEXT("ExploredBeaconSmoke"));
#endif
}

TStatId UExploredBeaconSmokeTest::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExploredBeaconSmokeTest, STATGROUP_Tickables);
}

APawn* UExploredBeaconSmokeTest::GetPawn() const
{
	return UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
}

UBeaconSubsystem* UExploredBeaconSmokeTest::GetBeacon() const
{
	return UBeaconSubsystem::Get(GetWorld());
}

AActor* UExploredBeaconSmokeTest::FindProp(const TCHAR* PropId) const
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

bool UExploredBeaconSmokeTest::Idle() const
{
	const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(GetPawn());
	return Survivor && !Survivor->IsBusy();
}

bool UExploredBeaconSmokeTest::Console(const TCHAR* Command) const
{
	UE_LOG(LogExplored, Display, TEXT("[BalizaSmoke] > %s"), Command);
	return GEngine && GEngine->Exec(GetWorld(), Command);
}

bool UExploredBeaconSmokeTest::Interact(const TCHAR* PropId, bool bMovePawn)
{
	AActor* Target = FindProp(PropId);
	APawn* Pawn = GetPawn();
	if (!Target || !Pawn || !Target->Implements<UExploredInteractable>())
	{
		UE_LOG(LogExplored, Error, TEXT("[BalizaSmoke] No hay %s o no es interactuable"), PropId);
		return false;
	}
	if (bMovePawn)
	{
		Pawn->SetActorLocation(Target->GetActorLocation() + FVector(-150.0, 0.0, 120.0), false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (!IExploredInteractable::Execute_CanInteract(Target, Pawn))
	{
		UE_LOG(LogExplored, Error, TEXT("[BalizaSmoke] %s no admite interacción ahora"), PropId);
		return false;
	}
	IExploredInteractable::Execute_Interact(Target, Pawn);
	return true;
}

void UExploredBeaconSmokeTest::BuildSteps()
{
	using namespace BeaconSmokeDetail;
	auto Add = [this](const TCHAR* Name, TFunction<bool()> Start, TFunction<bool()> IsDone, float Timeout = 10.0f)
	{
		Steps.Add({ Name, MoveTemp(Start), MoveTemp(IsDone), Timeout });
	};
	auto Stage = [this]() { const UBeaconSubsystem* B = GetBeacon(); return B ? B->GetStage() : EBeaconStage::Count; };
	auto Lamp = [this]() { return Cast<ALighthouseLamp>(FindProp(TEXT("faro_linterna"))); };

	Add(TEXT("empieza en el prólogo con objetivo"), [] { return true; },
		[this, Stage]() { return Stage() == EBeaconStage::Prologo && !GetBeacon()->GetObjectiveText().IsEmpty(); });
	Add(TEXT("leer la página 1"), [this]() { return Interact(TEXT("pagina_1")); },
		[this, Stage]() { return GetBeacon()->GetPagesRead() == 1 && Stage() == EBeaconStage::Faro; });
	Add(TEXT("BalizaStage Preparacion"), [this]() { return Console(TEXT("BalizaStage Preparacion")); },
		[Stage]() { return Stage() == EBeaconStage::Preparacion; });
	Add(TEXT("BalizaGive tarros, mecha y lente"), [this]()
		{
			return Console(TEXT("BalizaGive tarro_aceite 2")) && Console(TEXT("BalizaGive mecha 1")) && Console(TEXT("BalizaGive lente 1"));
		},
		[this]()
		{
			const UExploredSurvivorComponent* S = UExploredSurvivorComponent::FindOn(GetPawn());
			return S && S->GetItemCount(TEXT("tarro_aceite")) == 2 && S->GetItemCount(TEXT("mecha")) == 1 && S->GetHeldHeavy() == TEXT("lente");
		});
	Add(TEXT("subir a la linterna"), [this]() { return Interact(TEXT("faro_torre"), false); },
		[this]()
		{
			const AExploredLighthouseTower* Tower = Cast<AExploredLighthouseTower>(FindProp(TEXT("faro_torre")));
			return Tower && GetPawn() && Tower->IsUp(*GetPawn());
		});
	for (const TCHAR* SlotName : { TEXT("poner la lente"), TEXT("verter los tarros"), TEXT("apretar la mecha") })
	{
		Add(SlotName, [this]() { return Interact(TEXT("faro_linterna"), false); }, [this]() { return Idle(); });
	}
	Add(TEXT("linterna montada"), [] { return true; },
		[Stage, Lamp]() { return Stage() == EBeaconStage::LinternaMontada && Lamp() && Lamp()->NextSlot() == ELanternSlot::Count; });
	Add(TEXT("guardar en manual3"), [this]()
		{
			UExploredSaveSubsystem* Save = GetWorld()->GetGameInstance()->GetSubsystem<UExploredSaveSubsystem>();
			FString Error;
			return Save && Save->SaveToSlot(SaveSlot, Error);
		},
		[] { return true; });
	Add(TEXT("cargar manual3 tras volver al prólogo"), [this, Stage]()
		{
			Console(TEXT("BalizaStage Prologo"));
			if (Stage() != EBeaconStage::Prologo)
			{
				return false;
			}
			UExploredSaveSubsystem* Save = GetWorld()->GetGameInstance()->GetSubsystem<UExploredSaveSubsystem>();
			FString Error;
			return Save && Save->LoadFromSlot(SaveSlot, Error) == ESaveLoadResult::Ok;
		},
		[this, Stage]() { return Stage() == EBeaconStage::LinternaMontada && GetBeacon()->GetPagesRead() == 1; });
	Add(TEXT("partida del formato 1 rechazada sin romper nada"), [this]()
		{
			UExploredSaveSubsystem* Save = GetWorld()->GetGameInstance()->GetSubsystem<UExploredSaveSubsystem>();
			if (!Save)
			{
				return false;
			}
			FSaveDocument Old;
			Old.Header.FormatVersion = 1;
			const FString Path = FSaveSlotPolicy::JoinPath(Save->GetSaveDirectory(), FSaveSlotPolicy::MainFileName(ObsoleteSlot));
			if (!FFileHelper::SaveStringToFile(FSaveCodec::Encode(Old), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				return false;
			}
			FString Error;
			const ESaveLoadResult Result = Save->LoadFromSlot(ObsoleteSlot, Error);
			const bool bRejected = Result == ESaveLoadResult::ObsoleteVersion && Save->HasObsoleteSave();
			UE_LOG(LogExplored, Display, TEXT("[BalizaSmoke] Partida v1: %s (%s)"), LexToString(Result), *Error);
			IFileManager::Get().Delete(*Path);
			return bRejected;
		},
		[this, Stage]() { return Stage() == EBeaconStage::LinternaMontada; });
	Add(TEXT("leer la página 8"), [this]() { return Interact(TEXT("pagina_8"), false); },
		[this]() { return GetBeacon()->GetPagesRead() == 2; });
	Add(TEXT("BalizaTime 3 21.5"), [this]() { return Console(TEXT("BalizaTime 3 21.5")); }, [] { return true; });
	Add(TEXT("encender la baliza"), [this]() { return Interact(TEXT("faro_linterna"), false); },
		[Stage, Lamp]() { return Stage() == EBeaconStage::Final && Lamp() && Lamp()->IsLit(); });
	Add(TEXT("luces del Mistral"), [] { return true; },
		[this]() { return TActorIterator<AExploredMistralLights>(GetWorld()) ? true : false; });
	Add(TEXT("pantalla de epílogo"), [] { return true; },
		[this]()
		{
			const AExploredPlayerController* PC = Cast<AExploredPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
			return PC && PC->IsShowingEnding();
		}, EndingTimeoutSeconds);
	Add(TEXT("créditos del final"), [this]()
		{
			AExploredPlayerController* PC = Cast<AExploredPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
			if (PC)
			{
				PC->ShowEndCredits();
			}
			return PC != nullptr;
		},
		[] { return true; });
}

void UExploredBeaconSmokeTest::StartCurrent()
{
	Elapsed = 0.0f;
	if (!Steps[Current].Start())
	{
		UE_LOG(LogExplored, Error, TEXT("[BalizaSmoke] FALLO al empezar: %s"), *Steps[Current].Name);
		Finish(false);
	}
}

void UExploredBeaconSmokeTest::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bFinished)
	{
		return;
	}
	if (!bStarted)
	{
		WarmUp += DeltaTime;
		if (WarmUp < BeaconSmokeDetail::WarmUpSeconds || !GetPawn() || !GetBeacon() || !FindProp(TEXT("faro_linterna")))
		{
			return;
		}
		BuildSteps();
		bStarted = true;
		Current = 0;
		StartCurrent();
		return;
	}
	FStep& Step = Steps[Current];
	Elapsed += DeltaTime;
	if (Step.IsDone())
	{
		UE_LOG(LogExplored, Display, TEXT("[BalizaSmoke] %s ok (%.1f s)"), *Step.Name, Elapsed);
		++Current;
		if (Current >= Steps.Num())
		{
			Finish(true);
			return;
		}
		StartCurrent();
		return;
	}
	if (Elapsed > Step.TimeoutSeconds)
	{
		UE_LOG(LogExplored, Error, TEXT("[BalizaSmoke] FALLO por tiempo: %s"), *Step.Name);
		Finish(false);
	}
}

void UExploredBeaconSmokeTest::Finish(bool bOk)
{
	bFinished = true;
	UE_LOG(LogExplored, Display, TEXT("[BalizaSmoke] RESULTADO: %s (%d/%d pasos)"), bOk ? TEXT("OK") : TEXT("FALLO"),
		bOk ? Steps.Num() : FMath::Max(0, Current), Steps.Num());
	FGenericPlatformMisc::RequestExit(false);
}
