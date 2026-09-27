#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"

#include "Building/BuildingModel.h"
#include "Building/BuildingSubsystem.h"
#include "Items/ItemRegistrySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// Solo en el editor (usa el módulo Json): valida Content/Data/building_pieces.json con el
// parser real de UBuildingSubsystem. La lógica se prueba en BuildingSpec, también en el host.
BEGIN_DEFINE_SPEC(FBuildingDataSpec, "Explored.Building.Datos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FBuildingDataSpec)

void FBuildingDataSpec::Define()
{
	It("parsea building_pieces.json con encajes, costes y la fogata como reaparición", [this]()
	{
		FString Json;
		if (!TestTrue(TEXT("Se lee building_pieces.json"),
			FFileHelper::LoadFileToString(Json, *UItemRegistrySubsystem::GetDataFilePath(TEXT("building_pieces.json")))))
		{
			return;
		}
		FBuildingCatalog Catalog;
		FString Error;
		if (!TestTrue(FString::Printf(TEXT("JSON válido (%s)"), *Error), UBuildingSubsystem::ParseBuildingJson(Json, Catalog, Error)))
		{
			return;
		}
		TestEqual(TEXT("Cuatro niveles"), Catalog.Tiers.Num(), 4);
		TestTrue(TEXT("Hay piezas"), Catalog.Pieces.Num() >= 30);
		for (const FBuildingPieceDef& Piece : Catalog.Pieces)
		{
			TestTrue(FString::Printf(TEXT("%s tiene coste"), *Piece.Id.ToString()), Piece.Cost.Num() > 0);
			TestNotNull(FString::Printf(TEXT("%s tiene nivel"), *Piece.Id.ToString()), Catalog.FindTier(Piece.Tier));
		}
		const FBuildingPieceDef* Fire = Catalog.FindPiece(TEXT("fogata"));
		if (TestNotNull(TEXT("Fogata"), Fire))
		{
			TestTrue(TEXT("Punto de reaparición"), Fire->bRespawnPoint);
			TestEqual(TEXT("Solo sobre terreno"), Fire->Socket, EBuildSocket::GroundOnly);
		}
		const FBuildingPieceDef* Wall = Catalog.FindPiece(TEXT("pared_madera"));
		if (TestNotNull(TEXT("Pared de madera"), Wall))
		{
			TestEqual(TEXT("Encaje de pared"), Wall->Socket, EBuildSocket::Wall);
			TestEqual(TEXT("Malla"), Wall->Mesh, FName(TEXT("SM_Wall_Wood")));
		}
		const FBuildingPieceDef* BambooFloor = Catalog.FindPiece(TEXT("suelo_bambu"));
		if (TestNotNull(TEXT("Suelo de bambú"), BambooFloor))
		{
			TestEqual(TEXT("Malla del kit modular"), BambooFloor->Mesh, FName(TEXT("SM_Kit_Bamboo_Floor")));
		}
		const FBuildingPieceDef* Pending = Catalog.FindPiece(TEXT("astillero"));
		if (TestNotNull(TEXT("Astillero"), Pending))
		{
			TestTrue(TEXT("\"mesh\": null queda sin malla"), Pending->Mesh.IsNone());
		}

		// El catálogo real construye una cabaña de palma a mano.
		FBuildingModel Model(MoveTemp(Catalog));
		const int32 Base = Model.FindOrCreateBase(FVector::ZeroVector);
		TMap<FName, int32> Inventory;
		Inventory.Add(TEXT("hoja_palma"), 100);
		Inventory.Add(TEXT("palo_recto"), 100);
		Inventory.Add(TEXT("liana"), 100);
		FBuildingPlaceRequest Floor;
		Floor.DefId = TEXT("suelo_palma");
		Floor.Placement.BaseId = Base;
		Floor.bGroundContact = true;
		int32 Id = INDEX_NONE;
		TestEqual(TEXT("Suelo de palma sin herramientas"), Model.TryPlace(Floor, Inventory, TSet<FName>(), Id), EBuildFailReason::None);
	});
}

#endif
