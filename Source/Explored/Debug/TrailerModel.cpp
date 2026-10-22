#include "Debug/TrailerModel.h"

namespace TrailerModelDetail
{
	/** Muestras por tramo del Catmull-Rom para la tabla de longitud de arco. */
	constexpr int32 ArcSamplesPerSegment = 64;

	bool IsFiniteVector(const FVector& V)
	{
		return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
	}

	/** Tramo de Catmull-Rom centrípeto (alfa 0,5, Barry-Goldman): sin bucles ni retrocesos con puntos desiguales. */
	FVector CatmullRomSegment(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, double T)
	{
		auto Knot = [](const FVector& A, const FVector& B)
		{
			return FMath::Max(FMath::Sqrt((B - A).Size()), 1.0e-4);
		};
		const double T0 = 0.0;
		const double T1 = T0 + Knot(P0, P1);
		const double T2 = T1 + Knot(P1, P2);
		const double T3 = T2 + Knot(P2, P3);
		const double U = FMath::Lerp(T1, T2, T);
		const FVector A1 = P0 * ((T1 - U) / (T1 - T0)) + P1 * ((U - T0) / (T1 - T0));
		const FVector A2 = P1 * ((T2 - U) / (T2 - T1)) + P2 * ((U - T1) / (T2 - T1));
		const FVector A3 = P2 * ((T3 - U) / (T3 - T2)) + P3 * ((U - T2) / (T3 - T2));
		const FVector B1 = A1 * ((T2 - U) / (T2 - T0)) + A2 * ((U - T0) / (T2 - T0));
		const FVector B2 = A2 * ((T3 - U) / (T3 - T1)) + A3 * ((U - T1) / (T3 - T1));
		return B1 * ((T2 - U) / (T2 - T1)) + B2 * ((U - T1) / (T2 - T1));
	}

	float SmoothFade(float X)
	{
		const float C = FMath::Clamp(X, 0.0f, 1.0f);
		return C * C * (3.0f - 2.0f * C);
	}

	FVector LookDirection(const FTrailerShot& Shot, double Progress, const FVector& EyeNow, const FVector& EyeStart, const FVector& EyeEnd)
	{
		const int32 Count = Shot.LookM.Num();
		if (Count == 1)
		{
			return (Shot.LookM[0] - EyeNow).GetSafeNormal(UE_SMALL_NUMBER, FVector(1.0, 0.0, 0.0));
		}
		if (Count == 2)
		{
			return FTrailerModel::SlerpDirection(Shot.LookM[0] - EyeStart, Shot.LookM[1] - EyeEnd, Progress);
		}
		// Cada punto de mirada se ve desde el ojo del instante que le toca (repartidos a partes iguales).
		TArray<FVector> Dirs;
		Dirs.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const double At = static_cast<double>(Index) / (Count - 1);
			const FVector Eye = FTrailerModel::PathPoint(Shot.EyeM, At);
			Dirs.Add((Shot.LookM[Index] - Eye).GetSafeNormal(UE_SMALL_NUMBER, FVector(1.0, 0.0, 0.0)));
		}
		return FTrailerModel::CatmullRom(Dirs, Progress * (Count - 1)).GetSafeNormal(UE_SMALL_NUMBER, Dirs[0]);
	}

	FString ValidateShot(const FTrailerShot& Shot)
	{
		if (Shot.Name.IsEmpty())
		{
			return TEXT("plano sin nombre");
		}
		if (!FMath::IsFinite(Shot.DurationSeconds) || Shot.DurationSeconds <= 0.0f)
		{
			return FString::Printf(TEXT("%s: duración no válida"), *Shot.Name);
		}
		if (Shot.EyeM.Num() == 0 || Shot.LookM.Num() == 0)
		{
			return FString::Printf(TEXT("%s: faltan puntos de ojo o de mirada"), *Shot.Name);
		}
		for (const FVector& P : Shot.EyeM)
		{
			if (!IsFiniteVector(P))
			{
				return FString::Printf(TEXT("%s: punto de ojo no finito"), *Shot.Name);
			}
		}
		for (const FVector& P : Shot.LookM)
		{
			if (!IsFiniteVector(P))
			{
				return FString::Printf(TEXT("%s: punto de mirada no finito"), *Shot.Name);
			}
		}
		if (Shot.Day < 1 || Shot.HoursFrom < 0.0f || Shot.HoursFrom > 24.0f || Shot.HoursTo < 0.0f || Shot.HoursTo > 24.0f)
		{
			return FString::Printf(TEXT("%s: día u hora fuera de rango"), *Shot.Name);
		}
		if (Shot.FovFrom < 5.0f || Shot.FovFrom > 170.0f || Shot.FovTo < 5.0f || Shot.FovTo > 170.0f)
		{
			return FString::Printf(TEXT("%s: FOV fuera de rango"), *Shot.Name);
		}
		if (Shot.FadeInSeconds < 0.0f || Shot.FadeOutSeconds < 0.0f || Shot.FadeInSeconds + Shot.FadeOutSeconds > Shot.DurationSeconds)
		{
			return FString::Printf(TEXT("%s: fundidos más largos que el plano"), *Shot.Name);
		}
		if (Shot.EaseFraction < 0.0f || Shot.EaseFraction > 0.5f)
		{
			return FString::Printf(TEXT("%s: ease fuera de [0, 0,5]"), *Shot.Name);
		}
		return FString();
	}
}

