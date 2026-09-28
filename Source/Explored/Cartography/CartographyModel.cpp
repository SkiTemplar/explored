#include "Cartography/CartographyModel.h"

#include "Cartography/MapStroke.h"
#include "Core/ExploredRandom.h"
#include "WorldGen/ArchipelagoLayout.h"

static_assert(FCartographyModel::MapWorldSize == 2.0 * FArchipelagoLayout::WorldHalfExtent,
	"La hoja del mapa debe cubrir el mundo entero");

namespace CartographyDetail
{
	/** Longitud de onda (m de recorrido) del temblor de la mano: más nervioso al correr. */
	float TremorWavelength(ECartographyLocomotion Locomotion)
	{
		switch (Locomotion)
		{
		case ECartographyLocomotion::Running: return 5.0f;
		case ECartographyLocomotion::Swimming: return 9.0f;
		default: return 8.0f;
		}
	}

	float HeadingError(ECartographyLocomotion Locomotion)
	{
		switch (Locomotion)
		{
		case ECartographyLocomotion::Running: return FCartographyModel::HeadingErrorRunning;
		case ECartographyLocomotion::Swimming: return FCartographyModel::HeadingErrorSwimming;
		default: return FCartographyModel::HeadingErrorWalking;
		}
	}

	/** Longitud de onda (m) de la variación lenta del error de rumbo. */
	constexpr double HeadingWavelength = 150.0;

	float ToleranceInMap(float Meters)
	{
		return static_cast<float>(FCartographyModel::MetersToMap(Meters));
	}
}

float FMapSketch::ConfirmedFraction() const
{
	if (Confirmed.Num() == 0)
	{
		return 0.0f;
	}
	int32 Count = 0;
	for (const uint8 Flag : Confirmed)
	{
		Count += Flag != 0 ? 1 : 0;
	}
	return static_cast<float>(Count) / static_cast<float>(Confirmed.Num());
}

float FMapIslandCoverage::Fraction() const
{
	if (Visited.Num() == 0)
	{
		return 0.0f;
	}
	int32 Count = 0;
	for (const uint8 Flag : Visited)
	{
		Count += Flag != 0 ? 1 : 0;
	}
	return static_cast<float>(Count) / static_cast<float>(Visited.Num());
}

FCartographyModel::FCartographyModel(uint32 InSeed)
	: Seed(InSeed)
	, TremorNoise(ExploredHash::Hash32(InSeed ^ 0x7A3B11u))
	, HeadingNoise(ExploredHash::Hash32(InSeed ^ 0x51C0DEu))
	, SketchNoise(ExploredHash::Hash32(InSeed ^ 0x5CE7C4u))
{
	HeadingBiasSign = (ExploredHash::Hash32(InSeed ^ 0xB1A5u) & 1u) != 0 ? 1.0f : -1.0f;
}

FVector2D FCartographyModel::WorldToMap(const FVector2D& World)
{
	const double Half = MapWorldSize * 0.5;
	return FVector2D((World.Y + Half) / MapWorldSize, (Half - World.X) / MapWorldSize);
}

FVector2D FCartographyModel::MapToWorld(const FVector2D& Map)
{
	const double Half = MapWorldSize * 0.5;
	return FVector2D(Half - Map.Y * MapWorldSize, Map.X * MapWorldSize - Half);
}

float FCartographyModel::TremorAmplitude(ECartographyLocomotion Locomotion)
{
	switch (Locomotion)
	{
	case ECartographyLocomotion::Running: return TremorRunning;
	case ECartographyLocomotion::Swimming: return TremorSwimming;
	default: return TremorWalking;
	}
}

