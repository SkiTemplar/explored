#include "WorldGen/TerrainDensity.h"

#include "Async/ParallelFor.h"
#include "Core/ExploredRandom.h"

namespace
{
	/** Distancia normalizada máxima a la que una isla influye (fin de la plataforma). */
	constexpr float InfluenceLimit = 1.9f;
	/** Profundidad de la plataforma somera junto a la costa. */
	constexpr float ShelfDepth = -1.2f;
	/** Cota media del bajo rocoso de Los Dientes (FIslandShapeModel, TeethLand). */
	constexpr float TeethShoalDepth = -5.4f;

	FORCEINLINE float SmoothStep(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	float DistanceToSegment(const FVector& P, const FVector& A, const FVector& B, float& OutT)
	{
		const FVector AB = B - A;
		const float LenSq = AB.SizeSquared();
		OutT = LenSq > KINDA_SMALL_NUMBER ? FMath::Clamp(FVector::DotProduct(P - A, AB) / LenSq, 0.0f, 1.0f) : 0.0f;
		return FVector::Dist(P, A + AB * OutT);
	}

	struct FPalette
	{
		FLinearColor Sand;
		FLinearColor Grass;
		FLinearColor Rock;
	};

	FPalette PaletteFor(EIslandArchetype Archetype)
	{
		// Colores en sRGB convertidos a lineal para el color de vértice.
		auto C = [](uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); };
		switch (Archetype)
		{
		case EIslandArchetype::Landing: return {C(232, 212, 172), C(104, 128, 70), C(124, 116, 104)};
		case EIslandArchetype::Emerald: return {C(220, 200, 160), C(74, 112, 58), C(92, 100, 86)};
		case EIslandArchetype::Smoke: return {C(70, 64, 62), C(100, 110, 64), C(58, 46, 44)};
		case EIslandArchetype::Teeth: return {C(206, 196, 178), C(118, 140, 88), C(150, 148, 142)};
		case EIslandArchetype::Mangrove: return {C(150, 134, 102), C(92, 116, 66), C(98, 96, 84)};
		case EIslandArchetype::WhiteSands: return {C(246, 238, 220), C(116, 138, 78), C(196, 190, 176)};
		// Caliza kárstica: roca gris pálida (no el pardo rocoso genérico) y selva más
		// saturada en las laderas, como en los farallones de piedra caliza tropicales.
		case EIslandArchetype::Mesa: return {C(214, 190, 144), C(104, 132, 62), C(184, 180, 168)};
		default: return {C(230, 210, 170), C(90, 150, 60), C(120, 115, 105)};
		}
	}
}

FTerrainDensity::FTerrainDensity(const FArchipelagoLayout& InLayout)
	: Layout(InLayout)
	, Seafloor(InLayout)
	, DetailNoise(InLayout.Seed ^ 0x5F356495u)
	, OverhangNoise(InLayout.Seed ^ 0x2C1B3C6Du)
{
	// Primero el relieve: las cuevas buscan su entrada en la ladera ya erosionada.
	BuildReliefs();
	BuildCaves();
}

void FTerrainDensity::BuildReliefs()
{
	Reliefs.SetNum(Layout.Islands.Num());
	for (int32 I = 0; I < Layout.Islands.Num(); ++I)
	{
		if (Layout.Islands[I].Archetype == EIslandArchetype::Mesa)
		{
			Reliefs[I].Karst = MakeShared<FKarstLayout>(FKarstTowerModel::Generate(Layout.Islands[I].Seed));
		}
		BuildCoast(I);
	}
	// Cada isla se erosiona por separado (determinista por su semilla), así que pueden ir en
	// paralelo; la caché de FIslandReliefModel las comparte entre instancias.
	ParallelFor(Layout.Islands.Num(), [this](int32 I)
	{
		const FIslandDesc& Island = Layout.Islands[I];
		FIslandReliefSettings Settings;
		if (!FIslandReliefModel::SettingsFor(Island.Archetype, Island.Radius, Island.Seed, Settings))
		{
			return;
		}
		const FExploredNoise N(Island.Seed);
		const FKarstLayout* Karst = Reliefs[I].Karst ? &*Reliefs[I].Karst : nullptr;
		Reliefs[I].Grid = FIslandReliefModel::GetOrBuild(Island, Settings,
			[&Island, &N, Karst](float Qx, float Qy)
			{
				const FVector2D Q(Qx, Qy);
				const float Land = FIslandShapeModel::BaseLand(Island, N, Q, FIslandShapeModel::CoastT(Island, N, Q), Karst);
				return Land > -500.0f ? Land : 0.0f;
			},
			[Karst](float Qx, float Qy)
			{
				// Bajo las torres la caliza apenas se erosiona: sus derrubios se quedan al pie.
				float Core = 0.0f;
				if (Karst)
				{
					FKarstTowerModel::TowerHeight(*Karst, Qx, Qy, &Core);
				}
				return 1.0f - 0.85f * Core;
			});
	});
}

