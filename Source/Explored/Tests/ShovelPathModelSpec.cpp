#include "Misc/AutomationTest.h"

#include "WorldGen/ShovelPathModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ShovelPathSpecDetail
{
	/** Ladera que sube hacia +X con la pendiente dada (grados); sólido debajo. */
	struct FSlope
	{
		double Grade = 0.0;
		explicit FSlope(double Deg) : Grade(FMath::Tan(FMath::DegreesToRadians(Deg))) {}
		double Height(double X, double) const { return Grade * X; }
		float Density(const FVector& P) const { return static_cast<float>(P.Z - Grade * P.X); }
	};

	/** Superficie del terreno editado en (X, Y): bisección sobre la densidad. */
	double SurfaceZ(const FTerrainEditModel& Model, double X, double Y, FTerrainEditModel::FBaseDensity Base)
	{
		double Lo = -10.0;
		double Hi = 10.0;
		for (int32 I = 0; I < 60; ++I)
		{
			const double Mid = 0.5 * (Lo + Hi);
			(Model.Density(FVector(X, Y, Mid), Base) > 0.0f ? Hi : Lo) = Mid;
		}
		return 0.5 * (Lo + Hi);
	}

	FShovelPathRequest Request(const FVector& Start, const FVector& End, ETerrainMaterial Material = ETerrainMaterial::Tierra)
	{
		FShovelPathRequest R;
		R.Start = Start;
		R.End = End;
		R.Material = Material;
		R.ToolTier = 1;
		return R;
	}

	/** Da todos los golpes del tramo; devuelve el último resultado. */
	FShovelPathHitResult Finish(FShovelPathJob& Job, FTerrainEditModel& Terrain, FTerrainEditModel::FBaseDensity Base)
	{
		FShovelPathHitResult Last;
		for (int32 I = 0; I < 50 && !Job.bFinished; ++I)
		{
			Last = FShovelPathModel::ApplyHit(Job, Terrain, Base);
			if (Last.Edit.bRejected)
			{
				break;
			}
		}
		return Last;
	}
}