FVector2D FCartographyModel::TremorOffset(double TravelMeters, ECartographyLocomotion Locomotion) const
{
	const float T = static_cast<float>(TravelMeters / CartographyDetail::TremorWavelength(Locomotion));
	// Dos canales de fBm (la mano tiembla en las dos direcciones), acotados al disco unidad.
	FVector2D Offset(TremorNoise.Fbm2D(T, 3.7f, 2), TremorNoise.Fbm2D(T, 11.3f, 2));
	const double Size = Offset.Size();
	if (Size > 1.0)
	{
		Offset /= Size;
	}
	return Offset * TremorAmplitude(Locomotion);
}

void FCartographyModel::UpdateDrift(const FVector2D& Step, double StepLength, const FCartographySample& InSample)
{
	// Error de rumbo: sesgo propio del cartógrafo más una variación lenta. Girar cada paso
	// ese ángulo acumula deriva con la distancia, como al dibujar sin referencia fija.
	const float MaxError = CartographyDetail::HeadingError(InSample.Locomotion) * (InSample.bHasCompass ? CompassHeadingFactor : 1.0f);
	const float Wander = HeadingNoise.Fbm2D(static_cast<float>(State.TravelDistance / CartographyDetail::HeadingWavelength), 0.5f, 2);
	const float ErrorDegrees = MaxError * (0.5f * HeadingBiasSign + 0.5f * Wander);
	State.Drift += Step.GetRotated(ErrorDegrees) - Step;

	if (InSample.bHasCompass)
	{
		// La brújula rectifica el rumbo general: la deriva vuelve hacia cero con la distancia.
		State.Drift *= FMath::Exp(-StepLength / CompassCorrectionMeters);
	}
	const double DriftSize = State.Drift.Size();
	if (DriftSize > MaxDriftMeters)
	{
		State.Drift *= MaxDriftMeters / DriftSize;
	}
}

void FCartographyModel::Sample(const FCartographySample& InSample)
{
	const FVector2D Position = InSample.WorldPosition;
	// Una posición no finita no se dibuja ni se recuerda: mancharía el trazo, la cobertura y la
	// muestra siguiente. IsFinite explícito, que con matemáticas rápidas las comparaciones no bastan.
	if (!FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y))
	{
		EndStroke();
		return;
	}
	if (bHasLastPosition)
	{
		const FVector2D Step = Position - LastPosition;
		const double StepLength = Step.Size();
		if (StepLength > MaxStepMeters)
		{
			// Teletransporte o carga: no hay recorrido que dibujar entre medias.
			EndStroke();
		}
		else if (StepLength > UE_DOUBLE_KINDA_SMALL_NUMBER)
		{
			State.TravelDistance += StepLength;
			UpdateDrift(Step, StepLength, InSample);
		}
	}
	LastPosition = Position;
	bHasLastPosition = true;

	const float Reach = IsRecording() ? RecordDistance + RecordHysteresis : RecordDistance;
	if (!FMath::IsFinite(InSample.DistanceToShore) || InSample.DistanceToShore > Reach)
	{
		EndStroke();
		return;
	}

	// El hecho de haber estado aquí: cobertura y bocetos usan la posición real.
	MarkVisited(Position, InSample.IslandIndex);
	ConfirmSketches(Position);

	if (IsRecording() && State.Strokes[ActiveStroke].IslandIndex != InSample.IslandIndex)
	{
		EndStroke();
	}

	const FVector2D Drawn = WorldToMap(Position + State.Drift + TremorOffset(State.TravelDistance, InSample.Locomotion));
	if (!IsRecording())
	{
		FMapStroke& Stroke = State.Strokes.AddDefaulted_GetRef();
		Stroke.IslandIndex = InSample.IslandIndex;
		Stroke.ToleranceMeters = BaseToleranceMeters;
		Stroke.Points.Add(Drawn);
		ActiveStroke = State.Strokes.Num() - 1;
		return;
	}

	FMapStroke& Stroke = State.Strokes[ActiveStroke];
	if (FVector2D::Distance(Stroke.Points.Last(), Drawn) >= MetersToMap(MinPointSpacing))
	{
		Stroke.Points.Add(Drawn);
		State.bHasDrawnAnyCoast = true;
		if (Stroke.Points.Num() >= MaxStrokePoints)
		{
			CompactStroke(Stroke, MaxStrokePoints * 3 / 4);
		}
	}
}

