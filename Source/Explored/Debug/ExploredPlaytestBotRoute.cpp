// Guion de la partida completa de Baliza Siete por bot (UExploredPlaytestBot con «-BalizaRun»): la ruta
// competente del GDD v3 §3 por días. Las utilidades y el bucle están en ExploredPlaytestBotRun.cpp.

#include "Debug/ExploredPlaytestBot.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

#include "Boats/ExploredRaft.h"
#include "Cooking/ExploredFire.h"
#include "Explored.h"
#include "Items/ExploredPickup.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Narrative/BeaconDirectorModel.h"
#include "Narrative/BeaconSubsystem.h"
#include "Narrative/ExploredBeaconActors.h"
#include "Player/ExploredCharacter.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Survival/IslandBodyComponent.h"
#include "UI/ExploredPlayerController.h"
#include "Weather/StormClockModel.h"
#include "WorldGen/ExploredShelterVolume.h"

namespace BalizaRouteDetail
{
	double At(int32 Day, double Hour)
	{
		return FStormClockModel::ToAbsoluteHours(Day, Hour);
	}
}

// ---------------------------------------------------------------------------------------------
// Guion
// ---------------------------------------------------------------------------------------------

void UExploredPlaytestBot::BuildRunScript()
{
	AddDayOne();
	AddDayTwo();
	AddCyclone();
	AddDayThree();
}

void UExploredPlaytestBot::AddDayOne()
{
	// Prólogo: beber un coco en Cala Varada.
	AddGoTo(TEXT("palma_cala_1"));
	AddUse(TEXT("palma_cala_1"), 2);
	AddStep(TEXT("beber un coco"), [this]() { return UseItem(TEXT("coco")); },
		[this]() { return Count(TEXT("coco_vacio")) >= 1; }, 10.0);
	AddGoTo(TEXT("lena_cala_1"));
	AddUse(TEXT("lena_cala_1"));
	AddGoTo(TEXT("lena_cala_2"));
	AddUse(TEXT("lena_cala_2"));
	// Palmar: fibras para la cuerda de la caña, cocos y página 3.
	for (const TCHAR* Fiber : { TEXT("fibra_palmar_1"), TEXT("fibra_palmar_2") })
	{
		AddGoTo(Fiber);
		AddUse(Fiber);
	}
	AddCraft(TEXT("cuerda"));
	AddGoTo(TEXT("pagina_3"));
	AddUse(TEXT("pagina_3"), 1, true);
	// Bosque de Bambú: caña de pescar y página 4.
	AddGoTo(TEXT("cana_bambu_1"));
	AddUse(TEXT("cana_bambu_1"));
	AddCraft(TEXT("cana_pescar"));
	AddGoTo(TEXT("pagina_4"));
	AddUse(TEXT("pagina_4"), 1, true);
	// Rocas del Pescador: fuego, pesca, asar y comer; página 2.
	AddGoTo(TEXT("lena_rocas_1"));
	AddCraft(TEXT("hoguera"));
	AddFireJob([this]() { return NearestFire(); }, TEXT("avivar"), FString());
	AddGoTo(TEXT("punto_pesca_1"));
	AddFish(2);
	AddGoToActor(TEXT("la hoguera de las rocas"), [this]() { return Cast<AActor>(NearestFire()); });
	AddFireJob([this]() { return NearestFire(); }, TEXT("cocinar"), TEXT("pez_asado"));
	AddFireJob([this]() { return NearestFire(); }, TEXT("cocinar"), TEXT("pez_asado"));
	AddStep(TEXT("comer un pez asado"), [this]() { return UseItem(TEXT("pez_asado")); },
		[] { return true; }, 10.0);
	AddGoTo(TEXT("pagina_2"));
	AddUse(TEXT("pagina_2"), 1, true);
	// Colina del Faro: casa, página 1 y mirador (página 7).
	AddGoTo(TEXT("pagina_1"));
	AddUse(TEXT("pagina_1"));
	AddGoTo(TEXT("pagina_7"));
	AddUse(TEXT("pagina_7"), 1, true);
	AddStep(TEXT("beber el segundo coco (cuenco para el aceite)"), [this]() { return Count(TEXT("coco")) == 0 || UseItem(TEXT("coco")); },
		[this]() { return Count(TEXT("coco")) == 0; }, 10.0);
	// Tarde: fibras para las 3 cuerdas de la balsa y el remo; el chubasco llena el barril.
	for (int32 Index = 3; Index <= 8; ++Index)
	{
		const FString Fiber = FString::Printf(TEXT("fibra_palmar_%d"), Index);
		AddGoTo(Fiber);
		AddUse(Fiber);
	}
	for (int32 Rope = 0; Rope < 3; ++Rope)
	{
		AddCraft(TEXT("cuerda"));
	}
	AddWaitUntil(1, 17.1, TEXT("fin del chubasco"));
	AddGoTo(TEXT("barril_lluvia_faro"));
	AddUse(TEXT("barril_lluvia_faro"), 1, true);
	AddGoTo(TEXT("refugio_faro_volumen"));
	AddWaitUntil(1, 20.05, TEXT("hora de dormir"));
	AddStep(TEXT("dormir en el refugio del faro"), [this]() { return Body() && Body()->TrySleep(); },
		[this]() { return Body() && !Body()->IsFainting() && AbsHours() >= BalizaRouteDetail::At(2, 6.0); }, 120.0);
}

