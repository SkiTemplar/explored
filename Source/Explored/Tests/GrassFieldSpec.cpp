#include "Misc/AutomationTest.h"

#include "WorldGen/GrassFieldModel.h"
#include "WorldGen/GroundField.h"
#include "WorldGen/IslandPropsModel.h"
#include "Tests/GrassTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GrassFieldTest
{
	using namespace GrassTestWorld;

	void PutU32(TArray<uint8>& Out, uint32 V)
	{
		for (int32 B = 0; B < 4; ++B)
		{
			Out.Add(static_cast<uint8>((V >> (8 * B)) & 0xFF));
		}
	}

	void PutF32(TArray<uint8>& Out, float V)
	{
		uint32 Bits = 0;
		FMemory::Memcpy(&Bits, &V, sizeof(V));
		PutU32(Out, Bits);
	}

	/** Binario de 3 × 3 vértices (plano z = 0,5·x) con un punto de control de altura Probe. */
	TArray<uint8> MakeBytes(float Probe)
	{
		TArray<uint8> Out = {'E', 'X', 'G', 'F'};
		PutU32(Out, FGroundField::Version);
		PutU32(Out, 3);
		PutU32(Out, 3);
		PutF32(Out, 0.0f);
		PutF32(Out, 0.0f);
		PutF32(Out, 1.0f);
		PutU32(Out, 1);
		for (int32 I = 0; I < 3; ++I)
		{
			for (int32 J = 0; J < 3; ++J)
			{
				PutF32(Out, static_cast<float>(I));
				PutF32(Out, static_cast<float>(J));
				PutF32(Out, 0.5f * I);
			}
		}
		for (int32 V = 0; V < 9; ++V)
		{
			Out.Add(static_cast<uint8>(EGroundSurface::Grass));
		}
		for (int32 C = 0; C < 4; ++C)
		{
			Out.Add(static_cast<uint8>(C % 2));
		}
		PutF32(Out, 1.5f);
		PutF32(Out, 0.5f);
		PutF32(Out, Probe);
		return Out;
	}
}