void FTerrainDensity::BuildCoast(int32 IslandIndex)
{
	const FIslandDesc& Island = Layout.Islands[IslandIndex];
	const FExploredNoise N(Island.Seed);
	// Los cayos se asientan en espolones de la plataforma: antes salían sueltos del talud.
	TArray<FShelfCay> Cays;
	for (const FCayDesc& Cay : Island.Cays)
	{
		const FVector2D Q = FIslandShapeModel::WarpedLocal(Island, N, static_cast<float>(Cay.Center.X), static_cast<float>(Cay.Center.Y));
		FShelfCay Shelf;
		Shelf.Angle = FMath::Atan2(static_cast<float>(Q.Y), static_cast<float>(Q.X));
		Shelf.T = FIslandShapeModel::CoastT(Island, N, Q, false);
		const float Arc = FMath::Max(static_cast<float>(Q.Size()), 0.3f);
		Shelf.HalfWidth = FMath::Clamp(Cay.Radius * 5.0f / Island.Radius / Arc + 0.25f, 0.35f, 0.7f);
		Cays.Add(Shelf);
	}
	Reliefs[IslandIndex].Shelf = MakeShared<FIslandShelf>(FIslandShelfModel::Build(Island.Seed, Cays));
	FCliffStyle Style;
	if (FCoastalCliffModel::StyleFor(Island.Archetype, Style))
	{
		Reliefs[IslandIndex].Cliffs = MakeShared<FCoastalCliffs>(FCoastalCliffModel::Build(Island.Seed, Style));
	}
}

const FTerrainDensity::FIslandRelief* FTerrainDensity::FindRelief(const FIslandDesc& Island) const
{
	for (int32 I = 0; I < Layout.Islands.Num() && I < Reliefs.Num(); ++I)
	{
		if (&Layout.Islands[I] == &Island)
		{
			return &Reliefs[I];
		}
	}
	return nullptr;
}

float FTerrainDensity::UnderwaterHeight(const FIslandDesc& Island, const FIslandRelief* Relief, const FExploredNoise& N,
	const FVector2D& Q, float T, float Angle, float X, float Y) const
{
	// Los Dientes se asientan en su bajo rocoso: la plataforma empieza a su cota, no a -1,2 m
	// (subía en anillo alrededor del bajo).
	const float Foot = Island.Archetype == EIslandArchetype::Teeth ? TeethShoalDepth : ShelfDepth;
	if (T <= 1.0f)
	{
		return Foot;
	}
	// El talud acaba en el fondo real (dorsal, llanura abisal), no en una cota fija. Mar
	// adentro se mide sin el rizado corto de la costa, que dibujaba el talud en sierra.
	const float Floor = Seafloor.FloorHeight(X, Y);
	const FVector2D Coarse = FIslandShapeModel::WarpedLocal(Island, N, X, Y, false);
	const float Smooth = FMath::Lerp(T, FIslandShapeModel::CoastT(Island, N, Coarse, false), SmoothStep(1.0f, 1.25f, T));
	if (!Relief || !Relief->Shelf)
	{
		return FMath::Lerp(Foot, Floor, SmoothStep(1.0f, InfluenceLimit, Smooth));
	}
	const float Detail = N.Fbm2D(Q.X * 3.0f - 40.0f, Q.Y * 3.0f, 2);
	return FIslandShelfModel::Profile(*Relief->Shelf, Angle, Smooth, Floor, Foot, Detail);
}