void FCartographyModel::EndStroke()
{
	if (!IsRecording())
	{
		return;
	}
	FMapStroke& Stroke = State.Strokes[ActiveStroke];
	if (Stroke.Points.Num() < 2)
	{
		State.Strokes.RemoveAt(ActiveStroke);
	}
	else
	{
		Stroke.Points = MapStroke::Simplify(Stroke.Points, CartographyDetail::ToleranceInMap(Stroke.ToleranceMeters));
	}
	ActiveStroke = INDEX_NONE;
	EnforceTotalBudget();
}

void FCartographyModel::CompactStroke(FMapStroke& Stroke, int32 MaxPoints)
{
	Stroke.Points = MapStroke::Simplify(Stroke.Points, CartographyDetail::ToleranceInMap(Stroke.ToleranceMeters));
	// Si el temblor deja demasiados puntos, se sube la tolerancia hasta que quepa.
	while (Stroke.Points.Num() > MaxPoints && Stroke.ToleranceMeters < MaxDriftMeters)
	{
		Stroke.ToleranceMeters *= 1.5f;
		Stroke.Points = MapStroke::Simplify(Stroke.Points, CartographyDetail::ToleranceInMap(Stroke.ToleranceMeters));
	}
}

void FCartographyModel::EnforceTotalBudget()
{
	int32 Guard = 0;
	while (NumStrokePoints() > MaxTotalStrokePoints && Guard++ < 16)
	{
		for (int32 I = 0; I < State.Strokes.Num(); ++I)
		{
			if (I == ActiveStroke)
			{
				continue;
			}
			FMapStroke& Stroke = State.Strokes[I];
			Stroke.ToleranceMeters = FMath::Min(Stroke.ToleranceMeters * 2.0f, MaxDriftMeters);
			Stroke.Points = MapStroke::Simplify(Stroke.Points, CartographyDetail::ToleranceInMap(Stroke.ToleranceMeters));
		}
	}
}

int32 FCartographyModel::NumStrokePoints() const
{
	int32 Total = 0;
	for (const FMapStroke& Stroke : State.Strokes)
	{
		Total += Stroke.Points.Num();
	}
	return Total;
}

void FCartographyModel::MarkVisited(const FVector2D& WorldPosition, int32 IslandIndex)
{
	const FVector2D P = WorldToMap(WorldPosition);
	const double Radius = MetersToMap(CoverageRadius);
	const double RadiusSq = Radius * Radius;
	for (FMapIslandCoverage& Entry : State.Coverage)
	{
		if (IslandIndex != INDEX_NONE && Entry.IslandIndex != IslandIndex)
		{
			continue;
		}
		for (int32 I = 0; I < Entry.Coast.Num(); ++I)
		{
			if (Entry.Visited[I] == 0 && FVector2D::DistSquared(Entry.Coast[I], P) <= RadiusSq)
			{
				Entry.Visited[I] = 1;
			}
		}
	}
}

void FCartographyModel::ConfirmSketches(const FVector2D& WorldPosition)
{
	const FVector2D P = WorldToMap(WorldPosition);
	const double Radius = MetersToMap(CoverageRadius);
	const double RadiusSq = Radius * Radius;
	for (FMapSketch& Sketch : State.Sketches)
	{
		for (int32 I = 0; I < Sketch.Points.Num(); ++I)
		{
			if (Sketch.Confirmed[I] == 0 && FVector2D::DistSquared(Sketch.Points[I], P) <= RadiusSq)
			{
				Sketch.Confirmed[I] = 1;
			}
		}
	}
}

