#include "Misc/AutomationTest.h"

#include "Fauna/FaunaAnimation.h"
#include "Fauna/FaunaGroups.h"
#include "Fauna/FaunaSpawning.h"
#include "Fauna/FaunaTypes.h"
#include "Fauna/MarineCreatureBrain.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MarineFaunaSpecDetail
{
	/**
	 * Mundo de prueba en cm: islas de 5000 cm de radio (tierra a +300, con un
	 * cerro en el centro) rodeadas de una plataforma que se hunde un 10 %
	 * hasta −70 m. Arrecife a ~r 9000, talud a ~r 25000, aguas profundas más allá de r 45000.
	 */
	struct FTestWorld
	{
		TArray<FVector2D> Islands;
		FVector2D Current = FVector2D::ZeroVector;
		double WaveTime = 0.0;
		float WaveAmplitude = 0.0f;
		/** Fondo plano opcional (laguna, bajíos); NaN para usar las islas. */
		double FlatSeabed = TNumericLimits<double>::Max();
		/** Playa opcional: a partir de BeachStartX el fondo sube hasta tierra en BeachStartX + 2000. */
		double BeachStartX = TNumericLimits<double>::Max();

		double Seabed(const FVector2D& P) const
		{
			if (FlatSeabed != TNumericLimits<double>::Max())
			{
				if (P.X > BeachStartX)
				{
					return FMath::Min(200.0, FlatSeabed + (P.X - BeachStartX) * (-FlatSeabed + 100.0) / 2000.0);
				}
				return FlatSeabed;
			}
			double Nearest = 1.0e12;
			for (const FVector2D& C : Islands)
			{
				Nearest = FMath::Min(Nearest, FVector2D::Distance(P, C));
			}
			if (Nearest < 5000.0)
			{
				return 300.0 + (5000.0 - Nearest) * 0.3;
			}
			return -FMath::Min(7000.0, (Nearest - 5000.0) * 0.1);
		}

		FFaunaWorldQuery Query() const
		{
			FFaunaWorldQuery Q;
			const FTestWorld* Self = this;
			Q.Seabed = [Self](const FVector2D& P) { return static_cast<float>(Self->Seabed(P)); };
			Q.Surface = [Self](const FVector2D& P) { return Self->WaveAmplitude * FMath::Sin(static_cast<float>(P.X * 0.002 + Self->WaveTime)); };
			Q.Current = [Self](const FVector2D&) { return Self->Current; };
			Q.NearestLand = [Self](const FVector2D& P)
			{
				FVector2D Best = FVector2D::ZeroVector;
				double BestDist = 1.0e12;
				for (const FVector2D& C : Self->Islands)
				{
					if (FVector2D::Distance(P, C) < BestDist)
					{
						BestDist = FVector2D::Distance(P, C);
						Best = C;
					}
				}
				return Best;
			};
			return Q;
		}
	};

	FTestWorld OneIsland()
	{
		FTestWorld W;
		W.Islands.Add(FVector2D::ZeroVector);
		return W;
	}

	FFaunaStimuli SwimmerAt(const FVector& P, float Noise, float Hours = 12.0f)
	{
		FFaunaStimuli S;
		S.bHasPlayer = true;
		S.bPlayerInWater = true;
		S.PlayerCm = P;
		S.PlayerNoise01 = Noise;
		S.Hours = Hours;
		return S;
	}

	FFaunaStimuli BoatAt(const FVector& P, const FVector& Velocity, float Hours = 12.0f)
	{
		FFaunaStimuli S;
		S.bHasPlayer = true;
		S.bPlayerInBoat = true;
		S.PlayerCm = P;
		S.PlayerVelocityCmS = Velocity;
		S.PlayerNoise01 = 0.4f;
		S.Hours = Hours;
		return S;
	}

	FMarineBrainConfig Creature(EFaunaSpecies Species, const FVector& At, uint32 Seed, float HomeRadius = 3000.0f)
	{
		FMarineBrainConfig C;
		C.Species = Species;
		C.SpawnCm = At;
		C.HomeCm = At;
		C.HomeRadiusCm = HomeRadius;
		C.Seed = Seed;
		return C;
	}
}

