#include "Misc/AutomationTest.h"

#include "Cooking/CookingModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CookingTest
{
	TArray<FCookIngredient> Ingredients(const TArray<FName>& Ids)
	{
		TArray<FCookIngredient> Out;
		for (const FName& Id : Ids)
		{
			Out.Add(FCookIngredient(Id));
		}
		return Out;
	}

	/** Calor de una hoguera con buena leña. */
	FCookEnvironment GoodFire()
	{
		FCookEnvironment Env;
		Env.FireHeat = 0.9f;
		Env.Smoke = 0.35f;
		Env.FireLevel = EFireLevel::Hoguera;
		return Env;
	}

	/** Empieza y cocina hasta que deja de estar «haciéndose» (con tope). */
	FCookingPot CookUntilReady(ECookTechnique Technique, FName Vessel, const TArray<FName>& Ids, const FCookEnvironment& Env)
	{
		const FCookingData& Data = FCookingData::Default();
		FCookingPot Pot;
		FString Reason;
		if (!FCookingModel::StartPot(Pot, Data, Technique, Vessel, Ingredients(Ids), Env.FireLevel, Reason))
		{
			return Pot;
		}
		for (int32 I = 0; I < 2000 && Pot.Status == EPotStatus::Cooking; ++I)
		{
			FCookingModel::TickPot(Pot, Data, Env, 1.0f);
		}
		return Pot;
	}

	/** Un ingrediente de los datos que tenga esta etiqueta y esté crudo. */
	FName RawWithTag(const FCookingData& Data, FName Tag)
	{
		for (const FFoodDef& Food : Data.Foods)
		{
			if (Food.State == EFoodState::Raw && Food.Tags.Contains(Tag))
			{
				return Food.ItemId;
			}
		}
		return NAME_None;
	}
}

