#include "Building/ExploredBuildingPiece.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/SoftObjectPath.h"

#include "Building/BuildingModel.h"

namespace ExploredBuildingPieceDetail
{
	const TCHAR* FallbackCubePath = TEXT("/Engine/BasicShapes/Cube.Cube");

	/** Medidas (cm) de la forma básica de reserva por encaje, con el pivote en la base como el kit. */
	void FallbackShape(EBuildSocket Socket, FVector& OutSize, FVector& OutOffset)
	{
		const double Cell = FBuildingModel::CellSizeCm;
		switch (Socket)
		{
		case EBuildSocket::Pillar:
			OutSize = FVector(30.0, 30.0, FBuildingModel::FoundationHeightCm);
			break;
		case EBuildSocket::Floor:
			OutSize = FVector(Cell, Cell, FBuildingModel::FloorThicknessCm);
			break;
		case EBuildSocket::Wall:
		case EBuildSocket::Door:
			OutSize = FVector(Cell, 14.0, FBuildingModel::WallHeightCm);
			break;
		case EBuildSocket::Roof:
			OutSize = FVector(Cell, Cell, 20.0);
			break;
		case EBuildSocket::Stairs:
			OutSize = FVector(100.0, Cell * 2.0, FBuildingModel::StoreyHeightCm);
			break;
		case EBuildSocket::GroundOnly:
			OutSize = FVector(120.0, 120.0, 40.0);
			break;
		case EBuildSocket::Furniture:
		default:
			OutSize = FVector(100.0, 100.0, 80.0);
			break;
		}
		// El cubo de BasicShapes mide 100 cm con el pivote en el centro: se sube media altura
		// (y la escalera avanza media longitud, porque su pivote está en el arranque).
		OutOffset = FVector(0.0, Socket == EBuildSocket::Stairs ? OutSize.Y * 0.5 : 0.0, OutSize.Z * 0.5);
	}
}

AExploredBuildingPiece::AExploredBuildingPiece()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
}

UStaticMesh* AExploredBuildingPiece::LoadMeshForSocket(EBuildSocket Socket, const FString& MeshPath, FVector& OutScale, FVector& OutOffset)
{
	OutScale = FVector::OneVector;
	OutOffset = FVector::ZeroVector;
	if (!MeshPath.IsEmpty())
	{
		if (UStaticMesh* Loaded = Cast<UStaticMesh>(FSoftObjectPath(MeshPath).TryLoad()))
		{
			return Loaded;
		}
	}

	FVector Size;
	ExploredBuildingPieceDetail::FallbackShape(Socket, Size, OutOffset);
	OutScale = Size / 100.0;
	return Cast<UStaticMesh>(FSoftObjectPath(ExploredBuildingPieceDetail::FallbackCubePath).TryLoad());
}

void AExploredBuildingPiece::InitializePiece(int32 InPieceId, FName InDefId, EBuildSocket Socket, const FString& MeshPath)
{
	PieceId = InPieceId;
	DefId = InDefId;

	FVector Scale, Offset;
	if (UStaticMesh* StaticMesh = LoadMeshForSocket(Socket, MeshPath, Scale, Offset))
	{
		Mesh->SetStaticMesh(StaticMesh);
	}
	Mesh->SetRelativeLocation(Offset);
	Mesh->SetRelativeScale3D(Scale);
}