BEGIN_DEFINE_SPEC(FMarineFaunaSpec, "Explored.Fauna.Marine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FMarineFaunaSpec)

void FMarineFaunaSpec::Define()
{
	using namespace MarineFaunaSpecDetail;

	Describe("Bancos de peces", [this]()
	{
		It("nunca rompen la superficie con olas ni atraviesan el fondo ni entran en tierra", [this]()
		{
			FTestWorld World = OneIsland();
			World.WaveAmplitude = 40.0f;
			const FFaunaWorldQuery Q = World.Query();
			FFishSchoolConfig Config;
			Config.HomeCm = FVector(9000.0, 0.0, -200.0);
			Config.HomeRadiusCm = 1500.0f;
			Config.Count = 40;
			Config.Seed = 5;
			FFishSchoolModel School;
			School.Init(Config, Q);
			const FFaunaStimuli Stimuli;
			int32 Violations = 0;
			for (int32 Step = 0; Step < 1800; ++Step)
			{
				World.WaveTime = Step / 30.0;
				School.Tick(1.0f / 30.0f, Stimuli, Q);
				for (const FBoidAgent& Fish : School.GetBoids().GetAgents())
				{
					const FVector2D XY(Fish.Position.X, Fish.Position.Y);
					const bool bBelowSurface = Fish.Position.Z <= Q.SurfaceZ(XY) - Config.SurfaceClearanceCm + 1.0e-3;
					const bool bAboveSeabed = Fish.Position.Z >= Q.SeabedZ(XY) + Config.SeabedClearanceCm - 1.0e-3;
					if (!bBelowSurface || !bAboveSeabed || Q.SeabedZ(XY) > 0.0f)
					{
						++Violations;
					}
				}
			}
			TestEqual(TEXT("Siempre bajo la superficie, sobre el fondo y en el agua"), Violations, 0);
		});

		It("el banco de arrecife se dispersa con un chapoteo y se rehace después", [this]()
		{
			FTestWorld World = OneIsland();
			const FFaunaWorldQuery Q = World.Query();
			FFishSchoolConfig Config;
			Config.HomeCm = FVector(12000.0, 0.0, -300.0);
			Config.Count = 30;
			Config.Seed = 8;
			FFishSchoolModel School;
			School.Init(Config, Q);
			const FFaunaStimuli Calm;
			for (int32 I = 0; I < 300; ++I)
			{
				School.Tick(1.0f / 30.0f, Calm, Q);
			}
			const float SchoolingSpread = School.GetBoids().Spread();
			TestEqual(TEXT("Nadando en banco"), static_cast<int32>(School.GetState()), static_cast<int32>(EFishSchoolState::Schooling));

			float MaxSpread = 0.0f;
			for (int32 I = 0; I < 90; ++I)
			{
				const FFaunaStimuli Splash = SwimmerAt(School.GetBoids().Centroid(), 1.0f);
				School.Tick(1.0f / 30.0f, Splash, Q);
				MaxSpread = FMath::Max(MaxSpread, School.GetBoids().Spread());
			}
			TestEqual(TEXT("Disperso"), static_cast<int32>(School.GetState()), static_cast<int32>(EFishSchoolState::Scattered));
			TestTrue(FString::Printf(TEXT("Se abre (%.0f → %.0f cm)"), SchoolingSpread, MaxSpread), MaxSpread > SchoolingSpread * 1.5f);

			bool bRegrouped = false;
			for (int32 I = 0; I < 900 && !bRegrouped; ++I)
			{
				School.Tick(1.0f / 30.0f, Calm, Q);
				bRegrouped = School.GetState() == EFishSchoolState::Schooling;
			}
			TestTrue(FString::Printf(TEXT("Vuelve a nadar en banco (dispersión %.0f cm, en calma %.0f cm)"), School.GetBoids().Spread(), SchoolingSpread), bRegrouped);
			TestTrue(TEXT("Compacto de nuevo"), School.GetBoids().Spread() <= FMath::Max(Config.RegroupedSpreadCm, SchoolingSpread * 1.3f));
		});

		It("quien entra despacio se acerca más que quien chapotea", [this]()
		{
			FTestWorld World = OneIsland();
			const FFaunaWorldQuery Q = World.Query();
			FFishSchoolConfig Config;
			Config.HomeCm = FVector(12000.0, 0.0, -300.0);
			Config.Count = 20;
			FFishSchoolModel School;
			School.Init(Config, Q);
			const FVector Near = School.GetBoids().Centroid() + FVector(Config.ScatterRadiusCm * 0.6f + School.GetBoids().Spread(), 0.0, 0.0);
			TestFalse(TEXT("En silencio no se asustan"), School.IsThreatened(SwimmerAt(Near, 0.0f)));
			TestTrue(TEXT("Con chapoteo sí"), School.IsThreatened(SwimmerAt(Near, 1.0f)));
		});

		It("el banco de mar abierto migra y gira antes de llegar a tierra", [this]()
		{
			FTestWorld World = OneIsland();
			const FFaunaWorldQuery Q = World.Query();
			FFishSchoolConfig Config;
			Config.Kind = EFishSchoolKind::OpenSea;
			Config.HomeCm = FVector(40000.0, 0.0, -1000.0);
			Config.MigrationDirection = FVector2D(-1.0, 0.0);
			Config.Count = 80;
			Config.Seed = 3;
			FFishSchoolModel School;
			School.Init(Config, Q);
			const FVector Start = School.GetBoids().Centroid();
			const float MinDepth = FFaunaSpeciesInfo::Get(EFaunaSpecies::OpenSeaFish).MinWaterDepthCm;
			double ClosestToLand = 1.0e12;
			for (int32 Step = 0; Step < 3600; ++Step)
			{
				School.Tick(1.0f / 30.0f, FFaunaStimuli(), Q);
				ClosestToLand = FMath::Min(ClosestToLand, School.GetBoids().Centroid().Size2D());
			}
			TestEqual(TEXT("Migrando"), static_cast<int32>(School.GetState()), static_cast<int32>(EFishSchoolState::Migrating));
			TestTrue(TEXT("Recorre distancia"), FVector::Dist2D(School.GetBoids().Centroid(), Start) > 8000.0);
			// Profundidad mínima (8 m) a r = 13000 cm: el centro del banco no llega a los bajíos.
			TestTrue(FString::Printf(TEXT("Gira antes de la costa (mínimo %.0f cm)"), ClosestToLand), ClosestToLand > 5000.0 + MinDepth * 10.0 * 0.8);
		});
	});

	Describe("Bandadas", [this]()
	{
		It("siempre en vuelo: nunca bajan de la altura mínima ni de la velocidad mínima", [this]()
		{
			FTestWorld World = OneIsland();
			const FFaunaWorldQuery Q = World.Query();
			FBirdFlockConfig Config;
			Config.HomeCm = FVector(0.0, 0.0, 0.0);
			Config.Count = 16;
			FBirdFlockModel Flock;
			Flock.Init(Config, Q);
			const float MinSpeed = FBirdFlockModel::ParamsFor(Config.Species).MinSpeedCmS;
			int32 Violations = 0;
			for (int32 Step = 0; Step < 1800; ++Step)
			{
				// Incluye robar un pescado en la playa: bajan a por él, pero sin posarse.
				FFaunaStimuli S;
				S.Stealables.Add({FVector(5200.0, 0.0, 300.0), true, true});
				Flock.Tick(1.0f / 30.0f, S, Q);
				for (const FBoidAgent& Bird : Flock.GetBoids().GetAgents())
				{
					const FVector2D XY(Bird.Position.X, Bird.Position.Y);
					const double Ground = FMath::Max(Q.SeabedZ(XY), Q.SurfaceZ(XY));
					if (Bird.Position.Z < Ground + Config.MinAltitudeCm - 1.0e-3 || Bird.Velocity.Size() < MinSpeed - 1.0e-3)
					{
						++Violations;
					}
				}
			}
			TestEqual(TEXT("Siempre por encima de la altura mínima y volando"), Violations, 0);
		});

		It("al atardecer vuelan hacia la isla más cercana", [this]()
		{
			FTestWorld World;
			World.Islands.Add(FVector2D(0.0, 0.0));
			World.Islands.Add(FVector2D(200000.0, 0.0));
			const FFaunaWorldQuery Q = World.Query();
			FBirdFlockConfig Config;
			Config.HomeCm = FVector(160000.0, 30000.0, 0.0);
			Config.Count = 14;
			const FVector2D Nearest(200000.0, 0.0);

			auto Run = [&](float Hours, FBirdFlockModel& Flock)
			{
				Flock.Init(Config, Q);
				FFaunaStimuli S;
				S.Hours = Hours;
				double Heading = 0.0;
				for (int32 Step = 0; Step < 1200; ++Step)
				{
					Flock.Tick(1.0f / 30.0f, S, Q);
					if (Step >= 600)
					{
						const FVector C = Flock.GetBoids().Centroid();
						const FVector2D ToLand = (Nearest - FVector2D(C.X, C.Y)).GetSafeNormal();
						const FVector V = Flock.GetBoids().AverageVelocity();
						Heading += FVector2D::DotProduct(FVector2D(V.X, V.Y).GetSafeNormal(), ToLand);
					}
				}
				return Heading / 600.0;
			};

			FBirdFlockModel Dusk;
			const double StartDistance = FVector2D::Distance(FVector2D(Config.HomeCm.X, Config.HomeCm.Y), Nearest);
			const double DuskHeading = Run(18.25f, Dusk);
			const FVector DuskEnd = Dusk.GetBoids().Centroid();
			TestEqual(TEXT("Vuelven a tierra"), static_cast<int32>(Dusk.GetState()), static_cast<int32>(EBirdFlockState::ReturningToLand));
			TestTrue(FString::Printf(TEXT("Rumbo a la isla (%.2f)"), DuskHeading), DuskHeading > 0.8);
			TestTrue(TEXT("Se acercan a la isla más cercana"), FVector2D::Distance(FVector2D(DuskEnd.X, DuskEnd.Y), Nearest) < StartDistance - 20000.0);

			FBirdFlockModel Noon;
			Run(12.0f, Noon);
			const FVector NoonEnd = Noon.GetBoids().Centroid();
			TestTrue(TEXT("A mediodía siguen en su costa"), FVector::Dist2D(NoonEnd, Config.HomeCm) < Config.HomeRadiusCm * 1.5f);
		});

		It("se apartan del jugador", [this]()
		{
			FTestWorld World = OneIsland();
			const FFaunaWorldQuery Q = World.Query();
			FBirdFlockConfig Config;
			Config.HomeCm = FVector(20000.0, 0.0, 0.0);
			FBirdFlockModel Flock;
			Flock.Init(Config, Q);
			FFaunaStimuli S;
			S.bHasPlayer = true;
			S.PlayerCm = Flock.GetBoids().Centroid();
			for (int32 Step = 0; Step < 150; ++Step)
			{
				Flock.Tick(1.0f / 30.0f, S, Q);
			}
			double Sum = 0.0;
			for (const FBoidAgent& Bird : Flock.GetBoids().GetAgents())
			{
				Sum += FVector::Distance(Bird.Position, S.PlayerCm);
			}
			TestTrue(TEXT("A distancia"), Sum / Flock.GetBoids().Num() > Config.PlayerFleeRadiusCm * 0.6f);
		});

		It("siguen a la canoa que lleva pescado", [this]()
		{
			FTestWorld World = OneIsland();
			const FFaunaWorldQuery Q = World.Query();
			FBirdFlockConfig Config;
			Config.HomeCm = FVector(20000.0, 0.0, 0.0);
			FBirdFlockModel Flock;
			Flock.Init(Config, Q);
			FVector Boat(24000.0, 0.0, 0.0);
			for (int32 Step = 0; Step < 1800; ++Step)
			{
				Boat.Y += 300.0 / 30.0;
				FFaunaStimuli S = BoatAt(Boat, FVector(0.0, 300.0, 0.0));
				S.bPlayerCarriesFish = true;
				Flock.Tick(1.0f / 30.0f, S, Q);
			}
			TestEqual(TEXT("Siguiendo"), static_cast<int32>(Flock.GetState()), static_cast<int32>(EBirdFlockState::FollowingBoat));
			TestTrue(TEXT("Cerca de la canoa"), FVector::Dist2D(Flock.GetBoids().Centroid(), Boat) < 3000.0);
		});

		It("la consulta de robo solo ve pescado al aire sin el jugador cerca", [this]()
		{
			TArray<FStealableItem> Items;
			Items.Add({FVector(1000.0, 0.0, 0.0), false, true});   // no es pescado
			Items.Add({FVector(1200.0, 0.0, 0.0), true, false});   // en una cesta
			Items.Add({FVector(3000.0, 0.0, 0.0), true, true});    // pescado al aire
			Items.Add({FVector(2000.0, 0.0, 0.0), true, true});    // pescado al aire, junto al jugador
			const FVector Player(2100.0, 0.0, 0.0);
			TestEqual(TEXT("El único robable"), FGullTheft::FindStealable(Items, FVector::ZeroVector, 5000.0f, true, Player, 500.0f), 2);
			TestEqual(TEXT("Fuera de alcance"), FGullTheft::FindStealable(Items, FVector::ZeroVector, 2500.0f, true, Player, 500.0f), INDEX_NONE);
			TestEqual(TEXT("Sin jugador, el más cercano"), FGullTheft::FindStealable(Items, FVector::ZeroVector, 5000.0f, false, Player, 500.0f), 3);
		});

		It("roban el pescado dejado al aire, pero no delante del jugador", [this]()
		{
			FTestWorld World = OneIsland();
			const FFaunaWorldQuery Q = World.Query();
			FBirdFlockConfig Config;
			Config.HomeCm = FVector(8000.0, 0.0, 0.0);
			const FVector Fish(5500.0, 0.0, 250.0);

			auto Run = [&](const FVector& PlayerAt)
			{
				FBirdFlockModel Flock;
				Flock.Init(Config, Q);
				FFaunaStimuli S;
				S.bHasPlayer = true;
				S.PlayerCm = PlayerAt;
				S.Stealables.Add({Fish, true, true});
				for (int32 Step = 0; Step < 900; ++Step)
				{
					if (Flock.Tick(1.0f / 30.0f, S, Q).StolenItemIndex == 0)
					{
						return true;
					}
				}
				return false;
			};
			TestTrue(TEXT("Lo roban con el jugador lejos"), Run(FVector(-30000.0, 0.0, 0.0)));
			TestFalse(TEXT("No lo roban con el jugador al lado"), Run(Fish + FVector(300.0, 0.0, 0.0)));
		});
	});

	Describe("Criaturas", [this]()
	{
		It("la raya enterrada se aleja ondulando por el fondo al acercarse el jugador y vuelve a enterrarse", [this]()
		{
			FTestWorld World;
			World.FlatSeabed = -150.0;
			const FFaunaWorldQuery Q = World.Query();
			FMarineCreatureBrain Ray(Creature(EFaunaSpecies::Stingray, FVector::ZeroVector, 4), Q);
			TestEqual(TEXT("Enterrada"), static_cast<int32>(Ray.GetState()), static_cast<int32>(EMarineState::Buried));

			FVector Player(-600.0, 0.0, -60.0);
			bool bStartled = false;
			double MaxHeight = 0.0;
			for (int32 Step = 0; Step < 200; ++Step)
			{
				Player.X += 100.0 / 20.0;
				const FMarineBrainEvents E = Ray.Tick(0.05f, SwimmerAt(Player, 0.3f), Q);
				bStartled |= E.bStartled;
				TestFalse(TEXT("No pica si no la pisan"), E.bSting);
				MaxHeight = FMath::Max(MaxHeight, Ray.GetPosition().Z - Q.SeabedZ(FVector2D(Ray.GetPosition().X, Ray.GetPosition().Y)));
			}
			TestTrue(TEXT("Se levanta"), bStartled);
			TestTrue(TEXT("Se aleja"), FVector::Distance(Ray.GetPosition(), Player) > 1000.0);
			TestTrue(FString::Printf(TEXT("Pegada al fondo (%.0f cm)"), MaxHeight), MaxHeight <= 60.0);

			bool bBuried = false;
			for (int32 Step = 0; Step < 400 && !bBuried; ++Step)
			{
				Ray.Tick(0.05f, FFaunaStimuli(), Q);
				bBuried = Ray.GetState() == EMarineState::Buried;
			}
			TestTrue(TEXT("Se entierra otra vez"), bBuried);
		});

		It("la raya pica si se pisa y no si se arrastran los pies", [this]()
		{
			FTestWorld World;
			World.FlatSeabed = -150.0;
			const FFaunaWorldQuery Q = World.Query();
			FMarineCreatureBrain Stepped(Creature(EFaunaSpecies::Stingray, FVector::ZeroVector, 4), Q);
			const FMarineBrainEvents E = Stepped.Tick(0.05f, SwimmerAt(FVector(30.0, 0.0, -60.0), 0.0f), Q);
			TestTrue(TEXT("Pica al pisarla"), E.bSting && E.StingDamage > 0.0f);

			FMarineCreatureBrain Shuffled(Creature(EFaunaSpecies::Stingray, FVector::ZeroVector, 4), Q);
			FFaunaStimuli S = SwimmerAt(FVector(-1200.0, 0.0, -60.0), 0.1f);
			S.bPlayerShuffling = true;
			bool bStung = false;
			for (int32 Step = 0; Step < 300; ++Step)
			{
				S.PlayerCm.X += 100.0 / 20.0;
				bStung |= Shuffled.Tick(0.05f, S, Q).bSting;
			}
			TestFalse(TEXT("Arrastrando los pies se aparta antes"), bStung);
		});

		It("la medusa deriva exactamente con la corriente y pica en su radio", [this]()
		{
			FTestWorld World = OneIsland();
			World.Current = FVector2D(30.0, -10.0);
			const FFaunaWorldQuery Q = World.Query();
			const FVector Start(60000.0, 0.0, -500.0);
			FMarineCreatureBrain Jelly(Creature(EFaunaSpecies::Jellyfish, Start, 2), Q);
			for (int32 Step = 0; Step < 2000; ++Step)
			{
				Jelly.Tick(0.05f, FFaunaStimuli(), Q);
			}
			const FVector2D Drift(Jelly.GetPosition().X - Start.X, Jelly.GetPosition().Y - Start.Y);
			TestTrue(FString::Printf(TEXT("Deriva = corriente × tiempo (%.2f, %.2f)"), Drift.X, Drift.Y), Drift.Equals(FVector2D(3000.0, -1000.0), 1.0));
			TestEqual(TEXT("Estado"), static_cast<int32>(Jelly.GetState()), static_cast<int32>(EMarineState::Drift));
			TestTrue(TEXT("Pica cerca"), Jelly.IsStingHazard(Jelly.GetPosition() + FVector(100.0, 0.0, 0.0)));
			TestFalse(TEXT("No pica lejos"), Jelly.IsStingHazard(Jelly.GetPosition() + FVector(200.0, 0.0, 0.0)));
			const FMarineBrainEvents E = Jelly.Tick(0.05f, SwimmerAt(Jelly.GetPosition() + FVector(50.0, 0.0, 0.0), 0.0f), Q);
			TestTrue(TEXT("Evento de picadura"), E.bSting);
		});

		It("el tiburón de arrecife se acerca curioso y da vueltas sin atacar", [this]()
		{
			FTestWorld World = OneIsland();
			const FFaunaWorldQuery Q = World.Query();
			// Semilla cuya primera tirada de ataque sale negativa (la inmensa mayoría).
			uint32 Seed = 1;
			while (FMarineCreatureBrain::ReefSharkAttackRoll(Seed, 0, 0.0f, false))
			{
				++Seed;
			}
			const FVector Start(25000.0, 0.0, -600.0);
			FMarineCreatureBrain Shark(Creature(EFaunaSpecies::ReefShark, Start, Seed), Q);
			const FVector Player(25000.0, 1500.0, -100.0);
			bool bCircled = false;
			bool bBit = false;
			double MinCircle = 1.0e9;
			double MaxCircle = 0.0;
			for (int32 Step = 0; Step < 900; ++Step)
			{
				const FMarineBrainEvents E = Shark.Tick(0.05f, SwimmerAt(Player, 0.6f), Q);
				bBit |= E.bBite;
				if (Shark.GetState() == EMarineState::Circle && Shark.GetStateSeconds() > 3.0f)
				{
					bCircled = true;
					const double D = FVector::Dist2D(Shark.GetPosition(), Player);
					MinCircle = FMath::Min(MinCircle, D);
					MaxCircle = FMath::Max(MaxCircle, D);
				}
			}
			TestTrue(TEXT("Da vueltas"), bCircled);
			TestTrue(FString::Printf(TEXT("A distancia prudente (%.0f–%.0f cm)"), MinCircle, MaxCircle),
				MinCircle > FMarineCreatureBrain::ReefSharkCircleRadiusCm * 0.5 && MaxCircle < FMarineCreatureBrain::ReefSharkCircleRadiusCm * 2.0);
			TestFalse(TEXT("No muerde"), bBit);
			TestTrue(TEXT("Después se va"), Shark.GetState() == EMarineState::Retreat || Shark.GetState() == EMarineState::Wander);
		});

		It("el tiburón de arrecife rara vez ataca; más con sangre y nunca en modo Explorador", [this]()
		{
			int32 Plain = 0;
			int32 Blood = 0;
			int32 Peaceful = 0;
			for (int32 I = 0; I < 2000; ++I)
			{
				Plain += FMarineCreatureBrain::ReefSharkAttackRoll(77u, I, 0.0f, false) ? 1 : 0;
				Blood += FMarineCreatureBrain::ReefSharkAttackRoll(77u, I, 0.5f, false) ? 1 : 0;
				Peaceful += FMarineCreatureBrain::ReefSharkAttackRoll(77u, I, 0.5f, true) ? 1 : 0;
			}
			TestTrue(FString::Printf(TEXT("Raro sin sangre (%d/2000)"), Plain), Plain > 0 && Plain < 120);
			TestTrue(FString::Printf(TEXT("Más con sangre (%d/2000)"), Blood), Blood > 300 && Blood < 700);
			TestEqual(TEXT("Nunca en Explorador"), Peaceful, 0);
		});

		It("el tiburón tigre ataca en aguas profundas y nunca entra en las someras", [this]()
		{
			FTestWorld World = OneIsland();
			const FFaunaWorldQuery Q = World.Query();
			const float MinDepth = FFaunaSpeciesInfo::Get(EFaunaSpecies::TigerShark).MinWaterDepthCm;

			// Mar abierto: el jugador nada en 55 m de agua.
			const FVector DeepPlayer(60000.0, 0.0, -80.0);
			FMarineCreatureBrain Hunter(Creature(EFaunaSpecies::TigerShark, FVector(62000.0, 0.0, -2000.0), 3), Q);
			bool bStalked = false;
			bool bBit = false;
			for (int32 Step = 0; Step < 1200 && !bBit; ++Step)
			{
				const FMarineBrainEvents E = Hunter.Tick(0.05f, SwimmerAt(DeepPlayer, 0.5f), Q);
				bStalked |= Hunter.GetState() == EMarineState::Stalk;
				bBit |= E.bBite;
			}
			TestTrue(TEXT("Acecha"), bStalked);
			TestTrue(TEXT("Ataca en mar abierto"), bBit);

			// Aguas someras (25 m) junto al borde del profundo: nunca ataca ni entra.
			const FVector ShallowPlayer(30000.0, 0.0, -80.0);
			TestFalse(TEXT("No es agua de tiburón tigre"), FMarineCreatureBrain::IsTigerSharkWater(Q, FVector2D(ShallowPlayer.X, ShallowPlayer.Y)));
			FMarineCreatureBrain Border(Creature(EFaunaSpecies::TigerShark, FVector(40000.0, 0.0, -2000.0), 3, 10000.0f), Q);
			bool bShallowBite = false;
			double ShallowestDepth = 1.0e9;
			for (int32 Step = 0; Step < 2400; ++Step)
			{
				bShallowBite |= Border.Tick(0.05f, SwimmerAt(ShallowPlayer, 1.0f), Q).bBite;
				ShallowestDepth = FMath::Min(ShallowestDepth, static_cast<double>(Q.WaterDepth(FVector2D(Border.GetPosition().X, Border.GetPosition().Y))));
			}
			TestFalse(TEXT("No ataca en aguas someras"), bShallowBite);
			TestTrue(FString::Printf(TEXT("Siempre en aguas profundas (mínimo %.0f cm)"), ShallowestDepth), ShallowestDepth >= MinDepth);
		});

		It("el tiburón tigre no ataca en modo Explorador", [this]()
		{
			FTestWorld World = OneIsland();
			const FFaunaWorldQuery Q = World.Query();
			FMarineCreatureBrain Shark(Creature(EFaunaSpecies::TigerShark, FVector(62000.0, 0.0, -2000.0), 3), Q);
			FFaunaStimuli S = SwimmerAt(FVector(60000.0, 0.0, -80.0), 0.5f);
			S.bPeaceful = true;
			bool bBit = false;
			for (int32 Step = 0; Step < 1200; ++Step)
			{
				bBit |= Shark.Tick(0.05f, S, Q).bBite;
			}
			TestFalse(TEXT("Pacífico"), bBit);
		});

		It("los delfines acompañan a la canoa y saltan", [this]()
		{
			FTestWorld World = OneIsland();
			const FFaunaWorldQuery Q = World.Query();
			FVector Boat(60000.0, 0.0, 0.0);
			const FVector BoatVelocity(400.0, 0.0, 0.0);
			FMarineCreatureBrain Dolphin(Creature(EFaunaSpecies::Dolphin, FVector(60000.0, 1500.0, -300.0), 12), Q);
			int32 Jumps = 0;
			bool bAboveSurface = false;
			double FarthestAfterJoin = 0.0;
			for (int32 Step = 0; Step < 1200; ++Step)
			{
				Boat += BoatVelocity * 0.05;
				const FMarineBrainEvents E = Dolphin.Tick(0.05f, BoatAt(Boat, BoatVelocity), Q);
				Jumps += E.bJumped ? 1 : 0;
				bAboveSurface |= Dolphin.GetPosition().Z > 0.0;
				if (Step > 300)
				{
					FarthestAfterJoin = FMath::Max(FarthestAfterJoin, FVector::Dist2D(Dolphin.GetPosition(), Boat));
				}
			}
			TestTrue(FString::Printf(TEXT("Junto a la canoa (máximo %.0f cm)"), FarthestAfterJoin), FarthestAfterJoin < 1500.0);
			TestTrue(FString::Printf(TEXT("Salta (%d saltos)"), Jumps), Jumps >= 2);
			TestTrue(TEXT("Sale del agua"), bAboveSurface);
		});

		It("la tortuga nada en la laguna, sube a respirar y acude a desovar", [this]()
		{
			FTestWorld World;
			World.FlatSeabed = -400.0;
			World.BeachStartX = 8000.0;
			const FFaunaWorldQuery Q = World.Query();
			FMarineCreatureBrain Turtle(Creature(EFaunaSpecies::SeaTurtle, FVector(0.0, 0.0, -200.0), 6), Q);
			const float MinDepth = FFaunaSpeciesInfo::Get(EFaunaSpecies::SeaTurtle).MinWaterDepthCm;
			bool bBreathed = false;
			double Shallowest = 1.0e9;
			for (int32 Step = 0; Step < 3000; ++Step)
			{
				Turtle.Tick(0.05f, FFaunaStimuli(), Q);
				bBreathed |= Turtle.GetState() == EMarineState::Breathe && Turtle.GetPosition().Z > -80.0;
				Shallowest = FMath::Min(Shallowest, static_cast<double>(Q.WaterDepth(FVector2D(Turtle.GetPosition().X, Turtle.GetPosition().Y))));
				TestTrue(TEXT("Bajo la superficie"), Turtle.GetPosition().Z < 0.0);
			}
			TestTrue(TEXT("Sube a respirar"), bBreathed);
			TestTrue(TEXT("En la laguna"), Shallowest >= MinDepth);

			Turtle.BeginNesting(FVector(10500.0, 0.0, 100.0));
			bool bArrived = false;
			for (int32 Step = 0; Step < 6000 && !bArrived; ++Step)
			{
				bArrived = Turtle.Tick(0.05f, FFaunaStimuli(), Q).bNestingArrived;
			}
			TestTrue(TEXT("Llega a la orilla para desovar"), bArrived);
			TestEqual(TEXT("Estado de desove"), static_cast<int32>(Turtle.GetState()), static_cast<int32>(EMarineState::Nesting));
		});

		It("la ballena nunca se deja ver de cerca aunque la persigan", [this]()
		{
			FTestWorld World = OneIsland();
			const FFaunaWorldQuery Q = World.Query();
			FMarineBrainConfig Config = Creature(EFaunaSpecies::HumpbackWhale, FVector(100000.0, 0.0, -250.0), 9, 60000.0f);
			FMarineCreatureBrain Whale(Config, Q);
			FVector Boat(70000.0, 0.0, 0.0);
			bool bKeptDistance = true;
			bool bSounded = false;
			int32 Blows = 0;
			for (int32 Step = 0; Step < 6000; ++Step)
			{
				// Una canoa a vela más rápida que la ballena la persigue.
				const FVector ToWhale = FVector(Whale.GetPosition().X - Boat.X, Whale.GetPosition().Y - Boat.Y, 0.0).GetSafeNormal();
				const FVector BoatVelocity = ToWhale * 800.0;
				Boat += BoatVelocity * 0.05;
				const FMarineBrainEvents E = Whale.Tick(0.05f, BoatAt(Boat, BoatVelocity), Q);
				Blows += E.bBlow ? 1 : 0;
				bSounded |= !Whale.IsVisible();
				if (Whale.IsVisible() && FVector::Dist2D(Whale.GetPosition(), Boat) < FMarineCreatureBrain::WhaleMinDistanceCm)
				{
					bKeptDistance = false;
				}
			}
			TestTrue(TEXT("Visible siempre lejos"), bKeptDistance);
			TestTrue(TEXT("Se sumerge al verse perseguida"), bSounded);
			TestTrue(TEXT("Sopla de vez en cuando"), Blows > 0);
		});

		It("los cerebros son deterministas", [this]()
		{
			FTestWorld World = OneIsland();
			World.Current = FVector2D(10.0, 5.0);
			const FFaunaWorldQuery Q = World.Query();
			auto Run = [&Q]()
			{
				FMarineCreatureBrain Shark(Creature(EFaunaSpecies::ReefShark, FVector(25000.0, 0.0, -600.0), 42), Q);
				for (int32 Step = 0; Step < 600; ++Step)
				{
					Shark.Tick(0.05f, SwimmerAt(FVector(25000.0, 2000.0, -100.0), 0.5f), Q);
				}
				return Shark.GetPosition();
			};
			TestTrue(TEXT("Misma posición"), Run() == Run());
		});
	});

	Describe("Percepción y ciclo diario", [this]()
	{
		It("ve en un cono y más cerca de noche; oye más lejos cuanto más ruido", [this]()
		{
			const FVector Eye = FVector::ZeroVector;
			const FVector Forward(1.0, 0.0, 0.0);
			TestTrue(TEXT("Delante de día"), FFaunaPerception::CanSee(Eye, Forward, FVector(1500.0, 0.0, 0.0), 2000.0f, 60.0f, 1.0f));
			TestFalse(TEXT("Detrás no"), FFaunaPerception::CanSee(Eye, Forward, FVector(-500.0, 0.0, 0.0), 2000.0f, 60.0f, 1.0f));
			TestFalse(TEXT("De noche ve menos"), FFaunaPerception::CanSee(Eye, Forward, FVector(1500.0, 0.0, 0.0), 2000.0f, 60.0f, 0.0f));
			TestFalse(TEXT("En silencio no oye"), FFaunaPerception::CanHear(Eye, FVector(100.0, 0.0, 0.0), 0.0f, 3000.0f));
			TestTrue(TEXT("Un chapoteo se oye lejos"), FFaunaPerception::CanHear(Eye, FVector(2500.0, 0.0, 0.0), 1.0f, 3000.0f));
			TestFalse(TEXT("Nadar despacio, solo cerca"), FFaunaPerception::CanHear(Eye, FVector(2500.0, 0.0, 0.0), 0.3f, 3000.0f));
		});

		It("el olor de la sangre deriva con la corriente", [this]()
		{
			FTestWorld World = OneIsland();
			World.Current = FVector2D(50.0, 0.0);
			const FFaunaWorldQuery Q = World.Query();
			TArray<FBloodSource> Blood;
			Blood.Add({FVector(60000.0, 0.0, -100.0), 60.0f, 1.0f});
			const float Downstream = FFaunaPerception::SmellAt(FVector(63000.0, 0.0, -100.0), Blood, Q);
			const float Upstream = FFaunaPerception::SmellAt(FVector(57000.0, 0.0, -100.0), Blood, Q);
			TestTrue(FString::Printf(TEXT("Aguas abajo huele (%.3f)"), Downstream), Downstream > 0.02f);
			TestEqual(TEXT("Aguas arriba no"), Upstream, 0.0f);
			Blood[0].AgeSeconds = 900.0f;
			TestTrue(TEXT("Se desvanece con el tiempo"), FFaunaPerception::SmellAt(FFaunaPerception::PlumeCenter(Blood[0], Q), Blood, Q) < 0.001f);
		});

		It("cada especie tiene su hora de actividad", [this]()
		{
			TestTrue(TEXT("Peces de arrecife de día"), FFaunaActivity::Level(EFaunaSpecies::ReefFish, 12.0f) > FFaunaActivity::Level(EFaunaSpecies::ReefFish, 0.0f) + 0.4f);
			TestTrue(TEXT("Tiburones al atardecer"), FFaunaActivity::Level(EFaunaSpecies::ReefShark, 18.25f) > FFaunaActivity::Level(EFaunaSpecies::ReefShark, 12.0f) + 0.3f);
			TestTrue(TEXT("Tiburones de noche más que a mediodía"), FFaunaActivity::Level(EFaunaSpecies::TigerShark, 1.0f) > FFaunaActivity::Level(EFaunaSpecies::TigerShark, 12.0f));
			TestTrue(TEXT("Aves casi nada de noche"), FFaunaActivity::Level(EFaunaSpecies::Gull, 2.0f) < 0.1f);
			TestEqual(TEXT("Medusas siempre"), FFaunaActivity::Level(EFaunaSpecies::Jellyfish, 3.0f), 1.0f);
			TestTrue(TEXT("Ventana del atardecer"), FFaunaActivity::IsDusk(18.0f) && !FFaunaActivity::IsDusk(12.0f));
		});
	});

	Describe("Población y LOD", [this]()
	{
		It("clasifica las zonas por profundidad", [this]()
		{
			TestEqual(TEXT("Tierra"), static_cast<int32>(FFaunaSpawnRules::ClassifyZone(100.0f, false)), static_cast<int32>(EMarineZone::Land));
			TestEqual(TEXT("Bajío"), static_cast<int32>(FFaunaSpawnRules::ClassifyZone(-200.0f, false)), static_cast<int32>(EMarineZone::Shallows));
			TestEqual(TEXT("Laguna"), static_cast<int32>(FFaunaSpawnRules::ClassifyZone(-200.0f, true)), static_cast<int32>(EMarineZone::Lagoon));
			TestEqual(TEXT("Arrecife"), static_cast<int32>(FFaunaSpawnRules::ClassifyZone(-800.0f, false)), static_cast<int32>(EMarineZone::Reef));
			TestEqual(TEXT("Talud"), static_cast<int32>(FFaunaSpawnRules::ClassifyZone(-2500.0f, false)), static_cast<int32>(EMarineZone::Slope));
			TestEqual(TEXT("Profundo"), static_cast<int32>(FFaunaSpawnRules::ClassifyZone(-6000.0f, false)), static_cast<int32>(EMarineZone::Deep));
		});

		It("puebla de forma determinista y según zona, profundidad y hora", [this]()
		{
			int32 TigerOutsideDeep = 0;
			int32 MarineOnLand = 0;
			int32 BirdsOffshore = 0;
			int32 Whales = 0;
			int32 ReefDay = 0;
			int32 ReefNight = 0;
			int32 NightNotSubset = 0;
			int32 OutsideCell = 0;
			int32 Mismatch = 0;
			const float Depths[] = {150.0f, -200.0f, -800.0f, -2500.0f, -6000.0f};
			for (int32 X = -20; X < 20; ++X)
			{
				for (int32 Y = -20; Y < 20; ++Y)
				{
					const FIntPoint Cell(X, Y);
					FFaunaCellContext Ctx;
					Ctx.SeabedZCm = Depths[(X + Y + 40) % 5];
					Ctx.DistanceToCoastCm = FMath::Abs(X) * 10000.0f;
					Ctx.bNearTeeth = X == 3;
					TArray<FFaunaSpawn> Day;
					TArray<FFaunaSpawn> Again;
					TArray<FFaunaSpawn> Night;
					FFaunaSpawnRules::SpawnsForCell(20260926u, Cell, Ctx, Day);
					FFaunaSpawnRules::SpawnsForCell(20260926u, Cell, Ctx, Again);
					Ctx.Hours = 0.5f;
					FFaunaSpawnRules::SpawnsForCell(20260926u, Cell, Ctx, Night);
					if (Day.Num() != Again.Num())
					{
						++Mismatch;
					}
					for (int32 I = 0; I < Day.Num() && I < Again.Num(); ++I)
					{
						Mismatch += (Day[I].Species != Again[I].Species || Day[I].PositionCm != Again[I].PositionCm
							|| Day[I].GroupSize != Again[I].GroupSize || Day[I].Seed != Again[I].Seed) ? 1 : 0;
					}
					const EMarineZone Zone = FFaunaSpawnRules::ClassifyZone(Ctx.SeabedZCm, false);
					for (const FFaunaSpawn& Spawn : Day)
					{
						const bool bBird = FFaunaSpeciesInfo::Get(Spawn.Species).bBird;
						TigerOutsideDeep += Spawn.Species == EFaunaSpecies::TigerShark && Zone != EMarineZone::Deep ? 1 : 0;
						MarineOnLand += !bBird && Zone == EMarineZone::Land ? 1 : 0;
						BirdsOffshore += bBird && !Ctx.bNearTeeth && Ctx.DistanceToCoastCm >= FFaunaSpawnRules::BirdCoastRangeCm ? 1 : 0;
						Whales += Spawn.Species == EFaunaSpecies::HumpbackWhale ? 1 : 0;
						ReefDay += Spawn.Species == EFaunaSpecies::ReefFish ? 1 : 0;
						OutsideCell += FFaunaSpawnRules::CellOf(FVector2D(Spawn.PositionCm.X, Spawn.PositionCm.Y)) != Cell ? 1 : 0;
					}
					for (const FFaunaSpawn& Spawn : Night)
					{
						ReefNight += Spawn.Species == EFaunaSpecies::ReefFish ? 1 : 0;
						const bool bAlsoByDay = Day.ContainsByPredicate([&Spawn](const FFaunaSpawn& D) { return D.Species == Spawn.Species; });
						const bool bNightFavoured = FFaunaActivity::Level(Spawn.Species, 0.5f) > FFaunaActivity::Level(Spawn.Species, 12.0f);
						NightNotSubset += !bAlsoByDay && !bNightFavoured ? 1 : 0;
					}
				}
			}
			TestEqual(TEXT("Determinista"), Mismatch, 0);
			TestEqual(TEXT("Tiburón tigre solo en aguas profundas"), TigerOutsideDeep, 0);
			TestEqual(TEXT("Nada marino en tierra"), MarineOnLand, 0);
			TestEqual(TEXT("Aves solo cerca de la costa o en Los Dientes"), BirdsOffshore, 0);
			TestEqual(TEXT("Ballenas solo con el paso de ballenas"), Whales, 0);
			TestEqual(TEXT("Dentro de su celda"), OutsideCell, 0);
			TestTrue(FString::Printf(TEXT("Menos peces de arrecife de noche (%d día, %d noche)"), ReefDay, ReefNight), ReefDay > 0 && ReefNight < ReefDay);
			TestEqual(TEXT("De noche solo quedan grupos que ya estaban de día (salvo las especies nocturnas)"), NightNotSubset, 0);

			FFaunaCellContext Passage;
			Passage.SeabedZCm = -6500.0f;
			Passage.DistanceToCoastCm = 60000.0f;
			Passage.bWhalePassage = true;
			TestTrue(TEXT("Con paso de ballenas pueden aparecer lejos"), FFaunaSpawnRules::Chance(EFaunaSpecies::HumpbackWhale, Passage) > 0.0f);
			Passage.DistanceToCoastCm = 5000.0f;
			TestEqual(TEXT("Pero nunca cerca de la costa"), FFaunaSpawnRules::Chance(EFaunaSpecies::HumpbackWhale, Passage), 0.0f);
		});

		It("elige el nivel de LOD por distancia con histéresis", [this]()
		{
			const FFaunaLodSettings S = FFaunaLod::ForSpecies(EFaunaSpecies::ReefFish);
			TestEqual(TEXT("Cerca: completo"), static_cast<int32>(FFaunaLod::Tier(1000.0f, EFaunaLodTier::Frozen, S)), static_cast<int32>(EFaunaLodTier::Full));
			TestEqual(TEXT("Media: reducido"), static_cast<int32>(FFaunaLod::Tier(S.ReducedRadiusCm * 0.5f, EFaunaLodTier::Frozen, S)), static_cast<int32>(EFaunaLodTier::Reduced));
			TestEqual(TEXT("Lejos: congelado"), static_cast<int32>(FFaunaLod::Tier(S.ReducedRadiusCm * 2.0f, EFaunaLodTier::Full, S)), static_cast<int32>(EFaunaLodTier::Frozen));
			// Justo por fuera del radio: se queda en su nivel anterior.
			const float Edge = S.FullRadiusCm * 1.05f;
			TestEqual(TEXT("Histéresis al alejarse"), static_cast<int32>(FFaunaLod::Tier(Edge, EFaunaLodTier::Full, S)), static_cast<int32>(EFaunaLodTier::Full));
			TestEqual(TEXT("Histéresis al acercarse"), static_cast<int32>(FFaunaLod::Tier(S.FullRadiusCm * 0.95f, EFaunaLodTier::Reduced, S)), static_cast<int32>(EFaunaLodTier::Reduced));
			TestTrue(TEXT("La ballena se mueve de más lejos"), FFaunaLod::ForSpecies(EFaunaSpecies::HumpbackWhale).ReducedRadiusCm > S.ReducedRadiusCm * 3.0f);

			// Monótono: al alejarse nunca se sube de nivel.
			EFaunaLodTier Tier = EFaunaLodTier::Full;
			bool bMonotonic = true;
			for (float D = 0.0f; D < 40000.0f; D += 250.0f)
			{
				const EFaunaLodTier Next = FFaunaLod::Tier(D, Tier, S);
				bMonotonic &= static_cast<int32>(Next) >= static_cast<int32>(Tier);
				Tier = Next;
			}
			TestTrue(TEXT("Monótono"), bMonotonic);
		});

		It("reparte las actualizaciones reducidas y conserva el tiempo", [this]()
		{
			const int32 Interval = 4;
			bool bOncePerInterval = true;
			for (uint32 Id = 0; Id < 50; ++Id)
			{
				int32 Ticks = 0;
				float Accumulated = 0.0f;
				for (uint64 Frame = 0; Frame < 400; ++Frame)
				{
					if (FFaunaLod::ShouldTick(EFaunaLodTier::Reduced, Frame, Id, Interval))
					{
						++Ticks;
						Accumulated += FFaunaLod::TickDelta(EFaunaLodTier::Reduced, 1.0f / 60.0f, Interval);
					}
				}
				bOncePerInterval &= Ticks == 100 && FMath::IsNearlyEqual(Accumulated, 400.0f / 60.0f, 1.0e-3f);
			}
			TestTrue(TEXT("Una vez por intervalo y mismo tiempo total"), bOncePerInterval);
			TestFalse(TEXT("Congelado no se actualiza"), FFaunaLod::ShouldTick(EFaunaLodTier::Frozen, 0, 0, Interval));
			TestEqual(TEXT("Congelado no avanza"), FFaunaLod::TickDelta(EFaunaLodTier::Frozen, 0.016f, Interval), 0.0f);
			int32 SameFrame = 0;
			for (uint32 Id = 0; Id < 100; ++Id)
			{
				SameFrame += FFaunaLod::ShouldTick(EFaunaLodTier::Reduced, 7, Id, Interval) ? 1 : 0;
			}
			TestEqual(TEXT("Escalonado: un cuarto por fotograma"), SameFrame, 25);
		});
	});

	Describe("Animación procedural", [this]()
	{
		It("la onda de la columna se acelera y se amplía con la velocidad", [this]()
		{
			FFaunaAnimState Slow;
			FFaunaAnimState Fast;
			const FFaunaAnimParams A = FFaunaAnimation::Advance(EFaunaSpecies::ReefShark, Slow, 50.0f, 0.0f, 0.1f);
			const FFaunaAnimParams B = FFaunaAnimation::Advance(EFaunaSpecies::ReefShark, Fast, 500.0f, 0.0f, 0.1f);
			TestTrue(TEXT("Más frecuencia"), B.FrequencyHz > A.FrequencyHz * 1.5f);
			TestTrue(TEXT("Más amplitud"), B.Amplitude > A.Amplitude);
			TestEqual(TEXT("Mamíferos: onda vertical"), FFaunaAnimation::Advance(EFaunaSpecies::Dolphin, Slow, 300.0f, 0.0f, 0.1f).Secondary, 1.0f);
		});

		It("la fase es continua y queda en [0, 1)", [this]()
		{
			FFaunaAnimState State;
			State.Phase01 = FFaunaAnimation::InitialPhase(1234u);
			float Previous = State.Phase01;
			bool bOk = true;
			for (int32 I = 0; I < 2000; ++I)
			{
				const float Speed = 100.0f + 300.0f * FMath::Sin(I * 0.01f);
				const FFaunaAnimParams P = FFaunaAnimation::Advance(EFaunaSpecies::OpenSeaFish, State, Speed, 0.0f, 1.0f / 60.0f);
				float Delta = P.Phase01 - Previous;
				if (Delta < 0.0f)
				{
					Delta += 1.0f;
				}
				bOk &= P.Phase01 >= 0.0f && P.Phase01 < 1.0f && FMath::IsNearlyEqual(Delta, P.FrequencyHz / 60.0f, 1.0e-4f);
				Previous = P.Phase01;
			}
			TestTrue(TEXT("Sin saltos"), bOk);
		});

		It("la medusa late a su ritmo y las aves nunca pliegan las alas", [this]()
		{
			FFaunaAnimState Jelly;
			const FFaunaAnimParams Still = FFaunaAnimation::Advance(EFaunaSpecies::Jellyfish, Jelly, 0.0f, 0.0f, 0.1f);
			const FFaunaAnimParams Moving = FFaunaAnimation::Advance(EFaunaSpecies::Jellyfish, Jelly, 20.0f, 0.0f, 0.1f);
			TestEqual(TEXT("Misma frecuencia"), Still.FrequencyHz, Moving.FrequencyHz);
			TestTrue(TEXT("Late"), Still.Amplitude > 0.0f);

			FFaunaAnimState Bird;
			const FFaunaAnimParams Climb = FFaunaAnimation::Advance(EFaunaSpecies::Gull, Bird, 900.0f, 100.0f, 0.1f);
			const FFaunaAnimParams Dive = FFaunaAnimation::Advance(EFaunaSpecies::Gull, Bird, 900.0f, -400.0f, 0.1f);
			const FFaunaAnimParams Slowest = FFaunaAnimation::Advance(EFaunaSpecies::Frigatebird, Bird, 0.0f, 0.0f, 0.1f);
			TestTrue(TEXT("Aletea al subir"), Climb.Amplitude > 10.0f);
			TestTrue(TEXT("Planea al bajar"), Dive.Secondary > 0.9f && Dive.Amplitude < Climb.Amplitude);
			TestTrue(TEXT("Nunca plegadas"), Climb.WingFold == 0.0f && Dive.WingFold == 0.0f && Slowest.WingFold == 0.0f);

			FFaunaAnimState Ray;
			TestEqual(TEXT("Raya enterrada: quieta"), FFaunaAnimation::Advance(EFaunaSpecies::Stingray, Ray, 0.0f, 0.0f, 0.1f, true).Amplitude, 0.0f);
		});
	});
}

#endif