ETrailerShotKind FTrailerModel::ParseKind(const FString& Text)
{
	if (Text.Equals(TEXT("finale"), ESearchCase::IgnoreCase))
	{
		return ETrailerShotKind::Finale;
	}
	if (Text.Equals(TEXT("beam"), ESearchCase::IgnoreCase))
	{
		return ETrailerShotKind::Beam;
	}
	return ETrailerShotKind::Normal;
}

const TCHAR* FTrailerModel::KindName(ETrailerShotKind Kind)
{
	switch (Kind)
	{
	case ETrailerShotKind::Finale:
		return TEXT("finale");
	case ETrailerShotKind::Beam:
		return TEXT("beam");
	default:
		return TEXT("normal");
	}
}

bool FTrailerModel::BuildTimeline(const TArray<FTrailerShot>& Shots, int32 Fps, FTrailerTimeline& Out, FString& OutError)
{
	if (Fps <= 0 || Fps > 240)
	{
		OutError = FString::Printf(TEXT("fps no válidos: %d"), Fps);
		return false;
	}
	if (Shots.Num() == 0)
	{
		OutError = TEXT("sin planos");
		return false;
	}
	FTrailerTimeline Built;
	Built.Fps = Fps;
	int32 Frame = 0;
	for (const FTrailerShot& Source : Shots)
	{
		const FString Problem = TrailerModelDetail::ValidateShot(Source);
		if (!Problem.IsEmpty())
		{
			OutError = Problem;
			return false;
		}
		if (FindShotByName(Built, Source.Name) != INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("%s: nombre repetido"), *Source.Name);
			return false;
		}
		FTrailerShot Shot = Source;
		Shot.StartFrame = Frame;
		Shot.FrameCount = FMath::Max(1, FMath::RoundToInt(Shot.DurationSeconds * Fps));
		Frame += Shot.FrameCount;
		Built.Shots.Add(Shot);
	}
	Built.TotalFrames = Frame;
	Out = Built;
	return true;
}

