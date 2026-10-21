#include "WorldGen/IslandPropsLoader.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Explored.h"

namespace IslandPropsLoaderDetail
{
	using FJsonArray = TArray<TSharedPtr<FJsonValue>>;

	bool ReadNumbers(const FJsonObject& Obj, const TCHAR* Field, int32 Count, double* Out)
	{
		const FJsonArray* Values = nullptr;
		if (!Obj.TryGetArrayField(Field, Values) || !Values || Values->Num() != Count)
		{
			return false;
		}
		for (int32 I = 0; I < Count; ++I)
		{
			if (!(*Values)[I].IsValid() || !(*Values)[I]->TryGetNumber(Out[I]) || !FMath::IsFinite(Out[I]))
			{
				return false;
			}
		}
		return true;
	}

	float OptionalFloat(const FJsonObject& Obj, const TCHAR* Field, float Default = 0.0f)
	{
		double Value = Default;
		return Obj.TryGetNumberField(Field, Value) ? static_cast<float>(Value) : Default;
	}

	int32 OptionalInt(const FJsonObject& Obj, const TCHAR* Field)
	{
		int32 Value = 0;
		return Obj.TryGetNumberField(Field, Value) ? Value : 0;
	}

	bool ParseType(const FString& Name, const FJsonObject& Obj, FIslandPropType& Out, FString& OutError)
	{
		Out.Name = FName(*Name);
		Obj.TryGetStringField(TEXT("mesh"), Out.MeshPath);
		FString Shape;
		FString Collision;
		double Size[3] = { 0.0, 0.0, 0.0 };
		if (!Obj.TryGetStringField(TEXT("shape"), Shape) || !IslandPropsPlacement::ShapeFromName(Shape, Out.Shape)
			|| !Obj.TryGetStringField(TEXT("collision"), Collision) || !IslandPropsPlacement::CollisionFromName(Collision, Out.Collision)
			|| !ReadNumbers(Obj, TEXT("size"), 3, Size) || !Obj.TryGetNumberField(TEXT("color"), Out.ColorIndex))
		{
			OutError = FString::Printf(TEXT("tipo %s incompleto (shape, collision, size, color)"), *Name);
			return false;
		}
		Out.Size = FVector(Size[0], Size[1], Size[2]);
		Obj.TryGetBoolField(TEXT("label"), Out.bLabel); // loc: ignorar (nombre de campo JSON)
		Obj.TryGetBoolField(TEXT("gameplay"), Out.bGameplay);
		return true;
	}

	bool ParseProp(const FJsonObject& Obj, FIslandProp& Out, FString& OutError)
	{
		FString Id;
		FString Type;
		double At[2] = { 0.0, 0.0 };
		if (!Obj.TryGetStringField(TEXT("id"), Id) || !Obj.TryGetStringField(TEXT("type"), Type) || !ReadNumbers(Obj, TEXT("at"), 2, At))
		{
			OutError = FString::Printf(TEXT("colocación incompleta (id, type, at): %s"), *Id);
			return false;
		}
		Out.Id = FName(*Id);
		Out.Type = FName(*Type);
		Out.Location = FVector2D(At[0], At[1]);
		Out.Yaw = OptionalFloat(Obj, TEXT("yaw"));
		Out.Pitch = OptionalFloat(Obj, TEXT("pitch"));
		Out.Elevation = OptionalFloat(Obj, TEXT("elevation"));
		FString Anchor;
		if (Obj.TryGetStringField(TEXT("anchor"), Anchor) && !Anchor.IsEmpty())
		{
			Out.Anchor = FName(*Anchor);
		}
		Obj.TryGetStringField(TEXT("mesh"), Out.MeshOverride);
		Out.Amount = OptionalInt(Obj, TEXT("amount"));
		Out.Cocos = OptionalInt(Obj, TEXT("cocos"));
		Out.Page = OptionalInt(Obj, TEXT("page"));
		double Extent[3] = { 0.0, 0.0, 0.0 };
		if (ReadNumbers(Obj, TEXT("extent"), 3, Extent))
		{
			Out.Extent = FVector(Extent[0], Extent[1], Extent[2]);
		}
		Obj.TryGetBoolField(TEXT("respawn"), Out.bRespawn);
		return true;
	}

	TSharedRef<const FIslandProps> Load()
	{
		FString Json;
		FIslandProps Props;
		FString Error;
		const FString Path = ExploredIslandProps::IslandPropsJsonPath();
		if (!FFileHelper::LoadFileToString(Json, *Path))
		{
			UE_LOG(LogExplored, Warning, TEXT("[Isla] No se pudo leer %s: la isla queda sin colocaciones"), *Path);
		}
		else if (!ExploredIslandProps::ParseIslandPropsJson(Json, Props, Error))
		{
			UE_LOG(LogExplored, Warning, TEXT("[Isla] %s no es válido (%s): la isla queda sin colocaciones"), *Path, *Error);
			Props = FIslandProps();
		}
		else
		{
			UE_LOG(LogExplored, Display, TEXT("[Isla] %d tipos y %d colocaciones desde island_props.json"), Props.Types.Num(), Props.Props.Num());
		}
		return MakeShared<const FIslandProps>(MoveTemp(Props));
	}
}

namespace ExploredIslandProps
{
	FString IslandPropsJsonPath()
	{
		return FPaths::ProjectContentDir() / TEXT("Data") / TEXT("island_props.json");
	}

	bool ParseIslandPropsJson(const FString& Json, FIslandProps& OutProps, FString& OutError)
	{
		using namespace IslandPropsLoaderDetail;
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = TEXT("JSON mal formado");
			return false;
		}
		const TSharedPtr<FJsonObject>* TypesObj = nullptr;
		const FJsonArray* PropsArray = nullptr;
		if (!Root->TryGetObjectField(TEXT("types"), TypesObj) || !TypesObj || !Root->TryGetArrayField(TEXT("props"), PropsArray) || !PropsArray)
		{
			OutError = TEXT("faltan «types» o «props»");
			return false;
		}
		FIslandProps Props;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*TypesObj)->Values)
		{
			const TSharedPtr<FJsonObject> Obj = Pair.Value.IsValid() ? Pair.Value->AsObject() : nullptr;
			FIslandPropType Type;
			if (!Obj.IsValid() || !ParseType(Pair.Key, *Obj, Type, OutError))
			{
				OutError = OutError.IsEmpty() ? FString::Printf(TEXT("tipo %s no es un objeto"), *Pair.Key) : OutError;
				return false;
			}
			Props.Types.Add(Type.Name, Type);
		}
		for (const TSharedPtr<FJsonValue>& Value : *PropsArray)
		{
			const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Obj.IsValid() || !ParseProp(*Obj, Props.Props.AddDefaulted_GetRef(), OutError))
			{
				OutError = OutError.IsEmpty() ? TEXT("«props» contiene algo que no es un objeto") : OutError;
				return false;
			}
		}
		if (!Props.Validate(OutError))
		{
			return false;
		}
		OutProps = MoveTemp(Props);
		return true;
	}

	TSharedRef<const FIslandProps> Get()
	{
		static const TSharedRef<const FIslandProps> Cached = IslandPropsLoaderDetail::Load();
		return Cached;
	}
}
