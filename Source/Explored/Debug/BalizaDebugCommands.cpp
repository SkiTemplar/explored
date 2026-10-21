// Comandos de consola de F3d (no Shipping): reloj acelerado, cuerpo, sueño, desmayo y balsa.
//
//   BalizaTimeScale <x>            el reloj (y el desgaste del cuerpo) va x veces más rápido
//   BalizaBody <sed> <hambre> <energía> <calor>   pone las cuatro barras
//   BalizaSleep                    intenta dormir (mismas reglas que E en un refugio)
//   BalizaFaint                    desmayo inmediato
//   BalizaRaft [board]             deja la balsa hecha; con «board», Iria sube con remo

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

#include "Boats/ExploredRaft.h"
#include "Explored.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Player/ExploredCharacter.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Survival/IslandBodyComponent.h"

namespace BalizaDebugDetail
{
	UIslandBodyComponent* FindBody(UWorld* World)
	{
		UIslandBodyComponent* Body = World ? UIslandBodyComponent::FindOn(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
		if (!Body)
		{
			UE_LOG(LogExplored, Warning, TEXT("[Baliza] No hay Iria en el mundo"));
		}
		return Body;
	}

	void CmdTimeScale(const TArray<FString>& Args, UWorld* World)
	{
		UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
		if (Args.Num() < 1 || !Time)
		{
			UE_LOG(LogExplored, Warning, TEXT("[Baliza] Uso: BalizaTimeScale <x> (1 = 18 min por día)"));
			return;
		}
		Time->SetTimeScale(FCString::Atof(*Args[0]));
		UE_LOG(LogExplored, Display, TEXT("[Baliza] Reloj x%.2f"), Time->GetTimeScale());
	}

	void CmdBody(const TArray<FString>& Args, UWorld* World)
	{
		UIslandBodyComponent* Body = FindBody(World);
		if (Args.Num() < 4 || !Body)
		{
			UE_LOG(LogExplored, Warning, TEXT("[Baliza] Uso: BalizaBody <sed> <hambre> <energía> <calor>"));
			return;
		}
		FBodyBars Bars;
		Bars.Thirst = FCString::Atof(*Args[0]);
		Bars.Hunger = FCString::Atof(*Args[1]);
		Bars.Energy = FCString::Atof(*Args[2]);
		Bars.Warmth = FCString::Atof(*Args[3]);
		Body->RestoreBars(Bars, Body->GetLastShelterId());
		UE_LOG(LogExplored, Display, TEXT("[Baliza] Cuerpo: %.0f/%.0f/%.0f/%.0f"), Body->GetBars().Thirst, Body->GetBars().Hunger,
			Body->GetBars().Energy, Body->GetBars().Warmth);
	}

	void CmdSleep(const TArray<FString>&, UWorld* World)
	{
		if (UIslandBodyComponent* Body = FindBody(World))
		{
			UE_LOG(LogExplored, Display, TEXT("[Baliza] Dormir: %s"), Body->TrySleep() ? TEXT("sí") : TEXT("no"));
		}
	}

	void CmdFaint(const TArray<FString>&, UWorld* World)
	{
		if (UIslandBodyComponent* Body = FindBody(World))
		{
			Body->ForceFaint();
		}
	}

	void CmdRaft(const TArray<FString>& Args, UWorld* World)
	{
		AExploredRaftSite* Site = nullptr;
		for (TActorIterator<AExploredRaftSite> It(World); It && !Site; ++It)
		{
			Site = *It;
		}
		if (!Site)
		{
			UE_LOG(LogExplored, Warning, TEXT("[Baliza] No hay sitio de la balsa en el mapa"));
			return;
		}
		Site->DebugBuild();
		AExploredCharacter* Character = Cast<AExploredCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
		if (Args.Num() >= 1 && Args[0] == TEXT("board") && Character && Site->GetRaft())
		{
			if (UExploredSurvivorComponent* Survivor = Character->GetSurvivorComponent(); Survivor && Survivor->GetItemCount(TEXT("remo")) == 0)
			{
				Survivor->TryAddItem(TEXT("remo"), 1);
			}
			Site->GetRaft()->Board(Character);
		}
	}

	FAutoConsoleCommandWithWorldAndArgs TimeScaleCommand(TEXT("BalizaTimeScale"),
		TEXT("Acelera el reloj de Baliza Siete: BalizaTimeScale <x>"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdTimeScale));
	FAutoConsoleCommandWithWorldAndArgs BodyCommand(TEXT("BalizaBody"),
		TEXT("Pone las barras de Iria: BalizaBody <sed> <hambre> <energía> <calor>"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdBody));
	FAutoConsoleCommandWithWorldAndArgs SleepCommand(TEXT("BalizaSleep"),
		TEXT("Intenta dormir (refugio, 20:00-05:00)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdSleep));
	FAutoConsoleCommandWithWorldAndArgs FaintCommand(TEXT("BalizaFaint"),
		TEXT("Desmayo inmediato de Iria"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdFaint));
	FAutoConsoleCommandWithWorldAndArgs RaftCommand(TEXT("BalizaRaft"),
		TEXT("Deja la balsa hecha: BalizaRaft [board]"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdRaft));
}

#endif
