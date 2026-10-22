#include "Save/SavePlayerState.h"

namespace SavePlayerStateDetail
{
	bool IsFinite(double A, double B, double C)
	{
		return FMath::IsFinite(A) && FMath::IsFinite(B) && FMath::IsFinite(C);
	}
}

void FSavePlayerState::Save(FSaveArchive& Ar) const
{
	Ar.Write(TEXT("location"), Location);
	Ar.Write(TEXT("controlRotation"), ControlRotation);
	Ar.Write(TEXT("velocity"), Velocity);
	FSaveArchive Body;
	Body.Write(TEXT("health"), Health);
	Body.Write(TEXT("hunger"), Hunger);
	Body.Write(TEXT("thirst"), Thirst);
	Body.Write(TEXT("energy"), Energy);
	Body.Write(TEXT("rest"), Rest);
	Body.Write(TEXT("morale"), Morale);
	Body.Write(TEXT("bodyTemperature"), BodyTemperature);
	Body.Write(TEXT("wetness"), Wetness);
	Ar.Write(TEXT("body"), Body);
}

void FSavePlayerState::Load(const FSaveArchive& Ar)
{
	*this = FSavePlayerState();
	Ar.Read(TEXT("location"), Location);
	Ar.Read(TEXT("controlRotation"), ControlRotation);
	Ar.Read(TEXT("velocity"), Velocity);
	FSaveArchive Body;
	if (Ar.Read(TEXT("body"), Body))
	{
		Body.Read(TEXT("health"), Health);
		Body.Read(TEXT("hunger"), Hunger);
		Body.Read(TEXT("thirst"), Thirst);
		Body.Read(TEXT("energy"), Energy);
		Body.Read(TEXT("rest"), Rest);
		Body.Read(TEXT("morale"), Morale);
		Body.Read(TEXT("bodyTemperature"), BodyTemperature);
		Body.Read(TEXT("wetness"), Wetness);
	}
	if (!SavePlayerStateDetail::IsFinite(ControlRotation.Pitch, ControlRotation.Yaw, ControlRotation.Roll))
	{
		ControlRotation = FRotator::ZeroRotator;
	}
	if (!SavePlayerStateDetail::IsFinite(Velocity.X, Velocity.Y, Velocity.Z))
	{
		Velocity = FVector::ZeroVector;
	}
}

bool FSavePlayerState::HasPlausibleLocation(double MaxAbsCm) const
{
	// IsFinite explícito: con matemáticas rápidas (el editor compila con /fp:fast) el compilador da
	// por hecho que no hay NaN y `Abs(NaN) <= Tope` puede salir cierto (host tests con -ffast-math).
	if (!SavePlayerStateDetail::IsFinite(Location.X, Location.Y, Location.Z))
	{
		return false;
	}
	return FMath::Abs(Location.X) <= MaxAbsCm && FMath::Abs(Location.Y) <= MaxAbsCm && FMath::Abs(Location.Z) <= MaxAbsCm;
}

bool FSavePlayerState::operator==(const FSavePlayerState& Other) const
{
	return Location == Other.Location && ControlRotation == Other.ControlRotation && Velocity == Other.Velocity
		&& Health == Other.Health && Hunger == Other.Hunger && Thirst == Other.Thirst && Energy == Other.Energy
		&& Rest == Other.Rest && Morale == Other.Morale && BodyTemperature == Other.BodyTemperature
		&& Wetness == Other.Wetness;
}
