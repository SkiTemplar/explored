#include "WorldGen/IslandPropsModel.h"

namespace IslandPropsDetail
{
	struct FGddCount
	{
		const TCHAR* Type;
		int32 Count;
	};

	// GDD v3 §5.3 (recursos) y §6 (estructuras y puntos). Lo mismo comprueba Tools/Island/props.py.
	constexpr FGddCount GddCounts[] = {
		{ TEXT("palma"), 8 }, { TEXT("lena"), 10 }, { TEXT("fibra"), 15 }, { TEXT("cana_bambu"), 3 },
		{ TEXT("tronco"), 7 }, { TEXT("pagina"), 8 }, { TEXT("lente"), 1 }, { TEXT("punto_pesca"), 3 },
		{ TEXT("shelter_volume"), 2 }, { TEXT("refugio_palma"), 1 }, { TEXT("refugio_faro"), 1 },
		{ TEXT("barril_lluvia"), 1 }, { TEXT("caja_suministros"), 1 }, { TEXT("hoguera_fija"), 1 },
		{ TEXT("faro_linterna"), 1 }, { TEXT("balsa_sitio"), 1 }, { TEXT("faro_torre"), 1 },
		{ TEXT("faro_casa"), 1 }, { TEXT("carguero_casco"), 1 }, { TEXT("carguero_puente"), 1 },
		{ TEXT("caja_mapas"), 1 },
	};
}

const FIslandProp* FIslandProps::FindProp(FName Id) const
{
	return Props.FindByPredicate([Id](const FIslandProp& P) { return P.Id == Id; });
}

int32 FIslandProps::CountOf(FName Type) const
{
	int32 Count = 0;
	for (const FIslandProp& Prop : Props)
	{
		Count += Prop.Type == Type ? 1 : 0;
	}
	return Count;
}

int32 FIslandProps::TotalOf(FName Type, bool bCocos) const
{
	int32 Total = 0;
	for (const FIslandProp& Prop : Props)
	{
		if (Prop.Type == Type)
		{
			Total += bCocos ? Prop.Cocos : Prop.Amount;
		}
	}
	return Total;
}

FString FIslandProps::MeshPathFor(const FIslandProp& Prop) const
{
	if (!Prop.MeshOverride.IsEmpty())
	{
		return Prop.MeshOverride;
	}
	const FIslandPropType* Type = FindType(Prop.Type);
	return Type ? Type->MeshPath : FString();
}

bool FIslandProps::Validate(FString& OutError) const
{
	for (const TPair<FName, FIslandPropType>& Pair : Types)
	{
		const FIslandPropType& Type = Pair.Value;
		if (Type.Size.X <= 0.0 || Type.Size.Y <= 0.0 || Type.Size.Z <= 0.0)
		{
			OutError = FString::Printf(TEXT("tipo %s con tamaño no positivo"), *Pair.Key.ToString());
			return false;
		}
		if (Type.ColorIndex < 0 || Type.ColorIndex >= PaletteSize)
		{
			OutError = FString::Printf(TEXT("tipo %s con color fuera de la paleta"), *Pair.Key.ToString());
			return false;
		}
	}
	TSet<FName> Seen;
	for (const FIslandProp& Prop : Props)
	{
		if (Prop.Id.IsNone() || Seen.Contains(Prop.Id))
		{
			OutError = FString::Printf(TEXT("id vacío o repetido: %s"), *Prop.Id.ToString());
			return false;
		}
		if (!Types.Contains(Prop.Type))
		{
			OutError = FString::Printf(TEXT("%s: tipo sin definir %s"), *Prop.Id.ToString(), *Prop.Type.ToString());
			return false;
		}
		if (!Prop.Anchor.IsNone() && !Seen.Contains(Prop.Anchor))
		{
			OutError = FString::Printf(TEXT("%s: el ancla %s no está definida antes"), *Prop.Id.ToString(), *Prop.Anchor.ToString());
			return false;
		}
		if (!FMath::IsFinite(Prop.Location.X) || !FMath::IsFinite(Prop.Location.Y) || !FMath::IsFinite(Prop.Elevation))
		{
			OutError = FString::Printf(TEXT("%s: posición no finita"), *Prop.Id.ToString());
			return false;
		}
		Seen.Add(Prop.Id);
	}
	return true;
}