float FTerrainDensity::IslandHeight(const FIslandDesc& Island, float X, float Y, float& OutT) const
{
	const FExploredNoise N(Island.Seed);
	const FVector2D Q = FIslandShapeModel::WarpedLocal(Island, N, X, Y);
	const float T = FIslandShapeModel::CoastT(Island, N, Q);
	OutT = T;
	if (T >= InfluenceLimit)
	{
		return Seafloor.FloorHeight(X, Y);
	}
	const FIslandRelief* Relief = FindRelief(Island);
	const float Angle = FMath::Atan2(static_cast<float>(Q.Y), static_cast<float>(Q.X));
	const float Underwater = UnderwaterHeight(Island, Relief, N, Q, T, Angle, X, Y);
	float Land = FIslandShapeModel::BaseLand(Island, N, Q, T, Relief && Relief->Karst ? &*Relief->Karst : nullptr);
	if (Island.Archetype == EIslandArchetype::Teeth)
	{
		// El bajo rocoso se extiende más donde la plataforma es ancha: su contorno no es un disco.
		const float Reach = Relief && Relief->Shelf ? 0.04f + 1.1f * Relief->Shelf->Width(Angle) : 0.25f;
		return T < 1.0f ? Land : FMath::Lerp(Land, Underwater, SmoothStep(1.0f, 1.0f + Reach, T));
	}
	if (Land < -500.0f)
	{
		return Underwater;
	}
	Land = FinishLand(Island, Relief, Q, T, Land);
	if (T >= 1.0f)
	{
		// Transición suave entre la orilla y la plataforma.
		return FMath::Lerp(FMath::Min(Land, 0.0f) + ShelfDepth, Underwater, SmoothStep(1.0f, 1.12f, T));
	}
	const float U = 1.0f - T;
	const float Shore = FMath::Lerp(Underwater, Land, SmoothStep(-0.02f, 0.03f, U));
	// Acantilados marinos después de erosionar (la erosión térmica los tumbaría). Se suman a la
	// orilla ya fundida y valen 0 en la línea de costa: la pared no abre un escalón en T = 1.
	const FCoastalCliffs* Cliffs = Relief && Relief->Cliffs ? &*Relief->Cliffs : nullptr;
	return Cliffs ? Shore + FCoastalCliffModel::Uplift(*Cliffs, Angle, U, Island.Radius) : Shore;
}

float FTerrainDensity::FinishLand(const FIslandDesc& Island, const FIslandRelief* Relief, const FVector2D& Q, float T, float Land) const
{
	if (Relief && Relief->Grid)
	{
		// Erosión y ríos: diferencia guardada en la rejilla, que se apaga en su borde y en la
		// línea de costa. Ahí las gotas dejan su sedimento al llegar al mar; sin apagarla, ese
		// depósito subía la orilla, la costa se movía y crecía el salto de la base en T = 1.
		Land += Relief->Grid->SampleDelta(Q.X, Q.Y) * SmoothStep(0.0f, 0.08f, 1.0f - T);
	}
	if (Relief && Relief->Karst)
	{
		// Lagunas interiores y torres calizas a plomo sobre el llano ya erosionado (y sus
		// derrubios): después de erosionar, para que ni se ciegan ni se vuelven conos.
		Land = FKarstTowerModel::ApplyLagoons(*Relief->Karst, Q.X, Q.Y, Land);
		Land = FKarstTowerModel::ApplyTowers(*Relief->Karst, Q.X, Q.Y, Land, Island.MaxHeight);
	}
	return Land;
}