int32 FCartographyModel::AddSketch(int32 IslandIndex, const TArray<FVector2D>& WorldOutline)
{
	if (WorldOutline.Num() < 3)
	{
		return INDEX_NONE;
	}
	if (IslandIndex != INDEX_NONE)
	{
		const int32 Existing = State.Sketches.IndexOfByPredicate([IslandIndex](const FMapSketch& S) { return S.IslandIndex == IslandIndex; });
		if (Existing != INDEX_NONE)
		{
			return Existing;
		}
	}

	// Silueta orientativa: se pierde el detalle fino y se deforma un poco (puede estar mal).
	const TArray<FVector2D> Coarse = MapStroke::SimplifyClosed(WorldOutline, SketchToleranceMeters);
	const double Perimeter = MapStroke::Length(Coarse, true);
	const double Spacing = FMath::Max<double>(SketchSpacingMeters, Perimeter / MaxSketchPoints);
	const TArray<FVector2D> Even = MapStroke::Resample(Coarse, Spacing, true);

	const float Phase = static_cast<float>(State.Sketches.Num()) * 7.31f;
	FMapSketch& Sketch = State.Sketches.AddDefaulted_GetRef();
	Sketch.IslandIndex = IslandIndex;
	double Along = 0.0;
	for (int32 I = 0; I < Even.Num(); ++I)
	{
		if (I > 0)
		{
			Along += FVector2D::Distance(Even[I - 1], Even[I]);
		}
		const float T = static_cast<float>(Along / 180.0);
		const FVector2D Wobble(SketchNoise.Fbm2D(T, Phase + 0.5f, 2), SketchNoise.Fbm2D(T, Phase + 21.5f, 2));
		Sketch.Points.Add(WorldToMap(Even[I] + Wobble * SketchWobbleMeters));
	}
	Sketch.Confirmed.Init(0, Sketch.Points.Num());
	return State.Sketches.Num() - 1;
}

const TArray<FName>& FCartographyModel::KnownStamps()
{
	// Espejo de `Content/Data/story_es.json` → `map_marks`.
	static const TArray<FName> Stamps = {
		FName(TEXT("water")), FName(TEXT("cave")), FName(TEXT("danger")),
		FName(TEXT("resource")), FName(TEXT("ruin")), FName(TEXT("wreck"))
	};
	return Stamps;
}

bool FCartographyModel::IsKnownStamp(FName StampId)
{
	return KnownStamps().Contains(StampId);
}

FString FCartographyModel::CapMarkText(const FString& Text)
{
	// Cuenta caracteres TCHAR, como Unreal. (En el host el FString es UTF-8: los tests usan ASCII.)
	return Text.TrimStartAndEnd().Left(MaxMarkTextLength);
}

int32 FCartographyModel::AddMarkAt(FName StampId, const FVector2D& MapPosition, const FString& Text, EMapMarkSource Source)
{
	const bool bValidStamp = StampId.IsNone() ? !CapMarkText(Text).IsEmpty() : IsKnownStamp(StampId);
	if (!bValidStamp || State.Marks.Num() >= MaxMarks)
	{
		return INDEX_NONE;
	}
	FMapMark& Mark = State.Marks.AddDefaulted_GetRef();
	Mark.StampId = StampId;
	Mark.Position = MapPosition;
	Mark.Text = CapMarkText(Text);
	Mark.Source = Source;
	++State.MarkSerial;
	return State.Marks.Num() - 1;
}

int32 FCartographyModel::AddMark(FName StampId, const FVector2D& WorldPosition, const FString& Text)
{
	// La marca se dibuja donde el jugador cree estar: comparte la deriva de sus trazos.
	return AddMarkAt(StampId, WorldToMap(WorldPosition + State.Drift), Text, EMapMarkSource::Hand);
}

