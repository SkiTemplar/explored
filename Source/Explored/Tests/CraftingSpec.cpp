#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"

#include "Crafting/CraftingLibrary.h"
#include "Items/ItemRegistrySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CraftingTest
{
	FItemInstance MakeInstance(FName DefinitionId, int32 Quality = 3, float Durability = 1.0f)
	{
		FItemInstance Instance;
		Instance.DefinitionId = DefinitionId;
		Instance.Quality = Quality;
		Instance.Durability = Durability;
		Instance.Count = 1;
		return Instance;
	}
}

// Todo este Spec es puro: no crea ni un UWorld ni un GameInstance. Lee el
// JSON real del proyecto (para no divergir de lo que carga el juego) pero
// toda la lógica de fabricación se prueba con las variantes *WithData de
// UCraftingLibrary, que no dependen de ningún UObject.
BEGIN_DEFINE_SPEC(FCraftingSpec, "Explored.Crafting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	TMap<FName, FItemDefinition> Items;
	TArray<FCraftingTemplateDef> Templates;

END_DEFINE_SPEC(FCraftingSpec)

void FCraftingSpec::Define()
{
	using namespace CraftingTest;

	BeforeEach([this]()
	{
		FString ItemsJson, TemplatesJson, Error;
		FFileHelper::LoadFileToString(ItemsJson, *UItemRegistrySubsystem::GetDataFilePath(TEXT("items.json")));
		FFileHelper::LoadFileToString(TemplatesJson, *UItemRegistrySubsystem::GetDataFilePath(TEXT("templates.json")));
		if (!UItemRegistrySubsystem::ParseItemsJson(ItemsJson, Items, Error))
		{
			AddError(FString::Printf(TEXT("items.json: %s"), *Error));
		}
		if (!UItemRegistrySubsystem::ParseTemplatesJson(TemplatesJson, Templates, Error))
		{
			AddError(FString::Printf(TEXT("templates.json: %s"), *Error));
		}
	});

	Describe("Cada plantilla del catálogo", [this]()
	{
		It("es alcanzable con materiales del catálogo, en uno o dos pasos", [this]()
		{
			auto ExpectDirect = [this](const TCHAR* TemplateId, FName LeftId, FName RightId)
			{
				const int32 TemplateIndex = Templates.IndexOfByPredicate([TemplateId](const FCraftingTemplateDef& T) { return T.Id == FName(TemplateId); });
				if (TemplateIndex == INDEX_NONE)
				{
					AddError(FString::Printf(TEXT("No existe la plantilla «%s» en templates.json"), TemplateId));
					return;
				}
				const FName VerbId = Templates[TemplateIndex].Verbs[0];

				FItemInstance Result;
				FText FailReason;
				if (!UCraftingLibrary::ApplyWithData(MakeInstance(LeftId), MakeInstance(RightId), VerbId, Items, Templates, Result, FailReason))
				{
					AddError(FString::Printf(TEXT("Plantilla «%s» no alcanzable con %s + %s: %s"),
						TemplateId, *LeftId.ToString(), *RightId.ToString(), *FailReason.ToString()));
				}
			};

			// Un paso, directo del catálogo (biblia §3).
			ExpectDirect(TEXT("lasca_por_golpeo"), TEXT("canto_rodado"), TEXT("pedernal"));
			ExpectDirect(TEXT("estaca_por_tallado"), TEXT("lasca_pedernal"), TEXT("palo_recto"));
			ExpectDirect(TEXT("pasta_medicinal_por_machacado"), TEXT("canto_rodado"), TEXT("planta_medicinal_aloe"));
			ExpectDirect(TEXT("recipiente_de_coco"), TEXT("lasca_pedernal"), TEXT("coco_maduro"));
			ExpectDirect(TEXT("cordel"), TEXT("fibra_coco"), TEXT("corteza"));
			ExpectDirect(TEXT("cesta"), TEXT("hoja_palma"), TEXT("fibra_coco"));
			ExpectDirect(TEXT("atado_generico"), TEXT("palo_recto"), TEXT("liana"));
			ExpectDirect(TEXT("pegado_generico"), TEXT("palo_recto"), TEXT("resina"));
			ExpectDirect(TEXT("cuchillo"), TEXT("lasca_obsidiana"), TEXT("palo_recto"));
			ExpectDirect(TEXT("martillo"), TEXT("canto_rodado"), TEXT("palo_recto"));
			ExpectDirect(TEXT("antorcha"), TEXT("palo_recto"), TEXT("grasa"));
			ExpectDirect(TEXT("arco"), TEXT("vara_flexible"), TEXT("cuerda"));
			ExpectDirect(TEXT("flecha"), TEXT("bambu_fino"), TEXT("hueso_pequeno"));
			ExpectDirect(TEXT("pala"), TEXT("chapa_fuselaje"), TEXT("palo_recto"));

			// «cuerda» se trenza a partir de dos cordeles (biblia §2.2: cordel → cuerda).
			FItemInstance CordelA, CordelB, Cuerda;
			FText FailReason;
			UCraftingLibrary::ApplyWithData(MakeInstance(TEXT("fibra_coco")), MakeInstance(TEXT("corteza")), TEXT("Trenzar"), Items, Templates, CordelA, FailReason);
			UCraftingLibrary::ApplyWithData(MakeInstance(TEXT("fibra_coco")), MakeInstance(TEXT("corteza")), TEXT("Trenzar"), Items, Templates, CordelB, FailReason);
			TestTrue(TEXT("cuerda alcanzable con dos cordeles"),
				UCraftingLibrary::ApplyWithData(CordelA, CordelB, TEXT("Trenzar"), Items, Templates, Cuerda, FailReason));

			// Dos pasos: atar un mango y después la cabeza (hacha, lanza).
			FItemInstance MangoAtado;
			TestTrue(TEXT("paso 1: palo atado con liana"),
				UCraftingLibrary::ApplyWithData(MakeInstance(TEXT("palo_recto")), MakeInstance(TEXT("liana")), TEXT("Atar"), Items, Templates, MangoAtado, FailReason));

			FItemInstance Hacha;
			TestTrue(TEXT("paso 2: hacha con el mango atado y una lasca"),
				UCraftingLibrary::ApplyWithData(MangoAtado, MakeInstance(TEXT("lasca_pedernal")), TEXT("Atar"), Items, Templates, Hacha, FailReason));

			FItemInstance AstaAtada, Lanza;
			TestTrue(TEXT("paso 1: asta de bambú atada con liana"),
				UCraftingLibrary::ApplyWithData(MakeInstance(TEXT("bambu_grueso")), MakeInstance(TEXT("liana")), TEXT("Atar"), Items, Templates, AstaAtada, FailReason));
			TestTrue(TEXT("paso 2: lanza con el asta atada y un hueso"),
				UCraftingLibrary::ApplyWithData(AstaAtada, MakeInstance(TEXT("hueso_largo")), TEXT("Atar"), Items, Templates, Lanza, FailReason));

			// Afilar (no crea objeto nuevo: repara el que ya existe).
			FItemInstance Afilada;
			TestTrue(TEXT("afilar_generico alcanzable con un hacha y arenisca"),
				UCraftingLibrary::ApplyWithData(Hacha, MakeInstance(TEXT("arenisca")), TEXT("Afilar"), Items, Templates, Afilada, FailReason));
		});
	});

	Describe("La combinación por propiedades", [this]()
	{
		It("lasca + palo + liana producen un hacha con estadísticas heredadas de las piezas", [this]()
		{
			FText FailReason;
			FItemInstance MangoAtado;
			const bool bStep1 = UCraftingLibrary::ApplyWithData(
				MakeInstance(TEXT("palo_recto")), MakeInstance(TEXT("liana")), TEXT("Atar"), Items, Templates, MangoAtado, FailReason);
			TestTrue(TEXT("palo + liana se atan"), bStep1);

			FItemInstance Hacha;
			const bool bStep2 = UCraftingLibrary::ApplyWithData(
				MangoAtado, MakeInstance(TEXT("lasca_pedernal")), TEXT("Atar"), Items, Templates, Hacha, FailReason);
			TestTrue(TEXT("mango atado + lasca dan un hacha"), bStep2);
			if (!bStep2)
			{
				return;
			}

			TestEqual(TEXT("El hacha resultante es la definición «hacha»"), Hacha.DefinitionId, FName(TEXT("hacha")));

			const float Filo = ItemEffective::GetProperty(Hacha, TEXT("Filo"), Items);
			TestTrue(TEXT("El filo se hereda de la lasca (>0)"), Filo > 0.0f);
			TestEqual(TEXT("El filo es exactamente el de la lasca de pedernal"), Filo, Items[TEXT("lasca_pedernal")].GetProperty(TEXT("Filo")));

			const float Largo = ItemEffective::GetProperty(Hacha, TEXT("Largo"), Items);
			TestTrue(TEXT("El largo se hereda del palo (>0)"), Largo > 0.0f);

			const float ExpectedWeight = Items[TEXT("palo_recto")].WeightKg + Items[TEXT("liana")].WeightKg + Items[TEXT("lasca_pedernal")].WeightKg;
			TestEqual(TEXT("El peso es la suma de las tres piezas"), ItemEffective::GetWeightKg(Hacha, Items), ExpectedWeight);

			TestFalse(TEXT("El hacha tiene nombre generado"), Hacha.GeneratedName.IsEmpty());
		});

		It("es simétrica: da igual qué pieza vaya en cada mano", [this]()
		{
			const FItemInstance A = MakeInstance(TEXT("lasca_pedernal"));
			const FItemInstance B = MakeInstance(TEXT("palo_recto"));

			const TArray<FName> VerbsAB = UCraftingLibrary::FindActionsWithData(A, B, Items, Templates);
			const TArray<FName> VerbsBA = UCraftingLibrary::FindActionsWithData(B, A, Items, Templates);
			TestEqual(TEXT("Mismo número de verbos en cualquier orden"), VerbsAB.Num(), VerbsBA.Num());
			for (const FName& Verb : VerbsAB)
			{
				TestTrue(FString::Printf(TEXT("«%s» también aparece al invertir las manos"), *Verb.ToString()), VerbsBA.Contains(Verb));
			}

			TestTrue(TEXT("Hay al menos un verbo disponible"), VerbsAB.Num() > 0);
			FItemInstance ResultAB, ResultBA;
			FText FailReason;
			const bool bAB = UCraftingLibrary::ApplyWithData(A, B, VerbsAB[0], Items, Templates, ResultAB, FailReason);
			const bool bBA = UCraftingLibrary::ApplyWithData(B, A, VerbsAB[0], Items, Templates, ResultBA, FailReason);
			TestEqual(TEXT("Aplicar en un orden u otro da el mismo resultado"), bAB, bBA);
			if (bAB && bBA)
			{
				TestEqual(TEXT("Mismo objeto resultante"), ResultAB.DefinitionId, ResultBA.DefinitionId);
				TestEqual(TEXT("Mismo peso efectivo"), ItemEffective::GetWeightKg(ResultAB, Items), ItemEffective::GetWeightKg(ResultBA, Items));
			}
		});

		It("nunca devuelve más de tres verbos, para cualquier par del catálogo", [this]()
		{
			TArray<FName> Ids;
			Items.GenerateKeyArray(Ids);
			for (int32 IndexA = 0; IndexA < Ids.Num(); ++IndexA)
			{
				for (int32 IndexB = IndexA; IndexB < Ids.Num(); ++IndexB)
				{
					const TArray<FName> Verbs = UCraftingLibrary::FindActionsWithData(MakeInstance(Ids[IndexA]), MakeInstance(Ids[IndexB]), Items, Templates);
					if (Verbs.Num() > UCraftingLibrary::MaxActions)
					{
						AddError(FString::Printf(TEXT("%s + %s dan %d verbos"), *Ids[IndexA].ToString(), *Ids[IndexB].ToString(), Verbs.Num()));
						return;
					}
				}
			}
		});

		It("rechaza combinaciones sin sentido", [this]()
		{
			const TArray<FName> Verbs = UCraftingLibrary::FindActionsWithData(MakeInstance(TEXT("coco_maduro")), MakeInstance(TEXT("huevo")), Items, Templates);
			TestEqual(TEXT("Coco + huevo no sugieren ningún verbo"), Verbs.Num(), 0);

			FItemInstance Result;
			FText FailReason;
			const bool bApplied = UCraftingLibrary::ApplyWithData(MakeInstance(TEXT("coco_maduro")), MakeInstance(TEXT("huevo")), TEXT("Atar"), Items, Templates, Result, FailReason);
			TestFalse(TEXT("Coco + huevo no se pueden atar"), bApplied);
			TestFalse(TEXT("Da un motivo de rechazo"), FailReason.IsEmpty());
		});

		It("rechaza objetos inválidos", [this]()
		{
			const FItemInstance Invalid;
			const FItemInstance Valid = MakeInstance(TEXT("palo_recto"));
			TestEqual(TEXT("Sin verbos con un hueco vacío"), UCraftingLibrary::FindActionsWithData(Invalid, Valid, Items, Templates).Num(), 0);
		});
	});
}

#endif
