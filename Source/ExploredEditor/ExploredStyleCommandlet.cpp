#include "ExploredStyleCommandlet.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogExploredStyle, Log, All);

namespace ExploredStyleDetail
{
	/** Kits cuyas mallas van con una sola LOD (Tools/Unreal/import_props.py, SINGLE_LOD_KITS). */
	const TCHAR* const SingleLodKit = TEXT("K6_Cesped");

	/** Tabla del GDD §11 (la misma que BUDGETS de Tools/Style/explored_style/check_style.py). */
	int32 BudgetForType(const FString& Type)
	{
		static const TMap<FString, int32> Budgets = {
			{TEXT("objeto_mano"), 300}, {TEXT("vegetacion"), 400}, {TEXT("palmera"), 1500}, {TEXT("estructura"), 800},
			{TEXT("faro"), 3000}, {TEXT("carguero"), 4000}, {TEXT("manos"), 3000},
		};
		const int32* Budget = Budgets.Find(Type);
		return Budget ? *Budget : 0;
	}

	struct FManifestMesh
	{
		FString Kit;
		FString Name;
		int32 MaxTriangles = 0;
	};

	TArray<FManifestMesh> ReadManifests(TArray<FString>& OutErrors)
	{
		TArray<FManifestMesh> Meshes;
		const FString ExportDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Art"), TEXT("Export"));
		TArray<FString> Kits;
		IFileManager::Get().FindFiles(Kits, *FPaths::Combine(ExportDir, TEXT("K*")), /*Files*/ false, /*Directories*/ true);
		Kits.Sort();
		for (const FString& Kit : Kits)
		{
			const FString Path = FPaths::Combine(ExportDir, Kit, TEXT("manifest.json"));
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *Path))
			{
				continue;
			}
			TSharedPtr<FJsonObject> Root;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
			const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
			if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() || !Root->TryGetArrayField(TEXT("mallas"), Entries))
			{
				OutErrors.Add(FString::Printf(TEXT("%s: no se puede leer la lista «mallas»"), *Path));
				continue;
			}
			for (const TSharedPtr<FJsonValue>& Value : *Entries)
			{
				const TSharedPtr<FJsonObject> Entry = Value.IsValid() ? Value->AsObject() : nullptr;
				FManifestMesh Mesh;
				Mesh.Kit = Kit;
				FString Type;
				if (Entry.IsValid() && !Entry->TryGetNumberField(TEXT("tope_tris"), Mesh.MaxTriangles) && Entry->TryGetStringField(TEXT("presupuesto"), Type))
				{
					// K1 solo anota el tipo: el tope sale de la tabla del GDD.
					Mesh.MaxTriangles = BudgetForType(Type);
				}
				if (!Entry.IsValid() || !Entry->TryGetStringField(TEXT("malla"), Mesh.Name) || Mesh.MaxTriangles <= 0)
				{
					OutErrors.Add(FString::Printf(TEXT("%s: entrada sin «malla» o sin tope («tope_tris» o «presupuesto» conocido)"), *Path));
					continue;
				}
				Meshes.Add(Mesh);
			}
		}
		return Meshes;
	}

	/** Los materiales del estilo son M_Toon o sus variantes (M_ToonCesped…) y sus instancias. */
	bool IsToonMaterial(UMaterialInterface* Material)
	{
		const UMaterial* Base = Material ? Material->GetBaseMaterial() : nullptr;
		return Base && Base->GetName().StartsWith(TEXT("M_Toon"));
	}

	void CheckMesh(const FManifestMesh& Entry, const UStaticMesh& Mesh, TArray<FString>& OutErrors)
	{
		const FString Id = Entry.Kit / Entry.Name;
		const int32 Triangles = Mesh.GetNumTriangles(0);
		if (Triangles > Entry.MaxTriangles)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: %d triángulos > tope de %d"), *Id, Triangles, Entry.MaxTriangles));
		}
		const TArray<FStaticMaterial>& Materials = Mesh.GetStaticMaterials();
		if (Materials.Num() != 1)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: %d huecos de material (uno solo)"), *Id, Materials.Num()));
		}
		for (const FStaticMaterial& Slot : Materials)
		{
			if (!IsToonMaterial(Slot.MaterialInterface))
			{
				OutErrors.Add(FString::Printf(TEXT("%s: material %s fuera de la familia M_Toon"), *Id,
					Slot.MaterialInterface ? *Slot.MaterialInterface->GetName() : TEXT("vacío")));
			}
		}
		if (Entry.Kit == SingleLodKit && Mesh.GetNumLODs() != 1)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: %d LOD (el césped va con una sola)"), *Id, Mesh.GetNumLODs()));
		}
		UE_LOG(LogExploredStyle, Display, TEXT("[Estilo] %s: %d/%d triángulos, %d LOD"), *Id, Triangles, Entry.MaxTriangles, Mesh.GetNumLODs());
	}
}

UExploredStyleCommandlet::UExploredStyleCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UExploredStyleCommandlet::Main(const FString& Params)
{
	using namespace ExploredStyleDetail;
	TArray<FString> Errors;
	TArray<FString> Warnings;
	const TArray<FManifestMesh> Manifest = ReadManifests(Errors);
	if (Manifest.Num() == 0)
	{
		Errors.Add(TEXT("No hay mallas en Art/Export/K*/manifest.json"));
	}

	TSet<FString> Known;
	for (const FManifestMesh& Entry : Manifest)
	{
		const FString Package = FString::Printf(TEXT("/Game/Art/%s/%s"), *Entry.Kit, *Entry.Name);
		Known.Add(Package);
		const UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), *Package, *Entry.Name));
		if (!Mesh)
		{
			Errors.Add(FString::Printf(TEXT("%s: falta la malla importada"), *Package));
			continue;
		}
		CheckMesh(Entry, *Mesh, Errors);
	}

	// Mallas de los kits que no salen de ningún manifiesto (importadas a mano o restos).
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.SearchAllAssets(/*bSynchronousSearch*/ true);
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(TEXT("/Game/Art")), Assets, /*bRecursive*/ true);
	for (const FAssetData& Asset : Assets)
	{
		const FString Package = Asset.PackageName.ToString();
		if (Asset.AssetClassPath == UStaticMesh::StaticClass()->GetClassPathName() && Package.StartsWith(TEXT("/Game/Art/K"))
			&& !Known.Contains(Package))
		{
			Warnings.Add(FString::Printf(TEXT("%s: malla sin manifiesto"), *Package));
		}
	}

	for (const FString& Warning : Warnings)
	{
		UE_LOG(LogExploredStyle, Warning, TEXT("[Estilo] %s"), *Warning);
	}
	for (const FString& Error : Errors)
	{
		UE_LOG(LogExploredStyle, Error, TEXT("[Estilo] %s"), *Error);
	}
	UE_LOG(LogExploredStyle, Display, TEXT("[Estilo] Resumen: %d mallas, %d fallos, %d avisos: %s"), Manifest.Num(), Errors.Num(),
		Warnings.Num(), Errors.Num() == 0 ? TEXT("OK") : TEXT("FALLA"));
	return Errors.Num() == 0 ? 0 : 1;
}