BEGIN_DEFINE_SPEC(FShovelPathModelSpec, "Explored.ShovelPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FShovelPathModelSpec)

void FShovelPathModelSpec::Define()
{
	using namespace ShovelPathSpecDetail;

	Describe("tramo (biblia 02 §3)", [this]()
	{
		It("6 golpes por tramo de 4 m, en proporción, y nunca más de 4 m por uso", [this]()
		{
			auto Flat = [](double, double) { return 0.0; };
			const FShovelPathPlan Full = FShovelPathModel::Plan(Request(FVector::ZeroVector, FVector(4.0, 0.0, 0.0)), Flat);
			TestTrue(TEXT("válido"), Full.bValid);
			TestEqual(TEXT("4 m: 6 golpes"), Full.HitsRequired, 6);
			TestEqual(TEXT("2 m: 3 golpes"), FShovelPathModel::Plan(Request(FVector::ZeroVector, FVector(0.0, 2.0, 0.0)), Flat).HitsRequired, 3);
			TestEqual(TEXT("1 m: 2 golpes"), FShovelPathModel::Plan(Request(FVector::ZeroVector, FVector(1.0, 0.0, 0.0)), Flat).HitsRequired, 2);
			TestEqual(TEXT("medio paso: 1 golpe"), FShovelPathModel::Plan(Request(FVector::ZeroVector, FVector(0.25, 0.0, 0.0)), Flat).HitsRequired, 1);
			const FShovelPathPlan Long = FShovelPathModel::Plan(Request(FVector(1.0, 1.0, 0.0), FVector(1.0, 11.0, 0.0)), Flat);
			TestEqual(TEXT("10 m pedidos: 4 m"), Long.Length, 4.0, 1.0e-9);
			TestEqual(TEXT("en la misma dirección"), Long.End, FVector(1.0, 5.0, 0.0), 1.0e-9f);
			TestEqual(TEXT("y 6 golpes"), Long.HitsRequired, 6);
		});

		It("peticiones degeneradas no dan tramo", [this]()
		{
			auto Flat = [](double, double) { return 0.0; };
			auto Broken = [](double, double) { return static_cast<double>(NAN); };
			TestFalse(TEXT("demasiado corto"), FShovelPathModel::Plan(Request(FVector::ZeroVector, FVector(0.2, 0.0, 0.0)), Flat).bValid);
			TestFalse(TEXT("vertical"), FShovelPathModel::Plan(Request(FVector::ZeroVector, FVector(0.0, 0.0, 3.0)), Flat).bValid);
			TestFalse(TEXT("punto no finito"), FShovelPathModel::Plan(Request(FVector(NAN, 0.0, 0.0), FVector(3.0, 0.0, 0.0)), Flat).bValid);
			TestFalse(TEXT("suelo no finito"), FShovelPathModel::Plan(Request(FVector::ZeroVector, FVector(3.0, 0.0, 0.0)), Broken).bValid);
			TestEqual(TEXT("sin pasadas"), FShovelPathModel::Strokes(FShovelPathPlan()).Num(), 0);
		});

		It("la pendiente se recorta a 15° girando sobre el punto medio; una suave se respeta", [this]()
		{
			const FSlope Steep(30.0);
			const FShovelPathPlan P = FShovelPathModel::Plan(Request(FVector::ZeroVector, FVector(4.0, 0.0, 0.0)),
				[&Steep](double X, double Y) { return Steep.Height(X, Y); });
			TestEqual(TEXT("el suelo tiene 30°"), P.GroundSlopeDeg, 30.0, 1.0e-9);
			TestEqual(TEXT("el camino, 15°"), P.PathSlopeDeg, 15.0, 1.0e-9);
			TestEqual(TEXT("mismo punto medio"), 0.5 * (P.Start.Z + P.End.Z), Steep.Height(2.0, 0.0), 1.0e-9);
			const FSlope Mild(10.0);
			const FShovelPathPlan Q = FShovelPathModel::Plan(Request(FVector(4.0, 0.0, 0.0), FVector::ZeroVector),
				[&Mild](double X, double Y) { return Mild.Height(X, Y); });
			TestEqual(TEXT("10° se queda en 10°, también bajando"), Q.PathSlopeDeg, 10.0, 1.0e-9);
			TestEqual(TEXT("extremo alto sin tocar"), Q.Start.Z, Mild.Height(4.0, 0.0), 1.0e-9);
			const FSlope Limit(15.0);
			TestEqual(TEXT("15° justos"), FShovelPathModel::Plan(Request(FVector::ZeroVector, FVector(4.0, 0.0, 0.0)),
				[&Limit](double X, double Y) { return Limit.Height(X, Y); }).PathSlopeDeg, 15.0, 1.0e-9);
		});
	});

	Describe("aplanar y compactar", [this]()
	{
		It("en una ladera de 30° deja la franja a 15° y la compacta al terminar", [this]()
		{
			const FSlope Steep(30.0);
			auto Base = [&Steep](const FVector& P) { return Steep.Density(P); };
			FTerrainEditModel Terrain;
			FShovelPathJob Job;
			Job.Plan = FShovelPathModel::Plan(Request(FVector(0.0, 0.0, 0.0), FVector(4.0, 0.0, 0.0)),
				[&Steep](double X, double Y) { return Steep.Height(X, Y); });
			for (int32 I = 0; I < Job.Plan.HitsRequired - 1; ++I)
			{
				const FShovelPathHitResult R = FShovelPathModel::ApplyHit(Job, Terrain, Base);
				TestFalse(TEXT("a medias no es camino"), R.bFinished || Terrain.IsPath(2.0, 0.1));
			}
			const FShovelPathHitResult Last = FShovelPathModel::ApplyHit(Job, Terrain, Base);
			TestTrue(TEXT("terminado"), Last.bFinished && Job.bFinished);

			const double Z1 = SurfaceZ(Terrain, 1.0, 0.1, Base);
			const double Z3 = SurfaceZ(Terrain, 3.0, 0.1, Base);
			const double Deg = FMath::RadiansToDegrees(FMath::Atan2(Z3 - Z1, 2.0));
			TestTrue(FString::Printf(TEXT("pendiente del camino %.2f° ≤ 15° + 1°"), Deg), Deg <= 16.0 && Deg >= 13.0);
			TestEqual(TEXT("a lo ancho, plano"), SurfaceZ(Terrain, 2.0, 0.6, Base), SurfaceZ(Terrain, 2.0, -0.6, Base), 0.03);
			TestTrue(TEXT("camino en el eje"), Terrain.IsPath(2.0, 0.1));
			TestTrue(TEXT("camino a 0,6 m del eje"), Terrain.IsPath(2.0, -0.6));
			TestFalse(TEXT("a 1 m ya no"), Terrain.IsPath(2.0, 1.1));
			TestFalse(TEXT("más allá del final, no"), Terrain.IsPath(5.0, 0.1));
		});

		It("sobre el camino terminado se gasta un 15 % menos de resistencia", [this]()
		{
			auto Base = [](const FVector& P) { return static_cast<float>(P.Z); };
			FTerrainEditModel Terrain;
			FShovelPathJob Job;
			Job.Plan = FShovelPathModel::Plan(Request(FVector(0.0, 0.0, 0.0), FVector(0.0, 3.0, 0.0)), [](double, double) { return 0.0; });
			TestEqual(TEXT("antes"), FShovelPathModel::StaminaFactor(Terrain, 0.1, 1.5), 1.0f);
			Finish(Job, Terrain, Base);
			TestEqual(TEXT("sobre el camino"), FShovelPathModel::StaminaFactor(Terrain, 0.1, 1.5), 0.85f);
			TestEqual(TEXT("fuera"), FShovelPathModel::StaminaFactor(Terrain, 2.0, 1.5), 1.0f);
			// Picar encima le quita la compactación a esas columnas.
			FPickaxeHit Hit;
			Hit.ImpactPoint = FVector(0.1, 1.5, 0.0);
			Terrain.Pickaxe(Hit, Base);
			TestEqual(TEXT("picado, ya no es camino"), FShovelPathModel::StaminaFactor(Terrain, 0.1, 1.5), 1.0f);
		});

		It("lo que sobra sale como tierra suelta o arena; en llano no sobra nada", [this]()
		{
			// Montículo de 40 cm en medio del tramo.
			auto Mound = [](const FVector& P)
			{
				const double R = FVector2D(P.X - 2.0, P.Y).Size();
				return static_cast<float>(P.Z - 0.4 * FMath::Max(0.0, 1.0 - R / 1.2));
			};
			for (const ETerrainMaterial Material : { ETerrainMaterial::Tierra, ETerrainMaterial::Arena })
			{
				FTerrainEditModel Terrain;
				FShovelPathJob Job;
				Job.Plan = FShovelPathModel::Plan(Request(FVector::ZeroVector, FVector(4.0, 0.0, 0.0), Material), [](double, double) { return 0.0; });
				const FShovelPathHitResult Last = Finish(Job, Terrain, Mound);
				TestEqual(TEXT("objeto"), Last.LootItem, FString(Material == ETerrainMaterial::Arena ? TEXT("arena") : TEXT("tierra_suelta")));
				TestTrue(FString::Printf(TEXT("sobra tierra (%d)"), Last.LootUnits), Last.LootUnits > 0);
				TestEqual(TEXT("el sobrante se entrega"), Job.Spare, 0.0);
			}
			FTerrainEditModel Terrain;
			FShovelPathJob Job;
			Job.Plan = FShovelPathModel::Plan(Request(FVector::ZeroVector, FVector(4.0, 0.0, 0.0)), [](double, double) { return 0.0; });
			const FShovelPathHitResult Last = Finish(Job, Terrain, [](const FVector& P) { return static_cast<float>(P.Z); });
			TestEqual(TEXT("llano: nada"), Last.LootUnits, 0);
		});

		It("la pala no puede con la caliza: el golpe no cuenta; y terminado, más golpes no hacen nada", [this]()
		{
			auto Base = [](const FVector& P) { return static_cast<float>(P.Z); };
			FTerrainEditModel Terrain;
			FShovelPathJob Rock;
			Rock.Plan = FShovelPathModel::Plan(Request(FVector::ZeroVector, FVector(4.0, 0.0, 0.0), ETerrainMaterial::Caliza), [](double, double) { return 0.0; });
			TestTrue(TEXT("rechazado"), FShovelPathModel::ApplyHit(Rock, Terrain, Base).Edit.bRejected);
			TestEqual(TEXT("sin contar"), Rock.HitsDone, 0);
			TestTrue(TEXT("terreno intacto"), Terrain.IsEmpty());

			FShovelPathJob Job;
			Job.Plan = FShovelPathModel::Plan(Request(FVector::ZeroVector, FVector(2.0, 0.0, 0.0)), [](double, double) { return 0.0; });
			Finish(Job, Terrain, Base);
			const FShovelPathHitResult Extra = FShovelPathModel::ApplyHit(Job, Terrain, Base);
			TestFalse(TEXT("nada más"), Extra.bFinished || Extra.Edit.Changed());
			TestEqual(TEXT("golpes"), Job.HitsDone, 3);
		});

		It("mismo tramo y mismo suelo, mismo resultado (determinismo)", [this]()
		{
			const FSlope Steep(24.0);
			auto Base = [&Steep](const FVector& P) { return Steep.Density(P) + 0.05f * FMath::Sin(static_cast<float>(3.0 * P.Y)); };
			auto Run = [&](FTerrainEditModel& Terrain)
			{
				FShovelPathJob Job;
				Job.Plan = FShovelPathModel::Plan(Request(FVector(-1.0, 2.0, 0.0), FVector(2.0, 4.5, 0.0)),
					[&Steep](double X, double Y) { return Steep.Height(X, Y); });
				return Finish(Job, Terrain, Base).LootUnits;
			};
			FTerrainEditModel A;
			FTerrainEditModel B;
			TestEqual(TEXT("mismo sobrante"), Run(A), Run(B));
			TestTrue(TEXT("mismo terreno"), A == B);
			TestTrue(TEXT("mismo guardado"), A.ToValue() == B.ToValue());
		});
	});
}

#endif
