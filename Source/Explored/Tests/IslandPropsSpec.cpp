#include "Misc/AutomationTest.h"

#include "WorldGen/IslandPropsModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace IslandPropsSpecDetail
{
	FIslandPropType MakeType(const TCHAR* Name, FVector Size, int32 Color = 7)
	{
		FIslandPropType Type;
		Type.Name = Name;
		Type.Size = Size;
		Type.ColorIndex = Color;
		return Type;
	}

	FIslandProp MakeProp(const TCHAR* Id, const TCHAR* Type, int32 Amount = 0)
	{
		FIslandProp Prop;
		Prop.Id = Id;
		Prop.Type = Type;
		Prop.Amount = Amount;
		return Prop;
	}

	/** Colocaciones mínimas que cumplen las cantidades del GDD §5.3 y §6. */
	FIslandProps MakeGddProps()
	{
		FIslandProps Out;
		const TCHAR* Singles[] = { TEXT("lente"), TEXT("refugio_palma"), TEXT("refugio_faro"), TEXT("barril_lluvia"),
			TEXT("caja_suministros"), TEXT("hoguera_fija"), TEXT("faro_linterna"), TEXT("balsa_sitio"), TEXT("faro_torre"),
			TEXT("faro_casa"), TEXT("carguero_casco"), TEXT("carguero_puente"), TEXT("caja_mapas") };
		const TPair<const TCHAR*, int32> Multi[] = { { TEXT("palma"), 8 }, { TEXT("lena"), 10 }, { TEXT("fibra"), 15 },
			{ TEXT("cana_bambu"), 3 }, { TEXT("tronco"), 7 }, { TEXT("pagina"), 8 }, { TEXT("punto_pesca"), 3 },
			{ TEXT("shelter_volume"), 2 } };
		auto AddType = [&Out](const TCHAR* Name) { Out.Types.Add(Name, MakeType(Name, FVector(1.0))); };
		for (const TCHAR* Name : Singles)
		{
			AddType(Name);
			Out.Props.Add(MakeProp(Name, Name));
		}
		for (const TPair<const TCHAR*, int32>& Pair : Multi)
		{
			AddType(Pair.Key);
			for (int32 I = 1; I <= Pair.Value; ++I)
			{
				FIslandProp Prop = MakeProp(*FString::Printf(TEXT("%s_%d"), Pair.Key, I), Pair.Key);
				const FName Type(Pair.Key);
				Prop.Amount = Type == FName(TEXT("lena")) ? 2 : 1;
				Prop.Cocos = Type == FName(TEXT("palma")) ? 2 : 0;
				Prop.Page = Type == FName(TEXT("pagina")) ? I : 0;
				Out.Props.Add(Prop);
			}
		}
		return Out;
	}
}