int32 FCartographyModel::AddSpyglassMark(FName StampId, const FVector2D& ObserverWorld, const FVector2D& TargetWorld, const FString& Text)
{
	const uint32 Hash = ExploredHash::Hash2D(Seed, State.MarkSerial, 0x5B);
	const float BearingError = (ExploredHash::ToUnitFloat(Hash) * 2.0f - 1.0f) * SpyglassBearingErrorDegrees;
	const float RangeScale = 1.0f + (ExploredHash::ToUnitFloat(ExploredHash::Hash32(Hash)) * 2.0f - 1.0f) * SpyglassRangeError;
	const FVector2D Seen = (TargetWorld - ObserverWorld).GetRotated(BearingError) * RangeScale;
	return AddMarkAt(StampId, WorldToMap(ObserverWorld + State.Drift + Seen), Text, EMapMarkSource::Spyglass);
}

int32 FCartographyModel::AddSextantMark(FName StampId, const FVector2D& WorldPosition, const FString& Text)
{
	return AddMarkAt(StampId, WorldToMap(WorldPosition), Text, EMapMarkSource::Sextant);
}

int32 FCartographyModel::AddMarkOnSheet(FName StampId, const FVector2D& MapPosition, const FString& Text)
{
	if (!FMath::IsFinite(MapPosition.X) || !FMath::IsFinite(MapPosition.Y))
	{
		return INDEX_NONE;
	}
	return AddMarkAt(StampId, FVector2D(FMath::Clamp(MapPosition.X, 0.0, 1.0), FMath::Clamp(MapPosition.Y, 0.0, 1.0)), Text, EMapMarkSource::Hand);
}

bool FCartographyModel::NoteRecipe(FName RecipeId, bool bDoodle)
{
	if (RecipeId.IsNone())
	{
		return false;
	}
	for (FMapRecipeNote& Note : State.Recipes)
	{
		if (Note.RecipeId == RecipeId)
		{
			if (bDoodle && !Note.bDoodle)
			{
				Note.bDoodle = true;
				return true;
			}
			return false;
		}
	}
	FMapRecipeNote& Note = State.Recipes.AddDefaulted_GetRef();
	Note.RecipeId = RecipeId;
	Note.bDoodle = bDoodle;
	return true;
}

void FCartographyModel::TickWetness(const FCartographyExposure& Exposure, float DeltaSeconds)
{
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}
	float Gain = 0.0f;
	if (!Exposure.bStoredDry)
	{
		Gain = FMath::Clamp(Exposure.Rain, 0.0f, 1.0f) * RainWetRate + (Exposure.bInSeaWater ? SeaWetRate : 0.0f);
	}
	State.Wetness = FMath::Clamp(State.Wetness + (Gain - DryRate) * DeltaSeconds, 0.0f, 1.0f);

	if (State.Wetness > InkRunThreshold)
	{
		const float Soak = (State.Wetness - InkRunThreshold) / (1.0f - InkRunThreshold);
		State.InkRunProgress += Soak * DeltaSeconds / InkRunSeconds;
		while (State.InkRunProgress >= 1.0f)
		{
			State.InkRunProgress -= 1.0f;
			ApplyInkRun();
		}
	}
}

void FCartographyModel::ApplyInkRun()
{
	for (FMapStroke& Stroke : State.Strokes)
	{
		// La tinta corre: se funden los quiebros finos y se pierden vértices.
		MapStroke::Smooth(Stroke.Points, 0.5, 1);
		Stroke.ToleranceMeters = FMath::Min(Stroke.ToleranceMeters * 1.8f, MaxDriftMeters);
		Stroke.Points = MapStroke::Simplify(Stroke.Points, CartographyDetail::ToleranceInMap(Stroke.ToleranceMeters));
		Stroke.Ink = FMath::Max(InkFloor, Stroke.Ink * InkFadePerRun);
		Stroke.Blur = FMath::Min(1.0f, Stroke.Blur + 0.2f);
	}
	for (FMapSketch& Sketch : State.Sketches)
	{
		Sketch.Ink = FMath::Max(InkFloor, Sketch.Ink * InkFadePerRun);
	}
	for (FMapMark& Mark : State.Marks)
	{
		Mark.Ink = FMath::Max(InkFloor, Mark.Ink * InkFadePerRun);
		if (Mark.Ink < LegibleInk)
		{
			// El texto se emborrona del todo; el sello (el hecho) sigue ahí.
			Mark.Text.Empty();
		}
	}
	++State.InkRuns;
}

