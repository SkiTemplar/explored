#include "Debug/BeaconSoftlockModel.h"

#include "Narrative/BeaconDirectorModel.h"

namespace SoftlockDetail
{
	int32 Get(const TMap<FName, int32>& Map, const TCHAR* Id)
	{
		const int32* Found = Map.Find(FName(Id));
		return Found ? FMath::Max(*Found, 0) : 0;
	}

	TMap<FName, int32> With(TMap<FName, int32> Map, const TCHAR* Id, int32 Delta)
	{
		int32& Value = Map.FindOrAdd(FName(Id));
		Value = FMath::Max(Value + Delta, 0);
		return Map;
	}

	TMap<FName, int32> Kit()
	{
		TMap<FName, int32> Pack;
		Pack.Add(TEXT("cuchillo"), 1);
		Pack.Add(TEXT("cantimplora"), 1);
		Pack.Add(TEXT("barometro"), 1);
		return Pack;
	}

	void Require(TArray<FString>& Missing, const TCHAR* What, int32 Need, int32 Have)
	{
		if (Need > Have)
		{
			Missing.Add(FString::Printf(TEXT("%s: faltan %d"), What, Need - Have));
		}
	}

	/** Lo que pide el camino crítico que queda, en materiales básicos. */
	struct FNeeds
	{
		int32 Logs = 0;
		int32 Ropes = 0;
		int32 Fibers = 0;
		int32 Bamboo = 0;
		int32 Wood = 0;
		int32 Bowls = 0;
		int32 Fish = 0;
		bool bLens = false;
	};

	FNeeds ComputeNeeds(const FSoftlockSnapshot& S, const TMap<FName, int32>& Pack, const FSoftlockRules& Rules)
	{
		FNeeds Needs;
		const FBeaconProgressState& P = S.Progress;
		Needs.bLens = !P.bLensCollected && Get(Pack, TEXT("lente")) == 0;
		if (Needs.bLens)
		{
			if (!P.bRaftBuilt)
			{
				Needs.Logs += FMath::Max(Rules.LogsForRaft - FMath::Max(S.RaftLogsPlaced, 0), 0);
				Needs.Ropes += Rules.RopesForRaft;
			}
			if (Get(Pack, TEXT("remo")) == 0)
			{
				Needs.Logs += 1;
				Needs.Ropes += 1;
			}
		}
		const bool bWickDone = P.bWickMade || FBeaconProgressModel::IsSlotFilled(P, ELanternSlot::Wick);
		if (!bWickDone && Get(Pack, TEXT("mecha")) == 0)
		{
			Needs.Fibers += Rules.FibersPerWick;
		}
		const int32 Jars = FBeaconProgressModel::IsSlotFilled(P, ELanternSlot::Oil)
			? 0 : FMath::Max(FBeaconProgressModel::RequiredOilJars - Get(Pack, TEXT("tarro_aceite")), 0);
		if (Jars > 0)
		{
			Needs.Wood = Rules.WoodPerFire;
			Needs.Bowls = Jars;
			Needs.Fish = FMath::Max(Jars * Rules.FishPerJar - Get(Pack, TEXT("pez_crudo")), 0);
			if (Needs.Fish > 0 && Get(Pack, TEXT("cana_pescar")) == 0)
			{
				Needs.Bamboo += 1;
				Needs.Ropes += 1;
			}
		}
		Needs.Fibers += FMath::Max(Needs.Ropes - Get(Pack, TEXT("cuerda")), 0) * Rules.FibersPerRope;
		return Needs;
	}

	/** Suma la entrega de la caja a la mochila si la caja sigue cerrada y falta algo. */
	TMap<FName, int32> OpenCrate(const FSoftlockSnapshot& S, FSoftlockVerdict& Verdict)
	{
		TMap<FName, int32> Pack = S.Pack;
		if (S.Crate.bDelivered)
		{
			return Pack;
		}
		Verdict.CrateItems = FSupplyCrateModel::ComputeMissing(S.Pack, S.Progress, S.World);
		for (const TPair<FName, int32>& Pair : Verdict.CrateItems)
		{
			Pack.FindOrAdd(Pair.Key) += Pair.Value;
		}
		return Pack;
	}

