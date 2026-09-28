#include "WorldGen/TerrainDensity.h"

#include "Core/ExploredRandom.h"
#include "Misc/ScopeLock.h"

namespace
{
	/** Distancia normalizada máxima a la que una isla influye (fin de la plataforma). */
	constexpr float InfluenceLimit = 1.9f;
	/** Profundidad de la plataforma somera junto a la costa. */
	constexpr float ShelfDepth = -1.2f;

	FORCEINLINE float SmoothStep(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	/** Mínimo suave polinómico: une dos siluetas sin arista en la junta. */
	FORCEINLINE float SmoothMin(float A, float B, float K)
	{
		const float H = FMath::Max(K - FMath::Abs(A - B), 0.0f) / K;
		return FMath::Min(A, B) - H * H * K * 0.25f;
	}

	/** Profundidad del lomo de la dorsal submarina que une la cadena. */
	constexpr float RidgeDepth = -26.0f;

	float DistanceToPolyline(const TArray<FVector2D>& Points, const FVector2D& P)
	{
		float Best = TNumericLimits<float>::Max();
		for (int32 I = 0; I + 1 < Points.Num(); ++I)
		{
			const FVector2D A = Points[I];
			const FVector2D AB = Points[I + 1] - A;
			const float T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / FMath::Max(AB.SizeSquared(), 1.0f), 0.0f, 1.0f);
			Best = FMath::Min(Best, FVector2D::Distance(P, A + AB * T));
		}
		return Best;
	}

	/** Altura de un cayo satélite; muy negativa fuera de su alcance. */
	float CayHeight(const FCayDesc& Cay, float X, float Y, float Wobble)
	{
		const FVector2D D = FVector2D(X, Y) - Cay.Center;
		const float C = FMath::Cos(-Cay.Angle);
		const float S = FMath::Sin(-Cay.Angle);
		const FVector2D L(D.X * C - D.Y * S, (D.X * S + D.Y * C) / Cay.Aspect);
		const float R = L.Size() * (1.0f + 0.3f * Wobble) / Cay.Radius;
		if (R > 3.0f)
		{
			return -1000.0f;
		}
		// Falda sumergida: arena somera que se hunde hacia el talud.
		const float Skirt = FMath::Lerp(-0.8f, -22.0f, SmoothStep(1.0f, 3.0f, R));
		if (R >= 1.0f)
		{
			return Skirt;
		}
		const float Land = Cay.bRocky
			? -2.0f + (Cay.Height + 2.0f) * FMath::Pow(FMath::Max(0.0f, 1.0f - R * R), 0.35f)
			: -0.6f + (Cay.Height + 0.6f) * (1.0f - R * R);
		return FMath::Max(Land, Skirt);
	}

