// Mundo de prueba del césped compartido por GrassFieldSpec y GrassThinningSpec (solo modelos puros).
#pragma once

#include "CoreMinimal.h"

#include "WorldGen/GrassFieldModel.h"
#include "WorldGen/GroundField.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GrassTestWorld
{
	constexpr int32 N = 101;
	constexpr float CellM = 2.0f;
	inline const FVector2D GridMin(-100.0, -100.0);

	/** Suelo de prueba: llano a 2 m con hierba, arena al norte (x > 60), mar al sur (x < -80) y una ladera de 56° (y < -80). */
	inline float HeightAt(double X, double Y)
	{
		if (X < -80.0)
		{
			return -1.0f;
		}
		return Y < -80.0 ? static_cast<float>(2.0 + (-80.0 - Y) * 1.5) : 2.0f;
	}

	inline EGroundSurface SurfaceAt(double X, double /*Y*/)
	{
		if (X < -80.0)
		{
			return EGroundSurface::Sea;
		}
		return X > 60.0 ? EGroundSurface::Sand : EGroundSurface::Grass;
	}

	inline TSharedPtr<FGroundField> MakeGround()
	{
		TArray<FVector3f> Verts;
		TArray<uint8> Surface;
		for (int32 I = 0; I < N; ++I)
		{
			for (int32 J = 0; J < N; ++J)
			{
				const double X = GridMin.X + I * CellM;
				const double Y = GridMin.Y + J * CellM;
				Verts.Add(FVector3f(static_cast<float>(X), static_cast<float>(Y), HeightAt(X, Y)));
				Surface.Add(static_cast<uint8>(SurfaceAt(X, Y)));
			}
		}
		TArray<uint8> Flip;
		Flip.Init(0, (N - 1) * (N - 1));
		TSharedPtr<FGroundField> Ground = MakeShared<FGroundField>();
		FString Error;
		check(Ground->Init(N, N, GridMin, CellM, MoveTemp(Verts), MoveTemp(Surface), MoveTemp(Flip), Error));
		return Ground;
	}

	/** Zona densa en (-40, 40), camino norte-sur en x = 0, una exclusión en (30, 30) y una palmera en (30, -30). */
	inline TSharedPtr<FGrassWorld> MakeWorld()
	{
		TSharedPtr<FGrassWorld> World = MakeShared<FGrassWorld>();
		World->Ground = MakeGround();
		FIslandZone Dense;
		Dense.Id = FName(TEXT("densa"));
		Dense.Center = FVector2D(-40.0, 40.0);
		Dense.Radius = 25.0f;
		World->Layout.Zones.Add(Dense);
		FIslandPath Path;
		Path.Id = FName(TEXT("camino"));
		Path.Width = 3.0f;
		Path.Points = {FVector2D(0.0, -60.0), FVector2D(0.0, 60.0)};
		World->Layout.Paths.Add(Path);
		World->Exclusions.Add({FVector2D(30.0, 30.0), 5.0f});
		World->Palms.Add(FVector2D(30.0, -30.0));
		return World;
	}

	/** Densidad 3 en la zona y 1 fuera, sin claros, adelgazamiento corto (denso hasta 10 m, nada desde 40 m). */
	inline FGrassConfig MakeConfig()
	{
		FGrassConfig Config = FGrassConfig::Defaults();
		Config.ZoneDensity.Empty();
		Config.ZoneDensity.Add(FName(TEXT("densa")), 3.0f);
		Config.DensityPerM2 = 1.0f;
		Config.ClearingThreshold = 2.0f;  // sin claros salvo que el test los pida
		Config.Thinning.DenseM = 10.0f;
		Config.Thinning.EndM = 40.0f;
		Config.Thinning.EndFadeM = 10.0f;
		Config.Thinning.MarginM = 3.0f;
		Config.Thinning.HysteresisM = 2.0f;
		Config.WorkPerFrame = 400;
		return Config;
	}

	/** Matas generadas en un rectángulo de celdas (incluidos los extremos). */
	inline TArray<FGrassInstance> Generate(const FGrassConfig& Config, const FGrassWorld& World, int32 FromX, int32 ToX, int32 FromY, int32 ToY)
	{
		TArray<FGrassInstance> Out;
		for (int32 X = FromX; X <= ToX; ++X)
		{
			for (int32 Y = FromY; Y <= ToY; ++Y)
			{
				FGrassFieldModel::GenerateCell(Config, World, FIntPoint(X, Y), Out);
			}
		}
		return Out;
	}

	inline int32 CountIn(const TArray<FGrassInstance>& All, const FVector2D& Center, double Radius)
	{
		int32 Count = 0;
		for (const FGrassInstance& I : All)
		{
			Count += FVector2D::Distance(FVector2D(I.Location.X, I.Location.Y), Center) < Radius ? 1 : 0;
		}
		return Count;
	}

	inline double Dist2D(const FGrassInstance& I, const FVector2D& Camera)
	{
		return FVector2D::Distance(FVector2D(I.Location.X, I.Location.Y), Camera);
	}

	/**
	 * Componentes simulados: guarda la mata de cada ranura. Con una cámara, cuenta las matas que aparecen
	 * o desaparecen con tamaño > 0 en el material (lo que sería un salto visible).
	 */
	struct FFakeComponents
	{
		TArray<TArray<FGrassInstance>> Slots;
		TArray<TArray<uint8>> Live;
		int32 Pops = 0;

		FFakeComponents()
		{
			Slots.SetNum(GrassKindCount);
			Live.SetNum(GrassKindCount);
		}

		bool Apply(const FGrassUpdate& Update, const FGrassThinning* Thin = nullptr, const FVector2D& Camera = FVector2D::ZeroVector)
		{
			for (int32 K = 0; K < GrassKindCount; ++K)
			{
				for (const FGrassWrite& W : Update.Writes[K])
				{
					if (W.Slot > Slots[K].Num())
					{
						return false;  // hueco: el componente no puede añadir fuera de orden
					}
					if (W.Slot == Slots[K].Num())
					{
						Slots[K].Add(W.Instance);
						Live[K].Add(0);
					}
					CheckGone(K, W.Slot, Thin, Camera);
					if (Thin && Thin->Visibility(W.Instance.Rank, Dist2D(W.Instance, Camera)) > 0.0f)
					{
						++Pops;  // aparece ya visible
					}
					Slots[K][W.Slot] = W.Instance;
					Live[K][W.Slot] = 1;
				}
				for (const int32 Slot : Update.Hides[K])
				{
					if (!Slots[K].IsValidIndex(Slot))
					{
						return false;
					}
					CheckGone(K, Slot, Thin, Camera);
					Live[K][Slot] = 0;
				}
			}
			return true;
		}

		/** La mata que deja la ranura tenía que estar ya encogida a 0. */
		void CheckGone(int32 K, int32 Slot, const FGrassThinning* Thin, const FVector2D& Camera)
		{
			if (Thin && Live[K][Slot] && Thin->Visibility(Slots[K][Slot].Rank, Dist2D(Slots[K][Slot], Camera)) > 0.0f)
			{
				++Pops;
			}
		}

		int32 Visible() const
		{
			int32 Count = 0;
			for (const TArray<uint8>& Kind : Live)
			{
				for (const uint8 V : Kind)
				{
					Count += V;
				}
			}
			return Count;
		}
	};

	/** Actualiza hasta que no queda trabajo pendiente; devuelve los frames y el mayor trabajo de uno. */
	inline int32 Settle(FGrassFieldModel& Model, const FVector2D& Camera, FFakeComponents* Fake, int32& MaxWork)
	{
		int32 Frames = 0;
		do
		{
			const FGrassUpdate Update = Model.Update(Camera);
			MaxWork = FMath::Max(MaxWork, Update.Work);
			if (Fake)
			{
				check(Fake->Apply(Update));
			}
			++Frames;
		}
		while (Model.GetPendingCellCount() > 0 && Frames < 5000);
		return Frames;
	}
}

#endif
