#include "Misc/AutomationTest.h"

#include "Achievements/AchievementsModel.h"
#include "Core/ExploredRandom.h"
#include "Fauna/LivestockModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace LivestockSpecDetail
{
	using ES = ELivestockSpecies;
	using EX = ELivestockSex;
	using EO = ELivestockOrigin;
	using ER = ELivestockResult;

	/** Ajustes con la cría asegurada (o imposible) para probar reglas sin azar. */
	FLivestockSettings Chance(float P)
	{
		FLivestockSettings S;
		S.BreedingChancePerDay = P;
		return S;
	}

	int32 Add(FLivestockModel& Model, int32 Pen, ES Species, EX Sex, EO Origin = EO::Traded)
	{
		int32 Id = INDEX_NONE;
		verify(Model.AddAnimal(Pen, Species, Sex, Origin, Id) == ER::Ok);
		return Id;
	}

	/** Deja en el comedero justo la comida de un día para los animales del corral. */
	void Fill(FLivestockModel& Model, int32 Pen)
	{
		const int32 Need = Model.NumAnimalsInPen(Pen) * Model.GetSettings().FeedPerDay - Model.FindPen(Pen)->Feed;
		int32 Accepted = 0;
		if (Need > 0)
		{
			Model.AddFeed(Pen, Need, Accepted);
		}
	}

	/** Cierra los días First..Last llenando antes el comedero de cada corral. */
	FLivestockDayReport FedDays(FLivestockModel& Model, int32 First, int32 Last)
	{
		FLivestockDayReport Total;
		for (int32 Day = First; Day <= Last; ++Day)
		{
			for (const FLivestockPen& Pen : Model.GetPens())
			{
				Fill(Model, Pen.Id);
			}
			const FLivestockDayReport R = Model.EndDay(Day);
			Total.Births.Append(R.Births);
			Total.Tamed.Append(R.Tamed);
			Total.WentWild.Append(R.WentWild);
			Total.GrewUp.Append(R.GrewUp);
			Total.EggsLaid += R.EggsLaid;
			Total.MilkGiven += R.MilkGiven;
			Total.DaysClosed += R.DaysClosed;
		}
		return Total;
	}

	/** Invariantes que ningún camino (acciones, días o carga) puede romper. */
	bool Invariants(const FLivestockModel& Model)
	{
		const FLivestockSettings& S = Model.GetSettings();
		if (Model.NumAnimals() > S.MaxAlivePerBase)
		{
			return false;
		}
		int32 LastId = 0;
		for (const FLivestockAnimal& A : Model.GetAnimals())
		{
			const FLivestockPen* Pen = Model.FindPen(A.PenId);
			if (A.Id <= LastId || Pen == nullptr || !FLivestockModel::PenAccepts(Pen->Kind, A.Species)
				|| (!A.bAdult && A.AgeDays >= S.DaysToAdult))
			{
				return false;
			}
			LastId = A.Id;
		}
		for (const FLivestockPen& P : Model.GetPens())
		{
			if (Model.NumAnimalsInPen(P.Id) > S.MaxAnimalsPerPen || P.Feed < 0 || P.Feed > S.TroughCapacity
				|| P.Eggs < 0 || P.Eggs > S.MaxStoredProducts || P.Milk < 0 || P.Milk > S.MaxStoredProducts)
			{
				return false;
			}
		}
		return true;
	}

	/** Una granja con de todo: gallinero, pocilga y corral de cabras, con crías y productos. */
	FLivestockModel Busy(uint32 Seed)
	{
		FLivestockModel Model(Chance(0.5f), Seed);
		const int32 Coop = Model.AddPen(ELivestockPenKind::Coop);
		const int32 Sty = Model.AddPen(ELivestockPenKind::Sty);
		const int32 Pen = Model.AddPen(ELivestockPenKind::Pen);
		Add(Model, Coop, ES::Chicken, EX::Male);
		Add(Model, Coop, ES::Chicken, EX::Female);
		Add(Model, Sty, ES::Pig, EX::Male, EO::Captured);
		Add(Model, Sty, ES::Pig, EX::Female);
		Add(Model, Pen, ES::Goat, EX::Male);
		Add(Model, Pen, ES::Goat, EX::Female);
		FedDays(Model, 4, 9);
		return Model;
	}
}