void UExploredPlaytestBot::AddDayTwo()
{
	AddStep(TEXT("desayunar el pez asado de ayer"), [this]() { return Count(TEXT("pez_asado")) == 0 || UseItem(TEXT("pez_asado")); },
		[] { return true; }, 10.0);
	AddGoTo(TEXT("barril_lluvia_faro"));
	AddUse(TEXT("barril_lluvia_faro"), 1, true);
	// Troncos de uno en uno al sitio de la balsa; el quinto se ata como remo.
	for (int32 Log = 1; Log <= 4; ++Log)
	{
		const FString Id = FString::Printf(TEXT("tronco_bambu_%d"), Log);
		AddGoTo(Id);
		AddUse(Id);
		AddGoTo(TEXT("balsa_sitio"));
		AddUse(TEXT("balsa_sitio"));
	}
	AddGoTo(TEXT("tronco_bambu_5"));
	AddUse(TEXT("tronco_bambu_5"));
	AddCraft(TEXT("remo"));
	AddGoTo(TEXT("pagina_5"));
	AddUse(TEXT("pagina_5"), 1, true);
	AddGoTo(TEXT("balsa_sitio"));
	AddUse(TEXT("balsa_sitio"));
	auto Site = [this]() -> AExploredRaftSite*
	{
		TActorIterator<AExploredRaftSite> It(GetWorld());
		return It ? *It : nullptr;
	};
	auto RaftActor = [Site]() -> AActor* { return Site() ? Site()->GetRaft() : nullptr; };
	AddStep(TEXT("subir a la balsa"), [this, RaftActor]() { return InteractWith(RaftActor()); },
		[Site]() { return Site() && Site()->GetRaft() && Site()->GetRaft()->IsOccupied(); }, 10.0);
	AddRow(TEXT("el islote del carguero"), [this]()
	{
		const AActor* Hull = Prop(TEXT("carguero_casco"));
		return Hull ? Hull->GetActorLocation() : FVector::ZeroVector;
	}, 40.0);
	AddStep(TEXT("bajar en el islote"), [Site]()
	{
		Site()->GetRaft()->Leave();
		return true;
	}, [] { return true; }, 5.0);
	AddGoTo(TEXT("pagina_6"));
	AddUse(TEXT("pagina_6"), 1, true);
	AddGoTo(TEXT("lente"));
	AddUse(TEXT("lente"));
	AddGoToActor(TEXT("la balsa"), RaftActor);
	AddStep(TEXT("volver a subir con la lente"), [this, RaftActor]() { return InteractWith(RaftActor()); },
		[Site]() { return Site()->GetRaft()->IsOccupied(); }, 10.0);
	AddRow(TEXT("Playa Norte"), [Site]() { return Site()->GetShoreLocation(); }, 12.0);
	AddStep(TEXT("bajar en Playa Norte"), [Site]()
	{
		Site()->GetRaft()->Leave();
		return true;
	}, [] { return true; }, 5.0);
	AddGoTo(TEXT("caja_suministros_faro"));
	AddStep(TEXT("dejar la lente junto al faro"), [this]() { return Survivor() && Survivor()->DropHeavy(); },
		[this]() { return Survivor()->GetHeldHeavy().IsNone(); }, 10.0);
}

