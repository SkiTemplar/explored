// Conjunto de capturas «f8» de UExploredShotSubsystem: césped continuo (alfombra a la altura del jugador,
// el adelgazamiento visto desde arriba y dos capturas separadas 1 s caminando) y arena estilizada
// (orilla, de cerca y de cara al sol). docs/v3/capturas/f8-cesped-*.png y f8-arena-*.png.
#include "Debug/ExploredShotSubsystem.h"

#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#include "Explored.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "WorldGen/ExploredGrassField.h"
#include "WorldGen/GroundHeight.h"
#include "WorldGen/IslandLayout.h"

namespace ShotGrassDetail
{
	constexpr float EyeM = 1.7f;
	/** Paso al caminar (m/s): el de Iria andando deprisa. */
	constexpr float WalkSpeedM = 4.0f;

	/** Primer punto de A hacia B (pasos de 0,5 m) con el suelo por debajo de MaxHeight: la orilla. */
	FVector2D FindShore(const UWorld* World, const FVector2D& Land, const FVector2D& Sea, float MaxHeight)
	{
		const double Length = FVector2D::Distance(Land, Sea);
		const FVector2D Dir = (Sea - Land).GetSafeNormal();
		for (double T = 0.0; T <= Length; T += 0.5)
		{
			const FVector2D P = Land + Dir * T;
			if (ExploredGround::HeightAtMeters(World, P.X, P.Y) < MaxHeight)
			{
				return P;
			}
		}
		return Sea;
	}
}

void UExploredShotSubsystem::BuildGrassSandShots()
{
	using namespace ShotGrassDetail;
	const UWorld* World = GetWorld();
	auto GroundAt = [World](const FVector2D& P)
	{
		return FMath::Max(ExploredGround::HeightAtMeters(World, P.X, P.Y), FIslandLayout::SeaLevel);
	};
	auto Add = [this](const FString& Name, const FVector& EyeM3, const FVector& LookM3, float Hours)
	{
		FExploredShot Shot;
		Shot.Name = Name;
		Shot.Location = EyeM3 * 100.0;
		Shot.Rotation = ((LookM3 - EyeM3) * 100.0).Rotation();
		Shot.Hours = Hours;
		Shot.Day = 1;
		Shots.Add(Shot);
		return Shots.Num() - 1;
	};
	auto Eye = [&GroundAt](const FVector2D& P, float Height = EyeM)
	{
		return FVector(P.X, P.Y, GroundAt(P) + Height);
	};

	// Césped a la altura de los ojos: Palmar, ladera de la colina y la Cala mirando tierra adentro.
	const FVector2D PalmarEye(-62.0f, -38.0f);
	const FVector2D PalmarLook(-30.0f, -8.0f);
	Add(TEXT("f8-cesped-palmar"), Eye(PalmarEye), Eye(PalmarLook, 0.6f), 11.0f);
	const FVector2D HillEye(28.0f, 2.0f);
	const FVector2D HillLook(2.0f, -24.0f);
	Add(TEXT("f8-cesped-colina"), Eye(HillEye), Eye(HillLook, 0.4f), 15.0f);
	const FVector2D CalaEye(-108.0f, 0.0f);
	const FVector2D CalaLook(-78.0f, -8.0f);
	Add(TEXT("f8-cesped-cala"), Eye(CalaEye), Eye(CalaLook, 0.8f), 10.5f);
	// De cerca, mirando al suelo: hojas completas y sin suelo desnudo entre parches.
	Add(TEXT("f8-cesped-cerca"), Eye(PalmarEye), Eye(PalmarEye + (PalmarLook - PalmarEye).GetSafeNormal() * 3.0, 0.0f), 11.0f);
	// Desde la ladera, 5 m por encima: el adelgazamiento de 25 a 100 m sobre el Palmar.
	const FVector2D SlopeEye(22.0f, -6.0f);
	Add(TEXT("f8-cesped-lejos"), Eye(SlopeEye, 5.0f), Eye(FVector2D(-45.0f, -25.0f), 0.0f), 15.0f);

	// Dos capturas separadas 1 s caminando por el Palmar (tras 3 s de paseo).
	const FVector2D WalkDir = (PalmarLook - PalmarEye).GetSafeNormal();
	const FVector2D WalkStart = PalmarEye - WalkDir * 12.0;
	const int32 Walk = Add(TEXT("f8-cesped-paseo-a"), Eye(WalkStart), Eye(WalkStart + WalkDir * 20.0, 1.0f), 11.0f);
	Shots[Walk].Action = TEXT("walk");
	Shots[Walk].WalkVelocity = FVector(WalkDir.X, WalkDir.Y, 0.0) * WalkSpeedM * 100.0;
	Shots[Walk].WalkSeconds = 3.0f;
	FExploredShot Next = Shots[Walk];
	Next.Name = TEXT("f8-cesped-paseo-b");
	Next.Action = TEXT("walkon");
	Next.WalkSeconds = 1.0f;
	Shots.Add(Next);

	// Arena de la Cala Varada: la orilla a lo largo de la playa, de cerca y de cara al sol de la tarde.
	const FVector2D Shore = FindShore(World, CalaEye, FVector2D(-150.0f, -12.0f), 0.2f);
	const FVector2D Inland = (CalaEye - Shore).GetSafeNormal();
	const FVector2D Along(-Inland.Y, Inland.X);
	const FVector2D BeachEye = Shore + Inland * 4.0;
	Add(TEXT("f8-arena-orilla"), Eye(BeachEye), Eye(BeachEye + Along * 14.0 - Inland * 2.0, 0.0f), 10.5f);
	const FVector2D DryEye = Shore + Inland * 9.0;
	Add(TEXT("f8-arena-cerca"), Eye(DryEye), Eye(DryEye + Along * 2.5, 0.0f), 12.0f);
	const float DuskHours = 17.2f;
	const FVector Sun = ExploredSky::SunDirection(DuskHours, 1.0f + DuskHours / 24.0f).GetSafeNormal2D();
	const FVector2D SunFlat(Sun.X, Sun.Y);
	Add(TEXT("f8-arena-sol"), Eye(DryEye), Eye(DryEye + SunFlat * 6.0, 0.0f), DuskHours);
	// El encuadre de f6-relieve-cala (manchas blancas en la orilla) para comparar.
	Add(TEXT("f8-arena-cala"), FVector(-150.0f, -12.0f, 7.0f), FVector(-98.0f, 2.0f, 2.0f), 10.5f);
}