BEGIN_DEFINE_SPEC(FIslandPropsSpec, "Explored.WorldGen.IslandProps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FIslandPropsSpec)

void FIslandPropsSpec::Define()
{
	using namespace IslandPropsSpecDetail;

	Describe("Validate", [this]()
	{
		It("acepta un conjunto coherente y cuenta por tipo", [this]()
		{
			const FIslandProps Props = MakeGddProps();
			FString Error;
			TestTrue(TEXT("válido"), Props.Validate(Error));
			TestEqual(TEXT("palmas"), Props.CountOf(TEXT("palma")), 8);
			TestEqual(TEXT("cocos"), Props.TotalOf(TEXT("palma"), true), 16);
			TestEqual(TEXT("leña"), Props.TotalOf(TEXT("lena")), 20);
			TestEqual(TEXT("tipo inexistente"), Props.CountOf(TEXT("nada")), 0);
		});

		It("rechaza ids repetidos, tipos sin definir y anclas posteriores", [this]()
		{
			FString Error;
			FIslandProps Props = MakeGddProps();
			const FIslandProp Duplicate = Props.Props[0];
			Props.Props.Add(Duplicate);
			TestFalse(TEXT("id repetido"), Props.Validate(Error));

			Props = MakeGddProps();
			Props.Props[0].Type = TEXT("no_existe");
			TestFalse(TEXT("tipo sin definir"), Props.Validate(Error));

			Props = MakeGddProps();
			Props.Props[0].Anchor = Props.Props.Last().Id;
			TestFalse(TEXT("ancla posterior"), Props.Validate(Error));

			Props = MakeGddProps();
			Props.Props.Last().Anchor = Props.Props[0].Id;
			TestTrue(TEXT("ancla anterior"), Props.Validate(Error));
		});

		It("rechaza tamaños no positivos y colores fuera de la paleta", [this]()
		{
			FString Error;
			FIslandProps Props = MakeGddProps();
			Props.Types.Add(TEXT("plano"), MakeType(TEXT("plano"), FVector(1.0, 1.0, 0.0)));
			TestFalse(TEXT("tamaño cero"), Props.Validate(Error));
			Props = MakeGddProps();
			Props.Types.Add(TEXT("chillon"), MakeType(TEXT("chillon"), FVector(1.0), 32));
			TestFalse(TEXT("color 32"), Props.Validate(Error));
		});
	});

	Describe("MatchesGdd", [this]()
	{
		It("acepta las cantidades del GDD", [this]()
		{
			FString Error;
			TestTrue(TEXT("cantidades"), MakeGddProps().MatchesGdd(Error));
		});

		It("detecta una palma de menos, leña que no suma y una página perdida", [this]()
		{
			FString Error;
			FIslandProps Props = MakeGddProps();
			Props.Props.RemoveAll([](const FIslandProp& P) { return P.Id == FName(TEXT("palma_1")); });
			TestFalse(TEXT("7 palmas"), Props.MatchesGdd(Error));

			Props = MakeGddProps();
			for (FIslandProp& P : Props.Props)
			{
				P.Amount = P.Type == FName(TEXT("lena")) ? 1 : P.Amount;
			}
			TestFalse(TEXT("10 de leña"), Props.MatchesGdd(Error));

			Props = MakeGddProps();
			for (FIslandProp& P : Props.Props)
			{
				P.Page = P.Page == 8 ? 1 : P.Page;
			}
			TestFalse(TEXT("sin página 8"), Props.MatchesGdd(Error));
		});
	});

	Describe("Placement", [this]()
	{
		It("escala la primitiva a su tamaño y apoya su base en el pivote", [this]()
		{
			const FIslandPropType Type = MakeType(TEXT("torre"), FVector(4.0, 4.0, 12.0));
			TestEqual(TEXT("escala"), IslandPropsPlacement::FallbackScale(Type), FVector(4.0, 4.0, 12.0));
			TestEqual(TEXT("desplazamiento"), IslandPropsPlacement::FallbackOffset(Type), FVector(0.0, 0.0, 600.0));
		});

		It("coloca en centímetros con la elevación sobre el suelo", [this]()
		{
			FIslandProp Prop = MakeProp(TEXT("linterna"), TEXT("faro_linterna"));
			Prop.Location = FVector2D(45.0, 15.0);
			Prop.Elevation = 12.0f;
			TestEqual(TEXT("posición"), IslandPropsPlacement::WorldLocation(Prop, 35.0f), FVector(4500.0, 1500.0, 4700.0));
		});

		It("elige la malla propia antes que la del tipo", [this]()
		{
			FIslandProps Props = MakeGddProps();
			Props.Types[TEXT("palma")].MeshPath = TEXT("/Game/Art/K1_Naturaleza/SM_PalmA_01");
			FIslandProp Palm = *Props.FindProp(TEXT("palma_1"));
			TestEqual(TEXT("del tipo"), Props.MeshPathFor(Palm), FString(TEXT("/Game/Art/K1_Naturaleza/SM_PalmA_01")));
			Palm.MeshOverride = TEXT("/Game/Art/K1_Naturaleza/SM_PalmB_01");
			TestEqual(TEXT("propia"), Props.MeshPathFor(Palm), FString(TEXT("/Game/Art/K1_Naturaleza/SM_PalmB_01")));
		});

		It("traduce formas, colisiones y celdas de paleta", [this]()
		{
			EIslandPropShape Shape = EIslandPropShape::Cube;
			TestTrue(TEXT("cilindro"), IslandPropsPlacement::ShapeFromName(TEXT("cylinder"), Shape) && Shape == EIslandPropShape::Cylinder);
			TestFalse(TEXT("toro"), IslandPropsPlacement::ShapeFromName(TEXT("toro"), Shape));
			EIslandPropCollision Collision = EIslandPropCollision::Solid;
			TestTrue(TEXT("query"), IslandPropsPlacement::CollisionFromName(TEXT("query"), Collision) && Collision == EIslandPropCollision::Query);
			TestFalse(TEXT("rara"), IslandPropsPlacement::CollisionFromName(TEXT("rara"), Collision));
			TestEqual(TEXT("celda 0"), IslandPropsPlacement::PaletteU(0), 0.5f / 32.0f);
			TestEqual(TEXT("celda 31"), IslandPropsPlacement::PaletteU(40), 31.5f / 32.0f);
			TestEqual(TEXT("cono"), FString(IslandPropsPlacement::BasicShapePath(EIslandPropShape::Cone)), FString(TEXT("/Engine/BasicShapes/Cone.Cone")));
		});
	});
}

#endif
