#include "WorldGen/IslandDecorBuilder.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Explored.h"
#include "WorldGen/IslandDecorModel.h"

namespace IslandDecorDetail
{
	constexpr double MetersToCm = 100.0;
	/** Distancia (cm) a la que se deja de dibujar la hierba y los arbustos; el resto, siempre. */
	constexpr float SmallCullCm = 9000.0f;
	const FName DecorTag(TEXT("IslandDecor"));

	bool ParseInstance(const TSharedPtr<FJsonValue>& Value, FIslandDecorInstance& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Fields = nullptr;
		if (!Value.IsValid() || !Value->TryGetArray(Fields) || !Fields || Fields->Num() != 6)
		{
			return false;
		}
		double N[6];
		for (int32 I = 0; I < 6; ++I)
		{
			if (!(*Fields)[I].IsValid() || !(*Fields)[I]->TryGetNumber(N[I]))
			{
				return false;
			}
		}
		Out.Mesh = static_cast<int32>(N[0]);
		Out.Location = FVector(N[1], N[2], N[3]);
		Out.YawDeg = static_cast<float>(N[4]);
		Out.Scale = static_cast<float>(N[5]);
		return true;
	}

	/** Hierba, arbustos, rocas pequeñas y la vegetación media del kit K6 (helechos, hierba alta, flores). */
	bool IsSmall(const FString& MeshPath)
	{
		return MeshPath.Contains(TEXT("GrassClump")) || MeshPath.Contains(TEXT("Shrub")) || MeshPath.Contains(TEXT("RockSmall"))
			|| MeshPath.Contains(TEXT("/K6_Cesped/"));
	}

	/** Las matas bajas no proyectan sombra (coste sin lectura a esa escala). */
	bool CastsShadow(const FString& MeshPath)
	{
		return !MeshPath.Contains(TEXT("GrassClump")) && !MeshPath.Contains(TEXT("SM_Flower"));
	}
}

FString ExploredIslandDecor::DefaultPath()
{
	return FPaths::ProjectContentDir() / TEXT("Data/island_decor.json");
}

bool ExploredIslandDecor::Load(const FString& JsonPath, FIslandDecor& Out, FString& OutError)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *JsonPath))
	{
		OutError = FString::Printf(TEXT("no se puede leer %s"), *JsonPath);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		OutError = TEXT("JSON mal formado");
		return false;
	}
	FIslandDecor Decor;
	const TArray<TSharedPtr<FJsonValue>>* Meshes = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Instances = nullptr;
	if (!Root->TryGetArrayField(TEXT("meshes"), Meshes) || !Root->TryGetArrayField(TEXT("instances"), Instances))
	{
		OutError = TEXT("faltan «meshes» o «instances»");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Mesh : *Meshes)
	{
		Decor.Meshes.Add(Mesh.IsValid() ? Mesh->AsString() : FString());
	}
	Decor.Instances.Reserve(Instances->Num());
	for (const TSharedPtr<FJsonValue>& Value : *Instances)
	{
		FIslandDecorInstance Inst;
		if (!IslandDecorDetail::ParseInstance(Value, Inst))
		{
			OutError = FString::Printf(TEXT("instancia %d mal formada"), Decor.Instances.Num());
			return false;
		}
		Decor.Instances.Add(Inst);
	}
	if (!Decor.Validate(OutError))
	{
		return false;
	}
	Out = MoveTemp(Decor);
	return true;
}

int32 ExploredIslandDecor::Build(AActor& Owner)
{
	using namespace IslandDecorDetail;
	FIslandDecor Decor;
	FString Error;
	if (!Load(DefaultPath(), Decor, Error))
	{
		UE_LOG(LogExplored, Warning, TEXT("[Decor] island_decor.json: %s; la isla queda sin decoración"), *Error);
		return 0;
	}
	TArray<TArray<FTransform>> PerMesh;
	PerMesh.SetNum(Decor.Meshes.Num());
	for (const FIslandDecorInstance& Inst : Decor.Instances)
	{
		PerMesh[Inst.Mesh].Add(FTransform(FRotator(0.0, Inst.YawDeg, 0.0), Inst.Location * MetersToCm, FVector(Inst.Scale)));
	}
	int32 Added = 0;
	for (int32 M = 0; M < Decor.Meshes.Num(); ++M)
	{
		const FString& Path = Decor.Meshes[M];
		if (PerMesh[M].IsEmpty())
		{
			continue;
		}
		// Tolerante: una malla aún no importada (p. ej. el kit K6 antes de import_props.py) se omite con aviso.
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
		if (!Mesh)
		{
			UE_LOG(LogExplored, Warning, TEXT("[Decor] falta la malla %s (%d instancias sin dibujar)"), *Path, PerMesh[M].Num());
			continue;
		}
		UHierarchicalInstancedStaticMeshComponent* Hism = NewObject<UHierarchicalInstancedStaticMeshComponent>(&Owner, NAME_None, RF_Transient);
		Hism->SetStaticMesh(Mesh);
		Hism->SetMobility(EComponentMobility::Static);
		Hism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Hism->SetCollisionResponseToAllChannels(ECR_Ignore);
		Hism->SetGenerateOverlapEvents(false);
		Hism->SetCanEverAffectNavigation(false);
		Hism->ComponentTags.Add(DecorTag);
		if (IsSmall(Path))
		{
			Hism->SetCullDistances(SmallCullCm * 0.8f, SmallCullCm);
			Hism->SetCastShadow(CastsShadow(Path));
		}
		// Sin padre: el populador es móvil y estas instancias son estáticas en el origen del mundo.
		Hism->SetWorldTransform(FTransform::Identity);
		Hism->RegisterComponent();
		Owner.AddInstanceComponent(Hism);
		Hism->AddInstances(PerMesh[M], false, true);
		Added += Hism->GetInstanceCount();
	}
	UE_LOG(LogExplored, Display, TEXT("[Decor] %d instancias decorativas en %d mallas"), Added, Decor.Meshes.Num());
	return Added;
}