	FSoftlockSnapshot Snapshot(const TMap<FName, int32>& Pack, const TMap<FName, int32>& World, double Hours)
	{
		FSoftlockSnapshot S;
		S.Pack = Pack;
		S.World = World;
		S.AbsHours = Hours;
		return S;
	}

	enum class ETask : uint8 { Lens, Oil, Wick };

	const TCHAR* TaskName(ETask Task)
	{
		return Task == ETask::Lens ? TEXT("lente") : Task == ETask::Oil ? TEXT("aceite") : TEXT("mecha");
	}

	ELanternSlot SlotFor(ETask Task)
	{
		return Task == ETask::Lens ? ELanternSlot::Lens : Task == ETask::Oil ? ELanternSlot::Oil : ELanternSlot::Wick;
	}

	/** Una partida de la máquina de etapas con los hitos en el orden dado; true si llega al final. */
	bool PlayStory(const TArray<ETask>& Order, bool bPageOne, bool bLensEarly)
	{
		FBeaconRunState State;
		FBeaconFacts Facts;
		const auto Step = [&State, &Facts](double Hours)
		{
			Facts.AbsHours = Hours;
			State = FBeaconDirectorModel::Step(State, Facts).State;
		};
		Facts.MovedMeters = 20.0f;
		Facts.bDrankCoconut = true;
		Step(8.0);
		Facts.bFireLit = true;
		Facts.bHasCookedFish = true;
		Step(11.0);
		Facts.MetersToTower = 10.0f;
		Step(13.0);
		Facts.MetersToTower = -1.0f;
		if (bPageOne)
		{
			State = FBeaconDirectorModel::ReadPage(State, 1).State;
		}
		double Hours = bLensEarly ? 31.0 : 55.0;
		for (const ETask Task : Order)
		{
			Hours += 2.0;
			if (Task == ETask::Lens)
			{
				Facts.MetersToFreighter = 10.0f;
				Step(Hours - 0.5);
				Facts.bHasLens = true;
			}
			Facts.OilJarsInPack = Task == ETask::Oil ? 2 : Facts.OilJarsInPack;
			Facts.bHasWick = Facts.bHasWick || Task == ETask::Wick;
			Step(Hours);
			Facts.MetersToFreighter = -1.0f;
			if (bLensEarly && Task == Order.Last())
			{
				for (const double Cyclone : { 43.5, 46.0, 50.5, 54.0 })
				{
					Step(Cyclone);
				}
			}
		}
		Step(66.0);
		for (const ETask Task : Order)
		{
			State = FBeaconDirectorModel::FillSlot(State, SlotFor(Task)).State;
		}
		Facts.bHasLens = false;
		Facts.OilJarsInPack = 0;
		Facts.bHasWick = false;
		Step(67.0);
		State = FBeaconDirectorModel::Light(State, 68.5).State;
		Step(69.0);
		return State.Progress.Stage == EBeaconStage::Final && State.Progress.bBeaconLit;
	}
}

