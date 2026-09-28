#include "Misc/AutomationTest.h"

#include "Building/BuildingModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildingSpecDetail
{
	/** Añade una pieza al catálogo de prueba con los datos de building_pieces.json. */
	void AddPiece(FBuildingCatalog& Catalog, const TCHAR* Id, const TCHAR* Tier, EBuildSocket Socket,
		TArray<FBuildingCost> Cost, TArray<FName> Tools, TArray<FName> Requires,
		float Minutes, float Integrity, int32 Cyclone, const TCHAR* Mesh = nullptr, bool bRespawn = false)
	{
		FBuildingPieceDef& Def = Catalog.Pieces.AddDefaulted_GetRef();
		Def.Id = FName(Id);
		Def.NameEs = FString(Id);
		Def.Tier = FName(Tier);
		Def.Socket = Socket;
		Def.Cost = MoveTemp(Cost);
		Def.Tools = MoveTemp(Tools);
		Def.RequiresPieces = MoveTemp(Requires);
		Def.BuildMinutes = Minutes;
		Def.Integrity = Integrity;
		Def.MaxCycloneCategory = Cyclone;
		Def.Mesh = Mesh ? FName(Mesh) : FName();
		Def.bRespawnPoint = bRespawn;
	}

	/** Subconjunto fiel de Content/Data/building_pieces.json (costes, herramientas y cifras reales). */
	FBuildingCatalog MakeCatalog()
	{
		using S = EBuildSocket;
		FBuildingCatalog C;
		C.Tiers.Add({FName(TEXT("palma")), 0, TEXT("Hoja de palma"), {}});
		C.Tiers.Add({FName(TEXT("bambu")), 1, TEXT("Bambú"), {FName(TEXT("cuchillo"))}});
		C.Tiers.Add({FName(TEXT("madera")), 2, TEXT("Madera"), {FName(TEXT("hacha"))}});
		C.Tiers.Add({FName(TEXT("piedra")), 3, TEXT("Piedra"), {FName(TEXT("martillo"))}});

		AddPiece(C, TEXT("fogata"), TEXT("palma"), S::GroundOnly,
			{{TEXT("canto_rodado"), 6}, {TEXT("rama_seca"), 5}, {TEXT("fibra_coco"), 1}}, {}, {}, 10, 30, 0, TEXT("SM_Campfire"), true);
		AddPiece(C, TEXT("hoguera_senal"), TEXT("palma"), S::GroundOnly,
			{{TEXT("rama_seca"), 12}, {TEXT("tronco_pequeno"), 2}, {TEXT("hoja_palma"), 6}}, {TEXT("hacha")}, {}, 45, 30, 0, TEXT("SM_Bonfire_Signal"));
		AddPiece(C, TEXT("cama_hojas"), TEXT("palma"), S::Furniture,
			{{TEXT("hoja_palma"), 6}, {TEXT("hoja_platano"), 4}}, {}, {}, 10, 15, 0, TEXT("SM_Bed_Leaves"));
		AddPiece(C, TEXT("suelo_palma"), TEXT("palma"), S::Floor,
			{{TEXT("hoja_palma"), 6}, {TEXT("palo_recto"), 4}, {TEXT("liana"), 2}}, {}, {}, 20, 30, 0);
		AddPiece(C, TEXT("pared_palma"), TEXT("palma"), S::Wall,
			{{TEXT("hoja_palma"), 8}, {TEXT("palo_recto"), 3}, {TEXT("liana"), 2}}, {}, {}, 20, 30, 0);
		AddPiece(C, TEXT("techo_palma"), TEXT("palma"), S::Roof,
			{{TEXT("hoja_palma"), 10}, {TEXT("palo_recto"), 4}, {TEXT("liana"), 2}}, {}, {}, 25, 30, 0, TEXT("SM_Roof_PalmThatch"));
		AddPiece(C, TEXT("puerta_palma"), TEXT("palma"), S::Door,
			{{TEXT("hoja_palma"), 6}, {TEXT("palo_recto"), 2}, {TEXT("liana"), 2}}, {}, {}, 15, 25, 0);
		AddPiece(C, TEXT("bancal"), TEXT("palma"), S::GroundOnly,
			{{TEXT("palo_recto"), 6}, {TEXT("arena"), 2}}, {TEXT("pala")}, {}, 30, 50, 2);
		AddPiece(C, TEXT("suelo_bambu"), TEXT("bambu"), S::Floor,
			{{TEXT("bambu_grueso"), 6}, {TEXT("cordel"), 3}}, {TEXT("cuchillo")}, {}, 30, 50, 1);
		AddPiece(C, TEXT("pared_bambu"), TEXT("bambu"), S::Wall,
			{{TEXT("bambu_grueso"), 4}, {TEXT("bambu_fino"), 6}, {TEXT("cordel"), 3}}, {TEXT("cuchillo")}, {}, 30, 50, 1);
		AddPiece(C, TEXT("pilote_bambu"), TEXT("bambu"), S::Pillar,
			{{TEXT("bambu_grueso"), 2}, {TEXT("cordel"), 1}}, {TEXT("cuchillo")}, {}, 15, 45, 1);
		AddPiece(C, TEXT("escalera_bambu"), TEXT("bambu"), S::Stairs,
			{{TEXT("bambu_grueso"), 2}, {TEXT("bambu_fino"), 4}, {TEXT("cordel"), 2}}, {TEXT("cuchillo")}, {}, 20, 40, 1);
		AddPiece(C, TEXT("espaldera"), TEXT("bambu"), S::GroundOnly,
			{{TEXT("bambu_fino"), 6}, {TEXT("cordel"), 4}}, {TEXT("cuchillo")}, {TEXT("bancal")}, 20, 35, 1);
		AddPiece(C, TEXT("suelo_madera"), TEXT("madera"), S::Floor,
			{{TEXT("tronco_pequeno"), 2}, {TEXT("madera_dura"), 4}, {TEXT("cuerda"), 2}}, {TEXT("hacha")}, {}, 45, 75, 2, TEXT("SM_Floor_Wood"));
		AddPiece(C, TEXT("pared_madera"), TEXT("madera"), S::Wall,
			{{TEXT("tronco_pequeno"), 3}, {TEXT("cuerda"), 2}, {TEXT("resina"), 1}}, {TEXT("hacha")}, {}, 45, 75, 2, TEXT("SM_Wall_Wood"));
		AddPiece(C, TEXT("techo_madera"), TEXT("madera"), S::Roof,
			{{TEXT("madera_dura"), 4}, {TEXT("hoja_palma"), 10}, {TEXT("cuerda"), 2}}, {TEXT("hacha")}, {}, 50, 65, 2);
		AddPiece(C, TEXT("pilote_madera"), TEXT("madera"), S::Pillar,
			{{TEXT("tronco_pequeno"), 1}, {TEXT("resina"), 1}}, {TEXT("hacha")}, {}, 30, 80, 2);
		AddPiece(C, TEXT("mesa_cartografia"), TEXT("madera"), S::Furniture,
			{{TEXT("madera_dura"), 6}, {TEXT("cuerda"), 2}}, {TEXT("hacha")}, {TEXT("techo_madera")}, 60, 60, 2);
		AddPiece(C, TEXT("cimiento_piedra"), TEXT("piedra"), S::Pillar,
			{{TEXT("basalto"), 6}, {TEXT("arcilla_roja"), 3}, {TEXT("arena"), 2}}, {TEXT("martillo"), TEXT("pala")}, {}, 90, 100, 3);
		AddPiece(C, TEXT("muro_piedra"), TEXT("piedra"), S::Wall,
			{{TEXT("basalto"), 8}, {TEXT("arcilla_roja"), 4}, {TEXT("arena"), 2}}, {TEXT("martillo")}, {TEXT("cimiento_piedra")}, 90, 100, 3);
		AddPiece(C, TEXT("horno_barro"), TEXT("piedra"), S::GroundOnly,
			{{TEXT("arcilla_roja"), 8}, {TEXT("canto_rodado"), 6}, {TEXT("arena"), 2}}, {TEXT("pala")}, {TEXT("fogata")}, 120, 90, 3);
		AddPiece(C, TEXT("chimenea"), TEXT("piedra"), S::Furniture,
			{{TEXT("basalto"), 4}, {TEXT("arcilla_roja"), 4}}, {TEXT("martillo")}, {TEXT("horno_barro"), TEXT("muro_piedra")}, 90, 95, 3);
		return C;
	}

	TSet<FName> AllTools()
	{
		return {FName(TEXT("cuchillo")), FName(TEXT("hacha")), FName(TEXT("martillo")), FName(TEXT("pala"))};
	}

	/** Inventario sobrado de todo lo que usa el catálogo de prueba. */
	TMap<FName, int32> RichInventory()
	{
		TMap<FName, int32> Inventory;
		for (const TCHAR* Item : {TEXT("canto_rodado"), TEXT("rama_seca"), TEXT("fibra_coco"), TEXT("tronco_pequeno"),
			TEXT("hoja_palma"), TEXT("hoja_platano"), TEXT("palo_recto"), TEXT("liana"), TEXT("arena"),
			TEXT("bambu_grueso"), TEXT("bambu_fino"), TEXT("cordel"), TEXT("madera_dura"), TEXT("cuerda"),
			TEXT("resina"), TEXT("basalto"), TEXT("arcilla_roja")})
		{
			Inventory.Add(FName(Item), 10000);
		}
		return Inventory;
	}

	/** Coloca sin preocuparse del coste; devuelve el id o INDEX_NONE. */
	int32 Place(FBuildingModel& Model, const TCHAR* DefId, int32 BaseId, int32 X, int32 Y, int32 Z, int32 Rotation = 0,
		bool bGround = false, EBuildFailReason* OutReason = nullptr)
	{
		TMap<FName, int32> Inventory = RichInventory();
		FBuildingPlaceRequest Request;
		Request.DefId = FName(DefId);
		Request.Placement.BaseId = BaseId;
		Request.Placement.Cell = FIntVector(X, Y, Z);
		Request.Placement.Rotation = Rotation;
		Request.bGroundContact = bGround;
		int32 Id = INDEX_NONE;
		const EBuildFailReason Reason = Model.TryPlace(Request, Inventory, AllTools(), Id);
		if (OutReason)
		{
			*OutReason = Reason;
		}
		return Id;
	}

	EBuildFailReason Check(const FBuildingModel& Model, const TCHAR* DefId, int32 BaseId, int32 X, int32 Y, int32 Z,
		int32 Rotation = 0, bool bGround = false, float* OutStability = nullptr)
	{
		FBuildingPlaceRequest Request;
		Request.DefId = FName(DefId);
		Request.Placement.BaseId = BaseId;
		Request.Placement.Cell = FIntVector(X, Y, Z);
		Request.Placement.Rotation = Rotation;
		Request.bGroundContact = bGround;
		return Model.CanPlace(Request, OutStability);
	}

	/** Caseta de una celda: suelo en el terreno y pared en el lado sur (giro 0). */
	void BuildFloorAndWall(FBuildingModel& Model, int32 BaseId, const TCHAR* Floor, const TCHAR* Wall, int32& OutFloor, int32& OutWall)
	{
		OutFloor = Place(Model, Floor, BaseId, 0, 0, 0, 0, true);
		OutWall = Place(Model, Wall, BaseId, 0, 0, 0, 0, false);
	}
}