int32 FTrailerModel::FindShot(const FTrailerTimeline& Timeline, int32 Frame)
{
	for (int32 Index = 0; Index < Timeline.Shots.Num(); ++Index)
	{
		const FTrailerShot& Shot = Timeline.Shots[Index];
		if (Frame >= Shot.StartFrame && Frame < Shot.StartFrame + Shot.FrameCount)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 FTrailerModel::FindShotByName(const FTrailerTimeline& Timeline, const FString& Name)
{
	for (int32 Index = 0; Index < Timeline.Shots.Num(); ++Index)
	{
		if (Timeline.Shots[Index].Name == Name)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

float FTrailerModel::EaseProgress(float Alpha, float EaseFraction)
{
	const float A = FMath::Clamp(Alpha, 0.0f, 1.0f);
	const float E = FMath::Clamp(EaseFraction, 0.0f, 0.5f);
	if (E <= UE_KINDA_SMALL_NUMBER)
	{
		return A;
	}
	// Velocidad trapezoidal: sube linealmente durante E, constante y baja durante E; el área es 1.
	const float VMax = 1.0f / (1.0f - E);
	if (A < E)
	{
		return VMax * A * A / (2.0f * E);
	}
	if (A > 1.0f - E)
	{
		const float B = 1.0f - A;
		return 1.0f - VMax * B * B / (2.0f * E);
	}
	return VMax * (A - E * 0.5f);
}

FVector FTrailerModel::CatmullRom(const TArray<FVector>& Points, double U)
{
	const int32 Count = Points.Num();
	if (Count == 0)
	{
		return FVector::ZeroVector;
	}
	if (Count == 1)
	{
		return Points[0];
	}
	const double Clamped = FMath::Clamp(U, 0.0, static_cast<double>(Count - 1));
	const int32 Segment = FMath::Min(FMath::FloorToInt(Clamped), Count - 2);
	const double T = Clamped - Segment;
	const FVector& P1 = Points[Segment];
	const FVector& P2 = Points[Segment + 1];
	// Extremos: punto fantasma reflejado, para que la curva salga y llegue con la dirección del tramo.
	const FVector P0 = Segment > 0 ? Points[Segment - 1] : P1 * 2.0 - P2;
	const FVector P3 = Segment + 2 < Count ? Points[Segment + 2] : P2 * 2.0 - P1;
	return TrailerModelDetail::CatmullRomSegment(P0, P1, P2, P3, T);
}

FVector FTrailerModel::PathPoint(const TArray<FVector>& Points, double S)
{
	using namespace TrailerModelDetail;
	const int32 Count = Points.Num();
	if (Count <= 1)
	{
		return Count == 1 ? Points[0] : FVector::ZeroVector;
	}
	const int32 Samples = (Count - 1) * ArcSamplesPerSegment;
	TArray<double> Cumulative;
	Cumulative.SetNum(Samples + 1);
	Cumulative[0] = 0.0;
	FVector Previous = Points[0];
	for (int32 Index = 1; Index <= Samples; ++Index)
	{
		const FVector Point = CatmullRom(Points, static_cast<double>(Index) / ArcSamplesPerSegment);
		Cumulative[Index] = Cumulative[Index - 1] + (Point - Previous).Size();
		Previous = Point;
	}
	const double Total = Cumulative[Samples];
	if (Total <= UE_KINDA_SMALL_NUMBER)
	{
		return Points[0];
	}
	const double Wanted = FMath::Clamp(S, 0.0, 1.0) * Total;
	int32 Low = 0;
	int32 High = Samples;
	while (High - Low > 1)
	{
		const int32 Mid = (Low + High) / 2;
		(Cumulative[Mid] < Wanted ? Low : High) = Mid;
	}
	const double Span = Cumulative[High] - Cumulative[Low];
	const double Frac = Span > UE_SMALL_NUMBER ? (Wanted - Cumulative[Low]) / Span : 0.0;
	return CatmullRom(Points, (Low + Frac) / ArcSamplesPerSegment);
}

FVector FTrailerModel::SlerpDirection(const FVector& A, const FVector& B, double T)
{
	const FVector From = A.GetSafeNormal(UE_SMALL_NUMBER, FVector(1.0, 0.0, 0.0));
	const FVector To = B.GetSafeNormal(UE_SMALL_NUMBER, From);
	const double Cos = FMath::Clamp(FVector::DotProduct(From, To), -1.0, 1.0);
	const double Angle = FMath::Acos(Cos);
	const double Clamped = FMath::Clamp(T, 0.0, 1.0);
	if (Angle < 1.0e-5)
	{
		return From;
	}
	if (UE_DOUBLE_PI - Angle < 1.0e-4)
	{
		// Opuestas: el giro va por la horizontal (eje vertical), que es lo que espera un paneo.
		const FVector Ortho = FVector::CrossProduct(FVector(0.0, 0.0, 1.0), From).GetSafeNormal(UE_SMALL_NUMBER, FVector(0.0, 1.0, 0.0));
		return From * FMath::Cos(Angle * Clamped) + Ortho * FMath::Sin(Angle * Clamped);
	}
	const double Sin = FMath::Sin(Angle);
	const FVector Out = From * (FMath::Sin((1.0 - Clamped) * Angle) / Sin) + To * (FMath::Sin(Clamped * Angle) / Sin);
	return Out.GetSafeNormal(UE_SMALL_NUMBER, From);
}

float FTrailerModel::SmoothedGround(const FTrailerGroundFn& Ground, double X, double Y, double RadiusM)
{
	if (!Ground)
	{
		return 0.0f;
	}
	double Sum = Ground(X, Y);
	constexpr int32 Ring = 8;
	for (int32 Index = 0; Index < Ring; ++Index)
	{
		const double Angle = 2.0 * UE_DOUBLE_PI * Index / Ring;
		Sum += Ground(X + RadiusM * FMath::Cos(Angle), Y + RadiusM * FMath::Sin(Angle));
	}
	return static_cast<float>(Sum / (Ring + 1));
}

float FTrailerModel::FadeBlack(const FTrailerShot& Shot, float LocalSeconds, float FrameSeconds)
{
	using namespace TrailerModelDetail;
	float Black = 0.0f;
	if (Shot.FadeInSeconds > 0.0f)
	{
		Black = FMath::Max(Black, 1.0f - SmoothFade(LocalSeconds / Shot.FadeInSeconds));
	}
	if (Shot.FadeOutSeconds > 0.0f)
	{
		// El fundido de salida cuenta hasta el final del fotograma: el último sale negro del todo.
		const float Remaining = Shot.DurationSeconds - LocalSeconds - FrameSeconds;
		Black = FMath::Max(Black, 1.0f - SmoothFade(Remaining / Shot.FadeOutSeconds));
	}
	return FMath::Clamp(Black, 0.0f, 1.0f);
}

FTrailerCameraSample FTrailerModel::Evaluate(const FTrailerShot& Shot, int32 LocalFrame, int32 Fps, const FTrailerGroundFn& Ground)
{
	FTrailerCameraSample Sample;
	const int32 Frames = FMath::Max(1, Shot.FrameCount > 0 ? Shot.FrameCount : FMath::RoundToInt(Shot.DurationSeconds * FMath::Max(Fps, 1)));
	const int32 Local = FMath::Clamp(LocalFrame, 0, Frames - 1);
	const float Alpha = Frames > 1 ? static_cast<float>(Local) / (Frames - 1) : 0.0f;
	const double Progress = EaseProgress(Alpha, Shot.EaseFraction);

	auto EyeAt = [&Shot, &Ground](double S)
	{
		FVector Eye = PathPoint(Shot.EyeM, S);
		if (Shot.bFollowGround && Ground)
		{
			Eye.Z += SmoothedGround(Ground, Eye.X, Eye.Y, GroundSmoothRadiusM);
		}
		if (Ground)
		{
			Eye.Z = FMath::Max(Eye.Z, static_cast<double>(Ground(Eye.X, Eye.Y)) + MinClearanceM);
		}
		return Eye;
	};
	Sample.EyeM = EyeAt(Progress);
	Sample.LookDir = TrailerModelDetail::LookDirection(Shot, Progress, Sample.EyeM, EyeAt(0.0), EyeAt(1.0));
	Sample.Yaw = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Sample.LookDir.Y, Sample.LookDir.X)));
	Sample.Pitch = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Sample.LookDir.Z, Sample.LookDir.Size2D())));
	Sample.Fov = FMath::Lerp(Shot.FovFrom, Shot.FovTo, static_cast<float>(Progress));
	Sample.Day = Shot.Day;
	// La hora corre lineal con el tiempo del plano (no con el suavizado de la cámara).
	Sample.Hours = FMath::Lerp(Shot.HoursFrom, Shot.HoursTo, Alpha);
	Sample.Black = FadeBlack(Shot, static_cast<float>(Local) / FMath::Max(Fps, 1), 1.0f / FMath::Max(Fps, 1));
	return Sample;
}