BEGIN_DEFINE_SPEC(FCookingSpec, "Explored.Cooking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FCookingSpec)

void FCookingSpec::Define()
{
	using namespace CookingTest;

	Describe("las recetas (recipes.json)", [this]()
	{
		It("cada receta casa con sus propios ingredientes en su utensilio y su nivel de fuego", [this]()
		{
			const FCookingData& Data = FCookingData::Default();
			TestTrue(TEXT("Hay recetas"), Data.Recipes.Num() >= 12);
			for (const FCookRecipeDef& Recipe : Data.Recipes)
			{
				TArray<FCookIngredient> In;
				for (const FCookIngredientReq& Req : Recipe.Ingredients)
				{
					const FName Id = Req.ItemId.IsNone() ? RawWithTag(Data, Req.Tag) : Req.ItemId;
					TestFalse(FString::Printf(TEXT("«%s»: hay un ingrediente crudo con la etiqueta"), *Recipe.Id.ToString()), Id.IsNone());
					for (int32 C = 0; C < Req.Count; ++C)
					{
						In.Add(FCookIngredient(Id));
					}
				}
				const FName Vessel = Recipe.Vessels.Num() > 0 ? Recipe.Vessels[0] : FName(NAME_None);
				const FCookRecipeDef* Found = FCookingModel::FindRecipe(Data, Recipe.Technique, Vessel, In, EFireLevel::HornoArcilla);
				TestTrue(FString::Printf(TEXT("«%s» se encuentra y da su resultado"), *Recipe.Id.ToString()),
					Found != nullptr && Found->ResultItemId == Recipe.ResultItemId);
				TestTrue(FString::Printf(TEXT("«%s»: resultado y utensilios existen"), *Recipe.Id.ToString()),
					!Recipe.ResultItemId.IsNone() && !Recipe.Vessels.ContainsByPredicate([&Data](FName V) { return Data.FindVessel(V) == nullptr; }));
			}
		});

		It("gana la receta con más ingredientes: pescado, tubérculo y agua en vasija es estofado", [this]()
		{
			const FCookingData& Data = FCookingData::Default();
			const FCookRecipeDef* Stew = FCookingModel::FindRecipe(Data, ECookTechnique::Stew, TEXT("vasija_barro"),
				Ingredients({TEXT("pescado_arrecife"), TEXT("taro"), TEXT("agua_sin_tratar")}), EFireLevel::Fogata);
			TestTrue(TEXT("Estofado"), Stew && Stew->ResultItemId == FName(TEXT("estofado_pescado")));
			const FCookRecipeDef* Soup = FCookingModel::FindRecipe(Data, ECookTechnique::Stew, TEXT("olla_coco"),
				Ingredients({TEXT("agua_sin_tratar"), TEXT("pescado_arrecife")}), EFireLevel::Fogata);
			TestTrue(TEXT("Sopa, en cualquier orden"), Soup && Soup->ResultItemId == FName(TEXT("sopa_pescado")));
		});

		It("lo ya preparado no vuelve a entrar por etiqueta y hornear exige el horno", [this]()
		{
			const FCookingData& Data = FCookingData::Default();
			TestNull(TEXT("El pescado asado no se vuelve a asar"),
				FCookingModel::FindRecipe(Data, ECookTechnique::Roast, TEXT("espeto"), Ingredients({TEXT("pescado_asado")}), EFireLevel::Hoguera));
			const TArray<FCookIngredient> Clay = Ingredients({TEXT("arcilla_roja"), TEXT("arcilla_roja")});
			TestNull(TEXT("En una hoguera no hay cerámica"), FCookingModel::FindRecipe(Data, ECookTechnique::Bake, NAME_None, Clay, EFireLevel::Hoguera));
			const FCookRecipeDef* Pot = FCookingModel::FindRecipe(Data, ECookTechnique::Bake, NAME_None, Clay, EFireLevel::HornoArcilla);
			TestTrue(TEXT("En el horno sale la vasija"), Pot && Pot->ResultItemId == FName(TEXT("vasija_barro")));
		});

		It("respeta la capacidad del utensilio y exige recipiente estanco para hervir", [this]()
		{
			const FCookingData& Data = FCookingData::Default();
			FCookingPot Pot;
			FString Reason;
			TestFalse(TEXT("Tres cosas no caben en la olla de coco"), FCookingModel::StartPot(Pot, Data, ECookTechnique::Stew, TEXT("olla_coco"),
				Ingredients({TEXT("pescado_arrecife"), TEXT("taro"), TEXT("agua_sin_tratar")}), EFireLevel::Fogata, Reason));
			TestFalse(TEXT("Hay un motivo"), Reason.IsEmpty());
			TestFalse(TEXT("En un espeto no se hierve"), FCookingModel::StartPot(Pot, Data, ECookTechnique::Boil, TEXT("espeto"),
				Ingredients({TEXT("agua_sin_tratar")}), EFireLevel::Fogata, Reason));
			TestFalse(TEXT("Asar una piedra no"), FCookingModel::StartPot(Pot, Data, ECookTechnique::Roast, TEXT("espeto"),
				Ingredients({TEXT("canto_rodado")}), EFireLevel::Fogata, Reason));
			TestTrue(TEXT("La olla de coco se gasta y la vasija no"),
				FCookingModel::VesselWearPerUse(*Data.FindVessel(TEXT("olla_coco"))) > 0.0f
				&& FCookingModel::VesselWearPerUse(*Data.FindVessel(TEXT("vasija_barro"))) == 0.0f);
			TestEqual(TEXT("La vasija de barro es el utensilio de su objeto"), Data.VesselForItem(TEXT("vasija_barro")), FName(TEXT("vasija_barro")));
		});
	});

	Describe("la cocción", [this]()
	{
		It("hervir hace segura el agua (biblia §5.2)", [this]()
		{
			const FCookingData& Data = FCookingData::Default();
			const FFoodDef* Raw = Data.FindFood(TEXT("agua_sin_tratar"));
			TestTrue(TEXT("El agua sin tratar puede intoxicar"), Raw && Raw->Effects.Toxicity > 0.0f);

			const FCookingPot Pot = CookUntilReady(ECookTechnique::Boil, TEXT("olla_coco"), {TEXT("agua_sin_tratar")}, GoodFire());
			TestTrue(TEXT("Hecha"), Pot.Status == EPotStatus::Done);
			FCookedFood Out;
			TestTrue(TEXT("Se retira"), FCookingModel::Collect(Pot, Data, Out));
			TestEqual(TEXT("Agua hervida"), Out.ItemId, FName(TEXT("agua_hervida")));
			TestEqual(TEXT("Sin toxicidad"), Out.Effects.Toxicity, 0.0f);
			TestTrue(TEXT("Quita la sed"), Out.Effects.Water > 0.0f);

			// El agua no se quema aunque se quede horas en el fuego.
			FCookingPot Left = Pot;
			FCookingModel::TickPot(Left, Data, GoodFire(), 600.0f);
			TestTrue(TEXT("No se quema"), Left.Status == EPotStatus::Done);
		});

		It("cocer la yuca le quita la toxicidad, también en un guiso improvisado", [this]()
		{
			const FCookingData& Data = FCookingData::Default();
			const FFoodDef* Raw = Data.FindFood(TEXT("yuca"));
			TestTrue(TEXT("La yuca cruda es tóxica"), Raw && Raw->Effects.Toxicity >= 0.5f);

			FCookedFood Out;
			const FCookingPot Pot = CookUntilReady(ECookTechnique::Boil, TEXT("olla_coco"), {TEXT("yuca"), TEXT("agua_sin_tratar")}, GoodFire());
			TestTrue(TEXT("Se retira"), FCookingModel::Collect(Pot, Data, Out));
			TestEqual(TEXT("Yuca cocida"), Out.ItemId, FName(TEXT("yuca_cocida")));
			TestEqual(TEXT("Sin toxicidad"), Out.Effects.Toxicity, 0.0f);
			TestTrue(TEXT("Alimenta más que cruda"), Out.Effects.Food > Raw->Effects.Food);

			const FCookingPot Improvised = CookUntilReady(ECookTechnique::Stew, TEXT("vasija_barro"), {TEXT("yuca"), TEXT("coco_maduro")}, GoodFire());
			TestTrue(TEXT("Guiso improvisado"), FCookingModel::Collect(Improvised, Data, Out) && Out.ItemId == Data.Improvised.ResultItemId);
			TestEqual(TEXT("El guiso tampoco es tóxico"), Out.Effects.Toxicity, 0.0f);
			TestTrue(TEXT("Da calor"), Out.Effects.Warmth > 0.0f);
			TestTrue(TEXT("Suma lo que aportan los ingredientes con merma"),
				Out.Effects.Food < Raw->Effects.Food + Data.FindFood(TEXT("coco_maduro"))->Effects.Food && Out.Effects.Food > Raw->Effects.Food);
		});

		It("lo que se pasa en el fuego se quema", [this]()
		{
			const FCookingData& Data = FCookingData::Default();
			FCookingPot Pot;
			FString Reason;
			TestTrue(TEXT("Empieza"), FCookingModel::StartPot(Pot, Data, ECookTechnique::Roast, TEXT("espeto"),
				Ingredients({TEXT("pescado_arrecife")}), EFireLevel::Fogata, Reason));
			const FCookRecipeDef* Recipe = Data.FindRecipe(Pot.RecipeId);
			TestTrue(TEXT("Receta de pescado asado con margen para quemarse"), Recipe && Recipe->BurnAfterMinutes > 0.0f);
			FCookEnvironment Env = GoodFire();
			Env.FireHeat = FCookingModel::ReferenceHeat;
			FCookingModel::TickPot(Pot, Data, Env, Recipe->CookMinutes * 0.5f);
			TestTrue(TEXT("A medias"), Pot.Status == EPotStatus::Cooking);
			FCookedFood Out;
			TestFalse(TEXT("A medias no se retira"), FCookingModel::Collect(Pot, Data, Out));
			FCookingModel::TickPot(Pot, Data, Env, Recipe->CookMinutes * 0.5f + 1.0f);
			TestTrue(TEXT("Hecho"), Pot.Status == EPotStatus::Done);
			FCookingModel::TickPot(Pot, Data, Env, Recipe->BurnAfterMinutes);
			TestTrue(TEXT("Quemado"), Pot.Status == EPotStatus::Burnt);
			TestTrue(TEXT("Sale comida quemada"), FCookingModel::Collect(Pot, Data, Out) && Out.ItemId == Data.BurntItemId);
			TestTrue(TEXT("Baja el ánimo"), Out.Effects.Morale < 0.0f);
		});

		It("sin calor no avanza y en brasas va más despacio que en una hoguera", [this]()
		{
			FCookEnvironment Cold;
			TestEqual(TEXT("Sin fuego no asa"), FCookingModel::ProgressRate(ECookTechnique::Roast, Cold), 0.0f);
			FCookEnvironment Embers;
			Embers.FireHeat = 0.21f;
			const float Slow = FCookingModel::ProgressRate(ECookTechnique::Roast, Embers);
			TestTrue(TEXT("En brasas asa despacio"), Slow > 0.0f && Slow < FCookingModel::ProgressRate(ECookTechnique::Roast, GoodFire()));
		});

		It("ahumar necesita humo; salar y secar no necesitan fuego y la sal sale del agua de mar", [this]()
		{
			const FCookingData& Data = FCookingData::Default();
			FCookEnvironment NoSmoke;
			TestEqual(TEXT("Sin humo no ahúma"), FCookingModel::ProgressRate(ECookTechnique::Smoke, NoSmoke), 0.0f);
			FCookedFood Out;
			TestTrue(TEXT("Pescado ahumado"), FCookingModel::Collect(CookUntilReady(ECookTechnique::Smoke, TEXT("secadero"), {TEXT("pescado_arrecife")}, GoodFire()), Data, Out)
				&& Out.ItemId == FName(TEXT("pescado_ahumado")) && Out.State == EFoodState::Smoked);

			TestTrue(TEXT("Pescado salado sin fuego"), FCookingModel::Collect(CookUntilReady(ECookTechnique::Salt, NAME_None,
				{TEXT("sal_marina"), TEXT("pescado_arrecife")}, FCookEnvironment()), Data, Out) && Out.ItemId == FName(TEXT("pescado_salado")));

			FCookEnvironment Sunny;
			Sunny.Sun = 1.0f;
			TestTrue(TEXT("Sal al sol"), FCookingModel::Collect(CookUntilReady(ECookTechnique::Dry, TEXT("bandeja"), {TEXT("agua_mar")}, Sunny), Data, Out)
				&& Out.ItemId == FName(TEXT("sal_marina")));
			TestTrue(TEXT("Sal al fuego"), FCookingModel::Collect(CookUntilReady(ECookTechnique::Boil, TEXT("vasija_barro"), {TEXT("agua_mar")}, GoodFire()), Data, Out)
				&& Out.ItemId == FName(TEXT("sal_marina")));

			FCookEnvironment Rain;
			Rain.Rain = 0.8f;
			Rain.Sun = 0.5f;
			TestEqual(TEXT("A la intemperie con lluvia no seca"), FCookingModel::ProgressRate(ECookTechnique::Dry, Rain), 0.0f);
		});
	});

	Describe("la conservación", [this]()
	{
		It("crudo < cocinado < ahumado, salado o seco (GDD §8.8)", [this]()
		{
			const FCookingData& Data = FCookingData::Default();
			const float Raw = FCookingModel::ShelfLifeHours(Data, TEXT("pescado_arrecife"));
			const float Cooked = FCookingModel::ShelfLifeHours(Data, TEXT("pescado_asado"));
			const float Smoked = FCookingModel::ShelfLifeHours(Data, TEXT("pescado_ahumado"));
			const float Salted = FCookingModel::ShelfLifeHours(Data, TEXT("pescado_salado"));
			TestTrue(TEXT("El pescado crudo dura como mucho un día (biblia §4.4)"), Raw > 0.0f && Raw <= 24.0f);
			TestTrue(TEXT("Cocinado dura más"), Cooked > Raw);
			TestTrue(TEXT("Ahumado y salado duran más aún"), Smoked > Cooked && Salted > Cooked);
			const FPreservationDef& P = Data.Preservation;
			const auto Hours = [&P](EFoodState S) { return P.StateHours[static_cast<int32>(S)]; };
			TestTrue(TEXT("Orden general de los estados"), Hours(EFoodState::Raw) < Hours(EFoodState::Cooked)
				&& Hours(EFoodState::Cooked) < FMath::Min3(Hours(EFoodState::Smoked), Hours(EFoodState::Salted), Hours(EFoodState::Dried)));
			TestTrue(TEXT("La fruta seca dura más que la fresca"),
				FCookingModel::ShelfLifeHours(Data, TEXT("fruta_seca")) > FCookingModel::ShelfLifeHours(Data, TEXT("mango_fruta")));
			TestTrue(TEXT("El agua y la sal no se estropean"),
				FCookingModel::ShelfLifeHours(Data, TEXT("agua_hervida")) < 0.0f && FCookingModel::ShelfLifeHours(Data, TEXT("sal_marina")) < 0.0f);
		});

		It("la comida estropeada intoxica y más cuanto más se pudre", [this]()
		{
			const FCookingData& Data = FCookingData::Default();
			FFoodFreshness Fish;
			Fish.ItemId = TEXT("pescado_asado");
			const float Life = FCookingModel::ShelfLifeHours(Data, Fish.ItemId);
			const FConsumable Fresh = FCookingModel::CurrentEffects(Data, Fish);
			TestTrue(TEXT("Fresco"), FCookingModel::GetFreshness(Data, Fish) == EFreshness::Fresh);
			TestEqual(TEXT("Sin toxicidad"), Fresh.Toxicity, 0.0f);

			FCookingModel::Age(Fish, Life * 0.8f);
			TestTrue(TEXT("Pasado"), FCookingModel::GetFreshness(Data, Fish) == EFreshness::Stale);
			const FConsumable Stale = FCookingModel::CurrentEffects(Data, Fish);
			TestTrue(TEXT("Alimenta menos"), Stale.Food < Fresh.Food);
			TestEqual(TEXT("Aún no intoxica"), Stale.Toxicity, 0.0f);

			FCookingModel::Age(Fish, Life * 0.3f);
			TestTrue(TEXT("Estropeado"), FCookingModel::GetFreshness(Data, Fish) == EFreshness::Spoiled);
			const FConsumable Spoiled = FCookingModel::CurrentEffects(Data, Fish);
			TestTrue(TEXT("Intoxica"), Spoiled.Toxicity >= Data.Preservation.SpoiledToxicity);
			TestTrue(TEXT("Baja el ánimo"), Spoiled.Morale < Fresh.Morale);

			FCookingModel::Age(Fish, Life * 2.0f);
			TestEqual(TEXT("Podrido del todo"), FCookingModel::CurrentEffects(Data, Fish).Toxicity, Data.Preservation.RottenToxicity);

			FFoodFreshness Sealed;
			Sealed.ItemId = TEXT("pescado_asado");
			FCookingModel::Age(Sealed, Life * 1.1f, 0.5f);
			TestFalse(TEXT("En vasija sellada aguanta más"), FCookingModel::GetFreshness(Data, Sealed) == EFreshness::Spoiled);
		});

		It("la comida recién hecha da calor y se enfría en una hora", [this]()
		{
			const FCookingData& Data = FCookingData::Default();
			FCookedFood Soup;
			Soup.ItemId = TEXT("sopa_pescado");
			FFoodFreshness Hot = FCookingModel::FreshFromPot(Soup);
			TestTrue(TEXT("Caliente"), FCookingModel::CurrentEffects(Data, Hot).Warmth > 0.0f);
			FCookingModel::Age(Hot, Data.Preservation.WarmHours);
			TestEqual(TEXT("Fría"), FCookingModel::CurrentEffects(Data, Hot).Warmth, 0.0f);
		});
	});

	It("es determinista: la misma secuencia da la misma cocción y la misma frescura", [this]()
	{
		const FCookingData& Data = FCookingData::Default();
		auto Run = [&Data]()
		{
			FCookingPot Pot;
			FString Reason;
			FCookingModel::StartPot(Pot, Data, ECookTechnique::Stew, TEXT("vasija_barro"),
				Ingredients({TEXT("pescado_arrecife"), TEXT("batata"), TEXT("agua_hervida")}), EFireLevel::Hoguera, Reason);
			FCookEnvironment Env;
			for (int32 I = 0; I < 50; ++I)
			{
				Env.FireHeat = 0.2f + (I % 4) * 0.25f;
				FCookingModel::TickPot(Pot, Data, Env, 1.7f);
			}
			return Pot;
		};
		const FCookingPot A = Run();
		const FCookingPot B = Run();
		TestTrue(TEXT("Misma cocción"), A == B);
		TestEqual(TEXT("Estofado"), A.RecipeId, FName(TEXT("estofado_pescado")));
	});
}

#endif
