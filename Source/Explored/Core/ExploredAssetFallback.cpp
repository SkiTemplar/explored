#include "Core/ExploredAssetFallback.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeLock.h"
#include "UObject/SoftObjectPath.h"
#include "Explored.h"

namespace ExploredAssetFallbackDetail
{
	enum class EMissingReason : uint8
	{
		EmptyPath,
		NoPackage,
		NotStaticMesh,
	};

	FCriticalSection WarnedLock;
	TSet<FString> WarnedPaths;

	void WarnOnce(const FString& Path, EMissingReason Reason)
	{
		FScopeLock Lock(&WarnedLock);
		bool bAlreadyWarned = false;
		WarnedPaths.Add(Path, &bAlreadyWarned);
		if (bAlreadyWarned)
		{
			return;
		}
		switch (Reason)
		{
		case EMissingReason::EmptyPath:
			UE_LOG(LogExplored, Warning, TEXT("Malla no disponible: ruta vacía"));
			break;
		case EMissingReason::NoPackage:
			UE_LOG(LogExplored, Warning, TEXT("Malla no disponible (no existe el paquete): %s"), *Path);
			break;
		default:
			UE_LOG(LogExplored, Warning, TEXT("Malla no disponible (no es una StaticMesh): %s"), *Path);
			break;
		}
	}
}

namespace ExploredAssets
{
	UStaticMesh* LoadMeshIfPresent(const FSoftObjectPath& Path)
	{
		using ExploredAssetFallbackDetail::EMissingReason;
		if (Path.IsNull())
		{
			ExploredAssetFallbackDetail::WarnOnce(FString(), EMissingReason::EmptyPath);
			return nullptr;
		}
		if (UStaticMesh* Loaded = Cast<UStaticMesh>(Path.ResolveObject()))
		{
			return Loaded;
		}
		if (!FPackageName::DoesPackageExist(Path.GetLongPackageName()))
		{
			ExploredAssetFallbackDetail::WarnOnce(Path.ToString(), EMissingReason::NoPackage);
			return nullptr;
		}
		UStaticMesh* Mesh = Cast<UStaticMesh>(Path.TryLoad());
		if (!Mesh)
		{
			ExploredAssetFallbackDetail::WarnOnce(Path.ToString(), EMissingReason::NotStaticMesh);
		}
		return Mesh;
	}

	UStaticMesh* LoadMeshOrFallback(const FSoftObjectPath& Path, const TCHAR* FallbackPath)
	{
		if (UStaticMesh* Mesh = LoadMeshIfPresent(Path))
		{
			return Mesh;
		}
		return Cast<UStaticMesh>(FSoftObjectPath(FallbackPath).TryLoad());
	}
}