FTerrainColumn FTerrainDensity::SampleColumn(float X, float Y) const
{
	FTerrainColumn Column;
	// Fondo continuo: dorsal que une la cadena, llanura abisal, montículos e islotes.
	const float Floor = Seafloor.FloorHeight(X, Y);
	Column.Height = Floor;

	for (int32 I = 0; I < Layout.Islands.Num(); ++I)
	{
		const FIslandDesc& Island = Layout.Islands[I];
		const float Reach = Island.Radius * (InfluenceLimit + 0.4f);
		const float DistSq = FVector2D::DistSquared(Island.Center, FVector2D(X, Y));
		if (DistSq > Reach * Reach)
		{
			continue;
		}

		float T = 0.0f;
		// Al acercarse al alcance, la isla se funde con el fondo: sin esto el talud se cortaba
		// allí y quedaba un escalón en arco en el mar.
		const float H = FSeafloorModel::BlendIslandToFloor(IslandHeight(Island, X, Y, T), Floor, FMath::Sqrt(DistSq) / Reach);
		if (H > Column.Height)
		{
			Column.Height = H;
		}
		if (T < Column.NormalizedDistance)
		{
			Column.NormalizedDistance = T;
			Column.IslandIndex = I;
		}
	}

	// Cayos satélite: solo cuentan si asoman sobre lo que ya había.
	for (int32 I = 0; I < Layout.Islands.Num(); ++I)
	{
		for (const FCayDesc& Cay : Layout.Islands[I].Cays)
		{
			if (FVector2D::DistSquared(Cay.Center, FVector2D(X, Y)) > FMath::Square(Cay.Radius * 4.5f))
			{
				continue;
			}
			// La falda baja hasta lo que ya hay debajo y se funde con ello (antes, corte a -22 m).
			const float H = FSeafloorModel::CayHeight(Cay, X, Y, DetailNoise.Fbm2D(X / 60.0f, Y / 60.0f, 3), Column.Height);
			if (H > Column.Height)
			{
				Column.Height = H;
				if (H > -2.0f)
				{
					Column.IslandIndex = I;
				}
			}
		}
	}

	// Microrrelieve en tierra firme.
	if (Column.Height > 0.5f)
	{
		Column.Height += 0.6f * DetailNoise.Fbm2D(X / 14.0f, Y / 14.0f, 3);
	}
	return Column;
}

float FTerrainDensity::DensityWithColumn(const FVector& P, const FTerrainColumn& Column) const
{
	// La capa de ediciones va primero: fuera de los chunks editados no suma nada.
	float Delta = 0.0f;
	if (Edits)
	{
		Edits->DeltaAt(P, Delta);
	}
	return ProceduralDensityWithColumn(P, Column) + Delta;
}

float FTerrainDensity::ProceduralDensity(const FVector& P) const
{
	return ProceduralDensityWithColumn(P, SampleColumn(P.X, P.Y));
}

float FTerrainDensity::ProceduralDensityWithColumn(const FVector& P, const FTerrainColumn& Column) const
{
	float D = P.Z - Column.Height;

	// Voladizos y roca irregular por encima de las playas.
	const float RockMask = SmoothStep(3.0f, 18.0f, Column.Height);
	if (RockMask > 0.0f && FMath::Abs(D) < OverhangAmplitude * 3.0f)
	{
		D += RockMask * OverhangAmplitude * OverhangNoise.Fbm3D(P.X / 16.0f, P.Y / 16.0f, P.Z / 10.0f, 3);
	}

	// Muesca de marea en los farallones de caliza (macizo kárstico): un socavón festoneado
	// justo sobre el nivel del mar, solo donde ya hay pared vertical alta y cerca de la
	// superficie, así que no añade coste en el resto del mundo ni bajo tierra.
	if (RockMask > 0.5f && Column.Height < 40.0f && FMath::Abs(D) < OverhangAmplitude * 4.0f
		&& Column.IslandIndex != INDEX_NONE && Layout.Islands[Column.IslandIndex].Archetype == EIslandArchetype::Mesa)
	{
		const float Waterline = FMath::Abs(P.Z - 0.6f);
		const float NotchWidth = 1.3f + 0.5f * OverhangNoise.Fbm2D(P.X / 6.0f, P.Y / 6.0f, 2);
		D += (1.0f - SmoothStep(0.0f, NotchWidth, Waterline)) * 1.8f;
	}

	if (!Caves.IsEmpty())
	{
		D = FMath::Max(D, -CaveCarve(P));
	}
	return D;
}

float FTerrainDensity::Density(const FVector& P) const
{
	return DensityWithColumn(P, SampleColumn(P.X, P.Y));
}

FVector FTerrainDensity::Normal(const FVector& P, float Step) const
{
	const float Dx = Density(P + FVector(Step, 0, 0)) - Density(P - FVector(Step, 0, 0));
	const float Dy = Density(P + FVector(0, Step, 0)) - Density(P - FVector(0, Step, 0));
	const float Dz = Density(P + FVector(0, 0, Step)) - Density(P - FVector(0, 0, Step));
	return FVector(Dx, Dy, Dz).GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
}

