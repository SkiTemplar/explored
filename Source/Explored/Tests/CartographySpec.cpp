#include "Misc/AutomationTest.h"

#include "Cartography/CartographyModel.h"
#include "Cartography/CoastlineTrace.h"
#include "Cartography/MapStroke.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/TerrainDensity.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace CartographyTest
{
	constexpr uint32 Seed = 7;
	constexpr double IslandRadius = 300.0;
	/** El jugador camina 10 m tierra adentro de la orilla de la isla circular sintética. */
	constexpr double WalkRadius = 290.0;

	TArray<FVector2D> Circle(const FVector2D& Center, double Radius, int32 Count)
	{
		TArray<FVector2D> Points;
		for (int32 I = 0; I < Count; ++I)
		{
			const double Angle = UE_DOUBLE_PI * 2.0 * I / Count;
			Points.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		return Points;
	}

	FCartographySample MakeSample(const FVector2D& Position, ECartographyLocomotion Locomotion, bool bCompass, float DistanceToShore, int32 Island = 0)
	{
		FCartographySample Sample;
		Sample.WorldPosition = Position;
		Sample.DeltaSeconds = 0.3f;
		Sample.Locomotion = Locomotion;
		Sample.bHasCompass = bCompass;
		Sample.DistanceToShore = DistanceToShore;
		Sample.IslandIndex = Island;
		return Sample;
	}

	/** Recorre un arco de la isla circular (ángulos en radianes) con pasos de ~StepMeters. */
	void WalkArc(FCartographyModel& Model, double FromAngle, double ToAngle, double StepMeters,
		ECartographyLocomotion Locomotion = ECartographyLocomotion::Walking, bool bCompass = false)
	{
		const int32 Steps = FMath::Max(1, static_cast<int32>(FMath::Abs(ToAngle - FromAngle) * WalkRadius / StepMeters));
		for (int32 I = 0; I <= Steps; ++I)
		{
			const double Angle = FromAngle + (ToAngle - FromAngle) * I / Steps;
			const FVector2D P = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * WalkRadius;
			Model.Sample(MakeSample(P, Locomotion, bCompass, static_cast<float>(IslandRadius - WalkRadius)));
		}
	}

	/** Camina en línea recta hacia el norte por la orilla y devuelve la deriva final (m). */
	double WalkStraight(FCartographyModel& Model, double Meters, bool bCompass, double StartX = 0.0)
	{
		for (double X = StartX; X <= StartX + Meters; X += 1.0)
		{
			Model.Sample(MakeSample(FVector2D(X, 0.0), ECartographyLocomotion::Walking, bCompass, 5.0f));
		}
		return Model.GetDrift().Size();
	}

	float RmsTremor(const FCartographyModel& Model, ECartographyLocomotion Locomotion, float& OutMax)
	{
		double Sum = 0.0;
		int32 Count = 0;
		OutMax = 0.0f;
		for (double Travel = 0.0; Travel < 3000.0; Travel += 0.37)
		{
			const double Size = Model.TremorOffset(Travel, Locomotion).Size();
			OutMax = FMath::Max(OutMax, static_cast<float>(Size));
			Sum += Size * Size;
			++Count;
		}
		return static_cast<float>(FMath::Sqrt(Sum / Count));
	}
}

BEGIN_DEFINE_SPEC(FCartographySpec, "Explored.Cartography",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FCartographySpec)

void FCartographySpec::Define()
{
	using namespace CartographyTest;

	It("proyecta el mundo en la hoja con el norte arriba y el este a la derecha", [this]()
	{
		TestTrue(TEXT("Origen al centro"), FCartographyModel::WorldToMap(FVector2D::ZeroVector).Equals(FVector2D(0.5, 0.5)));
		TestTrue(TEXT("Norte arriba"), FCartographyModel::WorldToMap(FVector2D(3000.0, 0.0)).Equals(FVector2D(0.5, 0.0)));
		TestTrue(TEXT("Este a la derecha"), FCartographyModel::WorldToMap(FVector2D(0.0, 3000.0)).Equals(FVector2D(1.0, 0.5)));
		const FVector2D P(-1234.5, 876.25);
		TestTrue(TEXT("Ida y vuelta"), FCartographyModel::MapToWorld(FCartographyModel::WorldToMap(P)).Equals(P, 1.e-6));
	});

	It("tiembla dentro de su amplitud, más al correr, y nunca es un trazo de GPS", [this]()
	{
		const FCartographyModel Model(Seed);
		float MaxWalk = 0.0f;
		float MaxRun = 0.0f;
		const float RmsWalk = RmsTremor(Model, ECartographyLocomotion::Walking, MaxWalk);
		const float RmsRun = RmsTremor(Model, ECartographyLocomotion::Running, MaxRun);
		TestTrue(TEXT("Temblor al andar acotado"), MaxWalk <= FCartographyModel::TremorWalking + 1.e-4f);
		TestTrue(TEXT("Temblor al correr acotado"), MaxRun <= FCartographyModel::TremorRunning + 1.e-4f);
		TestTrue(TEXT("Correr tiembla más"), RmsRun > RmsWalk * 1.8f);

		// Con brújula (sin deriva apreciable) la línea dibujada sigue la ruta real con temblor.
		FCartographyModel Drawer(Seed);
		WalkStraight(Drawer, 600.0, true);
		Drawer.EndStroke();
		TestEqual(TEXT("Un trazo"), Drawer.GetState().Strokes.Num(), 1);
		double Sum = 0.0;
		double Max = 0.0;
		const TArray<FVector2D>& Points = Drawer.GetState().Strokes[0].Points;
		for (const FVector2D& P : Points)
		{
			const double Off = FMath::Abs(FCartographyModel::MapToWorld(P).Y);
			Sum += Off;
			Max = FMath::Max(Max, Off);
		}
		TestTrue(TEXT("No es preciso"), Sum / Points.Num() > 0.2);
		TestTrue(TEXT("Pero sigue la costa"), Max <= FCartographyModel::TremorWalking + 0.5);
	});

	It("acumula deriva sin brújula y la corrige con ella", [this]()
	{
		FCartographyModel Without(Seed);
		FCartographyModel With(Seed);
		const double DriftWithout = WalkStraight(Without, 1500.0, false);
		const double DriftWith = WalkStraight(With, 1500.0, true);
		TestTrue(FString::Printf(TEXT("Deriva sin brújula (%.1f m)"), DriftWithout), DriftWithout > 20.0);
		TestTrue(FString::Printf(TEXT("Deriva con brújula (%.2f m)"), DriftWith), DriftWith < 2.0);
		TestTrue(TEXT("Deriva acotada"), DriftWithout <= FCartographyModel::MaxDriftMeters + 1.e-3);

		// Sacar la brújula más tarde rectifica lo ya acumulado.
		const double Corrected = WalkStraight(Without, 200.0, true, 1501.0);
		TestTrue(FString::Printf(TEXT("Rectificada (%.2f m)"), Corrected), Corrected < 2.0);
	});

	It("no dibuja lejos de la orilla", [this]()
	{
		FCartographyModel Model(Seed);
		for (double X = 0.0; X < 300.0; X += 1.0)
		{
			Model.Sample(MakeSample(FVector2D(X, 0.0), ECartographyLocomotion::Walking, false, 200.0f));
		}
		TestFalse(TEXT("No graba"), Model.IsRecording());
		TestEqual(TEXT("Sin trazos"), Model.GetState().Strokes.Num(), 0);
		TestFalse(TEXT("Sin costa dibujada"), Model.HasDrawnAnyCoast());

		for (double X = 300.0; X < 400.0; X += 1.0)
		{
			Model.Sample(MakeSample(FVector2D(X, 0.0), ECartographyLocomotion::Walking, false, 8.0f));
		}
		TestTrue(TEXT("Graba en la orilla"), Model.IsRecording());
		// Alejarse más allá del margen de histéresis corta el trazo.
		Model.Sample(MakeSample(FVector2D(401.0, 0.0), ECartographyLocomotion::Walking, false,
			FCartographyModel::RecordDistance + FCartographyModel::RecordHysteresis + 1.0f));
		TestFalse(TEXT("Deja de grabar"), Model.IsRecording());
		TestEqual(TEXT("Un trazo"), Model.GetState().Strokes.Num(), 1);
		TestTrue(TEXT("Costa dibujada"), Model.HasDrawnAnyCoast());
	});

	It("simplifica acotando los puntos y conservando la forma dentro de la tolerancia", [this]()
	{
		// Polilínea ruidosa: un círculo de 5000 puntos con serpenteo.
		TArray<FVector2D> Raw;
		for (int32 I = 0; I < 5000; ++I)
		{
			const double Angle = UE_DOUBLE_PI * 1.9 * I / 5000.0;
			const double R = 300.0 + 2.0 * FMath::Sin(Angle * 40.0);
			Raw.Add(FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * R);
		}
		const double Tolerance = 0.5;
		const TArray<FVector2D> Simple = MapStroke::Simplify(Raw, Tolerance);
		TestTrue(FString::Printf(TEXT("Menos puntos (%d)"), Simple.Num()), Simple.Num() < Raw.Num() / 5);
		TestTrue(TEXT("Conserva el primer extremo"), Simple[0] == Raw[0]);
		TestTrue(TEXT("Conserva el último extremo"), Simple.Last() == Raw.Last());
		double Worst = 0.0;
		for (const FVector2D& P : Raw)
		{
			Worst = FMath::Max(Worst, MapStroke::DistanceToPolyline(P, Simple));
		}
		TestTrue(FString::Printf(TEXT("Dentro de la tolerancia (%.3f)"), Worst), Worst <= Tolerance + 1.e-9);

		// En el modelo: 20 km corriendo por la orilla no rebasan el presupuesto de puntos.
		FCartographyModel Model(Seed);
		for (int32 Lap = 0; Lap < 11; ++Lap)
		{
			WalkArc(Model, 0.0, UE_DOUBLE_PI * 2.0, 1.0, ECartographyLocomotion::Running);
		}
		TestTrue(TEXT("Trazo en curso acotado"), Model.GetState().Strokes.Last().Points.Num() <= FCartographyModel::MaxStrokePoints);
		Model.EndStroke();
		TestTrue(TEXT("Total acotado"), Model.NumStrokePoints() <= FCartographyModel::MaxStrokePoints);
		TestTrue(TEXT("Queda un trazo con forma"), Model.NumStrokePoints() > 50);
	});

	It("confirma los bocetos del mirador al recorrer esa costa", [this]()
	{
		FCartographyModel Model(Seed);
		const int32 Sketch = Model.AddSketch(0, Circle(FVector2D::ZeroVector, IslandRadius, 256));
		TestEqual(TEXT("Boceto añadido"), Sketch, 0);
		TestEqual(TEXT("Otro mirador no duplica la isla"), Model.AddSketch(0, Circle(FVector2D::ZeroVector, IslandRadius, 64)), 0);
		const FMapSketch& Outline = Model.GetState().Sketches[0];
		TestTrue(TEXT("Boceto con puntos"), Outline.Points.Num() > 50);
		TestEqual(TEXT("Sin confirmar"), Outline.ConfirmedFraction(), 0.0f);

		WalkArc(Model, 0.0, UE_DOUBLE_PI, 1.0);
		const float Half = Model.GetState().Sketches[0].ConfirmedFraction();
		TestTrue(FString::Printf(TEXT("Media costa confirmada (%.3f)"), Half), Half > 0.45f && Half < 0.6f);
		WalkArc(Model, UE_DOUBLE_PI, UE_DOUBLE_PI * 2.0, 1.0);
		TestTrue(TEXT("Costa entera confirmada"), Model.GetState().Sketches[0].ConfirmedFraction() > 0.97f);
		TestEqual(TEXT("Un contorno de menos de 3 puntos no es un boceto"), Model.AddSketch(1, {FVector2D(0.0, 0.0), FVector2D(1.0, 0.0)}), INDEX_NONE);
	});

	It("acepta sellos conocidos, limita el texto y sitúa catalejo y sextante", [this]()
	{
		FCartographyModel Model(Seed);
		const FString Long(TEXT("   Agua dulce bajo la cascada, detras de la roca grande con musgo   "));
		const int32 Index = Model.AddMark(FName(TEXT("water")), FVector2D(100.0, 50.0), Long);
		TestTrue(TEXT("Marca añadida"), Index != INDEX_NONE);
		const FMapMark& Mark = Model.GetState().Marks[Index];
		TestEqual(TEXT("Texto limitado"), Mark.Text.Len(), FCartographyModel::MaxMarkTextLength);
		TestTrue(TEXT("Recortado"), Mark.Text.StartsWith(TEXT("Agua")));
		TestEqual(TEXT("Sello desconocido"), Model.AddMark(FName(TEXT("dragon")), FVector2D::ZeroVector, TEXT("x")), INDEX_NONE);
		TestEqual(TEXT("Nota vacía"), Model.AddMark(NAME_None, FVector2D::ZeroVector, TEXT("   ")), INDEX_NONE);
		TestTrue(TEXT("Nota solo de texto"), Model.AddMark(NAME_None, FVector2D::ZeroVector, TEXT("Aqui empece")) != INDEX_NONE);
		for (const FName& Stamp : FCartographyModel::KnownStamps())
		{
			TestTrue(TEXT("Sellos de story_es.json"), Model.AddMark(Stamp, FVector2D::ZeroVector, FString()) != INDEX_NONE);
		}

		const FVector2D Observer(0.0, 0.0);
		const FVector2D Target(1000.0, 400.0);
		const int32 Seen = Model.AddSpyglassMark(FName(TEXT("ruin")), Observer, Target, TEXT("Humo"));
		const double Error = FVector2D::Distance(FCartographyModel::MapToWorld(Model.GetState().Marks[Seen].Position), Target);
		const double Range = FVector2D::Distance(Observer, Target);
		TestTrue(FString::Printf(TEXT("Catalejo aproximado (%.2f m)"), Error), Error > 0.01 &&
			Error <= Range * (FCartographyModel::SpyglassRangeError + 0.03) + 1.0);
		TestEqual(TEXT("Catalejo"), Model.GetState().Marks[Seen].Source, EMapMarkSource::Spyglass);

		const FVector2D AtSea(-2100.0, 1750.0);
		const int32 Exact = Model.AddSextantMark(FName(TEXT("wreck")), AtSea, TEXT("Pecio"));
		TestTrue(TEXT("Sextante exacto"), FCartographyModel::MapToWorld(Model.GetState().Marks[Exact].Position).Equals(AtSea, 1.e-6));
	});

	It("pone marcas a mano en el punto de la hoja que se elige, sin deriva (mapa en las manos)", [this]()
	{
		FCartographyModel Model(Seed);
		const int32 Placed = Model.AddMarkOnSheet(FName(TEXT("ruin")), FVector2D(0.25, 0.75), TEXT("  Marae  "));
		if (!TestTrue(TEXT("Se pone"), Placed != INDEX_NONE))
		{
			return;
		}
		const FMapMark& Mark = Model.GetState().Marks[Placed];
		TestTrue(TEXT("Donde se dibuja"), Mark.Position.Equals(FVector2D(0.25, 0.75), 1.e-12));
		TestEqual(TEXT("A mano"), Mark.Source, EMapMarkSource::Hand);
		TestEqual(TEXT("Texto recortado"), Mark.Text, FString(TEXT("Marae")));

		const int32 Outside = Model.AddMarkOnSheet(NAME_None, FVector2D(1.4, -0.2), TEXT("Nota"));
		TestTrue(TEXT("Fuera de la hoja se recorta al borde"), Outside != INDEX_NONE
			&& Model.GetState().Marks[Outside].Position.Equals(FVector2D(1.0, 0.0), 1.e-12));
		TestEqual(TEXT("Posición no finita"), Model.AddMarkOnSheet(FName(TEXT("cave")), FVector2D(std::numeric_limits<double>::quiet_NaN(), 0.5), FString()), INDEX_NONE);
		TestEqual(TEXT("Sello desconocido"), Model.AddMarkOnSheet(FName(TEXT("volcano")), FVector2D(0.5, 0.5), FString()), INDEX_NONE);
		TestEqual(TEXT("Nota vacía"), Model.AddMarkOnSheet(NAME_None, FVector2D(0.5, 0.5), TEXT("   ")), INDEX_NONE);
	});

	It("pierde detalle con el agua pero conserva la cobertura", [this]()
	{
		FCartographyModel Model(Seed);
		Model.RegisterIslandCoast(0, Circle(FVector2D::ZeroVector, IslandRadius, 256));
		WalkArc(Model, 0.0, UE_DOUBLE_PI * 1.5, 0.8, ECartographyLocomotion::Running);
		Model.EndStroke();
		Model.AddMark(FName(TEXT("cave")), FVector2D(WalkRadius, 0.0), TEXT("Cueva"));
		const float Coverage = Model.GetIslandCoverage(0);
		const int32 PointsBefore = Model.NumStrokePoints();
		const int32 StrokesBefore = Model.GetState().Strokes.Num();

		// Guardado en seco no se moja aunque diluvie.
		FCartographyExposure Dry;
		Dry.Rain = 1.0f;
		Dry.bStoredDry = true;
		for (int32 I = 0; I < 600; ++I)
		{
			Model.TickWetness(Dry, 1.0f);
		}
		TestEqual(TEXT("Seco"), Model.GetWetness(), 0.0f);
		TestEqual(TEXT("Sin tinta corrida"), Model.GetState().InkRuns, 0);

		// Un chapuzón largo con el mapa en el bolsillo.
		FCartographyExposure Sea;
		Sea.bInSeaWater = true;
		for (int32 I = 0; I < 600; ++I)
		{
			Model.TickWetness(Sea, 0.5f);
		}
		const FCartographyState& State = Model.GetState();
		TestTrue(TEXT("Empapado"), Model.GetWetness() > 0.99f);
		TestTrue(FString::Printf(TEXT("La tinta corre (%d pasadas)"), State.InkRuns), State.InkRuns >= 6);
		TestTrue(FString::Printf(TEXT("Pierde detalle (%d → %d)"), PointsBefore, Model.NumStrokePoints()), Model.NumStrokePoints() < PointsBefore);
		TestEqual(TEXT("Los trazos siguen ahí"), State.Strokes.Num(), StrokesBefore);
		TestTrue(TEXT("Tinta apagada"), State.Strokes[0].Ink < FCartographyModel::LegibleInk);
		TestTrue(TEXT("Nunca desaparece"), State.Strokes[0].Ink >= FCartographyModel::InkFloor);
		TestTrue(TEXT("Texto ilegible"), State.Marks[0].Text.IsEmpty());
		TestEqual(TEXT("Sello conservado"), State.Marks[0].StampId, FName(TEXT("cave")));
		TestEqual(TEXT("Cobertura intacta"), Model.GetIslandCoverage(0), Coverage);
		TestTrue(TEXT("Consta que dibujó costa"), Model.HasDrawnAnyCoast());
	});

	It("copia en limpio en la mesa lo que aún se lee", [this]()
	{
		FCartographyModel Model(Seed);
		Model.RegisterIslandCoast(0, Circle(FVector2D::ZeroVector, IslandRadius, 256));
		WalkArc(Model, 0.0, UE_DOUBLE_PI, 1.0);
		Model.EndStroke();
		Model.AddMark(FName(TEXT("water")), FVector2D(WalkRadius, 0.0), TEXT("Manantial"));
		Model.ApplyInkRun();
		Model.ApplyInkRun();
		const TArray<FVector2D> Blurred = Model.GetState().Strokes[0].Points;
		TestTrue(TEXT("Aún legible"), Model.GetState().Strokes[0].Ink >= FCartographyModel::LegibleInk);

		TestTrue(TEXT("Restaura algo"), Model.CopyToCleanSheet() >= 2);
		const FCartographyState& State = Model.GetState();
		TestEqual(TEXT("Tinta plena"), State.Strokes[0].Ink, 1.0f);
		TestEqual(TEXT("Sin corrido"), State.Strokes[0].Blur, 0.0f);
		TestTrue(TEXT("El detalle perdido no vuelve"), State.Strokes[0].Points == Blurred);
		TestEqual(TEXT("Texto conservado"), State.Marks[0].Text, FString(TEXT("Manantial")));
		TestEqual(TEXT("Papel seco"), State.Wetness, 0.0f);
		TestEqual(TEXT("Contador reiniciado"), State.InkRuns, 0);

		// Demasiado tarde: lo ilegible queda como rastro tenue.
		for (int32 I = 0; I < 8; ++I)
		{
			Model.ApplyInkRun();
		}
		const float Coverage = Model.GetIslandCoverage(0);
		Model.CopyToCleanSheet();
		TestEqual(TEXT("Rastro tenue"), Model.GetState().Strokes[0].Ink, FCartographyModel::InkFloor);
		TestTrue(TEXT("Texto perdido"), Model.GetState().Marks[0].Text.IsEmpty());
		TestEqual(TEXT("Cobertura intacta"), Model.GetIslandCoverage(0), Coverage);
	});

	It("ignora posiciones y distancias a la orilla no finitas sin manchar trazos ni cobertura", [this]()
	{
		const double NaN = std::numeric_limits<double>::quiet_NaN();
		FCartographyModel Model(Seed);
		Model.RegisterIslandCoast(0, Circle(FVector2D::ZeroVector, IslandRadius, 256));
		for (double X = 0.0; X < 20.0; X += 1.0)
		{
			Model.Sample(MakeSample(FVector2D(IslandRadius, X), ECartographyLocomotion::Walking, false, 5.0f));
		}
		TestTrue(TEXT("graba en la orilla"), Model.IsRecording());
		const float Coverage = Model.GetIslandCoverage(0);
		Model.Sample(MakeSample(FVector2D(NaN, 0.0), ECartographyLocomotion::Walking, false, 5.0f));
		Model.Sample(MakeSample(FVector2D(0.0, std::numeric_limits<double>::infinity()), ECartographyLocomotion::Walking, false, 5.0f));
		TestEqual(TEXT("posición NaN: la cobertura no cambia"), Model.GetIslandCoverage(0), Coverage);
		TestFalse(TEXT("posición NaN: corta el trazo"), Model.IsRecording());

		for (double X = 20.0; X < 40.0; X += 1.0)
		{
			Model.Sample(MakeSample(FVector2D(IslandRadius, X), ECartographyLocomotion::Walking, false, 5.0f));
		}
		TestTrue(TEXT("vuelve a grabar"), Model.IsRecording());
		Model.Sample(MakeSample(FVector2D(IslandRadius, 40.0), ECartographyLocomotion::Walking, false, std::numeric_limits<float>::quiet_NaN()));
		TestFalse(TEXT("distancia NaN: como lejos de la orilla"), Model.IsRecording());

		bool bAllFinite = true;
		for (const FMapStroke& Stroke : Model.GetState().Strokes)
		{
			for (const FVector2D& P : Stroke.Points)
			{
				bAllFinite &= FMath::IsFinite(P.X) && FMath::IsFinite(P.Y);
			}
		}
		TestTrue(TEXT("todos los puntos finitos"), bAllFinite);
		TestTrue(TEXT("deriva finita"), FMath::IsFinite(Model.GetState().Drift.X) && FMath::IsFinite(Model.GetState().Drift.Y));
	});

	It("el agua con tiempos o progreso no finitos o enormes no cuelga el mapa", [this]()
	{
		FCartographyExposure Sea;
		Sea.bInSeaWater = true;
		FCartographyState Soaked;
		Soaked.Wetness = 1.0f;
		// Por encima de 2^24, restar 1 a un float no cambia nada.
		Soaked.InkRunProgress = 3.0e7f;
		FCartographyModel Model(Seed);
		Model.LoadState(Soaked);
		Model.TickWetness(Sea, 0.1f);
		TestTrue(TEXT("progreso guardado enorme: pocas pasadas"), Model.GetState().InkRuns <= FCartographyModel::MaxInkRunsPerTick);
		TestTrue(TEXT("y queda en [0, 1)"), Model.GetState().InkRunProgress >= 0.0f && Model.GetState().InkRunProgress < 1.0f);

		Soaked.InkRunProgress = std::numeric_limits<float>::infinity();
		Model.LoadState(Soaked);
		Model.TickWetness(Sea, 0.1f);
		TestTrue(TEXT("progreso infinito: finito después"), FMath::IsFinite(Model.GetState().InkRunProgress));

		Soaked.InkRunProgress = 0.0f;
		Model.LoadState(Soaked);
		Model.TickWetness(Sea, std::numeric_limits<float>::quiet_NaN());
		TestEqual(TEXT("DeltaSeconds NaN no hace nada"), Model.GetWetness(), 1.0f);
		TestEqual(TEXT("ni corre la tinta"), Model.GetState().InkRuns, 0);
		TestEqual(TEXT("ni ensucia el progreso"), Model.GetState().InkRunProgress, 0.0f);
		Model.TickWetness(Sea, 1.0e30f);
		TestTrue(TEXT("DeltaSeconds enorme: pocas pasadas"), Model.GetState().InkRuns <= FCartographyModel::MaxInkRunsPerTick);
		TestTrue(TEXT("humedad finita"), FMath::IsFinite(Model.GetWetness()));
	});

	It("mide la cobertura de costa en una isla circular sintética", [this]()
	{
		FCartographyModel Model(Seed);
		Model.RegisterIslandCoast(0, Circle(FVector2D::ZeroVector, IslandRadius, 256));
		TestEqual(TEXT("Sin recorrer"), Model.GetIslandCoverage(0), 0.0f);
		TestEqual(TEXT("Isla sin costa de referencia"), Model.GetIslandCoverage(3), 0.0f);

		int32 Island = INDEX_NONE;
		TestEqual(TEXT("Distancia desde el centro"), Model.DistanceToReferenceCoast(FVector2D::ZeroVector, Island), static_cast<float>(IslandRadius), 1.0f);
		TestEqual(TEXT("Isla más cercana"), Island, 0);
		TestEqual(TEXT("Distancia en la orilla"), Model.DistanceToReferenceCoast(FVector2D(IslandRadius + 10.0, 0.0), Island), 10.0f, 0.5f);

		WalkArc(Model, 0.0, UE_DOUBLE_PI, 1.0);
		const float Half = Model.GetIslandCoverage(0);
		TestTrue(FString::Printf(TEXT("Media isla (%.3f)"), Half), Half >= 0.5f && Half < 0.58f);
		TestFalse(TEXT("Aún no es Cartógrafo"), Model.IsIslandCharted(0));
		WalkArc(Model, UE_DOUBLE_PI, UE_DOUBLE_PI * 2.0, 1.0);
		TestTrue(TEXT("Isla completa"), Model.IsIslandCharted(0));
		TestTrue(TEXT("Toda la costa"), Model.GetIslandCoverage(0) > 0.99f);
	});

	It("anota las recetas con su boceto", [this]()
	{
		FCartographyModel Model(Seed);
		TestTrue(TEXT("Nueva"), Model.NoteRecipe(FName(TEXT("stone_axe")), false));
		TestFalse(TEXT("Repetida"), Model.NoteRecipe(FName(TEXT("stone_axe")), false));
		TestTrue(TEXT("Gana su boceto"), Model.NoteRecipe(FName(TEXT("stone_axe")), true));
		TestFalse(TEXT("Sin id"), Model.NoteRecipe(NAME_None, true));
		TestEqual(TEXT("Una receta"), Model.GetState().Recipes.Num(), 1);
		TestTrue(TEXT("Con boceto"), Model.GetState().Recipes[0].bDoodle);
	});

	It("es determinista y se guarda y restaura como datos planos", [this]()
	{
		auto Run = [](uint32 InSeed)
		{
			FCartographyModel Model(InSeed);
			Model.RegisterIslandCoast(0, Circle(FVector2D::ZeroVector, IslandRadius, 128));
			Model.AddSketch(0, Circle(FVector2D::ZeroVector, IslandRadius, 128));
			WalkArc(Model, 0.0, 2.0, 1.0, ECartographyLocomotion::Running);
			WalkArc(Model, 2.0, 4.0, 1.0, ECartographyLocomotion::Swimming);
			Model.AddSpyglassMark(FName(TEXT("danger")), FVector2D::ZeroVector, FVector2D(800.0, -300.0), TEXT("Rompientes"));
			FCartographyExposure Rain;
			Rain.Rain = 1.0f;
			for (int32 I = 0; I < 200; ++I)
			{
				Model.TickWetness(Rain, 1.0f);
			}
			return Model;
		};
		const FCartographyModel A = Run(Seed);
		const FCartographyModel B = Run(Seed);
		const FCartographyModel C = Run(Seed + 1);
		TestEqual(TEXT("Mismos trazos"), A.GetState().Strokes.Num(), B.GetState().Strokes.Num());
		bool bSame = A.GetState().Strokes.Num() == B.GetState().Strokes.Num();
		for (int32 I = 0; bSame && I < A.GetState().Strokes.Num(); ++I)
		{
			bSame = A.GetState().Strokes[I].Points == B.GetState().Strokes[I].Points;
		}
		TestTrue(TEXT("Puntos idénticos"), bSame);
		TestTrue(TEXT("Misma marca"), A.GetState().Marks[0].Position == B.GetState().Marks[0].Position);
		TestEqual(TEXT("Misma humedad"), A.GetWetness(), B.GetWetness(), 0.0f);
		TestTrue(TEXT("Otra semilla, otra mano"), A.GetState().Strokes[0].Points != C.GetState().Strokes[0].Points);

		FCartographyModel Loaded(Seed);
		Loaded.LoadState(A.GetState());
		TestFalse(TEXT("Cargar cierra el trazo"), Loaded.IsRecording());
		TestEqual(TEXT("Cobertura restaurada"), Loaded.GetIslandCoverage(0), A.GetIslandCoverage(0));
		TestEqual(TEXT("Boceto restaurado"), Loaded.GetState().Sketches[0].ConfirmedFraction(), A.GetState().Sketches[0].ConfirmedFraction());
		// Volver a registrar la misma costa tras cargar no borra lo recorrido.
		Loaded.RegisterIslandCoast(0, Circle(FVector2D::ZeroVector, IslandRadius, 128));
		TestEqual(TEXT("Cobertura conservada"), Loaded.GetIslandCoverage(0), A.GetIslandCoverage(0));
		const int32 StrokesBefore = Loaded.GetState().Strokes.Num();
		WalkArc(Loaded, 4.0, 4.5, 1.0);
		TestEqual(TEXT("Sigue dibujando en un trazo nuevo"), Loaded.GetState().Strokes.Num(), StrokesBefore + 1);
	});

	It("sanea al cargar la deriva no finita y las listas de cobertura y boceto desparejadas", [this]()
	{
		FCartographyModel Model(Seed);
		Model.RegisterIslandCoast(0, Circle(FVector2D::ZeroVector, IslandRadius, 128));
		Model.AddSketch(0, Circle(FVector2D::ZeroVector, IslandRadius, 128));
		FCartographyState Corrupt = Model.GetState();
		Corrupt.Drift = FVector2D(std::numeric_limits<double>::quiet_NaN(), 0.0);
		Corrupt.TravelDistance = std::numeric_limits<double>::infinity();
		Corrupt.Coverage[0].Visited.SetNum(1);
		Corrupt.Sketches[0].Confirmed.Empty();

		FCartographyModel Loaded(Seed);
		Loaded.LoadState(Corrupt);
		WalkArc(Loaded, 0.0, 1.0, 1.0);
		const FCartographyState& State = Loaded.GetState();
		TestTrue(TEXT("deriva finita"), FMath::IsFinite(State.Drift.X) && FMath::IsFinite(State.Drift.Y));
		TestTrue(TEXT("recorrido finito"), FMath::IsFinite(State.TravelDistance));
		TestTrue(TEXT("sigue dibujando costa"), State.Strokes.Num() > 0);
		bool bAllFinite = true;
		for (const FMapStroke& Stroke : State.Strokes)
		{
			for (const FVector2D& P : Stroke.Points)
			{
				bAllFinite &= FMath::IsFinite(P.X) && FMath::IsFinite(P.Y);
			}
		}
		TestTrue(TEXT("puntos finitos"), bAllFinite);
		TestEqual(TEXT("una marca de visita por punto de costa"), State.Coverage[0].Visited.Num(), State.Coverage[0].Coast.Num());
		TestEqual(TEXT("una confirmación por punto de boceto"), State.Sketches[0].Confirmed.Num(), State.Sketches[0].Points.Num());
		TestTrue(TEXT("cuenta lo recorrido"), Loaded.GetIslandCoverage(0) > 0.0f);
		TestTrue(TEXT("y confirma el boceto"), State.Sketches[0].ConfirmedFraction() > 0.0f);
	});

	It("traza la costa real de las islas del archipiélago", [this]()
	{
		const FTerrainDensity Density(FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed));
		FCartographyModel Model(Seed);
		for (int32 Island = 0; Island < Density.GetLayout().Islands.Num(); ++Island)
		{
			const TArray<FVector2D> Coast = FCoastlineTrace::TraceIsland(Density, Island, 96);
			const FIslandDesc& Desc = Density.GetLayout().Islands[Island];
			// Los Dientes son islotes sueltos: parte de los rayos pasa entre ellos sin tocar tierra.
			const int32 MinRays = Desc.Archetype == EIslandArchetype::Teeth ? 48 : 80;
			TestTrue(FString::Printf(TEXT("%s: los rayos tocan tierra (%d)"), LexToString(Desc.Archetype), Coast.Num()), Coast.Num() >= MinRays);
			float Worst = 0.0f;
			for (const FVector2D& P : Coast)
			{
				Worst = FMath::Max(Worst, FMath::Abs(Density.SampleColumn(static_cast<float>(P.X), static_cast<float>(P.Y)).Height));
			}
			TestTrue(FString::Printf(TEXT("%s: en la orilla (%.2f m)"), LexToString(Desc.Archetype), Worst), Worst < 3.0f);
			Model.RegisterIslandCoast(Island, Coast);
		}

		// La distancia a la costa de referencia sirve para decidir cuándo se dibuja.
		const FIslandDesc& First = Density.GetLayout().Islands[0];
		int32 Nearest = INDEX_NONE;
		const float AtCenter = Model.DistanceToReferenceCoast(First.Center, Nearest);
		TestEqual(TEXT("Isla del centro"), Nearest, 0);
		TestTrue(TEXT("El centro está lejos de la orilla"), AtCenter > FCartographyModel::RecordDistance);
	});
}

#endif
