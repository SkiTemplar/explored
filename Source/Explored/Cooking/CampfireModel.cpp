#include "Cooking/CampfireModel.h"

FCampfireConfig FCampfireConfig::Default()
{
	FCampfireConfig Config;
	Config.FuelSeconds.Add(TEXT("lena"), DefaultWoodSeconds);
	Config.FuelSeconds.Add(TEXT("tarro_aceite"), DefaultOilJarSeconds);
	return Config;
}

FCampfireState FCampfireModel::Ignite(const FCampfireState& In, const FCampfireConfig& Config)
{
	if (In.bLit)
	{
		return In;
	}
	FCampfireState Out = In;
	Out.bLit = true;
	Out.BurnSeconds = FMath::Clamp(Config.BurnSeconds, 0.0f, FMath::Max(Config.MaxBurnSeconds, Config.BurnSeconds));
	return Out;
}

bool FCampfireModel::AddFuel(const FCampfireState& In, const FCampfireConfig& Config, FName Item, FCampfireState& Out)
{
	Out = In;
	const float* Seconds = Config.FuelSeconds.Find(Item);
	if (!In.bLit || !Seconds || *Seconds <= 0.0f)
	{
		return false;
	}
	Out.BurnSeconds = FMath::Min(In.BurnSeconds + *Seconds, FMath::Max(Config.MaxBurnSeconds, Config.BurnSeconds));
	return true;
}

FCampfireState FCampfireModel::Tick(const FCampfireState& In, float DeltaSeconds, bool bStormExposed, TArray<ECampfireEvent>& OutEvents)
{
	FCampfireState Out = In;
	if (!In.bLit || !(DeltaSeconds > 0.0f))
	{
		return Out;
	}
	if (bStormExposed)
	{
		Out.bLit = false;
		Out.BurnSeconds = 0.0f;
		OutEvents.Add(ECampfireEvent::Quenched);
		return Out;
	}
	// El trabajo solo avanza mientras hay llama: si el fuego se apaga a mitad, avanza lo que ardió.
	const float Burned = FMath::Min(DeltaSeconds, In.BurnSeconds);
	if (In.Job.IsActive() && !In.Job.IsDone())
	{
		Out.Job.RemainingSeconds = FMath::Max(0.0f, In.Job.RemainingSeconds - Burned);
		if (Out.Job.IsDone())
		{
			OutEvents.Add(ECampfireEvent::JobDone);
		}
	}
	Out.BurnSeconds = In.BurnSeconds - Burned;
	if (Out.BurnSeconds <= 0.0f)
	{
		Out.BurnSeconds = 0.0f;
		Out.bLit = false;
		OutEvents.Add(ECampfireEvent::BurnedOut);
	}
	return Out;
}

bool FCampfireModel::StartJob(const FCampfireState& In, FName RecipeId, FName OutputItem, int32 OutputCount, float Seconds, FCampfireState& Out)
{
	Out = In;
	if (!In.bLit || In.Job.IsActive() || RecipeId.IsNone() || OutputItem.IsNone() || OutputCount <= 0 || !(Seconds > 0.0f))
	{
		return false;
	}
	Out.Job.RecipeId = RecipeId;
	Out.Job.OutputItem = OutputItem;
	Out.Job.OutputCount = OutputCount;
	Out.Job.RemainingSeconds = Seconds;
	return true;
}

bool FCampfireModel::CollectJob(const FCampfireState& In, FCampfireState& Out, FName& OutItem, int32& OutCount)
{
	Out = In;
	if (!In.Job.IsDone())
	{
		return false;
	}
	OutItem = In.Job.OutputItem;
	OutCount = In.Job.OutputCount;
	Out.Job = FCampfireJob();
	return true;
}

bool FCampfireModel::Warms(const FCampfireState& State, const FCampfireConfig& Config, float DistanceMeters)
{
	return State.bLit && DistanceMeters >= 0.0f && DistanceMeters <= Config.HeatRadiusMeters;
}

float FCampfireModel::HeatAt(const FCampfireState& State, const FCampfireConfig& Config, float DistanceMeters)
{
	if (!State.bLit || Config.HeatRadiusMeters <= 0.0f || DistanceMeters < 0.0f)
	{
		return 0.0f;
	}
	if (DistanceMeters <= Config.HeatRadiusMeters)
	{
		return 1.0f;
	}
	return FMath::Clamp(2.0f - DistanceMeters / Config.HeatRadiusMeters, 0.0f, 1.0f);
}