float FTerrainDensity::CaveCarve(const FVector& P) const
{
	float Best = TNumericLimits<float>::Max();
	for (const FCaveDesc& Cave : Caves)
	{
		// Descarte rápido por caja.
		const FBox Bounds = FBox(
			FVector::Min(Cave.Start, Cave.End) - FVector(Cave.Radius * 2.0f),
			FVector::Max(Cave.Start, Cave.End) + FVector(Cave.Radius * 2.0f));
		if (!Bounds.IsInside(P))
		{
			continue;
		}

		float T = 0.0f;
		const float Dist = DistanceToSegment(P, Cave.Start, Cave.End, T);
		const FExploredNoise N(Cave.Seed);
		// La boca es algo más ancha y el fondo se estrecha.
		const float Taper = FMath::Lerp(1.25f, 0.7f, T);
		const float Wobble = 0.3f * N.Fbm3D(P.X / 6.0f, P.Y / 6.0f, P.Z / 6.0f, 2);
		Best = FMath::Min(Best, Dist - Cave.Radius * (Taper + Wobble));
	}
	return Best;
}

void FTerrainDensity::BuildCaves()
{
	FExploredRandom Rng(static_cast<uint64>(Layout.Seed) ^ 0xCAFEF00DULL);

	for (const FIslandDesc& Island : Layout.Islands)
	{
		int32 Count = 0;
		float EntranceFraction = 0.45f;
		switch (Island.Archetype)
		{
		case EIslandArchetype::Emerald: Count = 3; EntranceFraction = 0.35f; break;
		case EIslandArchetype::Smoke: Count = 2; EntranceFraction = 0.25f; break;
		case EIslandArchetype::Mesa: Count = 3; EntranceFraction = 0.3f; break;
		case EIslandArchetype::Landing: Count = 1; EntranceFraction = 0.4f; break;
		default: break;
		}

		for (int32 I = 0; I < Count; ++I)
		{
			// Busca, en una dirección aleatoria desde el centro, la ladera a la altura deseada.
			const float Angle = Rng.RangeFloat(0.0f, UE_TWO_PI);
			const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
			const float TargetHeight = Island.MaxHeight * EntranceFraction;
			FVector2D Entrance = Island.Center;
			for (float R = 0.0f; R < Island.Radius; R += 4.0f)
			{
				const FVector2D P = Island.Center + Dir * R;
				float T = 0.0f;
				if (IslandHeight(Island, P.X, P.Y, T) < TargetHeight)
				{
					Entrance = P;
					break;
				}
			}

			float T = 0.0f;
			const float EntranceHeight = IslandHeight(Island, Entrance.X, Entrance.Y, T);
			FCaveDesc Cave;
			Cave.Radius = Rng.RangeFloat(3.5f, 6.0f);
			Cave.Start = FVector(Entrance.X, Entrance.Y, EntranceHeight + Cave.Radius * 0.3f) - FVector(Dir, 0.0) * 2.0f;
			const float Length = Rng.RangeFloat(35.0f, 70.0f);
			Cave.End = Cave.Start - FVector(Dir, 0.0) * Length - FVector(0, 0, Rng.RangeFloat(2.0f, 8.0f));
			Cave.Seed = Rng.NextUInt32();
			Caves.Add(Cave);
		}

		// Arcos marinos en Los Dientes: túneles horizontales a nivel del mar.
		if (Island.Archetype == EIslandArchetype::Teeth)
		{
			for (int32 I = 0; I < FMath::Min(3, Island.Islets.Num()); ++I)
			{
				const FVector2D C = Island.Center + Island.Islets[I];
				const float Angle = Rng.RangeFloat(0.0f, UE_TWO_PI);
				const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
				FCaveDesc Arch;
				Arch.Radius = Rng.RangeFloat(5.0f, 8.0f);
				Arch.Start = FVector(C - Dir * 40.0f, 1.5);
				Arch.End = FVector(C + Dir * 40.0f, 1.5);
				Arch.Seed = Rng.NextUInt32();
				Caves.Add(Arch);
			}
		}
	}
}

namespace
{
	struct FSurfaceWeights
	{
		float Sand = 0.0f;
		float Rock = 0.0f;
		float Grass = 1.0f;
	};