void UExploredPlaytestBot::AddCyclone()
{
	AddGoTo(TEXT("refugio_faro_volumen"));
	AddWaitUntil(2, 20.1, TEXT("noche del ciclón"));
	AddStep(TEXT("en el ciclón no se duerme"), [this]() { return Body() && Body()->IsSheltered() && !Body()->TrySleep(); },
		[] { return true; }, 10.0);
	AddWaitUntil(2, 22.2, TEXT("ojo del ciclón"));
	AddGoTo(TEXT("barril_lluvia_faro"));
	AddUse(TEXT("barril_lluvia_faro"), 1, true);
	AddGoTo(TEXT("refugio_faro_volumen"));
	AddWaitUntil(3, 2.05, TEXT("fin del ciclón"));
	AddStep(TEXT("dormir tras el ciclón"), [this]() { return Body() && Body()->TrySleep(); },
		[this]() { return Body() && !Body()->IsFainting() && AbsHours() >= BalizaRouteDetail::At(3, 6.0); }, 120.0);
}

void UExploredPlaytestBot::AddDayThree()
{
	AddStep(TEXT("soltar el remo (ya no hace falta)"), [this]() { return Survivor() && Survivor()->DropItem(TEXT("remo")); },
		[this]() { return Count(TEXT("remo")) == 0; }, 10.0);
	AddGoTo(TEXT("barril_lluvia_faro"));
	AddUse(TEXT("barril_lluvia_faro"), 1, true);
	AddGoTo(TEXT("lena_rocas_1"));
	AddUse(TEXT("lena_rocas_1"));
	AddGoTo(TEXT("lena_rocas_2"));
	AddUse(TEXT("lena_rocas_2"));
	AddGoTo(TEXT("punto_pesca_1"));
	AddFish(5);
	auto Hearth = [this]() { return Prop(TEXT("hoguera_faro")); };
	AddGoTo(TEXT("hoguera_faro"));
	AddFireJob(Hearth, TEXT("encender"), FString());
	AddFireJob(Hearth, TEXT("avivar"), FString());
	AddFireJob(Hearth, TEXT("cocinar"), TEXT("pez_asado"));
	AddStep(TEXT("comer"), [this]() { return UseItem(TEXT("pez_asado")); }, [] { return true; }, 10.0);
	AddFireJob(Hearth, TEXT("hervir"), TEXT("tarro_aceite"));
	AddFireJob(Hearth, TEXT("hervir"), TEXT("tarro_aceite"));
	for (int32 Index = 1; Index <= 3; ++Index)
	{
		const FString Fiber = FString::Printf(TEXT("fibra_bambu_%d"), Index);
		AddGoTo(Fiber);
		AddUse(Fiber);
	}
	AddCraft(TEXT("mecha"));
	auto DroppedLens = [this]() -> AActor*
	{
		for (TActorIterator<AExploredPickup> It(GetWorld()); It; ++It)
		{
			if (It->IsAvailable() && It->GetItemId() == FName(TEXT("lente")))
			{
				return *It;
			}
		}
		return nullptr;
	};
	AddGoToActor(TEXT("la lente"), DroppedLens);
	AddStep(TEXT("coger la lente"), [this, DroppedLens]() { return InteractWith(DroppedLens()); },
		[this]() { return Idle() && Survivor()->GetHeldHeavy() == FName(TEXT("lente")); }, 10.0);
	AddGoTo(TEXT("faro_torre"));
	AddStep(TEXT("subir a la linterna"), [this]() { return InteractWith(Prop(TEXT("faro_torre"))); }, [this]()
	{
		const AExploredLighthouseTower* Tower = Cast<AExploredLighthouseTower>(Prop(TEXT("faro_torre")));
		return Tower && Character.IsValid() && Tower->IsUp(*Character.Get()) && Idle();
	}, 30.0);
	AddStep(TEXT("leer la página 8"), [this]() { return InteractWith(Prop(TEXT("pagina_8"))); },
		[this]() { return Idle(); }, 10.0);
	for (const TCHAR* Slot : { TEXT("poner la lente"), TEXT("verter los tarros"), TEXT("apretar la mecha") })
	{
		AddStep(Slot, [this]() { return InteractWith(Prop(TEXT("faro_linterna"))); },
			[this]() { return Idle() && Run.Clock.Est1xSeconds - StepStart1x > 0.5; }, 30.0);
	}
	AddWaitUntil(3, 20.05, TEXT("la noche"));
	AddStep(TEXT("encender la baliza"), [this]() { return InteractWith(Prop(TEXT("faro_linterna"))); }, [this]()
	{
		const UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(GetWorld());
		return Beacon && Beacon->IsBeaconLit();
	}, 30.0);
	AddStep(TEXT("Mistral, bocina y epílogo"), [] { return true; }, [this]()
	{
		const AExploredPlayerController* PC = Cast<AExploredPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
		return PC && PC->IsShowingEnding();
	}, 90.0);
}