BEGIN_DEFINE_SPEC(FLivestockModelSpec, "Explored.Fauna.Livestock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FLivestockModelSpec)

void FLivestockModelSpec::Define()
{
	using namespace LivestockSpecDetail;

	Describe("los datos", [this]()
	{
		It("usa los ids de fases_futuras.json y building_pieces.json", [this]()
		{
			TestEqual(TEXT("gallina"), FLivestockModel::SpeciesId(ES::Chicken), FName(TEXT("gallina")));
			TestEqual(TEXT("cerdo"), FLivestockModel::SpeciesId(ES::Pig), FName(TEXT("cerdo")));
			TestEqual(TEXT("cabra"), FLivestockModel::SpeciesId(ES::Goat), FName(TEXT("cabra")));
			TestEqual(TEXT("gallinero"), FLivestockModel::PenPieceId(ELivestockPenKind::Coop), FName(TEXT("gallinero")));
			TestEqual(TEXT("pocilga"), FLivestockModel::PenPieceId(ELivestockPenKind::Sty), FName(TEXT("pocilga")));
			TestEqual(TEXT("corral"), FLivestockModel::PenPieceId(ELivestockPenKind::Pen), FName(TEXT("corral")));
			for (int32 S = 0; S < static_cast<int32>(ES::Count); ++S)
			{
				ES Parsed = ES::Count;
				TestTrue(TEXT("Ida y vuelta de la especie"), FLivestockModel::ParseSpecies(FLivestockModel::SpeciesId(static_cast<ES>(S)), Parsed)
					&& Parsed == static_cast<ES>(S));
			}
			ES Unused = ES::Count;
			TestFalse(TEXT("Ninguna especie vacía"), FLivestockModel::ParseSpecies(NAME_None, Unused));
			TestFalse(TEXT("El cerdo salvaje no es doméstico"), FLivestockModel::ParseSpecies(FName(TEXT("cerdo_salvaje")), Unused));
			ELivestockPenKind Kind = ELivestockPenKind::Count;
			TestTrue(TEXT("pocilga"), FLivestockModel::ParsePenPiece(FName(TEXT("pocilga")), Kind) && Kind == ELivestockPenKind::Sty);
			TestFalse(TEXT("Un bancal no es corral"), FLivestockModel::ParsePenPiece(FName(TEXT("bancal")), Kind));
			TestEqual(TEXT("Estadística de crías"), FString(FLivestockModel::StatSpeciesRaised), FString(TEXT("livestock_species_raised")));
			TestEqual(TEXT("Estadística de huevos"), FString(FLivestockModel::StatEggsCollected), FString(TEXT("eggs_collected")));
		});

		It("empieza con los números de la biblia 02 §10.2 y §11.5", [this]()
		{
			const FLivestockSettings S;
			TestEqual(TEXT("8 por base"), S.MaxAlivePerBase, 8);
			TestEqual(TEXT("8 por corral"), S.MaxAnimalsPerPen, 8);
			TestEqual(TEXT("15 % al día"), S.BreedingChancePerDay, 0.15f);
			TestEqual(TEXT("6 días a adulto"), S.DaysToAdult, 6);
			TestEqual(TEXT("1 comida al día"), S.FeedPerDay, 1);
			TestEqual(TEXT("3 días para domar"), S.DaysFedToTame, 3);
			TestEqual(TEXT("2 días para volver a salvaje"), S.DaysUnfedToWild, 2);
		});

		It("sanea ajustes absurdos", [this]()
		{
			FLivestockSettings Bad;
			Bad.BreedingChancePerDay = std::numeric_limits<float>::quiet_NaN();
			Bad.MaxAlivePerBase = -3;
			Bad.MaxAnimalsPerPen = -1;
			Bad.DaysUnfedToWild = 0;
			Bad.MaxCatchUpDays = -5;
			Bad.TroughCapacity = -10;
			const FLivestockSettings S = Bad.Sanitized();
			TestEqual(TEXT("NaN no cría"), S.BreedingChancePerDay, 0.0f);
			TestEqual(TEXT("Tope de base"), S.MaxAlivePerBase, 0);
			TestEqual(TEXT("Tope de corral"), S.MaxAnimalsPerPen, 0);
			TestEqual(TEXT("Al menos un día para volver a salvaje"), S.DaysUnfedToWild, 1);
			TestEqual(TEXT("Al menos un día de recuperación"), S.MaxCatchUpDays, 1);
			TestEqual(TEXT("Comedero"), S.TroughCapacity, 0);

			FLivestockSettings Big;
			Big.BreedingChancePerDay = 7.0f;
			TestEqual(TEXT("Probabilidad hasta 1"), Big.Sanitized().BreedingChancePerDay, 1.0f);

			FLivestockModel Model(Bad);
			const int32 Pen = Model.AddPen(ELivestockPenKind::Pen);
			int32 Id = 0;
			TestEqual(TEXT("Con tope 0 no entra nadie"), Model.AddAnimal(Pen, ES::Goat, EX::Male, EO::Traded, Id), ER::BaseFull);
			TestEqual(TEXT("Y el id queda vacío"), Id, INDEX_NONE);
		});
	});

	Describe("los corrales", [this]()
	{
		It("el gallinero solo admite gallinas, la pocilga cerdos y el corral cualquiera", [this]()
		{
			FLivestockModel Model;
			const int32 Coop = Model.AddPen(ELivestockPenKind::Coop);
			const int32 Sty = Model.AddPen(ELivestockPenKind::Sty);
			const int32 Pen = Model.AddPen(ELivestockPenKind::Pen);
			TestEqual(TEXT("Cabra en el gallinero"), Model.CanAddAnimal(Coop, ES::Goat), ER::WrongPen);
			TestEqual(TEXT("Gallina en la pocilga"), Model.CanAddAnimal(Sty, ES::Chicken), ER::WrongPen);
			TestEqual(TEXT("Gallina en el gallinero"), Model.CanAddAnimal(Coop, ES::Chicken), ER::Ok);
			TestEqual(TEXT("Cerdo en la pocilga"), Model.CanAddAnimal(Sty, ES::Pig), ER::Ok);
			for (int32 S = 0; S < static_cast<int32>(ES::Count); ++S)
			{
				TestEqual(TEXT("El corral genérico admite todo"), Model.CanAddAnimal(Pen, static_cast<ES>(S)), ER::Ok);
			}
			TestEqual(TEXT("Corral inexistente"), Model.CanAddAnimal(99, ES::Pig), ER::UnknownPen);
			TestEqual(TEXT("Especie fuera de rango"), Model.CanAddAnimal(Pen, ES::Count), ER::InvalidArgument);
			TestEqual(TEXT("Tipo de corral fuera de rango"), Model.AddPen(ELivestockPenKind::Count), INDEX_NONE);
		});

		It("rechaza orígenes y sexos que no existen", [this]()
		{
			FLivestockModel Model;
			const int32 Pen = Model.AddPen(ELivestockPenKind::Pen);
			int32 Id = 0;
			TestEqual(TEXT("Una cría solo nace"), Model.AddAnimal(Pen, ES::Goat, EX::Male, EO::Born, Id), ER::InvalidArgument);
			TestEqual(TEXT("Sexo fuera de rango"), Model.AddAnimal(Pen, ES::Goat, static_cast<EX>(7), EO::Traded, Id), ER::InvalidArgument);
			TestEqual(TEXT("Origen fuera de rango"), Model.AddAnimal(Pen, ES::Goat, EX::Male, static_cast<EO>(9), Id), ER::InvalidArgument);
			TestEqual(TEXT("Nada entró"), Model.NumAnimals(), 0);
		});

		It("tope de 8 animales por corral y por base", [this]()
		{
			FLivestockModel Model;
			const int32 A = Model.AddPen(ELivestockPenKind::Pen);
			const int32 B = Model.AddPen(ELivestockPenKind::Coop);
			for (int32 I = 0; I < 5; ++I)
			{
				Add(Model, A, ES::Goat, I % 2 ? EX::Male : EX::Female);
			}
			for (int32 I = 0; I < 3; ++I)
			{
				Add(Model, B, ES::Chicken, EX::Female);
			}
			TestTrue(TEXT("Base llena"), Model.IsBaseFull());
			TestTrue(TEXT("Lleno se ve lleno aunque el corral tenga sitio"), Model.IsPenFull(A));
			int32 Id = 0;
			TestEqual(TEXT("El noveno no entra"), Model.AddAnimal(A, ES::Goat, EX::Male, EO::Traded, Id), ER::BaseFull);
			const int32 C = Model.AddPen(ELivestockPenKind::Pen);
			TestEqual(TEXT("Ni en un corral nuevo"), Model.CanAddAnimal(C, ES::Pig), ER::BaseFull);

			FLivestockSettings Wide;
			Wide.MaxAlivePerBase = 20;
			FLivestockModel Big(Wide);
			const int32 P = Big.AddPen(ELivestockPenKind::Pen);
			for (int32 I = 0; I < 8; ++I)
			{
				Add(Big, P, ES::Pig, EX::Female);
			}
			TestEqual(TEXT("Con base holgada manda el tope del corral"), Big.CanAddAnimal(P, ES::Pig), ER::PenFull);
			TestTrue(TEXT("Corral lleno"), Big.IsPenFull(P));
			const int32 Q = Big.AddPen(ELivestockPenKind::Sty);
			TestEqual(TEXT("Otro corral sí tiene sitio"), Big.CanAddAnimal(Q, ES::Pig), ER::Ok);
			const int32 Moved = Big.GetAnimals()[0].Id;
			TestEqual(TEXT("Se pasa un cerdo a la pocilga"), Big.MoveAnimal(Moved, Q), ER::Ok);
			TestEqual(TEXT("Ya no está lleno"), Big.IsPenFull(P), false);
			Add(Big, P, ES::Pig, EX::Male);
			TestEqual(TEXT("No vuelve a un corral lleno"), Big.MoveAnimal(Moved, P), ER::PenFull);
			TestEqual(TEXT("Moverlo a donde ya está"), Big.MoveAnimal(Moved, Q), ER::Ok);
			TestEqual(TEXT("Un cerdo no va al gallinero"), Big.MoveAnimal(Moved, Big.AddPen(ELivestockPenKind::Coop)), ER::WrongPen);
			TestEqual(TEXT("Animal inexistente"), Big.MoveAnimal(999, Q), ER::UnknownAnimal);
			TestEqual(TEXT("Corral inexistente"), Big.MoveAnimal(Moved, 999), ER::UnknownPen);
			TestEqual(TEXT("Se queda en la pocilga"), Big.FindAnimal(Moved)->PenId, Q);
		});

		It("no retira un corral con animales dentro", [this]()
		{
			FLivestockModel Model;
			const int32 Pen = Model.AddPen(ELivestockPenKind::Sty);
			const int32 Id = Add(Model, Pen, ES::Pig, EX::Male);
			TestEqual(TEXT("Con un cerdo dentro"), Model.RemovePen(Pen), ER::PenNotEmpty);
			TestEqual(TEXT("Se saca el cerdo"), Model.RemoveAnimal(Id), ER::Ok);
			TestEqual(TEXT("Ya no existe"), Model.RemoveAnimal(Id), ER::UnknownAnimal);
			TestEqual(TEXT("Vacío se retira"), Model.RemovePen(Pen), ER::Ok);
			TestEqual(TEXT("Dos veces no"), Model.RemovePen(Pen), ER::UnknownPen);
		});
	});

	Describe("la comida y la doma", [this]()
	{
		It("el comedero acepta lo que cabe y cada animal come uno al día por orden de id", [this]()
		{
			FLivestockModel Model;
			const int32 Pen = Model.AddPen(ELivestockPenKind::Pen);
			int32 Accepted = 0;
			TestEqual(TEXT("Cantidad cero"), Model.AddFeed(Pen, 0, Accepted), ER::InvalidArgument);
			TestEqual(TEXT("Cantidad negativa"), Model.AddFeed(Pen, -4, Accepted), ER::InvalidArgument);
			TestEqual(TEXT("Corral inexistente"), Model.AddFeed(42, 1, Accepted), ER::UnknownPen);
			TestEqual(TEXT("Cabe hasta el tope"), Model.AddFeed(Pen, 100, Accepted), ER::Ok);
			TestEqual(TEXT("16 unidades"), Accepted, Model.GetSettings().TroughCapacity);
			TestEqual(TEXT("Lleno"), Model.AddFeed(Pen, 1, Accepted), ER::TroughFull);
			TestEqual(TEXT("Nada aceptado"), Accepted, 0);

			FLivestockModel Two;
			const int32 P = Two.AddPen(ELivestockPenKind::Pen);
			const int32 First = Add(Two, P, ES::Goat, EX::Male);
			const int32 Second = Add(Two, P, ES::Goat, EX::Female);
			Two.AddFeed(P, 1, Accepted);
			Two.EndDay(4);
			TestTrue(TEXT("El primero come"), Two.FindAnimal(First)->bFedToday);
			TestFalse(TEXT("El segundo se queda sin"), Two.FindAnimal(Second)->bFedToday);
			TestEqual(TEXT("Comedero vacío"), Two.FindPen(P)->Feed, 0);
		});

		It("un animal capturado se doma con 3 días seguidos comiendo", [this]()
		{
			FLivestockModel Model;
			const int32 Pen = Model.AddPen(ELivestockPenKind::Sty);
			const int32 Id = Add(Model, Pen, ES::Pig, EX::Male, EO::Captured);
			TestFalse(TEXT("Llega sin domar"), Model.FindAnimal(Id)->bTame);
			FedDays(Model, 4, 5);
			Model.EndDay(6); // un día sin comer rompe la racha
			FedDays(Model, 7, 8);
			TestFalse(TEXT("Dos y dos no son tres seguidos"), Model.FindAnimal(Id)->bTame);
			const FLivestockDayReport R = FedDays(Model, 9, 9);
			TestTrue(TEXT("Domado al tercero"), Model.FindAnimal(Id)->bTame);
			TestTrue(TEXT("Y se avisa"), R.Tamed.Contains(Id));
		});

		It("un domado sin comer 2 días vuelve a salvaje y huye al abrir, pero no muere", [this]()
		{
			FLivestockModel Model(Chance(0.0f));
			const int32 Pen = Model.AddPen(ELivestockPenKind::Pen);
			const int32 Goat = Add(Model, Pen, ES::Goat, EX::Female);
			const int32 Wary = Add(Model, Pen, ES::Goat, EX::Male, EO::Captured);
			FedDays(Model, 4, 6);
			TestTrue(TEXT("Los dos domados"), Model.FindAnimal(Goat)->bTame && Model.FindAnimal(Wary)->bTame);
			TestFalse(TEXT("Un día sin comer aún no"), Model.EndDay(7).WentWild.Num() > 0);
			const FLivestockDayReport R = Model.EndDay(8);
			TestEqual(TEXT("Los dos vuelven a salvaje"), R.WentWild.Num(), 2);
			Model.EndDay(30);
			TestEqual(TEXT("Nadie muere de hambre"), Model.NumAnimals(), 2);
			TArray<int32> Escaped;
			TestEqual(TEXT("Abrir"), Model.OpenGate(Pen, Escaped), ER::Ok);
			TestEqual(TEXT("Huyen los dos"), Escaped.Num(), 2);
			TestEqual(TEXT("Corral vacío"), Model.NumAnimalsInPen(Pen), 0);
			TestEqual(TEXT("Puerta de un corral inexistente"), Model.OpenGate(77, Escaped), ER::UnknownPen);
		});

		It("abrir la puerta no deja escapar a los domados", [this]()
		{
			FLivestockModel Model;
			const int32 Pen = Model.AddPen(ELivestockPenKind::Pen);
			Add(Model, Pen, ES::Goat, EX::Female);
			const int32 Wild = Add(Model, Pen, ES::Goat, EX::Male, EO::Captured);
			TArray<int32> Escaped;
			Model.OpenGate(Pen, Escaped);
			TestEqual(TEXT("Solo el capturado"), Escaped.Num(), 1);
			TestTrue(TEXT("Es el capturado"), Escaped.Num() == 1 && Escaped[0] == Wild);
			TestEqual(TEXT("Queda la domada"), Model.NumAnimals(), 1);
		});
	});

	Describe("la cría", [this]()
	{
		It("necesita macho y hembra adultos, domados y comidos en el mismo corral", [this]()
		{
			FLivestockModel Model(Chance(1.0f));
			const int32 A = Model.AddPen(ELivestockPenKind::Pen);
			const int32 B = Model.AddPen(ELivestockPenKind::Pen);
			Add(Model, A, ES::Goat, EX::Female);
			Add(Model, A, ES::Goat, EX::Female);
			Add(Model, B, ES::Goat, EX::Male);
			Add(Model, A, ES::Pig, EX::Male);
			TestEqual(TEXT("Dos hembras y un cerdo no crían"), FedDays(Model, 4, 6).Births.Num(), 0);

			FLivestockModel Hungry(Chance(1.0f));
			const int32 P = Hungry.AddPen(ELivestockPenKind::Pen);
			Add(Hungry, P, ES::Goat, EX::Female);
			Add(Hungry, P, ES::Goat, EX::Male);
			TestEqual(TEXT("Sin comida no hay cría"), Hungry.EndDay(4).Births.Num(), 0);

			FLivestockModel Wild(Chance(1.0f));
			const int32 W = Wild.AddPen(ELivestockPenKind::Pen);
			Add(Wild, W, ES::Goat, EX::Female, EO::Captured);
			Add(Wild, W, ES::Goat, EX::Male, EO::Captured);
			TestEqual(TEXT("Mientras se doman, nada"), FedDays(Wild, 4, 5).Births.Num(), 0);
			TestEqual(TEXT("Domados, crían"), FedDays(Wild, 6, 6).Births.Num(), 1);
		});

		It("cada pareja cría por separado y nunca pasa del tope", [this]()
		{
			FLivestockModel Model(Chance(1.0f));
			const int32 Pen = Model.AddPen(ELivestockPenKind::Coop);
			for (int32 I = 0; I < 3; ++I)
			{
				Add(Model, Pen, ES::Chicken, EX::Male);
				Add(Model, Pen, ES::Chicken, EX::Female);
			}
			const FLivestockDayReport R = FedDays(Model, 4, 4);
			TestEqual(TEXT("Tres parejas, pero solo caben dos crías"), R.Births.Num(), 2);
			TestEqual(TEXT("Ocho animales"), Model.NumAnimals(), 8);
			TestEqual(TEXT("Y ni uno más en un mes"), FedDays(Model, 5, 34).Births.Num(), 0);
			TestTrue(TEXT("Invariantes"), Invariants(Model));
		});

		It("con un solo hueco libre no gana siempre el mismo corral", [this]()
		{
			int32 Chickens = 0;
			int32 Pigs = 0;
			for (uint32 Seed = 1; Seed <= 60; ++Seed)
			{
				FLivestockSettings S = Chance(1.0f);
				S.MaxAlivePerBase = 5;
				FLivestockModel Model(S, Seed);
				const int32 Coop = Model.AddPen(ELivestockPenKind::Coop);
				const int32 Sty = Model.AddPen(ELivestockPenKind::Sty);
				Add(Model, Coop, ES::Chicken, EX::Male);
				Add(Model, Coop, ES::Chicken, EX::Female);
				Add(Model, Sty, ES::Pig, EX::Male);
				Add(Model, Sty, ES::Pig, EX::Female);
				const FLivestockDayReport R = FedDays(Model, 4, 4);
				TestEqual(TEXT("Una sola cría"), R.Births.Num(), 1);
				for (const FLivestockBirth& B : R.Births)
				{
					(B.Species == ES::Chicken ? Chickens : Pigs)++;
				}
			}
			TestTrue(FString::Printf(TEXT("%d pollitos y %d lechones"), Chickens, Pigs), Chickens > 15 && Pigs > 15);
		});

		It("la cría es adulta a los 6 días justos", [this]()
		{
			FLivestockModel Model(Chance(1.0f));
			const int32 Pen = Model.AddPen(ELivestockPenKind::Sty);
			Add(Model, Pen, ES::Pig, EX::Male);
			Add(Model, Pen, ES::Pig, EX::Female);
			FLivestockSettings Once = Model.GetSettings();
			const FLivestockDayReport Born = FedDays(Model, 4, 4);
			if (!TestEqual(TEXT("Nace una"), Born.Births.Num(), 1))
			{
				return;
			}
			const int32 Kid = Born.Births[0].AnimalId;
			TestFalse(TEXT("Recién nacida"), Model.FindAnimal(Kid)->bAdult);
			TestEqual(TEXT("Especie de sus padres"), Model.FindAnimal(Kid)->Species, ES::Pig);
			Model.EndDay(9);
			TestFalse(TEXT("A los 5 días aún es cría"), Model.FindAnimal(Kid)->bAdult);
			const FLivestockDayReport Grow = Model.EndDay(10);
			TestTrue(TEXT("A los 6 es adulta"), Model.FindAnimal(Kid)->bAdult);
			TestTrue(TEXT("Y se avisa"), Grow.GrewUp.Contains(Kid));
			TestEqual(TEXT("Ajustes de fábrica"), Once.DaysToAdult, 6);
		});

		It("la probabilidad diaria por pareja ronda el 15 %", [this]()
		{
			int32 Rolls = 0;
			int32 Births = 0;
			for (uint32 Seed = 1; Seed <= 40; ++Seed)
			{
				FLivestockModel Model(FLivestockSettings(), Seed);
				const int32 Pen = Model.AddPen(ELivestockPenKind::Pen);
				Add(Model, Pen, ES::Goat, EX::Male);
				Add(Model, Pen, ES::Goat, EX::Female);
				for (int32 Day = 4; Day < 104; ++Day)
				{
					const FLivestockDayReport R = FedDays(Model, Day, Day);
					++Rolls;
					Births += R.Births.Num();
					for (const FLivestockBirth& B : R.Births)
					{
						Model.RemoveAnimal(B.AnimalId); // se vende la cría: la pareja sigue sola
					}
				}
			}
			const double Rate = static_cast<double>(Births) / Rolls;
			TestTrue(FString::Printf(TEXT("Tasa %.3f en %d días de pareja"), Rate, Rolls), Rate > 0.13 && Rate < 0.17);
		});

		It("las crías salen de los dos sexos", [this]()
		{
			FLivestockSettings S = Chance(1.0f);
			S.MaxAlivePerBase = 255;
			S.MaxAnimalsPerPen = 255;
			int32 Males = 0;
			int32 Females = 0;
			for (uint32 Seed = 1; Seed <= 50; ++Seed)
			{
				FLivestockModel Model(S, Seed);
				const int32 Pen = Model.AddPen(ELivestockPenKind::Pen);
				Add(Model, Pen, ES::Goat, EX::Male);
				Add(Model, Pen, ES::Goat, EX::Female);
				for (const FLivestockBirth& B : FedDays(Model, 4, 7).Births)
				{
					(Model.FindAnimal(B.AnimalId)->Sex == EX::Male ? Males : Females)++;
				}
			}
			TestTrue(FString::Printf(TEXT("%d machos y %d hembras"), Males, Females), Males > 60 && Females > 60);
		});
	});

	Describe("los productos", [this]()
	{
		It("una gallina adulta comida pone un huevo al día; el gallo no", [this]()
		{
			FLivestockModel Model(Chance(0.0f));
			const int32 Coop = Model.AddPen(ELivestockPenKind::Coop);
			Add(Model, Coop, ES::Chicken, EX::Female);
			Add(Model, Coop, ES::Chicken, EX::Male);
			TestEqual(TEXT("Tres días, tres huevos"), FedDays(Model, 4, 6).EggsLaid, 3);
			TestEqual(TEXT("Sin comer, ninguno"), Model.EndDay(7).EggsLaid, 0);
			FLivestockCollect Got;
			TestEqual(TEXT("Recoger"), Model.Collect(Coop, Got), ER::Ok);
			TestEqual(TEXT("Tres huevos"), Got.Eggs, 3);
			TestEqual(TEXT("Sin leche"), Got.Milk, 0);
			Model.Collect(Coop, Got);
			TestEqual(TEXT("Recoger otra vez no duplica"), Got.Eggs, 0);
			TestEqual(TEXT("Corral inexistente"), Model.Collect(9, Got), ER::UnknownPen);
		});

		It("los huevos se acumulan hasta el tope del corral", [this]()
		{
			FLivestockModel Model(Chance(0.0f));
			const int32 Coop = Model.AddPen(ELivestockPenKind::Coop);
			Add(Model, Coop, ES::Chicken, EX::Female);
			Add(Model, Coop, ES::Chicken, EX::Female);
			FedDays(Model, 4, 40);
			TestEqual(TEXT("Tope"), Model.FindPen(Coop)->Eggs, Model.GetSettings().MaxStoredProducts);
		});

		It("una cabra da leche mientras su cría es pequeña", [this]()
		{
			FLivestockModel Model(Chance(1.0f));
			const int32 Pen = Model.AddPen(ELivestockPenKind::Pen);
			Add(Model, Pen, ES::Goat, EX::Male);
			const int32 Mother = Add(Model, Pen, ES::Goat, EX::Female);
			TestEqual(TEXT("Sin cría, sin leche el primer día"), FedDays(Model, 4, 4).MilkGiven, 0);
			TestEqual(TEXT("Parió el día 4"), Model.FindAnimal(Mother)->LastBirthDay, 4);
			// Sin comer ya no cría ni da leche: la racha de leche se mide sin más partos.
			FLivestockModel Once(Chance(0.0f));
			TestTrue(TEXT("Guardado"), Once.FromValue(Model.ToValue()));
			const FLivestockDayReport R = FedDays(Once, 5, 12);
			TestEqual(TEXT("Seis días de leche"), R.MilkGiven, 6);
			FLivestockCollect Got;
			Once.Collect(Pen, Got);
			TestEqual(TEXT("Seis de leche en el corral"), Got.Milk, 6);
		});
	});

	Describe("el tiempo", [this]()
	{
		It("cerrar dos veces el mismo día no hace nada", [this]()
		{
			FLivestockModel Model = Busy(3);
			const FLivestockModel Before = Model;
			TestEqual(TEXT("Día ya cerrado"), Model.EndDay(9).DaysClosed, 0);
			TestEqual(TEXT("Día anterior"), Model.EndDay(2).DaysClosed, 0);
			TestEqual(TEXT("Día negativo"), Model.EndDay(-5).DaysClosed, 0);
			TestTrue(TEXT("Sin cambios"), Model == Before);
		});

		It("saltarse días equivale a cerrarlos uno a uno", [this]()
		{
			FLivestockModel A = Busy(11);
			FLivestockModel B = A;
			const FLivestockDayReport Jump = A.EndDay(15);
			for (int32 Day = 10; Day <= 15; ++Day)
			{
				B.EndDay(Day);
			}
			TestEqual(TEXT("Seis días"), Jump.DaysClosed, 6);
			TestTrue(TEXT("Mismo estado"), A == B);
		});

		It("acota los días que recupera de golpe", [this]()
		{
			FLivestockModel Model = Busy(5);
			const FLivestockDayReport R = Model.EndDay(MAX_int32);
			TestEqual(TEXT("Como mucho MaxCatchUpDays"), R.DaysClosed, Model.GetSettings().MaxCatchUpDays);
			TestEqual(TEXT("Queda cerrado hasta ese día"), Model.GetLastEndedDay(), MAX_int32);
			TestEqual(TEXT("Y ya no avanza más"), Model.EndDay(MAX_int32).DaysClosed, 0);
			TestTrue(TEXT("Invariantes"), Invariants(Model));
		});
	});

	Describe("el determinismo", [this]()
	{
		It("la misma semilla da la misma granja; otra semilla, otra", [this]()
		{
			TestTrue(TEXT("Misma semilla"), Busy(21) == Busy(21));
			bool bAnyDiffers = false;
			for (uint32 Seed = 22; Seed < 30 && !bAnyDiffers; ++Seed)
			{
				FLivestockModel A = Busy(21);
				FLivestockModel B = Busy(Seed);
				bAnyDiffers = A.ToValue() != B.ToValue();
			}
			TestTrue(TEXT("Alguna semilla cambia las crías"), bAnyDiffers);
		});

		It("guardar y cargar a mitad no cambia el futuro", [this]()
		{
			FLivestockModel Straight = Busy(8);
			FLivestockModel Loaded(Straight.GetSettings(), Straight.GetSeed());
			TestTrue(TEXT("Carga"), Loaded.FromValue(Straight.ToValue()));
			TestTrue(TEXT("Igual al cargar"), Loaded == Straight);
			FedDays(Straight, 10, 30);
			FedDays(Loaded, 10, 30);
			TestTrue(TEXT("Igual veinte días después"), Loaded == Straight);
		});

		It("el orden de las tiradas no depende de lo que haya en otros corrales", [this]()
		{
			// Retirar una pocilga vacía no puede cambiar las crías del corral de cabras: B la
			// retira y C la conserva (con un cerdo que come aparte), y el corral tiene el mismo id.
			FLivestockSettings Roomy = Chance(0.5f);
			Roomy.MaxAlivePerBase = 255;
			Roomy.MaxAnimalsPerPen = 255;
			FLivestockModel B(Roomy, 99);
			const int32 Spare = B.AddPen(ELivestockPenKind::Sty);
			const int32 PenB = B.AddPen(ELivestockPenKind::Pen);
			B.RemovePen(Spare);
			FLivestockModel C(Roomy, 99);
			Add(C, C.AddPen(ELivestockPenKind::Sty), ES::Pig, EX::Female);
			const int32 PenC = C.AddPen(ELivestockPenKind::Pen);
			TestEqual(TEXT("Ids iguales"), PenB, PenC);
			Add(B, PenB, ES::Goat, EX::Male);
			Add(B, PenB, ES::Goat, EX::Female);
			Add(C, PenC, ES::Goat, EX::Male);
			Add(C, PenC, ES::Goat, EX::Female);
			const FLivestockDayReport RB = FedDays(B, 4, 20);
			const FLivestockDayReport RC = FedDays(C, 4, 20);
			TestEqual(TEXT("Mismas crías"), RB.Births.Num(), RC.Births.Num());
			for (int32 I = 0; I < FMath::Min(RB.Births.Num(), RC.Births.Num()); ++I)
			{
				TestEqual(TEXT("Mismo día"), RB.Births[I].Day, RC.Births[I].Day);
			}
		});
	});

	Describe("el guardado", [this]()
	{
		It("ida y vuelta exacta", [this]()
		{
			const FLivestockModel Model = Busy(4);
			FLivestockModel Copy(Model.GetSettings(), Model.GetSeed());
			TestTrue(TEXT("Carga"), Copy.FromValue(Model.ToValue()));
			TestTrue(TEXT("Igual"), Copy == Model);
			FSaveValue Parsed;
			FString Error;
			TestTrue(TEXT("Pasa por texto"), FSaveText::Parse(FSaveText::Write(Model.ToValue()), Parsed, Error));
			FLivestockModel FromText(Model.GetSettings(), Model.GetSeed());
			TestTrue(TEXT("Carga del texto"), FromText.FromValue(Parsed));
			TestTrue(TEXT("Igual desde texto"), FromText == Model);
		});

		It("rechaza guardados rotos y deja la granja vacía", [this]()
		{
			const FSaveValue Good = Busy(6).ToValue();
			const auto Rejects = [this, &Good](const TCHAR* What, TFunctionRef<void(FSaveValue&)> Break)
			{
				FSaveValue Bad = Good;
				Break(Bad);
				FLivestockModel Model = Busy(1);
				TestFalse(What, Model.FromValue(Bad));
				TestEqual(FString::Printf(TEXT("%s: vacía"), What), Model.NumAnimals() + Model.GetPens().Num(), 0);
			};
			const auto AnimalField = [](FSaveValue& V, int32 Animal, int32 Field) -> FSaveValue&
			{
				return *V.Find(TEXT("animals"))->AtMutable(Animal * 11 + Field);
			};
			Rejects(TEXT("Versión"), [](FSaveValue& V) { V.Set(TEXT("v"), FSaveValue::MakeInt(2)); });
			Rejects(TEXT("Sin animales"), [](FSaveValue& V) { V.Remove(TEXT("animals")); });
			Rejects(TEXT("Lista truncada"), [](FSaveValue& V) { V.Set(TEXT("pens"), FSaveValue::MakeArray()); });
			Rejects(TEXT("No es objeto"), [](FSaveValue& V) { V = FSaveValue::MakeArray(); });
			Rejects(TEXT("Día como texto"), [](FSaveValue& V) { V.Set(TEXT("day"), FSaveValue::MakeString(TEXT("9"))); });
			Rejects(TEXT("Id siguiente cero"), [](FSaveValue& V) { V.Set(TEXT("nextAnimal"), FSaveValue::MakeInt(0)); });
			Rejects(TEXT("Id siguiente enorme"), [](FSaveValue& V) { V.Set(TEXT("nextPen"), FSaveValue::MakeInt(int64(1) << 40)); });
			Rejects(TEXT("Especie fuera de rango"), [&AnimalField](FSaveValue& V) { AnimalField(V, 0, 1) = FSaveValue::MakeInt(3); });
			Rejects(TEXT("Sexo fuera de rango"), [&AnimalField](FSaveValue& V) { AnimalField(V, 0, 2) = FSaveValue::MakeInt(2); });
			Rejects(TEXT("Corral inexistente"), [&AnimalField](FSaveValue& V) { AnimalField(V, 0, 3) = FSaveValue::MakeInt(1000); });
			Rejects(TEXT("Cabra en el gallinero"), [&AnimalField](FSaveValue& V) { AnimalField(V, 0, 1) = FSaveValue::MakeInt(2); });
			Rejects(TEXT("Id repetido"), [&AnimalField](FSaveValue& V) { AnimalField(V, 1, 0) = AnimalField(V, 0, 0); });
			Rejects(TEXT("Edad negativa"), [&AnimalField](FSaveValue& V) { AnimalField(V, 0, 4) = FSaveValue::MakeInt(-1); });
			Rejects(TEXT("Bandera 2"), [&AnimalField](FSaveValue& V) { AnimalField(V, 0, 5) = FSaveValue::MakeInt(2); });
			Rejects(TEXT("Cría vieja"), [&AnimalField](FSaveValue& V)
			{
				AnimalField(V, 0, 5) = FSaveValue::MakeInt(0);
				AnimalField(V, 0, 4) = FSaveValue::MakeInt(40);
			});
			Rejects(TEXT("Parto en el futuro"), [&AnimalField](FSaveValue& V) { AnimalField(V, 0, 10) = FSaveValue::MakeInt(1000); });
			Rejects(TEXT("Real no entero"), [&AnimalField](FSaveValue& V) { AnimalField(V, 0, 4) = FSaveValue::MakeDouble(1.5); });
			Rejects(TEXT("Comedero rebosante"), [](FSaveValue& V) { *V.Find(TEXT("pens"))->AtMutable(2) = FSaveValue::MakeInt(1000); });
			Rejects(TEXT("Tipo de corral"), [](FSaveValue& V) { *V.Find(TEXT("pens"))->AtMutable(1) = FSaveValue::MakeInt(3); });
			Rejects(TEXT("Más del tope"), [](FSaveValue& V)
			{
				FSaveValue* List = V.Find(TEXT("animals"));
				const int32 Fields = 11;
				int64 NextId = V.Find(TEXT("nextAnimal"))->AsInt();
				// Nueve cabras en el corral 3 (tope de 8 por base).
				FSaveValue Fresh = FSaveValue::MakeArray();
				for (int32 I = 0; I < 9; ++I)
				{
					const int64 Row[Fields] = { NextId++, 2, I % 2, 3, 6, 1, 1, 0, 0, 0, -1 };
					for (int64 X : Row)
					{
						Fresh.Add(FSaveValue::MakeInt(X));
					}
				}
				*List = Fresh;
				V.Set(TEXT("nextAnimal"), FSaveValue::MakeInt(NextId));
			});
		});

		It("ninguna mutación aleatoria rompe los invariantes", [this]()
		{
			const FSaveValue Good = Busy(12).ToValue();
			FExploredRandom Rng(0xF00D);
			int32 Loaded = 0;
			bool bAllOk = true;
			for (int32 Round = 0; Round < 600; ++Round)
			{
				FSaveValue Bad = Good;
				const TCHAR* ListName = Rng.Chance(0.5f) ? TEXT("animals") : TEXT("pens");
				FSaveValue* List = Bad.Find(ListName);
				if (List == nullptr || List->Num() == 0)
				{
					continue;
				}
				const int32 Index = Rng.RangeInt(0, List->Num() - 1);
				static const int64 Values[] = { -2, -1, 0, 1, 2, 3, 5, 6, 7, 8, 9, 16, 255, 1000, MAX_int32 };
				*List->AtMutable(Index) = FSaveValue::MakeInt(Values[Rng.RangeInt(0, UE_ARRAY_COUNT(Values) - 1)]);
				FLivestockModel Model(Chance(0.5f), 12);
				if (Model.FromValue(Bad))
				{
					++Loaded;
					FedDays(Model, Model.GetLastEndedDay() + 1, Model.GetLastEndedDay() + 10);
				}
				bAllOk &= Invariants(Model);
			}
			TestTrue(TEXT("Invariantes tras cada carga y diez días más"), bAllOk);
			TestTrue(FString::Printf(TEXT("Alguna mutación inocua carga (%d)"), Loaded), Loaded > 0);
		});
	});

	Describe("la red", [this]()
	{
		It("empaqueta el animal en los bytes de especie y banderas de biblia 08 §2.7 b", [this]()
		{
			FLivestockAnimal A;
			A.Species = ES::Goat;
			A.Sex = EX::Male;
			A.bAdult = false;
			A.bTame = true;
			A.bFedToday = true;
			const FLivestockAnimalNet Net = FLivestockModel::MakeAnimalNet(A);
			TestEqual(TEXT("Especie"), Net.Species, static_cast<uint8>(2));
			TestEqual(TEXT("Banderas"), Net.Flags,
				static_cast<uint8>(FLivestockAnimalNet::FlagMale | FLivestockAnimalNet::FlagTame | FLivestockAnimalNet::FlagFed));
		});

		It("el corral agregado cabe en 7 B y vuelve igual", [this]()
		{
			FLivestockModel Model = Busy(2);
			for (const FLivestockPen& Pen : Model.GetPens())
			{
				const FLivestockPenNet Net = Model.MakePenNet(Pen.Id);
				int32 Count = 0;
				for (int32 S = 0; S < static_cast<int32>(ES::Count); ++S)
				{
					Count += Net.Adults[S] + Net.Young[S];
				}
				TestEqual(TEXT("Cuenta los animales del corral"), Count, Model.NumAnimalsInPen(Pen.Id));
				TestEqual(TEXT("Lleno como el modelo"), Net.bFull, Model.IsPenFull(Pen.Id));
				uint8 Bytes[FLivestockPenNet::PackedBytes] = {};
				Net.Pack(Bytes);
				TestTrue(TEXT("Ida y vuelta"), FLivestockPenNet::Unpack(Bytes) == Net);
			}
			TestTrue(TEXT("Corral inexistente: vacío"), Model.MakePenNet(1234) == FLivestockPenNet());
			FLivestockPenNet Wide;
			Wide.Adults[0] = 200;
			uint8 Bytes[FLivestockPenNet::PackedBytes] = {};
			Wide.Pack(Bytes);
			TestEqual(TEXT("Satura a 15 en medio byte"), FLivestockPenNet::Unpack(Bytes).Adults[0], static_cast<uint8>(15));
		});
	});

	Describe("los logros", [this]()
	{
		It("las crías y los huevos desbloquean los logros de granja de biblia 07 §2.3", [this]()
		{
			using K = EAchievementStatKind;
			using S = EAchievementStatScope;
			TArray<FAchievementStatDef> Stats;
			FAchievementStatDef Raised;
			Raised.Id = FName(FLivestockModel::StatSpeciesRaised);
			Raised.Kind = K::Set;
			Raised.Scope = S::Run;
			Stats.Add(Raised);
			FAchievementStatDef Eggs;
			Eggs.Id = FName(FLivestockModel::StatEggsCollected);
			Eggs.Kind = K::Counter;
			Eggs.Scope = S::Profile;
			Stats.Add(Eggs);
			const auto Def = [](const TCHAR* Id, FAchievementCondition Cond)
			{
				FAchievementDef D;
				D.Id = FName(Id);
				D.NameEs = Id;
				D.NameEn = Id;
				D.Condition = MoveTemp(Cond);
				return D;
			};
			TArray<FAchievementDef> Defs;
			Defs.Add(Def(TEXT("primera_pareja"), FAchievementCondition::AtLeast(Raised.Id, 1)));
			Defs.Add(Def(TEXT("corral_completo"), FAchievementCondition::AtLeast(Raised.Id, 3)));
			Defs.Add(Def(TEXT("huevos_por_docenas"), FAchievementCondition::AtLeast(Eggs.Id, 100)));
			FAchievementsModel Achievements;
			FString Error;
			if (!TestTrue(TEXT("Configura"), Achievements.Configure(Stats, Defs, Error)))
			{
				return;
			}
			Achievements.BeginRun(TEXT("Explorer"));

			// Así lo cablea el subsistema: cada cría informa de su especie; cada recogida, de sus huevos.
			// Con la base casi llena (6 adultos de 8) las tres especies compiten por dos huecos.
			TArray<FName> Unlocked;
			FLivestockModel Model(Chance(0.5f));
			const int32 Coop = Model.AddPen(ELivestockPenKind::Coop);
			const int32 Pen = Model.AddPen(ELivestockPenKind::Pen);
			Add(Model, Coop, ES::Chicken, EX::Male);
			Add(Model, Coop, ES::Chicken, EX::Female);
			Add(Model, Pen, ES::Goat, EX::Male);
			Add(Model, Pen, ES::Goat, EX::Female);
			Add(Model, Pen, ES::Pig, EX::Male);
			Add(Model, Pen, ES::Pig, EX::Female);
			for (int32 Day = 4; Day < 120; ++Day)
			{
				for (const FLivestockBirth& B : FedDays(Model, Day, Day).Births)
				{
					Unlocked.Append(Achievements.ReportItem(Raised.Id, FLivestockModel::SpeciesId(B.Species)));
					Model.RemoveAnimal(B.AnimalId); // se vende para dejar sitio
				}
				FLivestockCollect Got;
				Model.Collect(Coop, Got);
				if (Got.Eggs > 0)
				{
					Unlocked.Append(Achievements.Report(Eggs.Id, Got.Eggs));
				}
			}
			TestTrue(TEXT("La primera pareja"), Unlocked.Contains(FName(TEXT("primera_pareja"))));
			TestTrue(TEXT("Corral completo"), Unlocked.Contains(FName(TEXT("corral_completo"))));
			TestTrue(TEXT("Huevos por docenas"), Unlocked.Contains(FName(TEXT("huevos_por_docenas"))));
		});
	});
}

#endif