int32 FCartographyModel::CopyToCleanSheet()
{
	int32 Restored = 0;
	for (FMapStroke& Stroke : State.Strokes)
	{
		if (Stroke.Ink >= LegibleInk)
		{
			Stroke.Ink = 1.0f;
			Stroke.Blur = 0.0f;
			++Restored;
		}
		else
		{
			// Lo ilegible se copia como un rastro tenue: se sabe que se estuvo, no cómo era.
			Stroke.Ink = InkFloor;
		}
	}
	for (FMapSketch& Sketch : State.Sketches)
	{
		Sketch.Ink = Sketch.Ink >= LegibleInk ? 1.0f : InkFloor;
		Restored += Sketch.Ink >= 1.0f ? 1 : 0;
	}
	for (FMapMark& Mark : State.Marks)
	{
		Mark.Ink = Mark.Ink >= LegibleInk ? 1.0f : InkFloor;
		Restored += Mark.Ink >= 1.0f ? 1 : 0;
	}
	State.Wetness = 0.0f;
	State.InkRunProgress = 0.0f;
	State.InkRuns = 0;
	return Restored;
}

void FCartographyModel::RegisterIslandCoast(int32 IslandIndex, const TArray<FVector2D>& WorldCoast)
{
	if (WorldCoast.Num() < 3)
	{
		return;
	}
	TArray<FVector2D> Even = MapStroke::Resample(WorldCoast, CoverageSpacing, true);
	TArray<FVector2D> Coast;
	Coast.Reserve(Even.Num());
	for (const FVector2D& P : Even)
	{
		Coast.Add(WorldToMap(P));
	}

	FMapIslandCoverage* Entry = State.Coverage.FindByPredicate([IslandIndex](const FMapIslandCoverage& E) { return E.IslandIndex == IslandIndex; });
	if (Entry == nullptr)
	{
		Entry = &State.Coverage.AddDefaulted_GetRef();
		Entry->IslandIndex = IslandIndex;
	}
	// Al volver a registrar la misma costa (p. ej. tras cargar) se conserva lo recorrido.
	if (Entry->Coast.Num() != Coast.Num())
	{
		Entry->Visited.Init(0, Coast.Num());
	}
	Entry->Coast = MoveTemp(Coast);
}

float FCartographyModel::GetIslandCoverage(int32 IslandIndex) const
{
	const FMapIslandCoverage* Entry = State.Coverage.FindByPredicate([IslandIndex](const FMapIslandCoverage& E) { return E.IslandIndex == IslandIndex; });
	return Entry != nullptr ? Entry->Fraction() : 0.0f;
}

float FCartographyModel::DistanceToReferenceCoast(const FVector2D& WorldPosition, int32& OutIslandIndex) const
{
	OutIslandIndex = INDEX_NONE;
	const FVector2D P = WorldToMap(WorldPosition);
	double Best = TNumericLimits<double>::Max();
	for (const FMapIslandCoverage& Entry : State.Coverage)
	{
		const double D = MapStroke::DistanceToPolyline(P, Entry.Coast, true);
		if (D < Best)
		{
			Best = D;
			OutIslandIndex = Entry.IslandIndex;
		}
	}
	return OutIslandIndex == INDEX_NONE ? TNumericLimits<float>::Max() : static_cast<float>(Best * MapWorldSize);
}

void FCartographyModel::LoadState(const FCartographyState& InState)
{
	State = InState;
	ActiveStroke = INDEX_NONE;
	bHasLastPosition = false;
}