TArray<FString> FTrailerModel::CheckMotion(const FTrailerShot& Shot, int32 Fps, const FTrailerGroundFn& Ground, const FTrailerLimits& Limits)
{
	TArray<FString> Issues;
	const int32 Frames = FMath::Max(1, Shot.FrameCount > 0 ? Shot.FrameCount : FMath::RoundToInt(Shot.DurationSeconds * FMath::Max(Fps, 1)));
	FTrailerCameraSample Previous = Evaluate(Shot, 0, Fps, Ground);
	double PreviousStep = -1.0;
	for (int32 Frame = 0; Frame < Frames; ++Frame)
	{
		const FTrailerCameraSample Sample = Frame == 0 ? Previous : Evaluate(Shot, Frame, Fps, Ground);
		if (Ground && Sample.EyeM.Z < Ground(Sample.EyeM.X, Sample.EyeM.Y) + Limits.MinClearanceM - 0.01f)
		{
			Issues.Add(FString::Printf(TEXT("%s f%d: ojo a ras del suelo"), *Shot.Name, Frame));
		}
		if (Frame > 0)
		{
			const double Step = (Sample.EyeM - Previous.EyeM).Size();
			const double Turn = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Sample.LookDir, Previous.LookDir), -1.0, 1.0)));
			if (Step > Limits.MaxEyeStepM)
			{
				Issues.Add(FString::Printf(TEXT("%s f%d: el ojo salta %.2f m"), *Shot.Name, Frame, Step));
			}
			if (Turn > Limits.MaxTurnStepDeg)
			{
				Issues.Add(FString::Printf(TEXT("%s f%d: la mirada gira %.2f grados"), *Shot.Name, Frame, Turn));
			}
			if (PreviousStep >= 0.0 && FMath::Abs(Step - PreviousStep) > Limits.MaxEyeAccelM)
			{
				Issues.Add(FString::Printf(TEXT("%s f%d: tirón de velocidad (%.3f → %.3f m)"), *Shot.Name, Frame, PreviousStep, Step));
			}
			PreviousStep = Step;
		}
		Previous = Sample;
	}
	return Issues;
}

