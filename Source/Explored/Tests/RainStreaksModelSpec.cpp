#include "Misc/AutomationTest.h"

#include "Weather/RainStreaksModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FRainStreaksModelSpec, "Explored.Weather.RainStreaks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FRainStreaksModelSpec)

void FRainStreaksModelSpec::Define()
{
	Describe(TEXT("CountFor"), [this]()
	{
		It(TEXT("no dibuja nada sin lluvia y escala con la intensidad"), [this]()
		{
			const FRainStreaksModel Model(FRainStreaksTuning(), 7u);
			TestEqual(TEXT("sin lluvia"), Model.CountFor(0.0f), 0);
			TestEqual(TEXT("bajo el mínimo"), Model.CountFor(0.04f), 0);
			TestEqual(TEXT("NaN"), Model.CountFor(NAN), 0);
			TestEqual(TEXT("mitad"), Model.CountFor(0.5f), 450);
			TestEqual(TEXT("tope"), Model.CountFor(3.0f), 900);
		});
	});

	Describe(TEXT("Evaluate"), [this]()
	{
		It(TEXT("deja todos los trazos en la caja alrededor de la cámara"), [this]()
		{
			const FRainStreaksTuning Tuning;
			const FRainStreaksModel Model(Tuning, 11u);
			const FVector Camera(12345.0, -6789.0, 800.0);
			for (const double Time : { 0.0, 3.7, 125.0 })
			{
				const FRainStreaksFrame Frame = Model.Evaluate(Time, 1.0f, 1.0f, FVector2D(1.0, 0.3), Camera);
				TestEqual(TEXT("cuántos"), Frame.Positions.Num(), Tuning.MaxStreaks);
				for (const FVector& P : Frame.Positions)
				{
					if (FVector2D::Distance(FVector2D(P.X, P.Y), FVector2D(Camera.X, Camera.Y)) < Tuning.NearClearCm - 1.0)
					{
						AddError(TEXT("trazo dentro del hueco de la cámara"));
						return;
					}
					if (FMath::Abs(P.X - Camera.X) > Tuning.HalfExtentCm + 1.0 || FMath::Abs(P.Y - Camera.Y) > Tuning.HalfExtentCm + 1.0
						|| P.Z < Camera.Z - Tuning.HeightCm / 3.0 - 1.0 || P.Z > Camera.Z + Tuning.HeightCm * 2.0 / 3.0 + 1.0)
					{
						AddError(FString::Printf(TEXT("trazo fuera de la caja: %.1f %.1f %.1f"), P.X, P.Y, P.Z));
						return;
					}
				}
			}
		});

		It(TEXT("cae con el tiempo y se inclina a favor del viento"), [this]()
		{
			const FRainStreaksModel Model(FRainStreaksTuning(), 3u);
			const FRainStreaksFrame A = Model.Evaluate(10.0, 1.0f, 0.0f, FVector2D::ZeroVector, FVector::ZeroVector);
			const FRainStreaksFrame B = Model.Evaluate(10.05, 1.0f, 0.0f, FVector2D::ZeroVector, FVector::ZeroVector);
			int32 Fell = 0;
			for (int32 Index = 0; Index < A.Positions.Num(); ++Index)
			{
				Fell += B.Positions[Index].Z < A.Positions[Index].Z ? 1 : 0;
			}
			TestTrue(TEXT("casi todos bajan (los demás vuelven arriba)"), Fell > A.Positions.Num() * 9 / 10);
			TestTrue(TEXT("vertical sin viento"), A.Direction.Equals(FVector(0.0, 0.0, -1.0), 1e-6));
			const FRainStreaksFrame Windy = Model.Evaluate(10.0, 1.0f, 1.0f, FVector2D(0.0, 2.0), FVector::ZeroVector);
			TestTrue(TEXT("inclinado hacia +Y"), Windy.Direction.Y > 0.3 && Windy.Direction.Z < 0.0);
			TestTrue(TEXT("trazo más largo con más lluvia"),
				Model.Evaluate(1.0, 1.0f, 0.0f, FVector2D::ZeroVector, FVector::ZeroVector).LengthCm
				> Model.Evaluate(1.0, 0.2f, 0.0f, FVector2D::ZeroVector, FVector::ZeroVector).LengthCm);
		});

		It(TEXT("no viaja con la cámara: el mismo trazo sigue en su sitio si la cámara avanza poco"), [this]()
		{
			const FRainStreaksModel Model(FRainStreaksTuning(), 5u);
			const FRainStreaksFrame A = Model.Evaluate(2.0, 1.0f, 0.0f, FVector2D::ZeroVector, FVector::ZeroVector);
			const FRainStreaksFrame B = Model.Evaluate(2.0, 1.0f, 0.0f, FVector2D::ZeroVector, FVector(100.0, 0.0, 0.0));
			int32 Same = 0;
			for (int32 Index = 0; Index < A.Positions.Num(); ++Index)
			{
				Same += FMath::IsNearlyEqual(A.Positions[Index].X, B.Positions[Index].X, 0.01) ? 1 : 0;
			}
			TestTrue(TEXT("la mayoría no se mueve"), Same > A.Positions.Num() * 9 / 10);
		});

		It(TEXT("es determinista y Wrap repliega en el intervalo"), [this]()
		{
			const FRainStreaksModel A(FRainStreaksTuning(), 9u);
			const FRainStreaksModel B(FRainStreaksTuning(), 9u);
			const FRainStreaksFrame FA = A.Evaluate(4.2, 0.7f, 0.5f, FVector2D(1.0, 0.0), FVector(5.0, 6.0, 7.0));
			const FRainStreaksFrame FB = B.Evaluate(4.2, 0.7f, 0.5f, FVector2D(1.0, 0.0), FVector(5.0, 6.0, 7.0));
			TestTrue(TEXT("mismas posiciones"), FA.Positions == FB.Positions);
			TestEqual(TEXT("dentro"), FRainStreaksModel::Wrap(5.0, 0.0, 10.0), 5.0);
			TestEqual(TEXT("por arriba"), FRainStreaksModel::Wrap(12.0, 0.0, 10.0), -8.0);
			TestEqual(TEXT("por abajo"), FRainStreaksModel::Wrap(-12.0, 0.0, 10.0), 8.0);
		});
	});
}

#endif