	/** Arena cerca del agua, roca en pendiente o bajo tierra, hierba en el resto. */
	FSurfaceWeights ComputeSurfaceWeights(EIslandArchetype Archetype, float Z, float ColumnHeight, float NormalZ, float Variation)
	{
		FSurfaceWeights W;
		const float SandLine = 2.2f + 1.2f * Variation;
		W.Sand = 1.0f - SmoothStep(SandLine - 0.8f, SandLine + 0.8f, Z);
		// En el trópico la selva se agarra a laderas muy empinadas: roca desnuda solo por encima de
		// unos 52° y del todo a partir de 65°.
		W.Rock = 1.0f - SmoothStep(0.42f, 0.62f, NormalZ);
		// Interior de cuevas y voladizos: roca.
		if (Z < ColumnHeight - 2.0f)
		{
			W.Rock = 1.0f;
		}
		if (Archetype == EIslandArchetype::Smoke)
		{
			// Ladera alta volcánica: roca y ceniza.
			W.Rock = FMath::Max(W.Rock, SmoothStep(120.0f, 220.0f, Z));
		}
		W.Sand *= 1.0f - W.Rock;
		W.Grass = FMath::Max(0.0f, 1.0f - W.Sand - W.Rock);
		return W;
	}

	/** Cuánto del suelo no rocoso es hojarasca de selva en lugar de hierba abierta. */
	float ForestFloorAmount(EIslandArchetype Archetype)
	{
		switch (Archetype)
		{
		case EIslandArchetype::Emerald: return 0.85f;
		case EIslandArchetype::Mangrove: return 0.8f;
		case EIslandArchetype::Landing: return 0.55f;
		case EIslandArchetype::Smoke: return 0.35f;
		case EIslandArchetype::Mesa: return 0.45f;
		default: return 0.15f;
		}
	}

	/** Roca y arena volcánicas (basalto, ceniza) frente a coralinas (caliza, arena blanca). */
	float VolcanicAmount(EIslandArchetype Archetype)
	{
		switch (Archetype)
		{
		case EIslandArchetype::Smoke: return 1.0f;
		case EIslandArchetype::Emerald: return 0.8f;
		case EIslandArchetype::Landing: return 0.7f;
		case EIslandArchetype::Mangrove: return 0.5f;
		case EIslandArchetype::Mesa: return 0.3f;
		default: return 0.0f;
		}
	}
}

FVector4f FTerrainDensity::SurfaceLayers(const FVector& P, const FVector& InNormal) const
{
	const FTerrainColumn Column = SampleColumn(P.X, P.Y);
	const EIslandArchetype Archetype = Column.IslandIndex != INDEX_NONE
		? Layout.Islands[Column.IslandIndex].Archetype
		: EIslandArchetype::Landing;
	const float Variation = DetailNoise.Fbm2D(P.X / 30.0f + 100.0f, P.Y / 30.0f, 3);
	const FSurfaceWeights W = ComputeSurfaceWeights(Archetype, P.Z, Column.Height, InNormal.Z, Variation);

	// Manchas de hojarasca y claros de hierba: ruido de baja frecuencia sobre la proporción de la isla,
	// con la hojarasca desapareciendo en las cumbres altas y expuestas.
	const float Patches = DetailNoise.Fbm2D(P.X / 55.0f - 40.0f, P.Y / 55.0f + 17.0f, 3);
	const float Forest = FMath::Clamp(ForestFloorAmount(Archetype) + 0.45f * Patches, 0.0f, 1.0f)
		* (1.0f - SmoothStep(140.0f, 260.0f, P.Z));
	return FVector4f(W.Sand, W.Grass * Forest, W.Rock, VolcanicAmount(Archetype));
}

