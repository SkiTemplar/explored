// UExploredTrailerSubsystem: lectura de Tools/Trailer/trailer.json y preparación del plano con interfaz.
#include "Debug/ExploredTrailerSubsystem.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Explored.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Narrative/BeaconSubsystem.h"
#include "Player/ExploredCharacter.h"
#include "Survival/BodyEventsModel.h"
#include "Survival/IslandBodyComponent.h"
#include "UI/ExploredHUD.h"
#include "UI/ExploredPlayerController.h"
#include "WorldGen/GroundHeight.h"
#include "WorldGen/IslandLayout.h"

namespace TrailerSetupDetail
{
	TArray<FVector> ReadPoints(const FJsonObject& Object, const TCHAR* Field)
	{
		TArray<FVector> Points;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.TryGetArrayField(Field, Values))
		{
			return Points;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TArray<TSharedPtr<FJsonValue>>* Xyz = nullptr;
			if (Value.IsValid() && Value->TryGetArray(Xyz) && Xyz->Num() == 3)
			{
				Points.Add(FVector((*Xyz)[0]->AsNumber(), (*Xyz)[1]->AsNumber(), (*Xyz)[2]->AsNumber()));
			}
		}
		return Points;
	}

	/** Par [desde, hasta] o un número suelto (mismo valor en los dos). */
	void ReadRange(const FJsonObject& Object, const TCHAR* Field, float& From, float& To)
	{
		const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
		double Single = 0.0;
		if (Object.TryGetArrayField(Field, Pair) && Pair->Num() == 2)
		{
			From = static_cast<float>((*Pair)[0]->AsNumber());
			To = static_cast<float>((*Pair)[1]->AsNumber());
		}
		else if (Object.TryGetNumberField(Field, Single))
		{
			From = To = static_cast<float>(Single);
		}
	}

	FTrailerShot ReadShot(const FJsonObject& Object)
	{
		FTrailerShot Shot;
		Object.TryGetStringField(TEXT("name"), Shot.Name);
		Object.TryGetStringField(TEXT("section"), Shot.Section);
		Object.TryGetStringField(TEXT("action"), Shot.Action);
		Object.TryGetNumberField(TEXT("duration"), Shot.DurationSeconds);
		Object.TryGetNumberField(TEXT("day"), Shot.Day);
		Object.TryGetNumberField(TEXT("fadeIn"), Shot.FadeInSeconds);
		Object.TryGetNumberField(TEXT("fadeOut"), Shot.FadeOutSeconds);
		Object.TryGetNumberField(TEXT("ease"), Shot.EaseFraction);
		Object.TryGetBoolField(TEXT("followGround"), Shot.bFollowGround);
		Object.TryGetBoolField(TEXT("ui"), Shot.bShowUI);
		ReadRange(Object, TEXT("hours"), Shot.HoursFrom, Shot.HoursTo);
		ReadRange(Object, TEXT("fov"), Shot.FovFrom, Shot.FovTo);
		FString Kind;
		Object.TryGetStringField(TEXT("kind"), Kind);
		Shot.Kind = FTrailerModel::ParseKind(Kind);
		Shot.EyeM = ReadPoints(Object, TEXT("eye"));
		Shot.LookM = ReadPoints(Object, TEXT("look"));
		return Shot;
	}
}

