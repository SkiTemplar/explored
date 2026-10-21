#include "Misc/AutomationTest.h"

#include "WorldGen/GrassFieldModel.h"
#include "WorldGen/GroundField.h"
#include "WorldGen/IslandPropsModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GrassFieldTest
{
	constexpr int32 N = 101;
	constexpr float CellM = 2.0f;
	const FVector2D GridMin(-100.0, -100.0);

	/** Suelo de prueba: llano a 2 m con hierba, arena al norte (x > 60), mar al sur (x < -80) y una ladera de 56° (y < -80). */
	float HeightAt(double X, double Y)
	{
		if (X < -80.0)
		{
			return -1.0f;
		}
		return Y < -80.0 ? static_cast<float>(2.0 + (-80.0 - Y) * 1.5) : 2.0f;
	}

	EGroundSurface SurfaceAt(double X, double Y)
	{
		if (X < -80.0)
		{
			return EGroundSurface::Sea;
		}
		return X > 60.0 ? EGroundSurface::Sand : EGroundSurface::Grass;
	}

	TSharedPtr<FGroundField> MakeGround()
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

	TSharedPtr<FGrassWorld> MakeWorld()
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

	FGrassConfig MakeConfig()
	{
		FGrassConfig Config = FGrassConfig::Defaults();
		Config.ZoneDensity.Empty();
		Config.ZoneDensity.Add(FName(TEXT("densa")), 3.0f);
		Config.DensityPerM2 = 1.0f;
		Config.RadiusM = 40.0f;
		Config.ClearingThreshold = 2.0f;  // sin claros salvo que el test los pida
		return Config;
	}

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

	/** Aplica las actualizaciones a «componentes» simulados: escala 0 = oculta. */
	struct FFakeComponents
	{
		TArray<TArray<float>> Scale;

		FFakeComponents() { Scale.SetNum(GrassKindCount); }

		bool Apply(const FGrassUpdate& Update)
		{
			for (int32 K = 0; K < GrassKindCount; ++K)
			{
				for (const FGrassWrite& W : Update.Writes[K])
				{
					if (W.Slot == Scale[K].Num())
					{
						Scale[K].Add(W.Instance.Scale);
					}
					else if (Scale[K].IsValidIndex(W.Slot))
					{
						Scale[K][W.Slot] = W.Instance.Scale;
					}
					else
					{
						return false;  // hueco: el componente no puede añadir fuera de orden
					}
				}
				for (const int32 Slot : Update.Hides[K])
				{
					Scale[K][Slot] = 0.0f;
				}
			}
			return true;
		}

		int32 Visible() const
		{
			int32 Count = 0;
			for (const TArray<float>& Kind : Scale)
			{
				for (const float S : Kind)
				{
					Count += S > 0.0f ? 1 : 0;
				}
			}
			return Count;
		}
	};

	int32 Settle(FGrassFieldModel& Model, const FVector2D& Camera, FFakeComponents* Fake, int32& MaxBuilt)
	{
		int32 Frames = 0;
		do
		{
			const FGrassUpdate Update = Model.Update(Camera);
			MaxBuilt = FMath::Max(MaxBuilt, Update.CellsBuilt);
			if (Fake)
			{
				check(Fake->Apply(Update));
			}
			++Frames;
		}
		while (Model.GetPendingCellCount() > 0 && Frames < 1000);
		return Frames;
	}

	int32 CountIn(const TArray<FGrassInstance>& All, const FVector2D& Center, double Radius)
	{
		int32 Count = 0;
		for (const FGrassInstance& I : All)
		{
			Count += FVector2D::Distance(FVector2D(I.Location.X, I.Location.Y), Center) < Radius ? 1 : 0;
		}
		return Count;
	}

	TArray<FGrassInstance> Generate(const FGrassConfig& Config, const FGrassWorld& World, int32 FromX, int32 ToX, int32 FromY, int32 ToY)
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
			Config.RadiusM = 1.0f;
			TestFalse(TEXT("radio menor que la celda"), Config.Validate(Error));
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
			TestTrue(TEXT("corona de palmera"), PalmRing > 10 && Ferns * 3 > PalmRing);
			TestTrue(TEXT("hierba alta en el borde del camino"), Edge > 50 && Tall * 4 > Edge);

			Config.ClearingThreshold = 0.5f;
			const TArray<FGrassInstance> WithClearings = Generate(Config, *World, -13, 12, -13, 12);
			int32 Clearing = 0, Flowers = 0;
			for (const FGrassInstance& I : WithClearings)
			{
				Clearing += I.Context == EGrassContext::Clearing ? 1 : 0;
				Flowers += I.Context == EGrassContext::Clearing && (I.Kind == EGrassKind::FlowerA || I.Kind == EGrassKind::FlowerB) ? 1 : 0;
			}
			TestTrue(TEXT("claros con flores"), Clearing > 100 && Flowers * 3 > Clearing);
			TestTrue(TEXT("los claros son más ralos"), WithClearings.Num() < All.Num());
		});
	});

	Describe(TEXT("Actualización incremental"), [this]()
	{
		It(TEXT("respeta el presupuesto y solo toca el borde al moverse"), [this]()
		{
			const FGrassConfig Config = MakeConfig();
			FGrassFieldModel Model(Config, MakeWorld());
			FFakeComponents Fake;
			int32 MaxBuilt = 0;
			const int32 Frames = Settle(Model, FVector2D(-20.0, 0.0), &Fake, MaxBuilt);
			const int32 Full = Model.GetActiveCellCount();
			TestTrue(TEXT("presupuesto por frame"), MaxBuilt <= Config.CellsPerFrame);
			TestTrue(TEXT("varios frames"), Frames >= Full / Config.CellsPerFrame);
			TestTrue(TEXT("celdas del círculo"), Full > 60 && Full < 120);
			TestEqual(TEXT("visibles = vivas"), Fake.Visible(), Model.GetLiveInstanceCount());
			TestTrue(TEXT("quieta, nada que hacer"), Model.Update(FVector2D(-20.0, 0.0)).IsEmpty());

			// Una celda hacia +X: entra una columna y sale otra (con histéresis).
			int32 Built = 0, Removed = 0;
			int32 Guard = 0;
			do
			{
				const FGrassUpdate U = Model.Update(FVector2D(-12.0, 0.0));
				TestTrue(TEXT("orden de ranuras"), Fake.Apply(U));
				Built += U.CellsBuilt;
				Removed += U.CellsRemoved;
			}
			while (Model.GetPendingCellCount() > 0 && ++Guard < 100);
			const int32 Border = 2 * (FMath::CeilToInt32(Config.RadiusM / Config.CellSizeM) + 1) + 1;
			TestTrue(TEXT("solo el borde se genera"), Built > 0 && Built <= Border);
			TestTrue(TEXT("solo el borde se quita"), Removed <= Border);
			TestEqual(TEXT("visibles = vivas tras moverse"), Fake.Visible(), Model.GetLiveInstanceCount());

			int32 Sum = 0;
			for (int32 X = -20; X <= 20; ++X)
			{
				for (int32 Y = -20; Y <= 20; ++Y)
				{
					const TArray<FGrassInstance>* Cell = Model.FindCell(FIntPoint(X, Y));
					Sum += Cell ? Cell->Num() : 0;
					if (Cell)
					{
						TArray<FGrassInstance> Fresh;
						FGrassFieldModel::GenerateCell(Config, *MakeWorld(), FIntPoint(X, Y), Fresh);
						TestEqual(TEXT("la celda del modelo es la determinista"), Cell->Num(), Fresh.Num());
					}
				}
			}
			TestEqual(TEXT("vivas = suma de las celdas"), Sum, Model.GetLiveInstanceCount());
		});

		It(TEXT("recicla las ranuras al ir y volver"), [this]()
		{
			const FGrassConfig Config = MakeConfig();
			FGrassFieldModel Model(Config, MakeWorld());
			FFakeComponents Fake;
			int32 MaxBuilt = 0;
			Settle(Model, FVector2D(-20.0, 0.0), &Fake, MaxBuilt);
			int32 Slots = 0;
			for (int32 K = 0; K < GrassKindCount; ++K)
			{
				Slots += Model.GetSlotCount(static_cast<EGrassKind>(K));
			}
			for (int32 Trip = 0; Trip < 6; ++Trip)
			{
				Settle(Model, FVector2D(Trip % 2 == 0 ? 4.0 : -20.0, 0.0), &Fake, MaxBuilt);
			}
			int32 SlotsAfter = 0;
			for (int32 K = 0; K < GrassKindCount; ++K)
			{
				SlotsAfter += Model.GetSlotCount(static_cast<EGrassKind>(K));
			}
			TestTrue(TEXT("las ranuras no crecen sin límite"), SlotsAfter < Slots * 3 / 2);
			TestEqual(TEXT("visibles = vivas"), Fake.Visible(), Model.GetLiveInstanceCount());
		});

		It(TEXT("sin suelo no genera nada"), [this]()
		{
			TSharedPtr<FGrassWorld> World = MakeShared<FGrassWorld>();
			FGrassFieldModel Model(MakeConfig(), World);
			int32 MaxBuilt = 0;
			Settle(Model, FVector2D::ZeroVector, nullptr, MaxBuilt);
			TestEqual(TEXT("matas"), Model.GetLiveInstanceCount(), 0);
		});
	});
}

#endif