BEGIN_DEFINE_SPEC(FBuildingSpec, "Explored.Building",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	TUniquePtr<FBuildingModel> Model;
	int32 Base = INDEX_NONE;

	void ExpectReason(const FString& What, EBuildFailReason Actual, EBuildFailReason Expected)
	{
		if (Actual != Expected)
		{
			AddError(FString::Printf(TEXT("%s: %s en vez de %s"), *What, LexToString(Actual), LexToString(Expected)));
		}
	}

END_DEFINE_SPEC(FBuildingSpec)

void FBuildingSpec::Define()
{
	using namespace BuildingSpecDetail;

	BeforeEach([this]()
	{
		Model = MakeUnique<FBuildingModel>(MakeCatalog());
		Base = Model->FindOrCreateBase(FVector(10.0, -30.0, 120.0));
	});

	AfterEach([this]()
	{
		Model = nullptr;
	});

	Describe("Rejilla", [this]()
	{
		It("crea bases con origen en la rejilla global y reutiliza la cercana", [this]()
		{
			const FBuildingBaseState* First = Model->FindBase(Base);
			TestNotNull(TEXT("Base creada"), First);
			if (!First)
			{
				return;
			}
			TestEqual(TEXT("Origen X en la rejilla"), First->Origin.X, 0.0);
			TestEqual(TEXT("Origen Y en la rejilla"), First->Origin.Y, 0.0);
			TestEqual(TEXT("Origen Z en el terreno"), First->Origin.Z, 120.0);
			TestEqual(TEXT("A 30 m es la misma base"), Model->FindOrCreateBase(FVector(3000.0, 0.0, 400.0)), Base);
			const int32 Far = Model->FindOrCreateBase(FVector(9000.0, 0.0, 0.0));
			TestNotEqual(TEXT("A 90 m es otra base"), Far, Base);
			TestEqual(TEXT("Búsqueda sin crear"), Model->FindBaseAt(FVector(8990.0, 50.0, 0.0)), Far);
		});

		It("encaja paredes en el lado más cercano con la paridad de giro correcta", [this]()
		{
			const FVector Origin = Model->FindBase(Base)->Origin;
			const FBuildingPlacement South = Model->SnapToGrid(Base, Origin + FVector(10.0, -90.0, 75.0), EBuildSocket::Wall, 1);
			TestEqual(TEXT("Lado sur: celda"), South.Cell, FIntVector(0, 0, 0));
			TestEqual(TEXT("Lado sur: giro par"), South.Rotation, 2);
			const FBuildingPlacement East = Model->SnapToGrid(Base, Origin + FVector(95.0, 5.0, 75.0), EBuildSocket::Wall, 0);
			TestEqual(TEXT("Lado este = oeste de la celda 1"), East.Cell, FIntVector(1, 0, 0));
			TestEqual(TEXT("Lado este: giro impar"), East.Rotation, 1);
			const FBuildingPlacement North = Model->SnapToGrid(Base, Origin + FVector(-20.0, 480.0, 75.0), EBuildSocket::Door, 6);
			TestEqual(TEXT("Lado norte de la celda (0,2)"), North.Cell, FIntVector(0, 3, 0));
			TestEqual(TEXT("Giro normalizado"), North.Rotation, 2);
		});

		It("encaja pilares en esquinas y techos en el piso de la coronación", [this]()
		{
			const FVector Origin = Model->FindBase(Base)->Origin;
			TestEqual(TEXT("Esquina mínima de la celda 0"),
				Model->SnapToGrid(Base, Origin + FVector(-95.0, -110.0, 0.0), EBuildSocket::Pillar, 0).Cell, FIntVector(0, 0, 0));
			TestEqual(TEXT("Esquina (1,1)"),
				Model->SnapToGrid(Base, Origin + FVector(90.0, 104.0, 0.0), EBuildSocket::Pillar, 0).Cell, FIntVector(1, 1, 0));
			TestEqual(TEXT("Techo sobre las paredes del piso 0"),
				Model->SnapToGrid(Base, Origin + FVector(0.0, 0.0, 325.0), EBuildSocket::Roof, 0).Cell, FIntVector(0, 0, 0));
			TestEqual(TEXT("Suelo del piso 1"),
				Model->SnapToGrid(Base, Origin + FVector(0.0, 0.0, 340.0), EBuildSocket::Floor, 0).Cell, FIntVector(0, 0, 1));
		});

		It("convierte la rejilla al mundo con las cotas del kit y vuelve al mismo hueco", [this]()
		{
			const FVector Origin = Model->FindBase(Base)->Origin;
			FBuildingPlacement Placement;
			Placement.BaseId = Base;
			Placement.Cell = FIntVector(2, -1, 1);
			Placement.Rotation = 1;
			FVector Location;
			float Yaw = 0.0f;
			TestTrue(TEXT("Suelo al mundo"), Model->PlacementToWorld(Placement, EBuildSocket::Floor, Location, Yaw));
			TestEqual(TEXT("Suelo del piso 1"), Location, Origin + FVector(400.0, -200.0, 325.0));
			TestEqual(TEXT("Guiñada"), Yaw, 90.0f);
			TestEqual(TEXT("Ida y vuelta"), Model->SnapToGrid(Base, Location + FVector(0.0, 0.0, 15.0), EBuildSocket::Floor, 1), Placement);

			TestTrue(TEXT("Pared al mundo"), Model->PlacementToWorld(Placement, EBuildSocket::Wall, Location, Yaw));
			TestEqual(TEXT("Pared oeste sobre la cara del suelo"), Location, Origin + FVector(300.0, -200.0, 340.0));
			TestTrue(TEXT("Techo al mundo"), Model->PlacementToWorld(Placement, EBuildSocket::Roof, Location, Yaw));
			TestEqual(TEXT("Techo en la coronación"), Location.Z, Origin.Z + 590.0);
			Placement.BaseId = 99;
			TestFalse(TEXT("Base inexistente"), Model->PlacementToWorld(Placement, EBuildSocket::Floor, Location, Yaw));
		});
	});

	Describe("Colocación", [this]()
	{
		It("apoya suelos en el terreno y rechaza solapes y suelos en el aire", [this]()
		{
			ExpectReason(TEXT("Suelo en el terreno"), Check(*Model, TEXT("suelo_palma"), Base, 0, 0, 0, 0, true), EBuildFailReason::None);
			ExpectReason(TEXT("Suelo en el aire"), Check(*Model, TEXT("suelo_palma"), Base, 5, 5, 0), EBuildFailReason::NoSupport);
			TestTrue(TEXT("Colocado"), Place(*Model, TEXT("suelo_palma"), Base, 0, 0, 0, 0, true) != INDEX_NONE);
			ExpectReason(TEXT("Mismo hueco"), Check(*Model, TEXT("suelo_bambu"), Base, 0, 0, 0, 0, true), EBuildFailReason::Occupied);
		});

		It("pone paredes y puertas en los lados de un suelo, no en el aire", [this]()
		{
			Place(*Model, TEXT("suelo_palma"), Base, 0, 0, 0, 0, true);
			ExpectReason(TEXT("Pared sur"), Check(*Model, TEXT("pared_palma"), Base, 0, 0, 0, 0), EBuildFailReason::None);
			ExpectReason(TEXT("Pared este (lado oeste de la celda 1)"), Check(*Model, TEXT("pared_palma"), Base, 1, 0, 0, 1), EBuildFailReason::None);
			ExpectReason(TEXT("Pared lejos"), Check(*Model, TEXT("pared_palma"), Base, 4, 0, 0, 0), EBuildFailReason::NoSupport);
			Place(*Model, TEXT("pared_palma"), Base, 0, 0, 0, 0);
			ExpectReason(TEXT("Puerta en un lado con pared"), Check(*Model, TEXT("puerta_palma"), Base, 0, 0, 0, 2), EBuildFailReason::Occupied);
			ExpectReason(TEXT("Puerta en el lado norte"), Check(*Model, TEXT("puerta_palma"), Base, 0, 1, 0, 0), EBuildFailReason::None);
			ExpectReason(TEXT("Giro fuera de rango"), Check(*Model, TEXT("pared_palma"), Base, 0, 1, 0, 5), EBuildFailReason::InvalidRotation);
			ExpectReason(TEXT("Pieza desconocida"), Check(*Model, TEXT("pared_de_oro"), Base, 0, 1, 0), EBuildFailReason::UnknownPiece);
			ExpectReason(TEXT("Base desconocida"), Check(*Model, TEXT("pared_palma"), 77, 0, 1, 0), EBuildFailReason::UnknownBase);
		});

		It("apoya el techo en paredes y nunca en el terreno", [this]()
		{
			Place(*Model, TEXT("suelo_palma"), Base, 0, 0, 0, 0, true);
			ExpectReason(TEXT("Techo sin paredes"), Check(*Model, TEXT("techo_palma"), Base, 0, 0, 0, 0, true), EBuildFailReason::NoSupport);
			Place(*Model, TEXT("pared_palma"), Base, 0, 0, 0, 0);
			ExpectReason(TEXT("Techo sobre una pared"), Check(*Model, TEXT("techo_palma"), Base, 0, 0, 0), EBuildFailReason::None);
			TestTrue(TEXT("Techo colocado"), Place(*Model, TEXT("techo_palma"), Base, 0, 0, 0) != INDEX_NONE);
			ExpectReason(TEXT("Suelo del piso 1 atraviesa el techo"), Check(*Model, TEXT("suelo_palma"), Base, 0, 0, 1, 0, true), EBuildFailReason::Occupied);
		});

		It("exige terreno a pilares y fuegos, y los fuegos no van sobre suelos", [this]()
		{
			ExpectReason(TEXT("Pilote en el aire"), Check(*Model, TEXT("pilote_bambu"), Base, 0, 0, 0), EBuildFailReason::NeedsGround);
			ExpectReason(TEXT("Fogata en el aire"), Check(*Model, TEXT("fogata"), Base, 3, 3, 0), EBuildFailReason::NeedsGround);
			ExpectReason(TEXT("Fogata en el terreno"), Check(*Model, TEXT("fogata"), Base, 3, 3, 0, 0, true), EBuildFailReason::None);
			Place(*Model, TEXT("suelo_palma"), Base, 0, 0, 0, 0, true);
			ExpectReason(TEXT("Fogata sobre el suelo"), Check(*Model, TEXT("fogata"), Base, 0, 0, 0, 0, true), EBuildFailReason::Occupied);
			ExpectReason(TEXT("Cama sobre el suelo"), Check(*Model, TEXT("cama_hojas"), Base, 0, 0, 0), EBuildFailReason::None);
			ExpectReason(TEXT("Cama en el aire"), Check(*Model, TEXT("cama_hojas"), Base, 2, 0, 0), EBuildFailReason::NoSupport);
			Place(*Model, TEXT("fogata"), Base, 3, 3, 0, 0, true);
			ExpectReason(TEXT("Suelo encima de la fogata"), Check(*Model, TEXT("suelo_palma"), Base, 3, 3, 0, 0, true), EBuildFailReason::Occupied);
		});

		It("apoya el suelo en pilotes con la pérdida vertical de su material", [this]()
		{
			Place(*Model, TEXT("pilote_bambu"), Base, 0, 0, 0, 0, true);
			float Stability = 0.0f;
			ExpectReason(TEXT("Suelo sobre un pilote"), Check(*Model, TEXT("suelo_bambu"), Base, 0, 0, 0, 0, false, &Stability), EBuildFailReason::None);
			TestEqual(TEXT("1 − pérdida vertical del bambú"), Stability, 0.8f, 1.0e-4f);
		});

		It("ocupa dos celdas con la escalera y no deja cerrar su hueco", [this]()
		{
			Place(*Model, TEXT("suelo_bambu"), Base, 0, 0, 0, 0, true);
			Place(*Model, TEXT("suelo_bambu"), Base, 0, 1, 0, 0, true);
			ExpectReason(TEXT("Escalera sin suelo"), Check(*Model, TEXT("escalera_bambu"), Base, 4, 4, 0), EBuildFailReason::NoSupport);
			TestTrue(TEXT("Escalera hacia +Y"), Place(*Model, TEXT("escalera_bambu"), Base, 0, 0, 0, 0) != INDEX_NONE);
			ExpectReason(TEXT("Cama en la segunda celda"), Check(*Model, TEXT("cama_hojas"), Base, 0, 1, 0), EBuildFailReason::Occupied);
			ExpectReason(TEXT("Suelo encima del arranque"), Check(*Model, TEXT("suelo_bambu"), Base, 0, 0, 1, 0, true), EBuildFailReason::Occupied);
		});
	});

	Describe("Apoyos", [this]()
	{
		It("limita el voladizo según el material", [this]()
		{
			struct FCase { const TCHAR* Floor; int32 Expected; };
			const FCase Cases[] = {{TEXT("suelo_palma"), 2}, {TEXT("suelo_bambu"), 3}, {TEXT("suelo_madera"), 4}};
			int32 Row = 0;
			for (const FCase& Case : Cases)
			{
				const int32 Y = Row++ * 3;
				Place(*Model, Case.Floor, Base, 0, Y, 0, 0, true);
				int32 Extended = 0;
				EBuildFailReason Reason = EBuildFailReason::None;
				for (int32 X = 1; X < 10; ++X)
				{
					if (Place(*Model, Case.Floor, Base, X, Y, 0, 0, false, &Reason) == INDEX_NONE)
					{
						break;
					}
					++Extended;
				}
				TestEqual(FString::Printf(TEXT("Voladizo de %s"), Case.Floor), Extended, Case.Expected);
				ExpectReason(FString::Printf(TEXT("Motivo del tope de %s"), Case.Floor), Reason, EBuildFailReason::Unstable);
			}
		});

		It("derrumba en cascada lo que se queda sin apoyo", [this]()
		{
			TArray<int32> Pillars;
			Pillars.Add(Place(*Model, TEXT("pilote_madera"), Base, 0, 0, 0, 0, true));
			Pillars.Add(Place(*Model, TEXT("pilote_madera"), Base, 1, 0, 0, 0, true));
			Pillars.Add(Place(*Model, TEXT("pilote_madera"), Base, 0, 1, 0, 0, true));
			Pillars.Add(Place(*Model, TEXT("pilote_madera"), Base, 1, 1, 0, 0, true));
			TArray<int32> Hut;
			Hut.Add(Place(*Model, TEXT("suelo_madera"), Base, 0, 0, 0));
			Hut.Add(Place(*Model, TEXT("pared_madera"), Base, 0, 0, 0, 0));
			Hut.Add(Place(*Model, TEXT("pared_madera"), Base, 0, 1, 0, 2));
			Hut.Add(Place(*Model, TEXT("pared_madera"), Base, 0, 0, 0, 1));
			Hut.Add(Place(*Model, TEXT("pared_madera"), Base, 1, 0, 0, 3));
			Hut.Add(Place(*Model, TEXT("techo_madera"), Base, 0, 0, 0));
			TestFalse(TEXT("Cabaña completa"), Hut.Contains(INDEX_NONE));
			TestEqual(TEXT("Techo: pilote → pared → techo (la pared se apoya en el pilote de su esquina)"), Model->GetStability(Hut.Last()), 0.75f, 1.0e-4f);

			TestEqual(TEXT("Quitar un pilote no tira nada"), Model->RemovePiece(Pillars[0]).Num(), 0);
			TestEqual(TEXT("Ni el segundo"), Model->RemovePiece(Pillars[1]).Num(), 0);
			TestEqual(TEXT("Ni el tercero"), Model->RemovePiece(Pillars[2]).Num(), 0);
			const TArray<int32> Fallen = Model->RemovePiece(Pillars[3]);
			TestEqual(TEXT("Con el último cae toda la cabaña"), Fallen, Hut);
			TestEqual(TEXT("No queda nada"), Model->GetPieces().Num(), 0);
		});

		It("mantiene lo que sigue apoyado por el otro extremo", [this]()
		{
			const int32 West = Place(*Model, TEXT("suelo_madera"), Base, 0, 0, 0, 0, true);
			const int32 A = Place(*Model, TEXT("suelo_madera"), Base, 1, 0, 0);
			const int32 B = Place(*Model, TEXT("suelo_madera"), Base, 2, 0, 0);
			const int32 East = Place(*Model, TEXT("suelo_madera"), Base, 3, 0, 0, 0, true);
			TestEqual(TEXT("El centro se apoya en los dos lados"), Model->GetStability(B), 0.8f, 1.0e-4f);
			TestEqual(TEXT("Nada cae al quitar el oeste"), Model->RemovePiece(West).Num(), 0);
			TestEqual(TEXT("A se apoya ahora desde el este"), Model->GetStability(A), 0.6f, 1.0e-4f);
			const TArray<int32> Fallen = Model->RemovePiece(East);
			TestEqual(TEXT("Sin apoyos caen A y B"), Fallen, TArray<int32>({A, B}));
		});

		It("sostiene dos pisos de madera con la pérdida vertical", [this]()
		{
			Place(*Model, TEXT("suelo_madera"), Base, 0, 0, 0, 0, true);
			const int32 Wall = Place(*Model, TEXT("pared_madera"), Base, 0, 0, 0, 0);
			const int32 Upper = Place(*Model, TEXT("suelo_madera"), Base, 0, 0, 1);
			const int32 UpperWall = Place(*Model, TEXT("pared_madera"), Base, 0, 0, 1, 0);
			TestEqual(TEXT("Pared baja"), Model->GetStability(Wall), 0.875f, 1.0e-4f);
			TestEqual(TEXT("Suelo alto"), Model->GetStability(Upper), 0.75f, 1.0e-4f);
			TestEqual(TEXT("Pared alta (apoyada en la de abajo)"), Model->GetStability(UpperWall), 0.75f, 1.0e-4f);
			TestTrue(TEXT("Lo de abajo cubre el suelo del piso 0"), Model->IsSheltered(Wall));
		});
	});

	Describe("Temporales", [this]()
	{
		It("un ciclón de categoría 1 tira la palma y respeta el bambú y la madera", [this]()
		{
			int32 PalmFloor, PalmWall, BambooFloor, BambooWall, WoodFloor, WoodWall;
			const int32 BaseB = Model->FindOrCreateBase(FVector(20000.0, 0.0, 0.0));
			const int32 BaseC = Model->FindOrCreateBase(FVector(40000.0, 0.0, 0.0));
			BuildFloorAndWall(*Model, Base, TEXT("suelo_palma"), TEXT("pared_palma"), PalmFloor, PalmWall);
			BuildFloorAndWall(*Model, BaseB, TEXT("suelo_bambu"), TEXT("pared_bambu"), BambooFloor, BambooWall);
			BuildFloorAndWall(*Model, BaseC, TEXT("suelo_madera"), TEXT("pared_madera"), WoodFloor, WoodWall);
			FBuildingWeather Cyclone;
			Cyclone.StormCategory = 1.0f;
			const FBuildingChangeResult Result = Model->Tick(10.0f, Cyclone);
			TestTrue(TEXT("Suelo de palma roto"), Result.Destroyed.Contains(PalmFloor));
			TestTrue(TEXT("Pared de palma rota"), Result.Destroyed.Contains(PalmWall));
			TestNotNull(TEXT("Bambú en pie"), Model->FindPiece(BambooWall));
			TestNotNull(TEXT("Madera en pie"), Model->FindPiece(WoodWall));
			TestTrue(TEXT("El bambú solo se desgasta"), Model->FindPiece(BambooWall)->Integrity > 49.0f);
		});

		It("un ciclón de categoría 2 rompe el bambú y la madera aguanta", [this]()
		{
			int32 BambooFloor, BambooWall, WoodFloor, WoodWall;
			const int32 BaseB = Model->FindOrCreateBase(FVector(20000.0, 0.0, 0.0));
			BuildFloorAndWall(*Model, Base, TEXT("suelo_bambu"), TEXT("pared_bambu"), BambooFloor, BambooWall);
			BuildFloorAndWall(*Model, BaseB, TEXT("suelo_madera"), TEXT("pared_madera"), WoodFloor, WoodWall);
			FBuildingWeather Cyclone;
			Cyclone.StormCategory = 2.0f;
			const FBuildingChangeResult Result = Model->Tick(10.0f, Cyclone);
			TestTrue(TEXT("Pared de bambú rota"), Result.Destroyed.Contains(BambooWall));
			TestTrue(TEXT("Madera intacta salvo desgaste"), Model->FindPiece(WoodWall) && Model->FindPiece(WoodWall)->Integrity > 74.0f);
		});

		It("lo mal apoyado aguanta una categoría menos", [this]()
		{
			// Suelo de madera en voladizo a tres celdas: estabilidad 0,4 < 0,5.
			Place(*Model, TEXT("suelo_madera"), Base, 0, 0, 0, 0, true);
			Place(*Model, TEXT("suelo_madera"), Base, 1, 0, 0);
			Place(*Model, TEXT("suelo_madera"), Base, 2, 0, 0);
			const int32 Far = Place(*Model, TEXT("suelo_madera"), Base, 3, 0, 0);
			const int32 Near = Place(*Model, TEXT("pared_madera"), Base, 0, 0, 0, 0);
			TestTrue(TEXT("Mal apoyado"), Model->GetStability(Far) < FBuildingModel::WellSupportedStability);
			FBuildingWeather Cyclone;
			Cyclone.StormCategory = 2.0f;
			Model->Tick(2.0f, Cyclone);
			// Exceso 1 durante 2 h: 40 % de 75 = 30 puntos (más un desgaste despreciable).
			TestEqual(TEXT("Daño al suelo en voladizo"), Model->FindPiece(Far)->Integrity, 45.0f, 0.1f);
			TestEqual(TEXT("La pared bien apoyada no sufre el temporal"), Model->FindPiece(Near)->Integrity, 75.0f, 0.1f);
		});

		It("una galerna daña la palma y no el bambú", [this]()
		{
			int32 PalmFloor, PalmWall, BambooFloor, BambooWall;
			const int32 BaseB = Model->FindOrCreateBase(FVector(20000.0, 0.0, 0.0));
			BuildFloorAndWall(*Model, Base, TEXT("suelo_palma"), TEXT("pared_palma"), PalmFloor, PalmWall);
			BuildFloorAndWall(*Model, BaseB, TEXT("suelo_bambu"), TEXT("pared_bambu"), BambooFloor, BambooWall);
			FBuildingWeather Gale;
			Gale.StormCategory = 0.5f;
			Model->Tick(1.0f, Gale);
			TestTrue(TEXT("Palma dañada"), Model->FindPiece(PalmFloor)->Integrity < 27.0f);
			TestTrue(TEXT("Bambú casi intacto"), Model->FindPiece(BambooFloor)->Integrity > 49.9f);
		});

		It("al romperse el suelo cae la pared que se apoyaba en él", [this]()
		{
			const int32 Floor = Place(*Model, TEXT("suelo_palma"), Base, 0, 0, 0, 0, true);
			const int32 Wall = Place(*Model, TEXT("pared_bambu"), Base, 0, 0, 0, 0);
			FBuildingWeather Cyclone;
			Cyclone.StormCategory = 1.0f;
			const FBuildingChangeResult Result = Model->Tick(10.0f, Cyclone);
			TestEqual(TEXT("Rotas"), Result.Destroyed, TArray<int32>({Floor}));
			TestEqual(TEXT("Derrumbadas"), Result.Collapsed, TArray<int32>({Wall}));
		});
	});

	Describe("Desgaste", [this]()
	{
		It("la lluvia pudre lo que no está a cubierto y la piedra apenas se entera", [this]()
		{
			Place(*Model, TEXT("suelo_palma"), Base, 0, 0, 0, 0, true);
			const int32 Wall = Place(*Model, TEXT("pared_palma"), Base, 0, 0, 0, 0);
			const int32 Bed = Place(*Model, TEXT("cama_hojas"), Base, 0, 0, 0);
			const int32 Exposed = Place(*Model, TEXT("suelo_palma"), Base, 5, 0, 0, 0, true);
			Place(*Model, TEXT("techo_palma"), Base, 0, 0, 0);
			const int32 Stone = Place(*Model, TEXT("cimiento_piedra"), Base, 8, 8, 0, 0, true);
			TestTrue(TEXT("La cama está a cubierto"), Model->IsSheltered(Bed));
			TestTrue(TEXT("La pared está a cubierto"), Model->IsSheltered(Wall));
			TestFalse(TEXT("El suelo suelto no"), Model->IsSheltered(Exposed));

			FBuildingWeather Rain;
			Rain.Rain = 1.0f;
			Model->Tick(24.0f, Rain);
			// Palma: 2 %/día × 30 = 0,6; bajo lluvia intensa ×3 = 1,8.
			TestEqual(TEXT("Suelo expuesto"), Model->FindPiece(Exposed)->Integrity, 28.2f, 1.0e-3f);
			TestEqual(TEXT("Cama a cubierto"), Model->FindPiece(Bed)->Integrity, 15.0f - 0.3f, 1.0e-3f);
			TestEqual(TEXT("Cimiento de piedra"), Model->FindPiece(Stone)->Integrity, 100.0f - 0.15f, 1.0e-3f);
			TestTrue(TEXT("Sin tiempo no pasa nada"), Model->Tick(0.0f, Rain).IsEmpty());
		});

		It("un paso de tiempo no finito no rompe ni derrumba nada", [this]()
		{
			const int32 Floor = Place(*Model, TEXT("suelo_madera"), Base, 0, 0, 0, 0, true);
			const int32 Wall = Place(*Model, TEXT("pared_madera"), Base, 0, 0, 0, 0);
			FBuildingWeather Cyclone;
			Cyclone.StormCategory = 3.0f;
			TestTrue(TEXT("NaN"), Model->Tick(std::numeric_limits<float>::quiet_NaN(), Cyclone).IsEmpty());
			TestTrue(TEXT("Infinito"), Model->Tick(std::numeric_limits<float>::infinity(), Cyclone).IsEmpty());
			TestTrue(TEXT("Suelo intacto"), Model->FindPiece(Floor) && Model->FindPiece(Floor)->Integrity == 75.0f);
			TestTrue(TEXT("Pared intacta"), Model->FindPiece(Wall) && Model->FindPiece(Wall)->Integrity == 75.0f);
		});
	});

	Describe("Reparación", [this]()
	{
		It("cuesta una fracción del coste proporcional al daño", [this]()
		{
			const int32 Floor = Place(*Model, TEXT("suelo_madera"), Base, 0, 0, 0, 0, true);
			TestEqual(TEXT("Intacto: nada que reparar"), Model->GetRepairCost(Floor).Num(), 0);
			Model->ApplyDamage(Floor, 60.0f);   // falta el 80 %: 0,5 × 0,8 = 40 % del coste
			const TArray<FBuildingCost> Cost = Model->GetRepairCost(Floor);
			const TArray<FBuildingCost> Expected = {{TEXT("tronco_pequeno"), 1}, {TEXT("madera_dura"), 2}, {TEXT("cuerda"), 1}};
			TestEqual(TEXT("Coste de reparación"), Cost, Expected);

			TMap<FName, int32> Inventory = {{TEXT("tronco_pequeno"), 1}, {TEXT("madera_dura"), 2}, {TEXT("cuerda"), 1}};
			ExpectReason(TEXT("Sin hacha"), Model->TryRepair(Floor, Inventory, {}), EBuildFailReason::MissingTools);
			TMap<FName, int32> Short = {{TEXT("tronco_pequeno"), 1}};
			ExpectReason(TEXT("Sin materiales"), Model->TryRepair(Floor, Short, AllTools()), EBuildFailReason::MissingMaterials);
			ExpectReason(TEXT("Reparación"), Model->TryRepair(Floor, Inventory, AllTools()), EBuildFailReason::None);
			TestEqual(TEXT("Integridad completa"), Model->FindPiece(Floor)->Integrity, 75.0f);
			TestEqual(TEXT("Materiales gastados"), Inventory.Num(), 0);
			ExpectReason(TEXT("Otra vez"), Model->TryRepair(Floor, Inventory, AllTools()), EBuildFailReason::NothingToRepair);
			ExpectReason(TEXT("Pieza inexistente"), Model->TryRepair(999, Inventory, AllTools()), EBuildFailReason::UnknownInstance);
		});

		It("romper una pieza con daño directo la quita y derrumba lo que sostenía", [this]()
		{
			const int32 Floor = Place(*Model, TEXT("suelo_bambu"), Base, 0, 0, 0, 0, true);
			const int32 Bed = Place(*Model, TEXT("cama_hojas"), Base, 0, 0, 0);
			TestTrue(TEXT("Golpe que no rompe"), Model->ApplyDamage(Floor, 10.0f).IsEmpty());
			const FBuildingChangeResult Result = Model->ApplyDamage(Floor, 100.0f);
			TestEqual(TEXT("Rota"), Result.Destroyed, TArray<int32>({Floor}));
			TestEqual(TEXT("La cama cae"), Result.Collapsed, TArray<int32>({Bed}));
		});

		It("un daño no finito se ignora", [this]()
		{
			const int32 Floor = Place(*Model, TEXT("suelo_bambu"), Base, 0, 0, 0, 0, true);
			TestTrue(TEXT("NaN"), Model->ApplyDamage(Floor, std::numeric_limits<float>::quiet_NaN()).IsEmpty());
			TestTrue(TEXT("Pieza en pie"), Model->FindPiece(Floor) && Model->FindPiece(Floor)->Integrity == 50.0f);
		});
	});

	Describe("Coste y herramientas", [this]()
	{
		It("exige las herramientas del nivel y las de la pieza", [this]()
		{
			TMap<FName, int32> Inventory = RichInventory();
			TArray<FName> MissingTools;
			ExpectReason(TEXT("Bambú sin cuchillo"), Model->CanAfford(TEXT("suelo_bambu"), Inventory, {}, nullptr, &MissingTools), EBuildFailReason::MissingTools);
			TestEqual(TEXT("Falta el cuchillo"), MissingTools, TArray<FName>({TEXT("cuchillo")}));
			ExpectReason(TEXT("Hoguera de señal sin hacha"), Model->CanAfford(TEXT("hoguera_senal"), Inventory, {}), EBuildFailReason::MissingTools);
			ExpectReason(TEXT("Palma a mano"), Model->CanAfford(TEXT("suelo_palma"), Inventory, {}), EBuildFailReason::None);
			MissingTools.Reset();
			Model->CanAfford(TEXT("cimiento_piedra"), Inventory, {}, nullptr, &MissingTools);
			TestEqual(TEXT("Piedra: martillo y pala"), MissingTools, TArray<FName>({TEXT("martillo"), TEXT("pala")}));
		});

		It("comprueba y descuenta los materiales exactos", [this]()
		{
			TMap<FName, int32> Inventory = {{TEXT("bambu_grueso"), 6}, {TEXT("cordel"), 2}, {TEXT("coco"), 1}};
			const TSet<FName> Tools = {FName(TEXT("cuchillo"))};
			TArray<FBuildingCost> Missing;
			ExpectReason(TEXT("Falta cordel"), Model->CanAfford(TEXT("suelo_bambu"), Inventory, Tools, &Missing), EBuildFailReason::MissingMaterials);
			TestEqual(TEXT("Un cordel"), Missing, TArray<FBuildingCost>({{TEXT("cordel"), 1}}));

			FBuildingPlaceRequest Request;
			Request.DefId = TEXT("suelo_bambu");
			Request.Placement.BaseId = Base;
			Request.bGroundContact = true;
			int32 Id = INDEX_NONE;
			ExpectReason(TEXT("Sin material no se coloca"), Model->TryPlace(Request, Inventory, Tools, Id), EBuildFailReason::MissingMaterials);
			TestEqual(TEXT("Y no gasta nada"), Inventory.FindRef(TEXT("cordel")), 2);
			Inventory.Add(TEXT("cordel"), 4);
			ExpectReason(TEXT("Con material"), Model->TryPlace(Request, Inventory, Tools, Id), EBuildFailReason::None);
			TestTrue(TEXT("Id válido"), Id != INDEX_NONE);
			TestFalse(TEXT("Bambú gastado"), Inventory.Contains(TEXT("bambu_grueso")));
			TestEqual(TEXT("Queda un cordel"), Inventory.FindRef(TEXT("cordel")), 1);
			TestEqual(TEXT("Lo ajeno no se toca"), Inventory.FindRef(TEXT("coco")), 1);
			ExpectReason(TEXT("Ocupado antes que el coste"), Model->TryPlace(Request, Inventory, Tools, Id), EBuildFailReason::Occupied);
		});

		It("escala el tiempo de trabajo con la eficiencia", [this]()
		{
			TestEqual(TEXT("Eficiencia 1"), Model->GetBuildMinutes(TEXT("suelo_madera")), 45.0f);
			TestEqual(TEXT("Eficiencia 1,5"), Model->GetBuildMinutes(TEXT("suelo_madera"), 1.5f), 30.0f, 1.0e-3f);
			TestEqual(TEXT("Eficiencia nula se limita a 0,1"), Model->GetBuildMinutes(TEXT("suelo_madera"), 0.0f), 450.0f, 1.0e-2f);
			TestEqual(TEXT("Pieza desconocida"), Model->GetBuildMinutes(TEXT("nada")), 0.0f);
		});
	});

	Describe("Piezas requeridas", [this]()
	{
		It("la espaldera necesita un bancal en la base", [this]()
		{
			ExpectReason(TEXT("Sin bancal"), Check(*Model, TEXT("espaldera"), Base, 0, 0, 0, 0, true), EBuildFailReason::MissingRequiredPiece);
			Place(*Model, TEXT("bancal"), Base, 1, 0, 0, 0, true);
			ExpectReason(TEXT("Con bancal"), Check(*Model, TEXT("espaldera"), Base, 0, 0, 0, 0, true), EBuildFailReason::None);
		});

		It("el muro de piedra necesita un cimiento en la misma base", [this]()
		{
			const int32 Other = Model->FindOrCreateBase(FVector(50000.0, 0.0, 0.0));
			Place(*Model, TEXT("cimiento_piedra"), Other, 0, 0, 0, 0, true);
			ExpectReason(TEXT("Cimiento en otra base"), Check(*Model, TEXT("muro_piedra"), Base, 0, 0, 0, 0, true), EBuildFailReason::MissingRequiredPiece);
			Place(*Model, TEXT("cimiento_piedra"), Base, 0, 0, 0, 0, true);
			ExpectReason(TEXT("Muro sobre el cimiento"), Check(*Model, TEXT("muro_piedra"), Base, 0, 0, 0, 0), EBuildFailReason::None);
		});

		It("la chimenea necesita horno y muro, y el horno una fogata", [this]()
		{
			Place(*Model, TEXT("cimiento_piedra"), Base, 0, 0, 0, 0, true);
			Place(*Model, TEXT("muro_piedra"), Base, 0, 0, 0, 0);
			ExpectReason(TEXT("Horno sin fogata"), Check(*Model, TEXT("horno_barro"), Base, 3, 0, 0, 0, true), EBuildFailReason::MissingRequiredPiece);
			ExpectReason(TEXT("Chimenea sin horno"), Check(*Model, TEXT("chimenea"), Base, 4, 0, 0, 0, true), EBuildFailReason::MissingRequiredPiece);
			Place(*Model, TEXT("fogata"), Base, 2, 0, 0, 0, true);
			TestTrue(TEXT("Horno"), Place(*Model, TEXT("horno_barro"), Base, 3, 0, 0, 0, true) != INDEX_NONE);
			ExpectReason(TEXT("Chimenea completa"), Check(*Model, TEXT("chimenea"), Base, 4, 0, 0, 0, true), EBuildFailReason::None);
		});
	});

	Describe("Reaparición", [this]()
	{
		It("solo las fogatas encendidas son puntos de reaparición", [this]()
		{
			const int32 Near = Place(*Model, TEXT("fogata"), Base, 1, 0, 0, 0, true);
			const int32 Far = Place(*Model, TEXT("fogata"), Base, 10, 0, 0, 0, true);
			const int32 Signal = Place(*Model, TEXT("hoguera_senal"), Base, 5, 5, 0, 0, true);
			TestEqual(TEXT("Apagadas no cuentan"), Model->GetRespawnPoints().Num(), 0);
			Model->SetLit(Near, true);
			Model->SetLit(Far, true);
			Model->SetLit(Signal, true);
			TestEqual(TEXT("Dos fogatas"), Model->GetRespawnPoints(), TArray<int32>({Near, Far}));
			TestFalse(TEXT("Pieza inexistente"), Model->SetLit(999, true));

			int32 Found = INDEX_NONE;
			FVector Location;
			const FVector Origin = Model->FindBase(Base)->Origin;
			TestTrue(TEXT("Hay una cerca"), Model->FindNearestRespawnPoint(Origin + FVector(1800.0, 0.0, 0.0), Found, Location));
			TestEqual(TEXT("La del este"), Found, Far);
			TestEqual(TEXT("En su celda"), Location, Origin + FVector(2000.0, 0.0, 0.0));

			Model->RemovePiece(Far);
			TestTrue(TEXT("Queda la otra"), Model->FindNearestRespawnPoint(Origin, Found, Location));
			TestEqual(TEXT("La del oeste"), Found, Near);
			Model->SetLit(Near, false);
			TestFalse(TEXT("Apagada, ninguna"), Model->FindNearestRespawnPoint(Origin, Found, Location));
		});
	});

	Describe("Guardado y determinismo", [this]()
	{
		It("guarda y restaura piezas, bases, estabilidad y contadores", [this]()
		{
			Place(*Model, TEXT("suelo_madera"), Base, 0, 0, 0, 0, true);
			const int32 Wall = Place(*Model, TEXT("pared_madera"), Base, 0, 0, 0, 0);
			const int32 Fire = Place(*Model, TEXT("fogata"), Base, 4, 0, 0, 0, true);
			Model->SetLit(Fire, true);
			Model->ApplyDamage(Wall, 20.0f);
			const FBuildingSaveState Saved = Model->SaveState();

			FBuildingModel Loaded(MakeCatalog());
			int32 Dropped = -1;
			TestTrue(TEXT("Carga limpia"), Loaded.LoadState(Saved, &Dropped));
			TestEqual(TEXT("Nada descartado"), Dropped, 0);
			TestEqual(TEXT("Mismas piezas"), Loaded.GetPieces(), Model->GetPieces());
			TestEqual(TEXT("Misma estabilidad"), Loaded.GetStability(Wall), Model->GetStability(Wall));
			TestEqual(TEXT("Reaparición"), Loaded.GetRespawnPoints(), TArray<int32>({Fire}));
			const FBuildingSaveState Again = Loaded.SaveState();
			TestEqual(TEXT("Siguiente id"), Again.NextPieceId, Saved.NextPieceId);
			TestEqual(TEXT("Bases"), Again.Bases, Saved.Bases);
			const int32 Next = Place(Loaded, TEXT("cama_hojas"), Base, 0, 0, 0);
			TestEqual(TEXT("Los ids siguen donde estaban"), Next, Saved.NextPieceId);
		});

		It("descarta al cargar las piezas desconocidas o solapadas", [this]()
		{
			Place(*Model, TEXT("suelo_palma"), Base, 0, 0, 0, 0, true);
			FBuildingSaveState Saved = Model->SaveState();
			FBuildingPieceState Unknown = Saved.Pieces[0];
			Unknown.Id = 50;
			Unknown.DefId = TEXT("pieza_retirada");
			FBuildingPieceState Duplicate = Saved.Pieces[0];
			Duplicate.Id = 51;
			Saved.Pieces.Add(Unknown);
			Saved.Pieces.Add(Duplicate);

			FBuildingModel Loaded(MakeCatalog());
			int32 Dropped = 0;
			TestFalse(TEXT("Carga con descartes"), Loaded.LoadState(Saved, &Dropped));
			TestEqual(TEXT("Dos descartadas"), Dropped, 2);
			TestEqual(TEXT("Queda una"), Loaded.GetPieces().Num(), 1);
			TestEqual(TEXT("Los ids no se reutilizan"), Loaded.SaveState().NextPieceId, 52);
		});

		It("la misma secuencia da el mismo resultado", [this]()
		{
			auto Run = []()
			{
				FBuildingModel M(MakeCatalog());
				const int32 B = M.FindOrCreateBase(FVector(0.0, 0.0, 0.0));
				for (int32 X = 0; X < 4; ++X)
				{
					Place(M, TEXT("suelo_bambu"), B, X, 0, 0, 0, X == 0);
					Place(M, TEXT("pared_palma"), B, X, 0, 0, 0);
				}
				FBuildingWeather Weather;
				Weather.Rain = 0.7f;
				Weather.StormCategory = 1.0f;
				const FBuildingChangeResult Result = M.Tick(6.0f, Weather);
				TArray<int32> Log = Result.Destroyed;
				Log.Append(Result.Collapsed);
				return TPair<FBuildingSaveState, TArray<int32>>(M.SaveState(), Log);
			};
			const TPair<FBuildingSaveState, TArray<int32>> A = Run();
			const TPair<FBuildingSaveState, TArray<int32>> B = Run();
			TestEqual(TEXT("Mismas piezas"), A.Key.Pieces, B.Key.Pieces);
			TestEqual(TEXT("Mismos cambios"), A.Value, B.Value);
			TestTrue(TEXT("El temporal ha hecho algo"), A.Value.Num() > 0);
		});
	});

	Describe("Mallas", [this]()
	{
		It("usa la malla propia o la del kit modular del material", [this]()
		{
			const FBuildingCatalog& Catalog = Model->GetCatalog();
			TestEqual(TEXT("Fogata"), Model->ResolveMeshPath(*Catalog.FindPiece(TEXT("fogata"))),
				FString(TEXT("/Game/Generated/Meshes/Construccion/SM_Campfire.SM_Campfire")));
			TestEqual(TEXT("Suelo de bambú del kit"), Model->ResolveMeshPath(*Catalog.FindPiece(TEXT("suelo_bambu"))),
				FString(TEXT("/Game/Generated/Meshes/KitBambu/SM_Kit_Bamboo_Floor.SM_Kit_Bamboo_Floor")));
			TestEqual(TEXT("Muro de piedra del kit"), Model->ResolveMeshPath(*Catalog.FindPiece(TEXT("muro_piedra"))),
				FString(TEXT("/Game/Generated/Meshes/KitPiedra/SM_Kit_Stone_Wall.SM_Kit_Stone_Wall")));
			TestTrue(TEXT("Bancal pendiente: forma básica"), Model->ResolveMeshPath(*Catalog.FindPiece(TEXT("bancal"))).IsEmpty());
		});

		It("lee los encajes del JSON sin distinguir mayúsculas", [this]()
		{
			EBuildSocket Socket = EBuildSocket::Count;
			TestTrue(TEXT("pared"), ParseBuildSocket(TEXT("pared"), Socket));
			TestEqual(TEXT("Pared"), Socket, EBuildSocket::Wall);
			TestTrue(TEXT("TERRENO"), ParseBuildSocket(TEXT("TERRENO"), Socket));
			TestEqual(TEXT("Terreno"), Socket, EBuildSocket::GroundOnly);
			TestFalse(TEXT("Desconocido"), ParseBuildSocket(TEXT("tejado"), Socket));
		});
	});
}

#endif