bool UExploredTrailerSubsystem::LoadTimeline(const FString& Path, FString& OutError)
{
	using namespace TrailerSetupDetail;
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		OutError = TEXT("no se puede leer");
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		OutError = TEXT("JSON no válido");
		return false;
	}
	int32 Fps = 30;
	Root->TryGetNumberField(TEXT("fps"), Fps);
	Root->TryGetNumberField(TEXT("settleSeconds"), SettleSeconds);
	SettleSeconds = FMath::Clamp(SettleSeconds, 0.5f, 30.0f);
	const TSharedPtr<FJsonObject>* LimitsObject = nullptr;
	if (Root->TryGetObjectField(TEXT("limits"), LimitsObject))
	{
		(*LimitsObject)->TryGetNumberField(TEXT("maxEyeStepM"), Limits.MaxEyeStepM);
		(*LimitsObject)->TryGetNumberField(TEXT("maxTurnStepDeg"), Limits.MaxTurnStepDeg);
		(*LimitsObject)->TryGetNumberField(TEXT("maxEyeAccelM"), Limits.MaxEyeAccelM);
	}
	TArray<FTrailerShot> Shots;
	const TArray<TSharedPtr<FJsonValue>>* ShotValues = nullptr;
	if (Root->TryGetArrayField(TEXT("shots"), ShotValues))
	{
		for (const TSharedPtr<FJsonValue>& Value : *ShotValues)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (Value.IsValid() && Value->TryGetObject(Object))
			{
				Shots.Add(ReadShot(**Object));
			}
		}
	}
	if (!FTrailerModel::BuildTimeline(Shots, Fps, Timeline, OutError))
	{
		return false;
	}
	// Selección de planos (-TrailerShots=a,b): por defecto, todos menos el final, que va en su proceso.
	FString Wanted;
	FParse::Value(FCommandLine::Get(), TEXT("TrailerShots="), Wanted, false);
	TArray<FString> Names;
	Wanted.ParseIntoArray(Names, TEXT(","), true);
	TArray<int32> Selected;
	for (int32 Index = 0; Index < Timeline.Shots.Num(); ++Index)
	{
		const bool bNamed = Names.Contains(Timeline.Shots[Index].Name);
		if (bNamed || (Names.IsEmpty() && Timeline.Shots[Index].Kind != ETrailerShotKind::Finale))
		{
			Selected.Add(Index);
		}
	}
	for (const FString& Name : Names)
	{
		if (FTrailerModel::FindShotByName(Timeline, Name) == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("no hay ningún plano «%s»"), *Name);
			return false;
		}
	}
	return FTrailerModel::CaptureOrder(Timeline, Selected, Order, OutError);
}

void UExploredTrailerSubsystem::PrepareHud(const FTrailerShot& Shot)
{
	UWorld* World = GetWorld();
	AExploredPlayerController* PC = Cast<AExploredPlayerController>(UGameplayStatics::GetPlayerController(World, 0));
	AExploredCharacter* Character = PC ? Cast<AExploredCharacter>(PC->GetPawn()) : nullptr;
	UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(World);
	UExploredSurvivorComponent* Survivor = Character ? Character->GetSurvivorComponent() : nullptr;
	UIslandBodyComponent* Body = Character ? Character->GetIslandBodyComponent() : nullptr;
	if (!PC || !Character || !Beacon || !Survivor || !Body)
	{
		UE_LOG(LogExplored, Warning, TEXT("[Trailer] %s: falta el controlador, Iria o la historia"), *Shot.Name);
		return;
	}
	// Partida en marcha a mitad del día 1: objetivo de subir al faro, barras con algo de desgaste y el
	// cuchillo en la mano. Iria de pie en el primer punto del plano; la vista gira con la cámara del plano.
	PC->ResumeGame();
	Beacon->DebugSetStage(EBeaconStage::Faro);
	const FVector2D Stand(Shot.EyeM[0].X, Shot.EyeM[0].Y);
	const float GroundCm = FMath::Max(ExploredGround::HeightAtMeters(World, Stand.X, Stand.Y), FIslandLayout::SeaLevel) * 100.0f;
	Character->SetActorHiddenInGame(false);
	Character->SetActorLocation(FVector(Stand.X * 100.0, Stand.Y * 100.0, GroundCm + 100.0), false, nullptr, ETeleportType::TeleportPhysics);
	PC->SetViewTarget(Character);
	const FName Knife(TEXT("cuchillo"));
	for (const TCHAR* Item : { TEXT("cuchillo"), TEXT("cantimplora"), TEXT("coco"), TEXT("fibra") })
	{
		if (Survivor->GetItemCount(FName(Item)) == 0)
		{
			Survivor->TryAddItem(FName(Item));
		}
	}
	const int32 KnifeSlot = Survivor->GetBackpack().Stacks.IndexOfByPredicate([&Knife](const FBackpackStack& Stack) { return Stack.Id == Knife; });
	Survivor->SelectQuickSlot(KnifeSlot == 0 ? 1 : 0);
	Survivor->SelectQuickSlot(FMath::Max(KnifeSlot, 0));
	FBodyBars Bars;
	Bars.Thirst = 58.0f;
	Bars.Hunger = 71.0f;
	Bars.Energy = 83.0f;
	Bars.Warmth = 92.0f;
	Body->RestoreBars(Bars, Body->GetLastShelterId());
	ClearHudTransient();
}

void UExploredTrailerSubsystem::ClearHudTransient() const
{
	const APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (AExploredHUD* HUD = PC ? Cast<AExploredHUD>(PC->GetHUD()) : nullptr)
	{
		HUD->ClearTransient();
	}
}