bool FIslandProps::MatchesGdd(FString& OutError) const
{
	for (const IslandPropsDetail::FGddCount& Expected : IslandPropsDetail::GddCounts)
	{
		const int32 Count = CountOf(Expected.Type);
		if (Count != Expected.Count)
		{
			OutError = FString::Printf(TEXT("%s: %d colocaciones, el GDD pide %d"), Expected.Type, Count, Expected.Count);
			return false;
		}
	}
	if (TotalOf(TEXT("palma"), true) != 16 || TotalOf(TEXT("lena")) != 20 || TotalOf(TEXT("fibra")) != 15)
	{
		OutError = TEXT("cocos, leña o fibra no suman lo del GDD (16, 20, 15)");
		return false;
	}
	uint32 PageMask = 0;
	for (const FIslandProp& Prop : Props)
	{
		if (Prop.Type == FName(TEXT("pagina")) && Prop.Page >= 1 && Prop.Page <= 8)
		{
			PageMask |= 1u << (Prop.Page - 1);
		}
	}
	if (PageMask != 0xFFu)
	{
		OutError = TEXT("faltan páginas del cuaderno (1-8)");
		return false;
	}
	return true;
}

namespace IslandPropsPlacement
{
	FVector FallbackScale(const FIslandPropType& Type)
	{
		// Las formas básicas miden 1 m: la escala es el tamaño en metros.
		return Type.Size;
	}

	FVector FallbackOffset(const FIslandPropType& Type)
	{
		return FVector(0.0, 0.0, Type.Size.Z * 50.0);
	}

	FVector WorldLocation(const FIslandProp& Prop, float GroundMeters)
	{
		return FVector(Prop.Location.X * 100.0, Prop.Location.Y * 100.0, (GroundMeters + Prop.Elevation) * 100.0);
	}

	const TCHAR* BasicShapePath(EIslandPropShape Shape)
	{
		switch (Shape)
		{
		case EIslandPropShape::Cylinder: return TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
		case EIslandPropShape::Sphere: return TEXT("/Engine/BasicShapes/Sphere.Sphere");
		case EIslandPropShape::Cone: return TEXT("/Engine/BasicShapes/Cone.Cone");
		default: return TEXT("/Engine/BasicShapes/Cube.Cube");
		}
	}

	float PaletteU(int32 ColorIndex)
	{
		const int32 Clamped = FMath::Clamp(ColorIndex, 0, FIslandProps::PaletteSize - 1);
		return (static_cast<float>(Clamped) + 0.5f) / static_cast<float>(FIslandProps::PaletteSize);
	}

	bool ShapeFromName(const FString& Name, EIslandPropShape& Out)
	{
		if (Name == TEXT("cube")) { Out = EIslandPropShape::Cube; return true; }
		if (Name == TEXT("cylinder")) { Out = EIslandPropShape::Cylinder; return true; }
		if (Name == TEXT("sphere")) { Out = EIslandPropShape::Sphere; return true; }
		if (Name == TEXT("cone")) { Out = EIslandPropShape::Cone; return true; }
		return false;
	}

	bool CollisionFromName(const FString& Name, EIslandPropCollision& Out)
	{
		if (Name == TEXT("solid")) { Out = EIslandPropCollision::Solid; return true; }
		if (Name == TEXT("query")) { Out = EIslandPropCollision::Query; return true; }
		if (Name == TEXT("none")) { Out = EIslandPropCollision::None; return true; }
		return false;
	}
}
