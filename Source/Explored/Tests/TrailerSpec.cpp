#include "Misc/AutomationTest.h"

#include "Debug/TrailerModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FTrailerSpec, "Explored.Debug.Trailer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	static FTrailerShot MakeShot(const FString& Name, float Seconds, TArray<FVector> Eye, TArray<FVector> Look)
	{
		FTrailerShot Shot;
		Shot.Name = Name;
		Shot.DurationSeconds = Seconds;
		Shot.EyeM = MoveTemp(Eye);
		Shot.LookM = MoveTemp(Look);
		return Shot;
	}
	static FTrailerShot Framed(FTrailerShot Shot, int32 Fps = 30)
	{
		Shot.FrameCount = FMath::RoundToInt(Shot.DurationSeconds * Fps);
		return Shot;
	}
	static FTrailerGroundFn Flat(float Height)
	{
		return [Height](double, double) { return Height; };
	}
END_DEFINE_SPEC(FTrailerSpec)

void FTrailerSpec::Define()
{
	Describe("BuildTimeline", [this]()
	{
		It("reparte los fotogramas del guion de 75 s: 2250 a 30 fps, sin huecos", [this]()
		{
			const float Durations[] = { 4.0f, 8.0f, 2.7f, 2.3f, 3.0f, 2.0f, 5.0f, 5.0f, 5.0f, 5.0f, 8.0f, 18.0f, 7.0f };
			TArray<FTrailerShot> Shots;
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Durations); ++Index)
			{
				Shots.Add(MakeShot(FString::Printf(TEXT("p%d"), Index), Durations[Index], { FVector::ZeroVector }, { FVector(1.0, 0.0, 0.0) }));
			}
			FTrailerTimeline Timeline;
			FString Error;
			TestTrue(TEXT("válida"), FTrailerModel::BuildTimeline(Shots, 30, Timeline, Error));
			TestEqual(TEXT("total"), Timeline.TotalFrames, 2250);
			int32 Expected = 0;
			for (const FTrailerShot& Shot : Timeline.Shots)
			{
				TestEqual(TEXT("empieza donde acaba el anterior"), Shot.StartFrame, Expected);
				Expected += Shot.FrameCount;
			}
			TestEqual(TEXT("2,7 s"), Timeline.Shots[2].FrameCount, 81);
			TestEqual(TEXT("primer fotograma del final"), FTrailerModel::FindShot(Timeline, 1710), 11);
			TestEqual(TEXT("último fotograma"), FTrailerModel::FindShot(Timeline, 2249), 12);
			TestEqual(TEXT("fuera"), FTrailerModel::FindShot(Timeline, 2250), INDEX_NONE);
		});

		It("rechaza planos rotos (casos negativos)", [this]()
		{
			FTrailerTimeline Timeline;
			FString Error;
			TestFalse(TEXT("sin ojo"), FTrailerModel::BuildTimeline({ MakeShot(TEXT("a"), 1.0f, {}, { FVector::ZeroVector }) }, 30, Timeline, Error));
			FTrailerShot Fades = MakeShot(TEXT("b"), 1.0f, { FVector::ZeroVector }, { FVector(1.0, 0.0, 0.0) });
			Fades.FadeInSeconds = 0.8f;
			Fades.FadeOutSeconds = 0.8f;
			TestFalse(TEXT("fundidos más largos que el plano"), FTrailerModel::BuildTimeline({ Fades }, 30, Timeline, Error));
			const FTrailerShot Ok = MakeShot(TEXT("c"), 1.0f, { FVector::ZeroVector }, { FVector(1.0, 0.0, 0.0) });
			TestFalse(TEXT("nombre repetido"), FTrailerModel::BuildTimeline({ Ok, Ok }, 30, Timeline, Error));
			TestFalse(TEXT("fps 0"), FTrailerModel::BuildTimeline({ Ok }, 0, Timeline, Error));
			FTrailerShot Late = Ok;
			Late.HoursTo = 25.0f;
			TestFalse(TEXT("hora fuera de rango"), FTrailerModel::BuildTimeline({ Late }, 30, Timeline, Error));
		});
	});

	Describe("EaseProgress", [this]()
	{
		It("va de 0 a 1, sube siempre, es simétrica y con E = 0 es lineal", [this]()
		{
			TestEqual(TEXT("inicio"), FTrailerModel::EaseProgress(0.0f, 0.25f), 0.0f, 1.0e-6f);
			TestEqual(TEXT("final"), FTrailerModel::EaseProgress(1.0f, 0.25f), 1.0f, 1.0e-6f);
			TestEqual(TEXT("mitad"), FTrailerModel::EaseProgress(0.5f, 0.25f), 0.5f, 1.0e-6f);
			TestEqual(TEXT("lineal"), FTrailerModel::EaseProgress(0.3f, 0.0f), 0.3f, 1.0e-6f);
			float Previous = 0.0f;
			for (int32 Step = 1; Step <= 100; ++Step)
			{
				const float A = Step / 100.0f;
				const float S = FTrailerModel::EaseProgress(A, 0.25f);
				TestTrue(TEXT("monótona"), S >= Previous);
				TestEqual(TEXT("simétrica"), S + FTrailerModel::EaseProgress(1.0f - A, 0.25f), 1.0f, 1.0e-5f);
				Previous = S;
			}
		});

		It("la velocidad no salta al acabar la aceleración", [this]()
		{
			const float E = 0.2f;
			const float H = 1.0e-3f;
			const float Before = (FTrailerModel::EaseProgress(E, E) - FTrailerModel::EaseProgress(E - H, E)) / H;
			const float After = (FTrailerModel::EaseProgress(E + H, E) - FTrailerModel::EaseProgress(E, E)) / H;
			TestEqual(TEXT("velocidad continua"), Before, After, 0.01f);
		});
	});

	Describe("Trayectoria", [this]()
	{
		It("el Catmull-Rom pasa por los puntos", [this]()
		{
			const TArray<FVector> Points = { FVector(0.0, 0.0, 0.0), FVector(10.0, 5.0, 0.0), FVector(20.0, 0.0, 2.0) };
			TestTrue(TEXT("primero"), FTrailerModel::CatmullRom(Points, 0.0).Equals(Points[0], 1.0e-9));
			TestTrue(TEXT("segundo"), FTrailerModel::CatmullRom(Points, 1.0).Equals(Points[1], 1.0e-9));
			TestTrue(TEXT("último"), FTrailerModel::CatmullRom(Points, 2.0).Equals(Points[2], 1.0e-9));
		});

		It("recorre la curva a velocidad constante aunque los puntos estén desiguales", [this]()
		{
			const TArray<FVector> Points = { FVector(0.0, 0.0, 0.0), FVector(2.0, 0.0, 0.0), FVector(40.0, 0.0, 0.0) };
			double MinStep = 1.0e9;
			double MaxStep = 0.0;
			FVector Previous = FTrailerModel::PathPoint(Points, 0.0);
			for (int32 Step = 1; Step <= 100; ++Step)
			{
				const FVector Point = FTrailerModel::PathPoint(Points, Step / 100.0);
				const double Length = (Point - Previous).Size();
				MinStep = FMath::Min(MinStep, Length);
				MaxStep = FMath::Max(MaxStep, Length);
				Previous = Point;
			}
			TestTrue(TEXT("pasos iguales (±2 %)"), MaxStep - MinStep < 0.02 * MaxStep);
			TestTrue(TEXT("llega al final"), Previous.Equals(Points.Last(), 1.0e-6));
		});

		It("el slerp da direcciones unitarias y gira a velocidad angular constante", [this]()
		{
			const FVector A(1.0, 0.0, 0.0);
			const FVector B(0.0, 3.0, 0.0);
			const FVector Mid = FTrailerModel::SlerpDirection(A, B, 0.5);
			TestEqual(TEXT("unitaria"), Mid.Size(), 1.0, 1.0e-9);
			TestTrue(TEXT("a 45 grados"), Mid.Equals(FVector(FMath::Sqrt(0.5), FMath::Sqrt(0.5), 0.0), 1.0e-9));
			const FVector Opposite = FTrailerModel::SlerpDirection(A, -A, 0.5);
			TestEqual(TEXT("opuestas: gira por la horizontal"), Opposite.Z, 0.0, 1.0e-9);
			TestEqual(TEXT("opuestas: a 90 grados"), FVector::DotProduct(Opposite, A), 0.0, 1.0e-9);
		});
	});

	Describe("Evaluate", [this]()
	{
		It("con bFollowGround el ojo va a su altura sobre el suelo y nunca por debajo del margen", [this]()
		{
			FTrailerShot Shot = Framed(MakeShot(TEXT("a"), 1.0f, { FVector(0.0, 0.0, 1.7), FVector(10.0, 0.0, 1.7) }, { FVector(100.0, 0.0, 1.7) }));
			Shot.bFollowGround = true;
			const FTrailerGroundFn Slope = [](double X, double) { return static_cast<float>(0.1 * X); };
			const FTrailerCameraSample End = FTrailerModel::Evaluate(Shot, Shot.FrameCount - 1, 30, Slope);
			TestEqual(TEXT("1,7 m sobre la pendiente"), End.EyeM.Z, 1.0 + 1.7, 1.0e-3);

			FTrailerShot Buried = Framed(MakeShot(TEXT("b"), 1.0f, { FVector(0.0, 0.0, -5.0) }, { FVector(10.0, 0.0, 0.0) }));
			const FTrailerCameraSample Lifted = FTrailerModel::Evaluate(Buried, 0, 30, Flat(2.0f));
			TestEqual(TEXT("margen sobre el suelo"), Lifted.EyeM.Z, 2.0 + FTrailerModel::MinClearanceM, 1.0e-4);
		});

		It("interpola hora y FOV, mira al punto y funde desde y hacia negro", [this]()
		{
			FTrailerShot Shot = Framed(MakeShot(TEXT("a"), 2.0f, { FVector(0.0, 0.0, 10.0) }, { FVector(10.0, 10.0, 10.0) }));
			Shot.HoursFrom = 6.0f;
			Shot.HoursTo = 7.0f;
			Shot.FovFrom = 60.0f;
			Shot.FovTo = 80.0f;
			Shot.FadeInSeconds = 0.5f;
			Shot.FadeOutSeconds = 1.0f;
			const FTrailerCameraSample First = FTrailerModel::Evaluate(Shot, 0, 30, Flat(0.0f));
			const FTrailerCameraSample Last = FTrailerModel::Evaluate(Shot, Shot.FrameCount - 1, 30, Flat(0.0f));
			const FTrailerCameraSample Middle = FTrailerModel::Evaluate(Shot, 20, 30, Flat(0.0f));
			TestEqual(TEXT("hora inicial"), First.Hours, 6.0f, 1.0e-4f);
			TestEqual(TEXT("hora final"), Last.Hours, 7.0f, 1.0e-4f);
			TestEqual(TEXT("FOV final"), Last.Fov, 80.0f, 1.0e-3f);
			TestEqual(TEXT("guiñada a 45"), First.Yaw, 45.0f, 1.0e-3f);
			TestEqual(TEXT("negro al empezar"), First.Black, 1.0f, 1.0e-4f);
			TestEqual(TEXT("limpio en medio"), Middle.Black, 0.0f, 1.0e-4f);
			TestEqual(TEXT("negro en el último fotograma"), Last.Black, 1.0f, 1.0e-4f);
		});
	});

	Describe("CheckMotion", [this]()
	{
		It("un travelling suave de 3 m/s con paneo de 40 grados pasa", [this]()
		{
			FTrailerShot Shot = Framed(MakeShot(TEXT("suave"), 4.0f, { FVector(0.0, 0.0, 1.7), FVector(6.0, 1.0, 1.7), FVector(12.0, 0.0, 1.7) },
				{ FVector(50.0, -20.0, 2.0), FVector(50.0, 30.0, 2.0) }));
			Shot.bFollowGround = true;
			const TArray<FString> Issues = FTrailerModel::CheckMotion(Shot, 30, Flat(1.0f), FTrailerLimits());
			TestEqual(TEXT("sin problemas"), Issues.Num(), 0);
		});

		It("falla si el ojo salta dentro del plano (caso negativo)", [this]()
		{
			const FTrailerShot Shot = Framed(MakeShot(TEXT("salto"), 1.0f, { FVector(0.0, 0.0, 5.0), FVector(300.0, 0.0, 5.0) }, { FVector(1000.0, 0.0, 5.0) }));
			const TArray<FString> Issues = FTrailerModel::CheckMotion(Shot, 30, Flat(0.0f), FTrailerLimits());
			TestTrue(TEXT("detecta el salto"), Issues.Num() > 0);
		});

		It("falla si la mirada gira de golpe (caso negativo)", [this]()
		{
			FTrailerShot Shot = Framed(MakeShot(TEXT("giro"), 0.5f, { FVector(0.0, 0.0, 5.0) }, { FVector(10.0, 0.0, 5.0), FVector(-10.0, 1.0, 5.0) }));
			Shot.EaseFraction = 0.0f;
			const TArray<FString> Issues = FTrailerModel::CheckMotion(Shot, 30, Flat(0.0f), FTrailerLimits());
			TestTrue(TEXT("detecta el giro"), Issues.Num() > 0);
		});

		It("un escalón de 3 m en el suelo bajo un plano a ras de suelo da tirón (caso negativo)", [this]()
		{
			FTrailerShot Shot = Framed(MakeShot(TEXT("escalon"), 4.0f, { FVector(0.0, 0.0, 1.0), FVector(8.0, 0.0, 1.0) }, { FVector(100.0, 0.0, 1.0) }));
			Shot.bFollowGround = true;
			const FTrailerGroundFn Step = [](double X, double) { return X > 4.0 ? 3.0f : 0.0f; };
			const TArray<FString> Issues = FTrailerModel::CheckMotion(Shot, 30, Step, FTrailerLimits());
			TestTrue(TEXT("detecta el tirón"), Issues.Num() > 0);
		});
	});

	Describe("Captura", [this]()
	{
		It("ordena normales, interfaz y haz, y no mezcla el final con nada", [this]()
		{
			TArray<FTrailerShot> Shots = {
				MakeShot(TEXT("haz"), 1.0f, { FVector::ZeroVector }, { FVector(1.0, 0.0, 0.0) }),
				MakeShot(TEXT("hud"), 1.0f, { FVector::ZeroVector }, { FVector(1.0, 0.0, 0.0) }),
				MakeShot(TEXT("mar"), 1.0f, { FVector::ZeroVector }, { FVector(1.0, 0.0, 0.0) }),
				MakeShot(TEXT("final"), 1.0f, { FVector::ZeroVector }, { FVector(1.0, 0.0, 0.0) }),
			};
			Shots[0].Kind = ETrailerShotKind::Beam;
			Shots[1].bShowUI = true;
			Shots[3].Kind = FTrailerModel::ParseKind(TEXT("finale"));
			FTrailerTimeline Timeline;
			FString Error;
			TestTrue(TEXT("válida"), FTrailerModel::BuildTimeline(Shots, 30, Timeline, Error));
			TArray<int32> Order;
			TestTrue(TEXT("sin el final"), FTrailerModel::CaptureOrder(Timeline, { 0, 1, 2 }, Order, Error));
			TestTrue(TEXT("orden"), Order == TArray<int32>({ 2, 1, 0 }));
			TestFalse(TEXT("final mezclado (caso negativo)"), FTrailerModel::CaptureOrder(Timeline, { 2, 3 }, Order, Error));
			TestTrue(TEXT("final solo"), FTrailerModel::CaptureOrder(Timeline, { 3 }, Order, Error));
		});

		It("encuentra fotogramas que faltan y repetidos, y nombra por índice", [this]()
		{
			TArray<int32> Missing;
			TArray<int32> Repeated;
			FTrailerModel::FindGaps({ 0, 1, 3, 3, 4 }, 0, 4, 1, Missing, Repeated);
			TestTrue(TEXT("falta el 2"), Missing == TArray<int32>({ 2 }));
			TestTrue(TEXT("el 3 repetido"), Repeated == TArray<int32>({ 3 }));
			FTrailerModel::FindGaps({ 0, 3, 6 }, 0, 8, 3, Missing, Repeated);
			TestEqual(TEXT("ensayo cada 3: completo"), Missing.Num(), 0);
			TestEqual(TEXT("nombre"), FTrailerModel::FrameFileName(42), FString(TEXT("f_00042.png")));
		});
	});
}

#endif