	FVector2D ToLocal(const FIslandDesc& Island, float X, float Y)
	{
		const FVector2D D = (FVector2D(X, Y) - Island.Center) / Island.Radius;
		const float C = FMath::Cos(-Island.Rotation);
		const float S = FMath::Sin(-Island.Rotation);
		return FVector2D(D.X * C - D.Y * S, D.X * S + D.Y * C);
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

namespace
{
	// --- Relieve erosionado por isla: forma base + erosión hidráulica/térmica -----------
	//
	// Varias islas (el macizo kárstico de La Meseta y los canales del Manglar) parten de una
	// forma base determinista por semilla y se tallan una sola vez, al construir la isla, con
	// el mismo FTerrainErosionModel que ya se usaba solo para La Meseta. La hidráulica traza
	// una red de drenaje con gradiente real (gotas que fluyen cuesta abajo, se concentran en
	// cauces y depositan sedimento al perder pendiente cerca de la costa) y la térmica limita
	// las pendientes según el "talud de reposo" del material: alto para caliza (paredes casi
	// verticales), bajo para barro de manglar (orillas tendidas, valles en V/U que se ensanchan).

	/** Resolución de la rejilla de erosión (celdas por lado); igual para todas las islas
	 * erosionadas para poder compartir la caché y el bilineal de muestreo. */
	constexpr int32 ErosionGridResolution = 420;
	/** Semiancho, en radios de isla, de la zona Q que cubre la rejilla. */
	constexpr float ErosionQExtent = 1.3f;

	/** Perfil base (antes de erosionar) del macizo: cresta irregular con 2-4 cumbres y
	 * espolones deterministas por semilla, sin mesetas ni escalones repetidos. Devuelve
	 * un factor que se multiplica por la altura máxima de la isla. */
	float KarstMassifShape(uint32 IslandSeed, const FExploredNoise& N, float Qx, float Qy)
	{
		const float R = FMath::Sqrt(Qx * Qx + Qy * Qy);
		const FVector2D Warped = N.Warp2D(Qx * 1.1f + 30.0f, Qy * 1.1f, 0.55f, 3) / 1.1f;
		const float U = FMath::Max(0.0f, 1.0f - R);
		const float Rise = FMath::Pow(SmoothStep(0.02f, 0.95f, U), 1.2f);
		const float Ridged = N.Ridged2D(Warped.X * 2.0f + 10.0f, Warped.Y * 2.0f, 5);

		// 2-4 cumbres a lo largo de una cresta que cruza la isla: posición, radio y fuerza
		// deterministas por semilla (cada macizo kárstico es distinto, no coordenadas fijas).
		float PeakBoost = 0.0f;
		const int32 PeakCount = 2 + static_cast<int32>(ExploredHash::Hash32(IslandSeed ^ 0x9C3u) % 3u);
		for (int32 P = 0; P < PeakCount; ++P)
		{
			const float Along = FMath::Lerp(-0.5f, 0.5f, ExploredHash::ToUnitFloat(ExploredHash::Hash2D(IslandSeed, P, 0x50)));
			const float Across = FMath::Lerp(-0.22f, 0.22f, ExploredHash::ToUnitFloat(ExploredHash::Hash2D(IslandSeed, P, 0x51)));
			const float PeakRadius = FMath::Lerp(0.24f, 0.42f, ExploredHash::ToUnitFloat(ExploredHash::Hash2D(IslandSeed, P, 0x52)));
			const float PeakStrength = FMath::Lerp(0.5f, 1.0f, ExploredHash::ToUnitFloat(ExploredHash::Hash2D(IslandSeed, P, 0x53)));
			const float D = FVector2D::Distance(FVector2D(Qx, Qy), FVector2D(Along, Across)) / PeakRadius;
			PeakBoost = FMath::Max(PeakBoost, PeakStrength * FMath::Square(FMath::Max(0.0f, 1.0f - D)));
		}
		return Rise * (0.4f + 0.35f * Ridged + 0.55f * PeakBoost);
	}

	/** Perfil base (antes de erosionar) de los canales del Manglar: un llano bajo con relieve
	 * ondulado a tres escalas (nunca una meseta perfectamente plana), para que la erosión
	 * hidráulica tenga pendiente real de la que partir y trace una red de drenaje con
	 * gradiente hacia la costa en vez del corte a profundidad constante de antes. */
	float MangroveBaseShape(const FExploredNoise& N, float Qx, float Qy)
	{
		const float R = FMath::Sqrt(Qx * Qx + Qy * Qy);
		const float U = FMath::Max(0.0f, 1.0f - R);
		// Relieve a tres escalas, con longitud de onda de unas pocas decenas de metros.
		// FExploredNoise repite cada ~1 unidad de su argumento, así que el multiplicador que da
		// una longitud de onda concreta es Radio/λ, no 2π/λ como en una onda seno: con los
		// multiplicadores "por radio de isla" del macizo kárstico (pensados para 1-2 crestas
		// cruzando la isla entera) todo el relieve medía unos pocos metros de longitud de onda
		// y se promediaba a "plano" en cualquier ventana de paisaje de decenas de metros (visto
		// con Tools/HostTests/tools/TerrainDiagnostics.cpp: la varianza de paisaje no subía
		// aunque se subiera la amplitud). Lomas de ~60 m, montículos de ~25 m y un rizado de
		// ~10 m; la amplitud (22, muy por encima de lo que parece "razonable" a ojo) compensa
		// que la erosión térmica se come buena parte del relieve de entrada.
		const float Relief = 0.5f * N.Fbm2D(Qx * 9.0f, Qy * 9.0f, 3)
			+ 0.35f * N.Fbm2D(Qx * 21.0f + 30.0f, Qy * 21.0f, 4)
			+ 0.15f * N.Fbm2D(Qx * 53.0f - 70.0f, Qy * 53.0f, 3);
		return 0.4f + 4.2f * SmoothStep(0.0f, 0.75f, U) + 22.0f * Relief * SmoothStep(0.0f, 0.2f, U);
	}

	/** Parámetros de erosión por arquetipo: el talud de reposo y la capacidad de sedimento
	 * cambian con el material (caliza casi vertical, barro de manglar muy tendido). */
	FErosionParams ErosionParamsFor(const FIslandDesc& Island)
	{
		FErosionParams Params;
		Params.Seed = Island.Seed;
		Params.CellSizeMeters = (2.0f * ErosionQExtent * Island.Radius) / ErosionGridResolution;
		switch (Island.Archetype)
		{
		case EIslandArchetype::Mesa:
			Params.DropletCount = 20000;
			Params.MaxDropletLifetime = 32;
			Params.ErosionRadius = 3;
			Params.ThermalIterations = 60;
			Params.TalusAngleTangent = 0.85f; // Caliza: laderas empinadas, no un talud arenoso.
			Params.ThermalTransferRate = 0.5f;
			break;
		case EIslandArchetype::Mangrove:
			// El relieve de partida es de pocos metros (no las decenas del macizo kárstico):
			// con los mismos parámetros que Mesa la hidráulica y la térmica aplanan casi todo
			// el terreno de fondo (les sobra material que mover) y solo dejan los cauces. Menos
			// gotas y vida más corta bastan para tallar un cauce de un par de metros sin
			// planchar las lomas y montículos de alrededor.
			Params.DropletCount = 5000;
			Params.MaxDropletLifetime = 18;
			Params.ErosionRadius = 2;
			Params.SedimentCapacityFactor = 2.2f;
			Params.ErodeSpeed = 0.25f;
			Params.DepositSpeed = 0.4f;
			Params.ThermalIterations = 4;
			Params.TalusAngleTangent = 0.7f; // Barro/tierra blanda: orillas tendidas, no un talud rocoso.
			Params.ThermalTransferRate = 0.35f;
			break;
		default:
			break;
		}
		return Params;
	}

	/** Erosiona (una vez por semilla de isla) y devuelve la rejilla de alturas de la isla, en
	 * coordenadas Q locales normalizadas por el radio. Comparte el resultado entre todas las
	 * instancias de FTerrainDensity construidas con la misma semilla de isla: cada
	 * construcción independiente de la misma isla (subsistemas de cartografía, ruinas, cámara
	 * de capturas...) paga el coste una sola vez por proceso. Medido: unos 0,35 s en una
	 * rejilla de 420x420 en Development x64 (ver Tools/HostTests).
	 */
	TSharedPtr<const FErosionHeightGrid> GetOrBuildErodedGrid(const FIslandDesc& Island,
		TFunctionRef<float(const FExploredNoise&, float, float)> BaseShape)
	{
		static FCriticalSection Mutex;
		static TMap<uint32, TSharedPtr<const FErosionHeightGrid>> Cache;

		FScopeLock Lock(&Mutex);
		if (const TSharedPtr<const FErosionHeightGrid>* Found = Cache.Find(Island.Seed))
		{
			return *Found;
		}

		TSharedPtr<FErosionHeightGrid> Grid = MakeShared<FErosionHeightGrid>();
		Grid->Init(ErosionGridResolution, ErosionGridResolution, 0.0f);
		const FExploredNoise N(Island.Seed);
		const float Step = (2.0f * ErosionQExtent) / (ErosionGridResolution - 1);
		for (int32 Gy = 0; Gy < ErosionGridResolution; ++Gy)
		{
			for (int32 Gx = 0; Gx < ErosionGridResolution; ++Gx)
			{
				const float Qx = -ErosionQExtent + Gx * Step;
				const float Qy = -ErosionQExtent + Gy * Step;
				Grid->At(Gx, Gy) = BaseShape(N, Qx, Qy);
			}
		}

		FTerrainErosionModel::Erode(*Grid, ErosionParamsFor(Island));

		TSharedPtr<const FErosionHeightGrid> Result = Grid;
		Cache.Add(Island.Seed, Result);
		return Result;
	}

	/** Altura de la rejilla erosionada en Q local (bilineal); 0 fuera de la rejilla. */
	float SampleErodedGrid(const FErosionHeightGrid& Grid, float Qx, float Qy)
	{
		if (FMath::Abs(Qx) >= ErosionQExtent || FMath::Abs(Qy) >= ErosionQExtent)
		{
			return 0.0f;
		}
		const float Step = (2.0f * ErosionQExtent) / (ErosionGridResolution - 1);
		return Grid.Sample((Qx + ErosionQExtent) / Step, (Qy + ErosionQExtent) / Step);
	}
}

FTerrainDensity::FTerrainDensity(const FArchipelagoLayout& InLayout)
	: Layout(InLayout)
	, FloorNoise(InLayout.Seed ^ 0x1F123BB5u)
	, DetailNoise(InLayout.Seed ^ 0x5F356495u)
	, OverhangNoise(InLayout.Seed ^ 0x2C1B3C6Du)
{
	BuildCaves();
	if (const FIslandDesc* Karst = Layout.FindIsland(EIslandArchetype::Mesa))
	{
		KarstGrid = GetOrBuildErodedGrid(*Karst, [Seed = Karst->Seed, MaxHeight = Karst->MaxHeight](const FExploredNoise& N, float Qx, float Qy)
		{
			return KarstMassifShape(Seed, N, Qx, Qy) * MaxHeight;
		});
	}
	if (const FIslandDesc* Mangrove = Layout.FindIsland(EIslandArchetype::Mangrove))
	{
		MangroveGrid = GetOrBuildErodedGrid(*Mangrove, &MangroveBaseShape);
	}
}

float FTerrainDensity::IslandHeight(const FIslandDesc& Island, float X, float Y, float& OutT) const
{
	const FExploredNoise N(Island.Seed);
	FVector2D Q = ToLocal(Island, X, Y);

	// Forma alargada propia de cada isla (las circulares no parecen naturales).
	const float Aspect = 0.62f + 0.3f * ExploredHash::ToUnitFloat(ExploredHash::Hash32(Island.Seed ^ 0xA5u));
	Q.Y /= Aspect;

	// Costa irregular: deformación del dominio a dos escalas.
	const FVector2D Warped = N.Warp2D(Q.X * 1.6f, Q.Y * 1.6f, 0.55f, 4) / 1.6f;
	Q = FMath::Lerp(Q, Warped, 0.85f);

	// Lóbulos y bahías: ruido sobre la dirección (continuo alrededor de la isla).
	const float Len = Q.Size();
	const FVector2D Dir = Len > KINDA_SMALL_NUMBER ? Q / Len : FVector2D(1.0f, 0.0f);
	const float Lobes = N.Fbm2D(Dir.X * 1.4f + 5.0f, Dir.Y * 1.4f - 3.0f, 3);
	const float Coast = 1.0f + 0.32f * Lobes + 0.1f * N.Fbm2D(Q.X * 6.0f + 11.0f, Q.Y * 6.0f - 7.0f, 3);
	float T = Len / FMath::Max(Coast, 0.45f);

	// Penínsulas: elipses secundarias unidas con mínimo suave a la silueta principal.
	for (const FIslandLobe& Lobe : Island.Lobes)
	{
		const FVector2D D = Q - Lobe.Offset;
		const float C = FMath::Cos(-Lobe.Angle);
		const float S = FMath::Sin(-Lobe.Angle);
		const FVector2D L(D.X * C - D.Y * S, (D.X * S + D.Y * C) / Lobe.Aspect);
		T = SmoothMin(T, L.Size() / (Lobe.Radius * FMath::Max(Coast, 0.45f)), 0.18f);
	}
	OutT = T;

	if (T >= InfluenceLimit)
	{
		return FArchipelagoLayout::OceanFloor;
	}

	// Perfil submarino común: plataforma somera, cresta de arrecife y talud.
	const float Floor = FArchipelagoLayout::OceanFloor;
	// Plataforma somera estrecha, cresta de arrecife discontinua y talud pronunciado.
	const float ShelfEnd = 1.22f + 0.08f * N.Fbm2D(Q.X * 3.0f - 20.0f, Q.Y * 3.0f, 2);
	float Underwater = FMath::Lerp(ShelfDepth, -6.0f, SmoothStep(1.02f, ShelfEnd, T));
	Underwater = FMath::Lerp(Underwater, Floor, SmoothStep(ShelfEnd, InfluenceLimit, T));
	const float ReefBreaks = SmoothStep(-0.1f, 0.3f, N.Fbm2D(Q.X * 8.0f, Q.Y * 8.0f + 50.0f, 2));
	Underwater += 3.2f * ReefBreaks * FMath::Exp(-FMath::Square((T - ShelfEnd + 0.03f) / 0.025f));

	const float U = 1.0f - T;
	const float Hmax = Island.MaxHeight;
	float Land = -1000.0f;

	switch (Island.Archetype)
	{
	case EIslandArchetype::Landing:
	{
		const float Rise = FMath::Pow(SmoothStep(0.06f, 1.0f, U), 1.6f);
		const float Hills = 0.7f + 0.3f * N.Fbm2D(Q.X * 3.0f, Q.Y * 3.0f, 4);
		Land = 1.8f * SmoothStep(-0.02f, 0.08f, U) + (Hmax - 2.0f) * Rise * Hills;
		// Laguna protegida (lugar del amaraje) con bocana hacia el mar.
		const float Lagoon = FVector2D::Distance(Q, FVector2D(0.58f, 0.0f)) / 0.26f;
		const float Inlet = FMath::Abs(Q.Y) / 0.07f + FMath::Max(0.0f, 0.62f - Q.X) * 10.0f;
		const float Water = FMath::Min(Lagoon, Inlet);
		if (Water < 1.3f)
		{
			const float LagoonFloor = -5.0f + 3.5f * SmoothStep(0.3f, 1.0f, Water);
			Land = FMath::Lerp(LagoonFloor, Land, SmoothStep(0.85f, 1.3f, Water));
		}
		break;
	}
	case EIslandArchetype::Emerald:
	{
		const float Rise = FMath::Pow(SmoothStep(0.04f, 1.0f, U), 1.15f);
		const float Ridges = N.Ridged2D(Q.X * 2.4f + 3.0f, Q.Y * 2.4f, 5);
		Land = 1.5f * SmoothStep(-0.02f, 0.06f, U) + Hmax * Rise * (0.45f + 0.55f * Ridges);
		break;
	}
	case EIslandArchetype::Smoke:
	{
		const float Cone = FMath::Pow(FMath::Max(U, 0.0f), 1.35f);
		const float Flows = N.Ridged2D(Q.X * 6.0f, Q.Y * 6.0f, 3);
		Land = 1.2f * SmoothStep(-0.02f, 0.05f, U) + Hmax * Cone * (0.92f + 0.08f * Flows);
		// Cráter con borde marcado.
		const float Crater = 0.14f;
		if (T < Crater * 1.3f)
		{
			const float Bowl = 1.0f - SmoothStep(0.0f, Crater, T);
			Land -= Bowl * Hmax * 0.28f;
		}
		break;
	}
	case EIslandArchetype::Mesa:
	{
		// Macizo kárstico (El Nido / Ha Long): la forma viene de una rejilla erosionada por
		// FTerrainErosionModel (ver GetOrBuildErodedGrid más arriba), no de ruido evaluado al
		// vuelo. Si por lo que sea no hay rejilla (no debería pasar: se construye para toda
		// isla Mesa), cae a un macizo sin erosionar en vez de dejar un agujero en el mundo.
		const float Eroded = KarstGrid ? SampleErodedGrid(*KarstGrid, Q.X, Q.Y) : KarstMassifShape(Island.Seed, N, Q.X, Q.Y) * Hmax;
		Land = 1.8f * SmoothStep(-0.02f, 0.06f, U) + Eroded;
		break;
	}
	case EIslandArchetype::Mangrove:
	{
		// Llano de manglar: la forma (relieve bajo + cauces) viene de una rejilla erosionada
		// por FTerrainErosionModel (ver GetOrBuildErodedGrid), igual que el macizo kárstico
		// pero con un talud de reposo mucho más bajo (barro, no caliza). Antes esto era un
		// domo casi plano con un corte a -1,6 m constante allí donde un ruido superaba un
		// umbral: cauces sin gradiente y todos a la misma profundidad. La erosión da cauces
		// que bajan de verdad hacia la costa, con anchura y profundidad variables.
		Land = MangroveGrid ? SampleErodedGrid(*MangroveGrid, Q.X, Q.Y) : MangroveBaseShape(N, Q.X, Q.Y);
		break;
	}
	case EIslandArchetype::WhiteSands:
	{
		// Anillo de arena alrededor de una laguna somera.
		const float Ring = FMath::Abs(T - 0.78f) / 0.14f;
		const float RingLand = 0.6f + Hmax * FMath::Square(FMath::Max(0.0f, 1.0f - Ring));
		const float LagoonFloor = -3.5f - 1.5f * SmoothStep(0.64f, 0.2f, T);
		Land = Ring < 1.0f ? RingLand : (T < 0.78f ? FMath::Lerp(RingLand, LagoonFloor, SmoothStep(1.0f, 1.8f, Ring)) : -1000.0f);
		// Pasos entre el anillo y la laguna (motus).
		const float Gap = N.Fbm2D(Q.X * 4.0f, Q.Y * 4.0f, 2);
		if (Ring < 1.2f && Gap > 0.35f)
		{
			Land = FMath::Lerp(Land, -0.8f, SmoothStep(0.35f, 0.5f, Gap));
		}
		break;
	}
	case EIslandArchetype::Teeth:
	{
		float Best = -1000.0f;
		for (int32 I = 0; I < Island.Islets.Num(); ++I)
		{
			const FVector2D Center = Island.Islets[I] / Island.Radius;
			const float R = 0.13f + 0.07f * ExploredHash::ToUnitFloat(ExploredHash::Hash2D(Island.Seed, I, 0));
			const float D = FVector2D::Distance(Q, Center) / R;
			if (D < 1.6f)
			{
				const float Stack = FMath::Pow(FMath::Max(0.0f, 1.0f - D), 0.45f);
				const float Height = Hmax * (0.5f + 0.5f * ExploredHash::ToUnitFloat(ExploredHash::Hash2D(Island.Seed, I, 1)));
				Best = FMath::Max(Best, D < 1.0f ? 1.0f + Height * Stack : FMath::Lerp(-2.5f, 1.0f, 1.6f - D));
			}
		}
		// Entre islotes, fondo rocoso somero; nunca tierra.
		Land = FMath::Max(Best, -3.5f + 1.5f * N.Fbm2D(Q.X * 5.0f, Q.Y * 5.0f, 3));
		return T < 1.0f ? Land : FMath::Lerp(Land, Underwater, SmoothStep(1.0f, 1.25f, T));
	}
	default:
		break;
	}

	if (T >= 1.0f)
	{
		if (Land < -500.0f)
		{
			return Underwater;
		}
		// Transición suave entre la orilla y la plataforma.
		return FMath::Lerp(FMath::Min(Land, 0.0f) + ShelfDepth, Underwater, SmoothStep(1.0f, 1.12f, T));
	}
	if (Land < -500.0f)
	{
		return Underwater;
	}
	return FMath::Lerp(Underwater, Land, SmoothStep(-0.02f, 0.03f, U));
}

FTerrainColumn FTerrainDensity::SampleColumn(float X, float Y) const
{
	FTerrainColumn Column;
	Column.Height = FArchipelagoLayout::OceanFloor + 6.0f * FloorNoise.Fbm2D(X / 420.0f, Y / 420.0f, 4);

	// Dorsal submarina: las islas de la cadena comparten un zócalo menos profundo.
	if (Layout.Spine.Num() >= 2)
	{
		const float Ridge = DistanceToPolyline(Layout.Spine, FVector2D(X, Y)) * (1.0f + 0.35f * FloorNoise.Fbm2D(X / 300.0f + 50.0f, Y / 300.0f, 3));
		Column.Height = FMath::Lerp(RidgeDepth, Column.Height, SmoothStep(250.0f, 1100.0f, Ridge));
	}

	for (int32 I = 0; I < Layout.Islands.Num(); ++I)
	{
		const FIslandDesc& Island = Layout.Islands[I];
		const float Reach = Island.Radius * (InfluenceLimit + 0.4f);
		if (FVector2D::DistSquared(Island.Center, FVector2D(X, Y)) > Reach * Reach)
		{
			continue;
		}

		float T = 0.0f;
		const float H = IslandHeight(Island, X, Y, T);
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
			if (FVector2D::DistSquared(Cay.Center, FVector2D(X, Y)) > FMath::Square(Cay.Radius * 4.0f))
			{
				continue;
			}
			const float H = CayHeight(Cay, X, Y, DetailNoise.Fbm2D(X / 60.0f, Y / 60.0f, 3));
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
	OutMin = TNumericLimits<float>::Max();
	OutMax = TNumericLimits<float>::Lowest();
	for (float X = Rect.Min.X; X <= Rect.Max.X + KINDA_SMALL_NUMBER; X += SampleSpacing)
	{
		for (float Y = Rect.Min.Y; Y <= Rect.Max.Y + KINDA_SMALL_NUMBER; Y += SampleSpacing)
		{
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
