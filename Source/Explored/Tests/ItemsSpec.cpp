#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"

#include "Items/ItemRegistrySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FItemsSpec, "Explored.Items",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	TMap<FName, FItemDefinition> Items;
	TArray<FCraftingTemplateDef> Templates;
	TArray<FCraftingVerbDef> Verbs;

END_DEFINE_SPEC(FItemsSpec)

void FItemsSpec::Define()
{
	BeforeEach([this]()
	{
		FString ItemsJson, TemplatesJson, VerbsJson, Error;
		TestTrue(TEXT("Se lee items.json"), FFileHelper::LoadFileToString(ItemsJson, *UItemRegistrySubsystem::GetDataFilePath(TEXT("items.json"))));
		TestTrue(TEXT("Se lee templates.json"), FFileHelper::LoadFileToString(TemplatesJson, *UItemRegistrySubsystem::GetDataFilePath(TEXT("templates.json"))));
		TestTrue(TEXT("Se lee verbs.json"), FFileHelper::LoadFileToString(VerbsJson, *UItemRegistrySubsystem::GetDataFilePath(TEXT("verbs.json"))));

		if (!UItemRegistrySubsystem::ParseItemsJson(ItemsJson, Items, Error))
		{
			AddError(FString::Printf(TEXT("items.json: %s"), *Error));
		}
		if (!UItemRegistrySubsystem::ParseTemplatesJson(TemplatesJson, Templates, Error))
		{
			AddError(FString::Printf(TEXT("templates.json: %s"), *Error));
		}
		if (!UItemRegistrySubsystem::ParseVerbsJson(VerbsJson, Verbs, Error))
		{
			AddError(FString::Printf(TEXT("verbs.json: %s"), *Error));
		}
	});

	Describe("El catálogo de objetos", [this]()
	{
		It("tiene al menos 60 objetos, todos con id, nombre y malla", [this]()
		{
			TestTrue(TEXT("Al menos 60 objetos (biblia §3)"), Items.Num() >= 60);
			for (const TPair<FName, FItemDefinition>& Pair : Items)
			{
				const FItemDefinition& Item = Pair.Value;
				if (Item.Id.IsNone())
				{
					AddError(TEXT("Hay un objeto sin id"));
				}
				if (Item.NameEs.IsEmpty())
				{
					AddError(FString::Printf(TEXT("«%s» no tiene nombre en español"), *Item.Id.ToString()));
				}
				if (Item.MeshPath.IsNull())
				{
					AddError(FString::Printf(TEXT("«%s» no tiene malla (ni de marcador)"), *Item.Id.ToString()));
				}
			}
		});

		It("no tiene propiedades fuera del rango 0-5 de la biblia §2.1", [this]()
		{
			for (const TPair<FName, FItemDefinition>& Pair : Items)
			{
				for (const TPair<FName, float>& Prop : Pair.Value.Properties)
				{
					if (Prop.Value < 0.0f || Prop.Value > 5.0f)
					{
						AddError(FString::Printf(TEXT("«%s».%s = %.1f fuera de [0,5]"), *Pair.Key.ToString(), *Prop.Key.ToString(), Prop.Value));
					}
				}
			}
		});
	});

	Describe("Las plantillas de fabricación", [this]()
	{
		It("solo referencian objetos que existen en items.json", [this]()
		{
			TestTrue(TEXT("Hay al menos 15 plantillas (encargo M2)"), Templates.Num() >= 15);
			for (const FCraftingTemplateDef& Template : Templates)
			{
				if (!Items.Contains(Template.ResultDefinitionId))
				{
					AddError(FString::Printf(TEXT("Plantilla «%s» produce «%s», que no está en items.json"),
						*Template.Id.ToString(), *Template.ResultDefinitionId.ToString()));
				}
			}
		});

		It("solo usan verbos que existen en verbs.json", [this]()
		{
			TSet<FName> VerbIds;
			for (const FCraftingVerbDef& Verb : Verbs)
			{
				VerbIds.Add(Verb.Id);
			}

			TSet<FName> ExpectedVerbs = { TEXT("Golpear"), TEXT("Tallar"), TEXT("Atar"), TEXT("Pegar"),
				TEXT("Afilar"), TEXT("Trenzar"), TEXT("Machacar"), TEXT("Raspar") };
			for (const FName& Expected : ExpectedVerbs)
			{
				if (!VerbIds.Contains(Expected))
				{
					AddError(FString::Printf(TEXT("Falta el verbo «%s» en verbs.json (biblia §2.2)"), *Expected.ToString()));
				}
			}

			for (const FCraftingTemplateDef& Template : Templates)
			{
				for (const FName& VerbId : Template.Verbs)
				{
					if (!VerbIds.Contains(VerbId))
					{
						AddError(FString::Printf(TEXT("Plantilla «%s» usa el verbo «%s», que no está en verbs.json"),
							*Template.Id.ToString(), *VerbId.ToString()));
					}
				}
			}
		});

		It("no tiene ids duplicados", [this]()
		{
			TSet<FName> Seen;
			for (const FCraftingTemplateDef& Template : Templates)
			{
				if (Seen.Contains(Template.Id))
				{
					AddError(FString::Printf(TEXT("Id de plantilla duplicado: %s"), *Template.Id.ToString()));
				}
				Seen.Add(Template.Id);
			}
		});
	});
}

#endif