bool UExploredShotSubsystem::TickWalk(float DeltaTime)
{
	const FExploredShot& Shot = Shots[Current];
	if (WalkElapsed < 0.0f)
	{
		// Primer frame asentado: empieza a caminar y el coste del césped se mide desde aquí.
		WalkElapsed = 0.0f;
		for (TActorIterator<AExploredGrassField> It(GetWorld()); It; ++It)
		{
			It->ResetSeedStats();
		}
		return false;
	}
	WalkElapsed += DeltaTime;
	if (Camera)
	{
		FVector Location = Camera->GetActorLocation() + Shot.WalkVelocity * DeltaTime;
		Location.Z = (ExploredGround::HeightAtMeters(GetWorld(), Location.X / 100.0, Location.Y / 100.0) * 100.0) + Shot.WalkEyeCm;
		Camera->SetActorLocation(Location);
	}
	return WalkElapsed >= Shot.WalkSeconds;
}

void UExploredShotSubsystem::LogGrassCost(const FString& Name) const
{
	for (TActorIterator<AExploredGrassField> It(GetWorld()); It; ++It)
	{
		UE_LOG(LogExplored, Display, TEXT("[Shots] césped %s: %d parches y matas, %.2f M triángulos a la vista, siembra media %.3f ms y máx. %.2f ms"),
			*Name, It->GetGrassInstanceCount(), It->GetGrassTrianglesInView() / 1.0e6, It->GetGrassSeedMs(), It->GetGrassSeedMaxMs());
	}
}