bool FTrailerModel::CaptureOrder(const FTrailerTimeline& Timeline, const TArray<int32>& Selected, TArray<int32>& OutOrder, FString& OutError)
{
	TArray<int32> Normal;
	TArray<int32> Ui;
	TArray<int32> Beam;
	int32 Finales = 0;
	for (const int32 Index : Selected)
	{
		if (!Timeline.Shots.IsValidIndex(Index))
		{
			OutError = FString::Printf(TEXT("plano %d fuera de la línea de tiempo"), Index);
			return false;
		}
		if (Normal.Contains(Index) || Ui.Contains(Index) || Beam.Contains(Index))
		{
			continue;
		}
		const FTrailerShot& Shot = Timeline.Shots[Index];
		switch (Shot.Kind)
		{
		case ETrailerShotKind::Finale:
			++Finales;
			Normal.Add(Index);
			break;
		case ETrailerShotKind::Beam:
			Beam.Add(Index);
			break;
		default:
			(Shot.bShowUI || !Shot.Action.IsEmpty() ? Ui : Normal).Add(Index);
			break;
		}
	}
	const int32 Total = Normal.Num() + Ui.Num() + Beam.Num();
	if (Finales > 0 && Total > 1)
	{
		OutError = TEXT("un plano «finale» se captura solo (el final deja la partida acabada)");
		return false;
	}
	if (Beam.Num() > 1)
	{
		OutError = TEXT("solo un plano «beam» por proceso (arranca el final del juego)");
		return false;
	}
	if (Total == 0)
	{
		OutError = TEXT("sin planos que capturar");
		return false;
	}
	OutOrder = Normal;
	OutOrder.Append(Ui);
	OutOrder.Append(Beam);
	return true;
}

FString FTrailerModel::FrameFileName(int32 Frame)
{
	return FString::Printf(TEXT("f_%05d.png"), Frame);
}

void FTrailerModel::FindGaps(const TArray<int32>& Captured, int32 First, int32 Last, int32 Stride, TArray<int32>& OutMissing, TArray<int32>& OutRepeated)
{
	OutMissing.Reset();
	OutRepeated.Reset();
	const int32 Step = FMath::Max(1, Stride);
	TArray<int32> Counts;
	Counts.SetNumZeroed(FMath::Max(0, Last - First + 1));
	for (const int32 Frame : Captured)
	{
		if (Frame >= First && Frame <= Last)
		{
			if (++Counts[Frame - First] == 2)
			{
				OutRepeated.Add(Frame);
			}
		}
	}
	for (int32 Frame = First; Frame <= Last; ++Frame)
	{
		if (Frame % Step == 0 && Counts[Frame - First] == 0)
		{
			OutMissing.Add(Frame);
		}
	}
}