FSoftlockVerdict FBeaconSoftlockModel::Evaluate(const FSoftlockSnapshot& Snapshot, const FSoftlockRules& Rules)
{
	using namespace SoftlockDetail;
	FSoftlockVerdict Verdict;
	const TMap<FName, int32> Pack = OpenCrate(Snapshot, Verdict);
	const FNeeds Needs = ComputeNeeds(Snapshot, Pack, Rules);
	const TMap<FName, int32>& World = Snapshot.World;
	const auto Avail = [&Pack, &World](const TCHAR* Id) { return Get(Pack, Id) + Get(World, Id); };

	if (Needs.bLens)
	{
		const bool bReopens = Rules.RaftReopenHour < TNumericLimits<double>::Max();
		if (!(Snapshot.AbsHours < Rules.RaftLastHour) && !bReopens)
		{
			Verdict.Missing.Add(TEXT("lente: el mar ya no vuelve a abrirse"));
		}
	}
	Require(Verdict.Missing, TEXT("tronco"), Needs.Logs, Avail(TEXT("tronco")));
	Require(Verdict.Missing, TEXT("fibra"), Needs.Fibers, Avail(TEXT("fibra")));
	Require(Verdict.Missing, TEXT("cana_bambu"), Needs.Bamboo, Avail(TEXT("cana_bambu")));
	Require(Verdict.Missing, TEXT("lena"), Needs.Wood, Avail(TEXT("lena")));
	Require(Verdict.Missing, TEXT("coco_vacio"), Needs.Bowls, Get(Pack, TEXT("coco_vacio")) + Avail(TEXT("coco")));
	if (Needs.Fish > 0 && !Rules.bFishRespawns)
	{
		Require(Verdict.Missing, TEXT("pez_crudo"), Needs.Fish, Get(World, TEXT("pez_crudo")));
	}
	Verdict.bCompletable = Verdict.Missing.IsEmpty();
	return Verdict;
}

TArray<FSoftlockScenario> FBeaconSoftlockModel::StandardScenarios(const TMap<FName, int32>& IslandAtStart)
{
	using namespace SoftlockDetail;
	TArray<FSoftlockScenario> Out;
	const TMap<FName, int32> Kit = SoftlockDetail::Kit();
	const TMap<FName, int32>& Island = IslandAtStart;
	// Caña de pescar del día 1: 2 fibras y 1 caña de bambú; primera hoguera: 3 leñas.
	const TMap<FName, int32> AfterDay1 = With(With(With(Island, TEXT("fibra"), -2), TEXT("cana_bambu"), -1), TEXT("lena"), -3);
	const TMap<FName, int32> RodPack = With(Kit, TEXT("cana_pescar"), 1);
	// Balsa y remo: 5 troncos y 3 cuerdas (6 fibras).
	const TMap<FName, int32> AfterRaft = With(With(AfterDay1, TEXT("tronco"), -5), TEXT("fibra"), -6);

	Out.Add({ TEXT("ruta competente: inicio de la partida"), Snapshot(Kit, Island, 7.0) });

	FSoftlockScenario Early{ TEXT("orden: mecha y aceite el día 1, balsa después"),
		Snapshot(With(With(With(RodPack, TEXT("mecha"), 1), TEXT("tarro_aceite"), 2), TEXT("coco_vacio"), 0),
			With(With(AfterDay1, TEXT("fibra"), -3), TEXT("coco"), -2), 18.0) };
	Early.Snapshot.Progress.bWickMade = true;
	Early.Snapshot.Progress.OilJars = 2;
	Out.Add(Early);

	FSoftlockScenario LensFirst{ TEXT("orden: balsa y lente primero, aceite y mecha el día 3"),
		Snapshot(With(With(RodPack, TEXT("remo"), 1), TEXT("lente"), 1), AfterRaft, 55.0) };
	LensFirst.Snapshot.Progress.bRaftBuilt = true;
	LensFirst.Snapshot.Progress.bLensCollected = true;
	LensFirst.Snapshot.RaftLogsPlaced = 4;
	Out.Add(LensFirst);

	FSoftlockScenario NoPage{ TEXT("página 1 sin leer y sin lente al cerrarse el mar (D2 19:00)"),
		Snapshot(With(RodPack, TEXT("remo"), 1), AfterRaft, 43.0) };
	NoPage.Snapshot.Progress.Stage = EBeaconStage::Faro;
	NoPage.Snapshot.Progress.bRaftBuilt = true;
	NoPage.Snapshot.RaftLogsPlaced = 4;
	Out.Add(NoPage);

	FSoftlockScenario NoWood{ TEXT("agotados: toda la leña quemada en hogueras (caja)"),
		Snapshot(RodPack, With(AfterRaft, TEXT("lena"), -1000), 55.0) };
	NoWood.Snapshot.Progress.bRaftBuilt = true;
	NoWood.Snapshot.Progress.bLensCollected = true;
	NoWood.Snapshot.Pack = With(NoWood.Snapshot.Pack, TEXT("lente"), 1);
	Out.Add(NoWood);

	// Sumidero real de fibras: cañas de pescar de más (una por caña de bambú que quede).
	FSoftlockScenario Fibers{ TEXT("agotados: fibras en dos cañas de pescar de más"),
		Snapshot(RodPack, With(With(AfterDay1, TEXT("fibra"), -4), TEXT("cana_bambu"), -2), 18.0) };
	Out.Add(Fibers);

	FSoftlockScenario Burnt{ TEXT("agotados: los dos tarros quemados como combustible"),
		Snapshot(With(With(RodPack, TEXT("mecha"), 1), TEXT("lente"), 1),
			With(With(With(AfterRaft, TEXT("lena"), -6), TEXT("coco"), -4), TEXT("fibra"), -3), 56.0) };
	Burnt.Snapshot.Progress.bRaftBuilt = true;
	Burnt.Snapshot.Progress.bLensCollected = true;
	Burnt.Snapshot.Progress.bWickMade = true;
	Burnt.Snapshot.Progress.OilJars = 2;
	Out.Add(Burnt);

	FSoftlockScenario Coconuts{ TEXT("agotados: cocos de la isla bebidos (vacíos en la mochila)"),
		Snapshot(With(With(RodPack, TEXT("coco_vacio"), 4), TEXT("lente"), 1), With(AfterRaft, TEXT("coco"), -1000), 56.0) };
	Coconuts.Snapshot.Progress.bRaftBuilt = true;
	Coconuts.Snapshot.Progress.bLensCollected = true;
	Out.Add(Coconuts);
	return Out;
}

