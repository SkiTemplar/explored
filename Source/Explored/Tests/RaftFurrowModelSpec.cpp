#include "Misc/AutomationTest.h"

#include "Boats/RaftFurrowModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace RaftFurrowSpecDetail
{
	const float NaN = std::numeric_limits<float>::quiet_NaN();
	/** Pleamar muy baja: toda la arena del test está seca. */
	constexpr double DryTide = -10.0;
	/** Pleamar muy alta: toda está húmeda. */
	constexpr double WetTide = 10.0;

	FHullPiece Piece(EHullPieceType Type, const FVector& Center, const FVector& Size = FVector::ZeroVector)
	{
		FHullPiece P;
		P.Type = Type;
		P.CenterCm = Center;
		P.SizeCm = Size;
		return P;
	}

	/** La misma balsa que RaftYardModelSpec: 6 troncos de 3 m × 22 cm (1,32 m de manga) y dos travesaños. */
	FRaftYardModel SixLogRaft()
	{
		FRaftYardModel Yard;
		const double Width = FHullAssemblyModel::Spec(EHullPieceType::Log).DefaultSizeCm.Y;
		for (int32 I = 0; I < 6; ++I)
		{
			Yard.AddPiece(Piece(EHullPieceType::Log, FVector(0.0, (I - 2.5) * Width, 11.0)));
		}
		const int32 Fore = Yard.AddPiece(Piece(EHullPieceType::Plank, FVector(100.0, 0.0, 24.0), FVector(25.0, 140.0, 4.0)));
		const int32 Aft = Yard.AddPiece(Piece(EHullPieceType::Plank, FVector(-100.0, 0.0, 24.0), FVector(25.0, 140.0, 4.0)));
		for (int32 I = 0; I < 5; ++I)
		{
			Yard.AddJoint(I, I + 1, ERaftJointKind::Rope);
		}
		for (int32 I = 0; I < 6; ++I)
		{
			Yard.AddJoint(Fore, I, ERaftJointKind::Rope);
			Yard.AddJoint(Aft, I, ERaftJointKind::Rope);
		}
		return Yard;
	}

	/** Dos flotadores de un tronco separados 1,02 m (catamarán) con un tablero encima. */
	FRaftYardModel Catamaran()
	{
		FRaftYardModel Yard;
		const int32 A = Yard.AddPiece(Piece(EHullPieceType::Log, FVector(0.0, -62.0, 11.0)));
		const int32 B = Yard.AddPiece(Piece(EHullPieceType::Log, FVector(0.0, 62.0, 11.0)));
		const int32 Deck = Yard.AddPiece(Piece(EHullPieceType::Plank, FVector(0.0, 0.0, 24.0), FVector(200.0, 150.0, 4.0)));
		Yard.AddJoint(A, Deck, ERaftJointKind::Rope);
		Yard.AddJoint(B, Deck, ERaftJointKind::Rope);
		return Yard;
	}

	FLaunchPath Path(ELaunchSurface Surface, float LengthCm, const FVector& StartCm = FVector::ZeroVector, float YawDeg = 0.0f)
	{
		FLaunchPath P;
		P.StartCm = StartCm;
		P.YawDeg = YawDeg;
		FLaunchSegment Segment;
		Segment.Surface = Surface;
		Segment.LengthCm = LengthCm;
		P.Segments.Add(Segment);
		return P;
	}

	/** Arrastra de SFrom a STo en tramos de StepCm, como harían los Push de varios fotogramas. */
	FRaftFurrowResult DragInSteps(FRaftYardModel& Yard, FSandModel& Sand, float SFrom, float STo, float StepCm, double HighTide,
		FSandModel::FBaseHeight Base)
	{
		FRaftFurrowResult Total;
		Yard.PlaceOnPath(Yard.GetPath(), SFrom);
		float S = SFrom;
		while (S < STo)
		{
			const float Next = FMath::Min(S + StepCm, STo);
			Yard.PlaceOnPath(Yard.GetPath(), Next);
			const FRaftFurrowResult R = FRaftFurrowModel::Drag(Yard, S, Next, HighTide, false, Sand, Base);
			Total.Sand.Mass += R.Sand.Mass;
			Total.BlockedMass += R.BlockedMass;
			Total.Sand.DirtyChunks.Append(R.Sand.DirtyChunks);
			Total.DrySinkMm = R.DrySinkMm;
			Total.WetSinkMm = R.WetSinkMm;
			S = Next;
		}
		return Total;
	}

	int32 MinDelta(const FSandModel& Sand, int32 X0, int32 X1, int32 Y0, int32 Y1)
	{
		int32 Min = MAX_int32;
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			for (int32 X = X0; X <= X1; ++X)
			{
				Min = FMath::Min(Min, Sand.DeltaMm(FIntPoint(X, Y)));
			}
		}
		return Min;
	}
}