BEGIN_DEFINE_SPEC(FGrassFieldSpec, "Explored.WorldGen.GrassField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FGrassFieldSpec)

void FGrassFieldSpec::Define()
{
	using namespace GrassFieldTest;

	Describe(TEXT("FGroundField"), [this]()
	{
		It(TEXT("da la altura y la normal del triángulo de la malla"), [this]()
		{
			const TSharedPtr<FGroundField> Ground = MakeGround();
			FGroundSample S;
			TestTrue(TEXT("dentro"), Ground->Sample(10.3, -7.9, S));
			TestEqual(TEXT("llano"), S.Height, 2.0f, 1e-4f);
			TestEqual(TEXT("normal"), S.Normal.Z, 1.0f, 1e-4f);
			TestTrue(TEXT("hierba"), S.Surface == EGroundSurface::Grass);
			TestTrue(TEXT("ladera"), Ground->Sample(10.0, -90.0, S) && FMath::IsNearlyEqual(S.Height, 17.0f, 1e-3f));
			TestTrue(TEXT("pendiente de 56°"), S.Normal.Z < 0.6f);
			TestFalse(TEXT("fuera"), Ground->Sample(150.0, 0.0, S));
			TestFalse(TEXT("NaN"), Ground->Sample(std::numeric_limits<double>::quiet_NaN(), 0.0, S));
		});

		It(TEXT("lee el binario y comprueba sus puntos de control"), [this]()
		{
			FGroundField Field;
			FString Error;
			TestTrue(TEXT("válido"), Field.Parse(MakeBytes(0.75f), Error));
			TestEqual(TEXT("controles"), Field.GetProbeCount(), 1);
			FGroundSample S;
			TestTrue(TEXT("muestra"), Field.Sample(1.5, 0.5, S) && FMath::IsNearlyEqual(S.Height, 0.75f, 1e-5f));
			FGroundField Bad;
			TestFalse(TEXT("control que no cuadra"), Bad.Parse(MakeBytes(0.9f), Error));
			TestFalse(TEXT("sin cargar"), Bad.IsValid());
			TArray<uint8> Bytes = MakeBytes(0.75f);
			Bytes[0] = 'Z';
			TestFalse(TEXT("cabecera"), Bad.Parse(Bytes, Error));
			Bytes = MakeBytes(0.75f);
			Bytes.Pop();
			TestFalse(TEXT("tamaño"), Bad.Parse(Bytes, Error));
		});
	});

	Describe(TEXT("Configuración"), [this]()
	{
		It(TEXT("los valores por defecto son válidos y los nombres van y vuelven"), [this]()
		{
			FGrassConfig Config = FGrassConfig::Defaults();
			FString Error;
			TestTrue(TEXT("válida"), Config.Validate(Error));
			TestEqual(TEXT("densidad máxima"), Config.MaxDensity(), FGrassConfig::ZonePalmarDensity);
			for (int32 K = 0; K < GrassKindCount; ++K)
			{
				EGrassKind Kind;
				TestTrue(TEXT("malla"), GrassKindFromName(LexToString(static_cast<EGrassKind>(K)), Kind) && static_cast<int32>(Kind) == K);
			}
			for (int32 C = 0; C < GrassContextCount; ++C)
			{
				EGrassContext Context;
				TestTrue(TEXT("sitio"), GrassContextFromName(LexToString(static_cast<EGrassContext>(C)), Context) && static_cast<int32>(Context) == C);
			}
			Config.Mixes[static_cast<int32>(EGrassContext::Common)] = FGrassMix();
			TestFalse(TEXT("mezcla vacía"), Config.Validate(Error));
			Config = FGrassConfig::Defaults();
			Config.Thinning.EndM = Config.Thinning.DenseM;
			TestFalse(TEXT("sin franja de adelgazamiento"), Config.Validate(Error));
			Config = FGrassConfig::Defaults();
			Config.WorkPerFrame = 0;
			TestFalse(TEXT("sin presupuesto de siembra"), Config.Validate(Error));
		});

		It(TEXT("la mezcla reparte por pesos"), [this]()
		{
			FGrassMix Mix;
			Mix.Weights[static_cast<int32>(EGrassKind::Fern)] = 1.0f;
			Mix.Weights[static_cast<int32>(EGrassKind::Clover)] = 3.0f;
			TestTrue(TEXT("helecho"), Mix.Pick(0.1f) == EGrassKind::Fern);
			TestTrue(TEXT("trébol"), Mix.Pick(0.5f) == EGrassKind::Clover);
			TestTrue(TEXT("extremo"), Mix.Pick(1.0f) == EGrassKind::Clover);
		});

		It(TEXT("las exclusiones salen de island_props y de los puntos de autor"), [this]()
		{
			FIslandProps Props;
			FIslandPropType Type;
			Type.Name = FName(TEXT("caja"));
			Type.Size = FVector(3.0, 4.0, 1.0);
			Props.Types.Add(Type.Name, Type);
			FIslandProp A;
			A.Id = FName(TEXT("a"));
			A.Type = Type.Name;
			A.Location = FVector2D(5.0, 5.0);
			Props.Props.Add(A);
			FIslandProp OnTop = A;
			OnTop.Id = FName(TEXT("encima"));
			OnTop.Anchor = A.Id;
			Props.Props.Add(OnTop);
			FIslandProp Palm = A;
			Palm.Id = FName(TEXT("p"));
			Palm.Type = FName(TEXT("palma"));
			Props.Props.Add(Palm);
			FIslandLayout Layout;
			Layout.Points.Add({FName(TEXT("raft_site")), FVector2D(-3.0, 0.0), 0.0f});
			const FGrassConfig Config = FGrassConfig::Defaults();
			const TArray<FGrassExclusion> Ex = FGrassWorld::ExclusionsFrom(Props, Layout, Config);
			TestEqual(TEXT("caja, palma y punto (sin la anclada)"), Ex.Num(), 3);
			TestEqual(TEXT("radio de la caja"), Ex[0].Radius, 2.5f + Config.ClearanceM, 1e-4f);
			TestEqual(TEXT("punto"), Ex[2].Radius, Config.PointClearanceM);
			TestEqual(TEXT("palmeras"), FGrassWorld::PalmsFrom(Props).Num(), 1);
		});
	});

	Describe(TEXT("Colocación"), [this]()
	{
		It(TEXT("es determinista por celda"), [this]()
		{
			const TSharedPtr<FGrassWorld> World = MakeWorld();
			const FGrassConfig Config = MakeConfig();
			TArray<FGrassInstance> A, B, C;
			FGrassFieldModel::GenerateCell(Config, *World, FIntPoint(2, 3), A);
			FGrassFieldModel::GenerateCell(Config, *World, FIntPoint(2, 3), B);
			FGrassFieldModel::GenerateCell(Config, *World, FIntPoint(3, 3), C);
			TestTrue(TEXT("hay matas"), A.Num() > 20);
			bool bSame = A.Num() == B.Num();
			for (int32 I = 0; bSame && I < A.Num(); ++I)
			{
				bSame = A[I].Location == B[I].Location && A[I].Kind == B[I].Kind && A[I].Scale == B[I].Scale;
			}
			TestTrue(TEXT("misma celda, misma salida"), bSame);
			TestTrue(TEXT("otra celda, otra salida"), C.Num() != A.Num() || C[0].Location != A[0].Location);
			for (const FGrassInstance& I : A)
			{
				TestTrue(TEXT("dentro de su celda"), I.Location.X >= 16.0 && I.Location.X < 24.0 && I.Location.Y >= 24.0 && I.Location.Y < 32.0);
			}
		});

		It(TEXT("respeta caminos, exclusiones, arena, mar y pendiente, apoyada en el suelo"), [this]()
		{
			const TSharedPtr<FGrassWorld> World = MakeWorld();
			const FGrassConfig Config = MakeConfig();
			const TArray<FGrassInstance> All = Generate(Config, *World, -13, 12, -13, 12);
			TestTrue(TEXT("hay matas"), All.Num() > 5000);
			int32 Bad = 0;
			for (const FGrassInstance& I : All)
			{
				const double HalfPath = 1.5 + Config.PathMarginM;
				Bad += FMath::Abs(I.Location.X) < HalfPath && FMath::Abs(I.Location.Y) < 60.0 ? 1 : 0;
				Bad += FVector2D::Distance(FVector2D(I.Location.X, I.Location.Y), FVector2D(30.0, 30.0)) < 5.0 ? 1 : 0;
				FGroundSample S;
				Bad += World->Ground->Sample(I.Location.X, I.Location.Y, S) && S.Surface == EGroundSurface::Grass ? 0 : 1;
				Bad += I.Location.X > 62.0 || I.Location.X < -80.0 || I.Location.Y < -80.0 ? 1 : 0;
				Bad += I.Location.Z > 2.0 - Config.SinkM + 1e-3 || I.Location.Z < 2.0 - 0.2 ? 1 : 0;
			}
			TestEqual(TEXT("matas en sitios prohibidos o mal apoyadas"), Bad, 0);
		});

		It(TEXT("densidad por zona, helechos al pie de las palmeras y flores en los claros"), [this]()
		{
			const TSharedPtr<FGrassWorld> World = MakeWorld();
			FGrassConfig Config = MakeConfig();
			const TArray<FGrassInstance> All = Generate(Config, *World, -13, 12, -13, 12);
			const double Area = UE_DOUBLE_PI * 10.0 * 10.0;
			const double DenseRate = CountIn(All, FVector2D(-40.0, 40.0), 10.0) / Area;
			const double PlainRate = CountIn(All, FVector2D(-40.0, -40.0), 10.0) / Area;
			TestTrue(TEXT("densa ≈ 3 / m²"), DenseRate > 2.5 && DenseRate < 3.5);
			TestTrue(TEXT("llano ≈ 1 / m²"), PlainRate > 0.8 && PlainRate < 1.2);
			int32 Ferns = 0, PalmRing = 0, Tall = 0, Edge = 0;
			for (const FGrassInstance& I : All)
			{
				PalmRing += I.Context == EGrassContext::Palm ? 1 : 0;
				Ferns += I.Context == EGrassContext::Palm && I.Kind == EGrassKind::Fern ? 1 : 0;
				Edge += I.Context == EGrassContext::Edge ? 1 : 0;
				Tall += I.Context == EGrassContext::Edge && I.Kind == EGrassKind::Tall ? 1 : 0;
			}
			TestTrue(TEXT("corona de palmera"), PalmRing > 10 && Ferns * 8 > PalmRing);
			TestTrue(TEXT("hierba alta en el borde del camino"), Edge > 50 && Tall * 4 > Edge);

			Config.ClearingThreshold = 0.5f;
			const TArray<FGrassInstance> WithClearings = Generate(Config, *World, -13, 12, -13, 12);
			int32 Clearing = 0, Flowers = 0;
			for (const FGrassInstance& I : WithClearings)
			{
				Clearing += I.Context == EGrassContext::Clearing ? 1 : 0;
				Flowers += I.Context == EGrassContext::Clearing && (I.Kind == EGrassKind::FlowerA || I.Kind == EGrassKind::FlowerB) ? 1 : 0;
			}
			TestTrue(TEXT("claros con flores"), Clearing > 100 && Flowers * 10 > Clearing);
			TestTrue(TEXT("los claros son más ralos"), WithClearings.Num() < All.Num());
		});

		It(TEXT("el borde de la alfombra se funde en densidad y altura, sin corte"), [this]()
		{
			const TSharedPtr<FGrassWorld> World = MakeWorld();
			const FGrassConfig Config = MakeConfig();
			const TArray<FGrassInstance> All = Generate(Config, *World, -13, 12, -13, 12);
			// Arena desde x ≈ 61 (vértice más cercano), y de -70 a 70 para no tocar la ladera ni el mar.
			auto Band = [&All](double X0, double X1, float& OutMaxHeight)
			{
				int32 Count = 0;
				OutMaxHeight = 0.0f;
				for (const FGrassInstance& I : All)
				{
					if (I.Location.X >= X0 && I.Location.X < X1 && FMath::Abs(I.Location.Y) < 70.0)
					{
						++Count;
						OutMaxHeight = FMath::Max(OutMaxHeight, I.HeightScale);
					}
				}
				return Count;
			};
			float EdgeHeight = 0.0f, MidHeight = 0.0f, InnerHeight = 0.0f;
			// Franjas de 0,5 m: a 0-0,5 m del borde, a 1-1,5 m y lejos.
			const int32 Edge = Band(60.5, 61.0, EdgeHeight);
			const int32 Mid = Band(59.5, 60.0, MidHeight);
			const int32 Inner = Band(50.0, 50.5, InnerHeight);
			TestTrue(TEXT("hay hierba lejos del borde"), Inner > 40);
			TestTrue(TEXT("junto a la arena hay menos"), Edge * 2 < Inner);
			TestTrue(TEXT("y aumenta hacia dentro"), Mid > Edge * 2);
			TestTrue(TEXT("más baja en el borde"), EdgeHeight < 0.85f && EdgeHeight >= Config.EdgeMinHeight);
			TestEqual(TEXT("altura entera dentro"), InnerHeight, 1.0f);
			// Junto al camino (borde en |x| = 1,5 + margen) también baja la densidad.
			float PathHeight = 0.0f;
			int32 NearPath = 0;
			int32 FarPath = 0;
			for (const FGrassInstance& I : All)
			{
				const double Ax = FMath::Abs(I.Location.X);
				const bool bRow = FMath::Abs(I.Location.Y) < 50.0 && FMath::Abs(I.Location.Y - 30.0) > 8.0;
				if (bRow && Ax >= 1.9 && Ax < 2.4) { ++NearPath; PathHeight = FMath::Max(PathHeight, I.HeightScale); }
				if (bRow && Ax >= 6.0 && Ax < 6.5) { ++FarPath; }
			}
			TestTrue(TEXT("camino: menos parches junto al borde"), NearPath * 2 < FarPath);
			TestTrue(TEXT("camino: más bajos junto al borde"), PathHeight < 0.85f);
		});
	});

	Describe(TEXT("Actualización incremental"), [this]()
	{
		It(TEXT("respeta el presupuesto y cada celda queda en su nivel"), [this]()
		{
			const FGrassConfig Config = MakeConfig();
			const TSharedPtr<FGrassWorld> World = MakeWorld();
			FGrassFieldModel Model(Config, World);
			FFakeComponents Fake;
			int32 MaxWork = 0;
			const int32 Frames = Settle(Model, FVector2D(-20.0, 0.0), &Fake, MaxWork);
			const int32 Full = Model.GetActiveCellCount();
			const int32 PerCell = Config.Subdivisions() * Config.Subdivisions();
			TestTrue(TEXT("presupuesto por frame (más un trabajo)"), MaxWork <= Config.WorkPerFrame + PerCell);
			TestTrue(TEXT("varios frames"), Frames > 1);
			TestTrue(TEXT("celdas del alcance"), Full > 60 && Full < 200);
			TestEqual(TEXT("visibles = vivas"), Fake.Visible(), Model.GetLiveInstanceCount());
			TestTrue(TEXT("quieta, nada que hacer"), Model.Update(FVector2D(-20.0, 0.0)).IsEmpty());

			// Tras moverse 8 m, cada celda tiene un nivel entre el de quedarse y el que se necesita, y sus
			// matas son exactamente las de rango menor que f_k (deterministas).
			Settle(Model, FVector2D(-12.0, 0.0), &Fake, MaxWork);
			TestEqual(TEXT("visibles = vivas tras moverse"), Fake.Visible(), Model.GetLiveInstanceCount());
			const FGrassThinning& Thin = Config.Thinning;
			int32 Sum = 0;
			int32 BadLevel = 0;
			int32 BadCount = 0;
			for (int32 X = -20; X <= 20; ++X)
			{
				for (int32 Y = -20; Y <= 20; ++Y)
				{
					const FIntPoint Cell(X, Y);
					const TArray<FGrassInstance>* Instances = Model.FindCell(Cell);
					if (!Instances)
					{
						continue;
					}
					Sum += Instances->Num();
					const double Dist = Model.DistanceToCell(FVector2D(-12.0, 0.0), Cell);
					const int32 Level = Model.GetCellLevel(Cell);
					const int32 Need = Thin.LevelFor(Dist, Thin.MarginM);
					const int32 Keep = Thin.LevelFor(Dist, Thin.MarginM + Thin.HysteresisM);
					BadLevel += Keep != INDEX_NONE && Level >= Keep && (Need == INDEX_NONE || Level <= Need) ? 0 : 1;
					TArray<FGrassInstance> Fresh;
					FGrassFieldModel::GenerateCell(Config, *World, Cell, 0.0f, Thin.LevelFraction(Level), Fresh);
					BadCount += Fresh.Num() == Instances->Num() ? 0 : 1;
				}
			}
			TestEqual(TEXT("niveles fuera de su franja"), BadLevel, 0);
			TestEqual(TEXT("celdas distintas de la determinista"), BadCount, 0);
			TestEqual(TEXT("vivas = suma de las celdas"), Sum, Model.GetLiveInstanceCount());
		});

		It(TEXT("recicla las ranuras al ir y volver"), [this]()
		{
			const FGrassConfig Config = MakeConfig();
			FGrassFieldModel Model(Config, MakeWorld());
			FFakeComponents Fake;
			int32 MaxWork = 0;
			Settle(Model, FVector2D(-20.0, 0.0), &Fake, MaxWork);
			int32 Slots = 0;
			for (int32 K = 0; K < GrassKindCount; ++K)
			{
				Slots += Model.GetSlotCount(static_cast<EGrassKind>(K));
			}
			for (int32 Trip = 0; Trip < 6; ++Trip)
			{
				Settle(Model, FVector2D(Trip % 2 == 0 ? 4.0 : -20.0, 0.0), &Fake, MaxWork);
			}
			int32 SlotsAfter = 0;
			for (int32 K = 0; K < GrassKindCount; ++K)
			{
				SlotsAfter += Model.GetSlotCount(static_cast<EGrassKind>(K));
			}
			TestTrue(TEXT("las ranuras no crecen sin límite"), SlotsAfter < Slots * 3 / 2);
			TestEqual(TEXT("visibles = vivas"), Fake.Visible(), Model.GetLiveInstanceCount());
		});

		It(TEXT("la reserva de ranuras es la marca de agua de asentarse en cada punto"), [this]()
		{
			const FGrassConfig Config = MakeConfig();
			const TSharedPtr<FGrassWorld> World = MakeWorld();
			FGrassFieldModel Model(Config, World);
			int32 MaxWork = 0;
			Settle(Model, FVector2D(-20.0, 0.0), nullptr, MaxWork);

			int32 One[GrassKindCount] = {};
			FGrassFieldModel::EstimateSlotHighWater(Config, World, {FVector2D(-20.0, 0.0)}, One);
			int32 Two[GrassKindCount] = {};
			FGrassFieldModel::EstimateSlotHighWater(Config, World, {FVector2D(-20.0, 0.0), FVector2D(40.0, 0.0)}, Two);
			int32 None[GrassKindCount] = {};
			FGrassFieldModel::EstimateSlotHighWater(Config, World, {}, None);

			int32 Total = 0;
			int32 Mismatch = 0;
			int32 Shrunk = 0;
			int32 NonZero = 0;
			for (int32 K = 0; K < GrassKindCount; ++K)
			{
				Total += One[K];
				Mismatch += One[K] != Model.GetSlotCount(static_cast<EGrassKind>(K)) ? 1 : 0;
				Shrunk += Two[K] < One[K] ? 1 : 0;
				NonZero += None[K] != 0 ? 1 : 0;
			}
			TestTrue(TEXT("reserva algo"), Total > 0);
			TestEqual(TEXT("igual que el modelo con presupuesto, por malla"), Mismatch, 0);
			TestEqual(TEXT("más puntos nunca reservan menos"), Shrunk, 0);
			TestEqual(TEXT("sin puntos no reserva nada"), NonZero, 0);
		});

		It(TEXT("sin suelo no genera nada"), [this]()
		{
			TSharedPtr<FGrassWorld> World = MakeShared<FGrassWorld>();
			FGrassFieldModel Model(MakeConfig(), World);
			int32 MaxWork = 0;
			Settle(Model, FVector2D::ZeroVector, nullptr, MaxWork);
			TestEqual(TEXT("matas"), Model.GetLiveInstanceCount(), 0);
		});
	});
}

#endif