TArray<FString> FBeaconSoftlockModel::StoryOrderFailures()
{
	using namespace SoftlockDetail;
	TArray<FString> Failures;
	TArray<TArray<ETask>> Orders;
	const ETask Tasks[] = { ETask::Lens, ETask::Oil, ETask::Wick };
	for (int32 A = 0; A < 3; ++A)
	{
		for (int32 B = 0; B < 3; ++B)
		{
			const int32 C = 3 - A - B;
			if (A != B && C >= 0 && C < 3 && C != A && C != B)
			{
				Orders.Add({ Tasks[A], Tasks[B], Tasks[C] });
			}
		}
	}
	for (const TArray<ETask>& Order : Orders)
	{
		for (const bool bPageOne : { true, false })
		{
			for (const bool bLensEarly : { true, false })
			{
				if (!PlayStory(Order, bPageOne, bLensEarly))
				{
					Failures.Add(FString::Printf(TEXT("%s → %s → %s, página 1 %s, lente %s del ciclón"), TaskName(Order[0]),
						TaskName(Order[1]), TaskName(Order[2]), bPageOne ? TEXT("leída") : TEXT("sin leer"),
						bLensEarly ? TEXT("antes") : TEXT("después")));
				}
			}
		}
	}
	return Failures;
}

FString FBeaconSoftlockModel::Describe(const FSoftlockVerdict& Verdict)
{
	TArray<FString> Parts;
	for (const TPair<FName, int32>& Pair : Verdict.CrateItems)
	{
		Parts.Add(FString::Printf(TEXT("%d %s"), Pair.Value, *Pair.Key.ToString()));
	}
	const FString Crate = Parts.IsEmpty() ? FString(TEXT("la caja no hace falta")) : TEXT("la caja da ") + FString::Join(Parts, TEXT(", "));
	return Verdict.bCompletable ? Crate : TEXT("faltan: ") + FString::Join(Verdict.Missing, TEXT("; ")) + TEXT(" (") + Crate + TEXT(")");
}