FLinearColor FTerrainDensity::SurfaceColor(const FVector& P, const FVector& InNormal) const
{
	const FTerrainColumn Column = SampleColumn(P.X, P.Y);
	const EIslandArchetype Archetype = Column.IslandIndex != INDEX_NONE
		? Layout.Islands[Column.IslandIndex].Archetype
		: EIslandArchetype::Landing;
	const FPalette Palette = PaletteFor(Archetype);

	const float Variation = DetailNoise.Fbm2D(P.X / 30.0f + 100.0f, P.Y / 30.0f, 3);
	const float Z = P.Z;

	const FSurfaceWeights W = ComputeSurfaceWeights(Archetype, Z, Column.Height, InNormal.Z, Variation);
	const float Sand = W.Sand;
	const float Rock = W.Rock;
	const float Grass = W.Grass;

	FLinearColor Color = Palette.Sand * Sand + Palette.Grass * Grass + Palette.Rock * Rock;

	// Arena mojada junto a la orilla y fondo marino más claro en someros.
	if (Z < 0.6f)
	{
		const float Wet = SmoothStep(0.6f, -0.2f, Z);
		Color = FMath::Lerp(Color, Color * 0.72f, Wet);
	}
	if (Z < -0.5f)
	{
		const float Depth = SmoothStep(-0.5f, -25.0f, Z);
		const FLinearColor Seabed = FMath::Lerp(Palette.Sand * 1.05f, Palette.Sand * 0.45f, Depth);
		Color = FMath::Lerp(Color, Seabed, SmoothStep(-0.5f, -2.0f, Z) * (1.0f - Rock * 0.5f));
	}

	// Variación suave de tono para romper la uniformidad.
	Color *= 0.92f + 0.16f * (Variation * 0.5f + 0.5f);
	Color.A = Rock;
	return Color;
}

void FTerrainDensity::HeightBounds(const FBox2D& Rect, float SampleSpacing, float& OutMin, float& OutMax) const
{
	// Paso no válido, rectángulo no finito, vacío o con un muestreo absurdo: rango nulo. Sin
	// muestras OutMin quedaba en FLT_MAX y el FloorToInt32 de FindCandidateChunks era UB.
	OutMin = 0.0f;
	OutMax = 0.0f;
	constexpr double MaxSamples = 1 << 22;
	const double Width = Rect.Max.X - Rect.Min.X + KINDA_SMALL_NUMBER;
	const double Depth = Rect.Max.Y - Rect.Min.Y + KINDA_SMALL_NUMBER;
	if (!FMath::IsFinite(SampleSpacing) || SampleSpacing <= 0.0f
		|| !FMath::IsFinite(Rect.Min.X) || !FMath::IsFinite(Rect.Min.Y) || !FMath::IsFinite(Width) || !FMath::IsFinite(Depth)
		|| Width < 0.0 || Depth < 0.0 || (Width / SampleSpacing + 1.0) * (Depth / SampleSpacing + 1.0) > MaxSamples)
	{
		return;
	}
	// Contador entero y posición en double: con |X| ~ 1e8 un X += paso en float no avanzaba.
	const int32 NumX = static_cast<int32>(FMath::FloorToDouble(Width / SampleSpacing)) + 1;
	const int32 NumY = static_cast<int32>(FMath::FloorToDouble(Depth / SampleSpacing)) + 1;
	OutMin = TNumericLimits<float>::Max();
	OutMax = TNumericLimits<float>::Lowest();
	for (int32 IX = 0; IX < NumX; ++IX)
	{
		const float X = static_cast<float>(Rect.Min.X + static_cast<double>(IX) * SampleSpacing);
		for (int32 IY = 0; IY < NumY; ++IY)
		{
			const float Y = static_cast<float>(Rect.Min.Y + static_cast<double>(IY) * SampleSpacing);
			const float H = SampleColumn(X, Y).Height;
			OutMin = FMath::Min(OutMin, H);
			OutMax = FMath::Max(OutMax, H);
		}
	}

	// Margen por el muestreo discreto, el ruido 3D y las cuevas.
	OutMin -= 12.0f;
	OutMax += 6.0f;
	for (const FCaveDesc& Cave : Caves)
	{
		const FBox2D CaveRect(
			FVector2D(FMath::Min(Cave.Start.X, Cave.End.X), FMath::Min(Cave.Start.Y, Cave.End.Y)) - FVector2D(Cave.Radius * 2.0f),
			FVector2D(FMath::Max(Cave.Start.X, Cave.End.X), FMath::Max(Cave.Start.Y, Cave.End.Y)) + FVector2D(Cave.Radius * 2.0f));
		if (CaveRect.Intersect(Rect))
		{
			OutMin = FMath::Min(OutMin, static_cast<float>(FMath::Min(Cave.Start.Z, Cave.End.Z)) - Cave.Radius * 2.0f);
		}
	}
}