BEGIN_DEFINE_SPEC(FRaftFurrowModelSpec, "Explored.RaftFurrow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FRaftFurrowModelSpec)

void FRaftFurrowModelSpec::Define()
{
	using namespace RaftFurrowSpecDetail;
	auto Flat = [](double, double) { return 0.0; };

	Describe("FSandModel::Transfer", [this, Flat]()
	{
		It("conserva la masa y recorta a lo que cabe en el montón", [this, Flat]()
		{
			FSandModel Sand;
			TArray<FSandMove> Moves;
			Moves.Add({ FIntPoint(0, 0), FIntPoint(1, 0), 1500 });
			Moves.Add({ FIntPoint(2, 0), FIntPoint(1, 0), 1500 });
			const FSandResult R = Sand.Transfer(Moves, Flat);
			TestEqual(TEXT("el segundo solo hasta el tope del montón"), R.Mass, static_cast<int64>(FSandModel::MaxPileHeightMm));
			TestEqual(TEXT("primer origen"), Sand.DeltaMm(FIntPoint(0, 0)), -1500);
			TestEqual(TEXT("segundo origen"), Sand.DeltaMm(FIntPoint(2, 0)), 1500 - FSandModel::MaxPileHeightMm);
			TestEqual(TEXT("destino"), Sand.DeltaMm(FIntPoint(1, 0)), FSandModel::MaxPileHeightMm);
			TestEqual(TEXT("masa total"), Sand.TotalMass(), static_cast<int64>(0));
			TestEqual(TEXT("tres columnas"), R.ColumnsChanged, 3);
			TestTrue(TEXT("columnas sucias con sus vecinas"), Sand.IsDirty(FIntPoint(0, 0)) && Sand.IsDirty(FIntPoint(3, 0)));
		});

		It("no saca más de la capa de arena", [this, Flat]()
		{
			FSandModel Sand;
			FSandMove Move;
			Move.From = FIntPoint(0, 0);
			Move.To = FIntPoint(1, 0);
			Move.Mm = 1000;
			Sand.Transfer({ Move }, Flat);
			Move.To = FIntPoint(2, 0);
			const FSandResult R = Sand.Transfer({ Move }, Flat);
			TestEqual(TEXT("solo lo que queda hasta la roca"), R.Mass, static_cast<int64>(FSandModel::MaxDigDepthMm - 1000));
			TestEqual(TEXT("en la roca"), Sand.DeltaMm(FIntPoint(0, 0)), -FSandModel::MaxDigDepthMm);
			TestEqual(TEXT("masa total"), Sand.TotalMass(), static_cast<int64>(0));
		});

		It("ignora traslados vacíos, a sí mismo, bajo una estructura o fuera de la rejilla", [this, Flat]()
		{
			FSandModel Sand;
			Sand.SetAnchor(FVector2D(0.9, -0.1), FVector2D(1.1, 0.1), true, Flat);
			TArray<FSandMove> Moves;
			Moves.Add({ FIntPoint(0, 0), FIntPoint(0, 0), 100 });
			Moves.Add({ FIntPoint(0, 0), FIntPoint(1, 0), -100 });
			Moves.Add({ FIntPoint(0, 0), FIntPoint(4, 0), 100 });
			Moves.Add({ FIntPoint(4, 0), FIntPoint(0, 0), 100 });
			Moves.Add({ FIntPoint(0, 0), FIntPoint(MAX_int32, 0), 100 });
			const FSandResult R = Sand.Transfer(Moves, Flat);
			TestEqual(TEXT("nada"), R.Mass, static_cast<int64>(0));
			TestTrue(TEXT("vacía"), Sand.IsEmpty());
		});
	});

	Describe("el hundimiento", [this]()
	{
		It("es tres veces mayor en arena seca y tiene suelo y techo", [this]()
		{
			TestEqual(TEXT("seca, 1 kPa"), FRaftFurrowModel::SinkMm(1.0f, false), 12);
			TestEqual(TEXT("húmeda, 1 kPa"), FRaftFurrowModel::SinkMm(1.0f, true), 4);
			TestEqual(TEXT("sin marca"), FRaftFurrowModel::SinkMm(0.5f, true), 0);
			TestEqual(TEXT("techo"), FRaftFurrowModel::SinkMm(1000.0f, false), FRaftFurrowModel::MaxSinkMm);
			TestEqual(TEXT("NaN"), FRaftFurrowModel::SinkMm(NaN, false), 0);
			TestEqual(TEXT("infinito"), FRaftFurrowModel::SinkMm(std::numeric_limits<float>::infinity(), false), 0);
			TestEqual(TEXT("negativa"), FRaftFurrowModel::SinkMm(-3.0f, false), 0);
		});
	});

	Describe("arrastrar la balsa por la arena", [this, Flat]()
	{
		It("abre un surco con cordones a los lados y conserva la masa", [this, Flat]()
		{
			FRaftYardModel Yard = SixLogRaft();
			Yard.PlaceOnPath(Path(ELaunchSurface::Sand, 2000.0f), 200.0f);
			FSandModel Sand;
			const FRaftFurrowResult R = FRaftFurrowModel::Drag(Yard, 200.0f, 600.0f, DryTide, false, Sand, Flat);
			// 451 kg en 3,96 m² de troncos: ~1,1 kPa → 13 mm en seco.
			TestTrue(TEXT("presión de una balsa de troncos"), R.PressureKPa > 1.0f && R.PressureKPa < 1.3f);
			TestEqual(TEXT("surco en seco"), R.DrySinkMm, 13);
			TestEqual(TEXT("masa"), Sand.TotalMass() + Sand.SeaBankMass(), static_cast<int64>(0));
			TestEqual(TEXT("nada bloqueado"), R.BlockedMass, static_cast<int64>(0));
			TestTrue(TEXT("masa movida"), R.Sand.Mass > 0);
			// Manga ±66 cm → columnas Y = -2..2 (celdas de 25 cm). Tramo: popa en 0,5 m, proa en 7,5 m.
			TestEqual(TEXT("fondo del surco"), MinDelta(Sand, 4, 28, -2, 2), -R.DrySinkMm);
			TestEqual(TEXT("fondo parejo"), Sand.DeltaMm(FIntPoint(16, 0)), -R.DrySinkMm);
			TestTrue(TEXT("cordón a estribor"), Sand.DeltaMm(FIntPoint(16, 3)) > 0);
			TestTrue(TEXT("cordón a babor"), Sand.DeltaMm(FIntPoint(16, -3)) > 0);
			TestEqual(TEXT("más allá del cordón no hay nada"), Sand.DeltaMm(FIntPoint(16, 4)), 0);
			TestEqual(TEXT("delante de la proa no hay nada"), Sand.DeltaMm(FIntPoint(32, 0)), 0);
			// Los dos cordones reciben lo mismo: 5 columnas de surco por fila, 13 mm cada una.
			TestEqual(TEXT("fila completa"), Sand.DeltaMm(FIntPoint(16, 3)) + Sand.DeltaMm(FIntPoint(16, -3)), 5 * R.DrySinkMm);
		});

		It("en arena húmeda el surco es menos hondo", [this, Flat]()
		{
			FRaftYardModel Yard = SixLogRaft();
			Yard.PlaceOnPath(Path(ELaunchSurface::WetSand, 2000.0f), 200.0f);
			FSandModel Wet;
			const FRaftFurrowResult R = FRaftFurrowModel::Drag(Yard, 200.0f, 600.0f, WetTide, false, Wet, Flat);
			TestEqual(TEXT("húmeda"), Wet.DeltaMm(FIntPoint(16, 0)), -R.WetSinkMm);
			TestTrue(TEXT("menos que en seco"), R.WetSinkMm < R.DrySinkMm);
			FSandModel Rain;
			FRaftFurrowModel::Drag(Yard, 200.0f, 600.0f, DryTide, true, Rain, Flat);
			TestEqual(TEXT("con lluvia también"), Rain.DeltaMm(FIntPoint(16, 0)), -R.WetSinkMm);
			TestTrue(TEXT("mismo resultado"), Wet == Rain);
		});

		It("volver a pasar por el mismo sitio no ahonda el surco", [this, Flat]()
		{
			FRaftYardModel Yard = SixLogRaft();
			Yard.PlaceOnPath(Path(ELaunchSurface::Sand, 2000.0f), 200.0f);
			FSandModel Sand;
			const FRaftFurrowResult First = FRaftFurrowModel::Drag(Yard, 200.0f, 600.0f, DryTide, false, Sand, Flat);
			const FSandModel After = Sand;
			const FRaftFurrowResult Back = FRaftFurrowModel::Drag(Yard, 600.0f, 200.0f, DryTide, false, Sand, Flat);
			TestEqual(TEXT("nada que mover"), Back.Sand.Mass, static_cast<int64>(0));
			TestTrue(TEXT("igual"), Sand == After);
			TestEqual(TEXT("hondo"), MinDelta(Sand, 4, 28, -2, 2), -First.DrySinkMm);
		});

		It("da lo mismo en un tramo que en muchos fotogramas (rumbo alineado con la rejilla)", [this, Flat]()
		{
			FRaftYardModel Yard = SixLogRaft();
			Yard.PlaceOnPath(Path(ELaunchSurface::Sand, 2000.0f), 200.0f);
			FSandModel One;
			FRaftFurrowModel::Drag(Yard, 200.0f, 600.0f, DryTide, false, One, Flat);
			FSandModel Many;
			DragInSteps(Yard, Many, 200.0f, 600.0f, 1.7f, DryTide, Flat);
			TestTrue(TEXT("mismo surco"), One == Many);
		});

		It("es determinista y conserva la masa con cualquier rumbo", [this, Flat]()
		{
			for (const float Yaw : { 17.0f, 45.0f, 133.0f, -71.0f })
			{
				FRaftYardModel Yard = SixLogRaft();
				Yard.PlaceOnPath(Path(ELaunchSurface::Sand, 2000.0f, FVector(-313.0, 127.0, 0.0), Yaw), 200.0f);
				FSandModel A;
				FSandModel B;
				const FRaftFurrowResult RA = DragInSteps(Yard, A, 200.0f, 700.0f, 3.1f, DryTide, Flat);
				DragInSteps(Yard, B, 200.0f, 700.0f, 3.1f, DryTide, Flat);
				TestTrue(FString::Printf(TEXT("determinista a %.0f°"), Yaw), A == B);
				TestEqual(FString::Printf(TEXT("masa a %.0f°"), Yaw), A.TotalMass(), static_cast<int64>(0));
				TestTrue(FString::Printf(TEXT("hay surco a %.0f°"), Yaw), RA.Sand.Mass > 0);
				// Bajo el eje, a medio tramo, el fondo está justo a −hundimiento: la huella solo pierde arena
				// (los cordones caen siempre fuera de ella) y nunca más de lo que marca el surco.
				const FVector W = Yard.GetPath().WorldAt(450.0f);
				const FIntPoint Mid = A.ColumnOf(W.X / 100.0, W.Y / 100.0);
				TestEqual(FString::Printf(TEXT("fondo a %.0f°"), Yaw), A.DeltaMm(Mid), -RA.DrySinkMm);
			}
		});

		It("aplana un montón del camino y no toca un hoyo más hondo que el surco", [this, Flat]()
		{
			FRaftYardModel Yard = SixLogRaft();
			Yard.PlaceOnPath(Path(ELaunchSurface::Sand, 2000.0f), 200.0f);
			FSandModel Sand;
			FSandBrush Heap;
			Heap.Center = FVector2D(4.0, 0.0);
			Heap.Radius = 0.5f;
			Heap.Depth = 0.3f;
			Heap.MassBudget = 1000000;
			Sand.Pile(Heap, Flat);
			FSandBrush Hole;
			Hole.Center = FVector2D(6.0, 0.0);
			Hole.Radius = 0.4f;
			Hole.Depth = 0.3f;
			Sand.Dig(Hole, Flat);
			const int64 Before = Sand.TotalMass();
			const int32 HoleBottom = Sand.DeltaMm(FIntPoint(24, 0));
			const FRaftFurrowResult R = FRaftFurrowModel::Drag(Yard, 200.0f, 600.0f, DryTide, false, Sand, Flat);
			TestEqual(TEXT("montón aplanado"), Sand.DeltaMm(FIntPoint(16, 0)), -R.DrySinkMm);
			TestEqual(TEXT("hoyo intacto"), Sand.DeltaMm(FIntPoint(24, 0)), HoleBottom);
			TestEqual(TEXT("masa"), Sand.TotalMass(), Before);
		});

		It("abre dos surcos con un catamarán aunque cada tronco sea más estrecho que la celda", [this, Flat]()
		{
			FRaftYardModel Yard = Catamaran();
			Yard.PlaceOnPath(Path(ELaunchSurface::Sand, 2000.0f), 200.0f);
			FSandModel Sand;
			const FRaftFurrowResult R = FRaftFurrowModel::Drag(Yard, 200.0f, 600.0f, DryTide, false, Sand, Flat);
			// Flotadores de 22 cm en Y = ±62 cm (de 51 a 73): ningún punto de la rejilla cae dentro, pero la huella ocupa una celda y marca la columna Y = ±2 (±50 cm).
			TestTrue(TEXT("hay surco"), R.DrySinkMm > 0);
			TestEqual(TEXT("surco de estribor"), Sand.DeltaMm(FIntPoint(16, 2)), -R.DrySinkMm);
			TestEqual(TEXT("surco de babor"), Sand.DeltaMm(FIntPoint(16, -2)), -R.DrySinkMm);
			TestEqual(TEXT("cordón de estribor"), Sand.DeltaMm(FIntPoint(16, 3)), R.DrySinkMm);
			TestEqual(TEXT("cordón de babor"), Sand.DeltaMm(FIntPoint(16, -3)), R.DrySinkMm);
			TestEqual(TEXT("el eje no se toca"), Sand.DeltaMm(FIntPoint(16, 0)), 0);
			TestEqual(TEXT("masa"), Sand.TotalMass(), static_cast<int64>(0));
		});
	});

	Describe("no deja surco", [this, Flat]()
	{
		It("sobre rodillos, roca, hierba o la rampa, ni a flote", [this, Flat]()
		{
			for (const ELaunchSurface Surface : { ELaunchSurface::Rock, ELaunchSurface::Grass, ELaunchSurface::PlankRamp })
			{
				FRaftYardModel Yard = SixLogRaft();
				Yard.PlaceOnPath(Path(Surface, 2000.0f), 200.0f);
				FSandModel Sand;
				FRaftFurrowModel::Drag(Yard, 200.0f, 600.0f, DryTide, false, Sand, Flat);
				TestTrue(FString::Printf(TEXT("sin surco en %s"), LexToString(Surface)), Sand.IsEmpty());
			}
			FRaftYardModel Rolling = SixLogRaft();
			Rolling.PlaceOnPath(Path(ELaunchSurface::Sand, 2000.0f), 200.0f);
			Rolling.PlaceRoller(110.0f);
			Rolling.PlaceRoller(290.0f);
			FSandModel Sand;
			FRaftFurrowModel::Drag(Rolling, 200.0f, 210.0f, DryTide, false, Sand, Flat);
			TestTrue(TEXT("sin surco sobre rodillos"), Sand.IsEmpty());
			FRaftYardModel Afloat = SixLogRaft();
			Afloat.PlaceOnPath(Path(ELaunchSurface::Sand, 2000.0f), 200.0f);
			Afloat.SetAfloat();
			FRaftFurrowModel::Drag(Afloat, 200.0f, 600.0f, DryTide, false, Sand, Flat);
			TestTrue(TEXT("sin surco a flote"), Sand.IsEmpty());
		});

		It("solo en el tramo de arena de un camino mixto", [this, Flat]()
		{
			FRaftYardModel Yard = SixLogRaft();
			FLaunchPath P = Path(ELaunchSurface::Rock, 400.0f);
			FLaunchSegment Beach;
			Beach.Surface = ELaunchSurface::Sand;
			Beach.LengthCm = 1600.0f;
			P.Segments.Add(Beach);
			Yard.PlaceOnPath(P, 200.0f);
			FSandModel Sand;
			FRaftFurrowModel::Drag(Yard, 200.0f, 800.0f, DryTide, false, Sand, Flat);
			// La roca acaba en S = 4 m: con la popa en la roca (hasta S = 3,99 m) la huella no pisa arena
			// antes de 2,5 m; en la arena el casco abarca de S − 1,5 m a S + 1,5 m.
			TestEqual(TEXT("sin surco al principio de la roca"), Sand.DeltaMm(FIntPoint(4, 0)), 0);
			// Con el centro ya en la arena (S > 4 m) la popa sigue sobre la roca: la roca no se cava.
			for (int32 X = 12; X <= 15; ++X)
			{
				TestEqual(*FString::Printf(TEXT("sin surco en la roca bajo la popa (columna %d)"), X), Sand.DeltaMm(FIntPoint(X, 0)), 0);
			}
			TestTrue(TEXT("surco en la arena"), Sand.DeltaMm(FIntPoint(28, 0)) < 0);
			TestEqual(TEXT("masa"), Sand.TotalMass(), static_cast<int64>(0));
		});

		It("la proa que ya pisa la rampa no deja marca aunque el centro siga en la arena", [this, Flat]()
		{
			FRaftYardModel Yard = SixLogRaft();
			FLaunchPath P = Path(ELaunchSurface::Sand, 400.0f);
			FLaunchSegment Ramp;
			Ramp.Surface = ELaunchSurface::PlankRamp;
			Ramp.LengthCm = 1600.0f;
			P.Segments.Add(Ramp);
			Yard.PlaceOnPath(P, 200.0f);
			FSandModel Sand;
			FRaftFurrowModel::Drag(Yard, 200.0f, 380.0f, DryTide, false, Sand, Flat);
			TestTrue(TEXT("surco en la arena"), Sand.DeltaMm(FIntPoint(8, 0)) < 0);
			for (int32 X = 17; X <= 21; ++X)
			{
				TestEqual(*FString::Printf(TEXT("sin surco bajo la rampa (columna %d)"), X), Sand.DeltaMm(FIntPoint(X, 0)), 0);
			}
			TestEqual(TEXT("masa"), Sand.TotalMass(), static_cast<int64>(0));
		});

		It("la arena mojada del camino se hunde como la de bajo la pleamar", [this, Flat]()
		{
			FRaftYardModel DryYard = SixLogRaft();
			DryYard.PlaceOnPath(Path(ELaunchSurface::Sand, 2000.0f), 200.0f);
			FSandModel Dry;
			const FRaftFurrowResult R = FRaftFurrowModel::Drag(DryYard, 200.0f, 600.0f, DryTide, false, Dry, Flat);
			FRaftYardModel WetYard = SixLogRaft();
			WetYard.PlaceOnPath(Path(ELaunchSurface::WetSand, 2000.0f), 200.0f);
			FSandModel Wet;
			FRaftFurrowModel::Drag(WetYard, 200.0f, 600.0f, DryTide, false, Wet, Flat);
			TestEqual(TEXT("seca"), Dry.DeltaMm(FIntPoint(16, 0)), -R.DrySinkMm);
			TestEqual(TEXT("mojada"), Wet.DeltaMm(FIntPoint(16, 0)), -R.WetSinkMm);
		});

		It("con entradas degeneradas no toca nada ni se cuelga", [this, Flat]()
		{
			FRaftYardModel Yard = SixLogRaft();
			Yard.PlaceOnPath(Path(ELaunchSurface::Sand, 2000.0f), 200.0f);
			FSandModel Sand;
			FRaftFurrowModel::Drag(Yard, NaN, 600.0f, DryTide, false, Sand, Flat);
			FRaftFurrowModel::Drag(Yard, 200.0f, NaN, DryTide, false, Sand, Flat);
			FRaftFurrowModel::Drag(Yard, 200.0f, 200.0f, DryTide, false, Sand, Flat);
			FRaftFurrowModel::Drag(Yard, 200.0f, 600.0f, NaN, false, Sand, Flat);
			FRaftYardModel Empty;
			Empty.PlaceOnPath(Path(ELaunchSurface::Sand, 2000.0f), 200.0f);
			FRaftFurrowModel::Drag(Empty, 200.0f, 600.0f, DryTide, false, Sand, Flat);
			FRaftYardModel Far = SixLogRaft();
			Far.PlaceOnPath(Path(ELaunchSurface::Sand, 2000.0f, FVector(1.0e30, 0.0, 0.0)), 200.0f);
			FRaftFurrowModel::Drag(Far, 200.0f, 600.0f, DryTide, false, Sand, Flat);
			FRaftYardModel Edge = SixLogRaft();
			Edge.PlaceOnPath(Path(ELaunchSurface::Sand, 2000.0f, FVector(FSandModel::MaxAbsColumn * 25.0, 0.0, 0.0)), 200.0f);
			FRaftFurrowModel::Drag(Edge, 200.0f, 600.0f, DryTide, false, Sand, Flat);
			TestTrue(TEXT("vacía"), Sand.IsEmpty());
			// Un salto enorme se muestrea con MaxStations y acaba.
			FSandModel Long;
			FRaftYardModel Big = SixLogRaft();
			Big.PlaceOnPath(Path(ELaunchSurface::Sand, 1.0e6f), 0.0f);
			const FRaftFurrowResult R = FRaftFurrowModel::Drag(Big, 0.0f, 1.0e6f, DryTide, false, Long, Flat);
			TestTrue(TEXT("acaba"), R.FootprintColumns > 0);
			TestEqual(TEXT("masa"), Long.TotalMass(), static_cast<int64>(0));
			// Una pieza desmesurada no hace recorrer millones de columnas por muestra.
			FSandModel Huge;
			FRaftYardModel Giant = SixLogRaft();
			Giant.AddPiece(Piece(EHullPieceType::Plank, FVector(0.0, 0.0, 0.0), FVector(1.0e7, 1.0e7, 4.0)));
			Giant.PlaceOnPath(Path(ELaunchSurface::Sand, 2000.0f), 200.0f);
			FRaftFurrowModel::Drag(Giant, 200.0f, 600.0f, DryTide, false, Huge, Flat);
			TestEqual(TEXT("masa con pieza desmesurada"), Huge.TotalMass(), static_cast<int64>(0));
		});
	});

	Describe("junto a estructuras", [this, Flat]()
	{
		It("echa el cordón al otro lado si un costado está bajo un muelle", [this, Flat]()
		{
			FRaftYardModel Yard = SixLogRaft();
			Yard.PlaceOnPath(Path(ELaunchSurface::Sand, 2000.0f), 200.0f);
			FSandModel Sand;
			// Tablones de contención a estribor, justo en la columna del cordón (Y = 0,75 m).
			Sand.SetAnchor(FVector2D(-1.0, 0.7), FVector2D(10.0, 1.2), true, Flat);
			const FRaftFurrowResult R = FRaftFurrowModel::Drag(Yard, 200.0f, 600.0f, DryTide, false, Sand, Flat);
			TestEqual(TEXT("cordón de estribor vacío"), Sand.DeltaMm(FIntPoint(16, 3)), 0);
			TestEqual(TEXT("todo a babor"), Sand.DeltaMm(FIntPoint(16, -3)), 5 * R.DrySinkMm);
			TestEqual(TEXT("masa"), Sand.TotalMass(), static_cast<int64>(0));
			TestEqual(TEXT("nada bloqueado"), R.BlockedMass, static_cast<int64>(0));
		});

		It("deja la arena en su sitio si los dos costados están bajo estructuras", [this, Flat]()
		{
			FRaftYardModel Yard = SixLogRaft();
			Yard.PlaceOnPath(Path(ELaunchSurface::Sand, 2000.0f), 200.0f);
			FSandModel Sand;
			Sand.SetAnchor(FVector2D(-1.0, 0.7), FVector2D(10.0, 1.2), true, Flat);
			Sand.SetAnchor(FVector2D(-1.0, -1.2), FVector2D(10.0, -0.7), true, Flat);
			const FRaftFurrowResult R = FRaftFurrowModel::Drag(Yard, 200.0f, 600.0f, DryTide, false, Sand, Flat);
			TestTrue(TEXT("bloqueada"), R.BlockedMass > 0);
			TestEqual(TEXT("sin surco"), Sand.DeltaMm(FIntPoint(16, 0)), 0);
			TestEqual(TEXT("masa"), Sand.TotalMass(), static_cast<int64>(0));
			TestEqual(TEXT("nada movido"), R.Sand.Mass, static_cast<int64>(0));
		});
	});

	Describe("bordes de chunk y la simulación de la arena", [this, Flat]()
	{
		It("cruza bordes de chunk en coordenadas negativas y marca sucios los chunks de los dos lados", [this, Flat]()
		{
			FRaftYardModel Yard = SixLogRaft();
			// Empieza en x = −12 m (chunk −2) y cruza x = −8 m y x = 0 m. Eje en y = 0: borde de chunk en Y.
			Yard.PlaceOnPath(Path(ELaunchSurface::Sand, 2000.0f, FVector(-1200.0, 0.0, 0.0)), 200.0f);
			FSandModel Sand;
			const FRaftFurrowResult R = FRaftFurrowModel::Drag(Yard, 200.0f, 1400.0f, DryTide, false, Sand, Flat);
			TestEqual(TEXT("masa"), Sand.TotalMass(), static_cast<int64>(0));
			TestEqual(TEXT("fondo a ambos lados de x = −8 m"),
				Sand.DeltaMm(FIntPoint(-33, 0)) == -R.DrySinkMm && Sand.DeltaMm(FIntPoint(-32, 0)) == -R.DrySinkMm, true);
			TestEqual(TEXT("fondo a ambos lados de y = 0"),
				Sand.DeltaMm(FIntPoint(-20, -1)) == -R.DrySinkMm && Sand.DeltaMm(FIntPoint(-20, 1)) == -R.DrySinkMm, true);
			for (const FIntPoint& Chunk : { FIntPoint(-2, -1), FIntPoint(-2, 0), FIntPoint(-1, -1), FIntPoint(-1, 0), FIntPoint(0, 0) })
			{
				TestTrue(FString::Printf(TEXT("chunk (%d, %d) sucio"), Chunk.X, Chunk.Y), R.Sand.DirtyChunks.Contains(Chunk));
			}
			// Lo que sale por la red es exactamente lo que ha cambiado: un cliente con los paquetes queda igual.
			FSandModel Client;
			for (const FIntPoint& Chunk : Sand.EditedChunks())
			{
				for (const TArray<uint8>& Packet : Sand.EncodeFullChunk(Chunk))
				{
					TestTrue(TEXT("paquete válido"), Client.ApplyPacket(Packet, Flat));
				}
				TestEqual(TEXT("checksum"), Client.ChunkChecksum(Chunk), Sand.ChunkChecksum(Chunk));
			}
		});

		It("los cordones solo se asientan cerca de un jugador y la avalancha conserva la masa", [this, Flat]()
		{
			// Balsa cargada: 3 personas de 75 kg y 200 kg de carga → surco al tope y cordones empinados.
			FRaftYardModel Yard = SixLogRaft();
			for (int32 I = 0; I < 3; ++I)
			{
				FHullLoad Person;
				Person.MassKg = 75.0f;
				Person.CenterCm = FVector(-80.0 + I * 80.0, 0.0, 100.0);
				Person.bPassenger = true;
				Yard.AddLoad(Person);
			}
			FHullLoad Cargo;
			Cargo.MassKg = 2000.0f;
			Cargo.CenterCm = FVector(0.0, 0.0, 40.0);
			Yard.AddLoad(Cargo);
			Yard.PlaceOnPath(Path(ELaunchSurface::Sand, 2000.0f), 200.0f);
			FSandModel Sand;
			const FRaftFurrowResult R = FRaftFurrowModel::Drag(Yard, 200.0f, 600.0f, DryTide, false, Sand, Flat);
			TestEqual(TEXT("surco al tope"), R.DrySinkMm, FRaftFurrowModel::MaxSinkMm);
			const int32 Ridge = Sand.DeltaMm(FIntPoint(16, 3));
			// En la fila X = 16 (par) la columna del eje va a estribor con las dos de su lado: 3 × 60 mm.
			TestEqual(TEXT("cordón"), Ridge, 3 * FRaftFurrowModel::MaxSinkMm);
			TestTrue(TEXT("más empinado que el reposo en seco"), Ridge > FSandModel::ReposeDropMm(FSandModel::DryReposeDeg, 0.25f));

			FSandEnvironment Away;
			Away.HighTide = DryTide;
			Away.Focus = FVector2D(500.0, 500.0);
			const FSandResult Frozen = Sand.Tick(Away, Flat);
			TestEqual(TEXT("lejos no se revisa nada"), Frozen.ActiveColumns, 0);
			TestTrue(TEXT("todo congelado"), Frozen.DormantColumns > 0);
			TestEqual(TEXT("nada cambia lejos"), Sand.DeltaMm(FIntPoint(16, 3)), Ridge);

			FSandEnvironment Near = Away;
			Near.Focus = FVector2D(4.0, 0.0);
			FSandResult Last;
			for (int32 I = 0; I < 200 && Sand.NumDirtyColumns() > 0; ++I)
			{
				Last = Sand.Tick(Near, Flat);
			}
			TestTrue(TEXT("el cordón se ha derrumbado"), Sand.DeltaMm(FIntPoint(16, 3)) < Ridge);
			TestTrue(TEXT("revisadas solo las cercanas"), Last.ActiveColumns <= 2000);
			TestEqual(TEXT("masa tras la avalancha"), Sand.TotalMass() + Sand.SeaBankMass(), static_cast<int64>(0));
		});

		It("el oleaje rellena el surco de la franja intermareal y el mar guarda la cuenta", [this, Flat]()
		{
			FRaftYardModel Yard = SixLogRaft();
			Yard.PlaceOnPath(Path(ELaunchSurface::WetSand, 2000.0f), 200.0f);
			FSandModel Sand;
			FRaftFurrowModel::Drag(Yard, 200.0f, 600.0f, WetTide, false, Sand, Flat);
			const int32 Before = Sand.DeltaMm(FIntPoint(16, 0));
			FSandTide Tide;
			Tide.HighTide = 1.0;
			Tide.LowTide = -1.0;
			for (int32 I = 0; I < 12; ++I)
			{
				Sand.ApplyHalfTide(Tide, Flat);
			}
			TestTrue(TEXT("rellenado"), Sand.DeltaMm(FIntPoint(16, 0)) > Before);
			TestEqual(TEXT("masa con el banco del mar"), Sand.TotalMass() + Sand.SeaBankMass(), static_cast<int64>(0));
		});
	});

	Describe("con el astillero", [this, Flat]()
	{
		It("empujar sin rodillos por la arena deja surco; con rodillos no", [this, Flat]()
		{
			FRaftYardModel Yard = SixLogRaft();
			Yard.PlaceOnPath(Path(ELaunchSurface::Sand, 2000.0f), 200.0f);
			FSandModel Sand;
			float Moved = 0.0f;
			for (int32 Frame = 0; Frame < 600 && Yard.GetCenterS() < 500.0f; ++Frame)
			{
				const float S = Yard.GetCenterS();
				const FRaftPushReport Push = Yard.Push(Yard.RequiredPushForceN() + 600.0f, 1.0f / 60.0f);
				Moved += Push.MovedCm;
				if (!Push.bOnRollers)
				{
					FRaftFurrowModel::Drag(Yard, S, Yard.GetCenterS(), DryTide, false, Sand, Flat);
				}
			}
			TestTrue(TEXT("se ha movido"), Moved > 250.0f);
			TestTrue(TEXT("surco"), Sand.DeltaMm(FIntPoint(12, 0)) < 0);
			TestEqual(TEXT("masa"), Sand.TotalMass(), static_cast<int64>(0));

			FRaftYardModel Rolling = SixLogRaft();
			Rolling.PlaceOnPath(Path(ELaunchSurface::Sand, 2000.0f), 200.0f);
			Rolling.PlaceRoller(110.0f);
			Rolling.PlaceRoller(290.0f);
			FSandModel Clean;
			for (int32 Frame = 0; Frame < 60; ++Frame)
			{
				const float S = Rolling.GetCenterS();
				const FRaftPushReport Push = Rolling.Push(Rolling.RequiredPushForceN() + 200.0f, 1.0f / 60.0f);
				if (!Push.bOnRollers)
				{
					FRaftFurrowModel::Drag(Rolling, S, Rolling.GetCenterS(), DryTide, false, Clean, Flat);
				}
			}
			TestTrue(TEXT("sobre rodillos se ha movido"), Rolling.GetCenterS() > 200.0f);
			TestTrue(TEXT("sin surco"), Clean.IsEmpty());
		});
	});
}

#endif
