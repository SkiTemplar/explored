#include "Debug/ExploredTrailerSubsystem.h"

#include "AssetCompilingManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "ContentStreaming.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "ShaderCompiler.h"
#include "Tasks/Task.h"
#include "UnrealClient.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

#include "Explored.h"
#include "Narrative/BeaconSubsystem.h"
#include "Narrative/FinaleSequenceModel.h"
#include "Narrative/LighthouseLamp.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "UI/ExploredPlayerController.h"
#include "WorldGen/GroundHeight.h"
#include "WorldGen/IslandLayout.h"

namespace TrailerDetail
{
	/** Espera inicial tras terminar los shaders (streaming de Nanite y distance fields), en s de juego. */
	constexpr float WarmupSeconds = 8.0f;
	/** Espera máxima adicional a que el streaming de texturas alcance los mips pedidos. */
	constexpr float MaxStreamingWaitSeconds = 20.0f;
	/** Plano «beam»: segundos entre encender la baliza y grabar (el cono crece en 1,5 s). */
	constexpr float BeamLeadSeconds = 2.5f;
	/** PNG comprimiéndose a la vez como mucho (cada uno retiene 8 MB a 1080p). */
	constexpr int32 MaxInFlight = 8;
}

bool UExploredTrailerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FString(FCommandLine::Get()).Contains(TEXT("-ExploredTrailer="));
#endif
}

TStatId UExploredTrailerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExploredTrailerSubsystem, STATGROUP_Tickables);
}

FTrailerGroundFn UExploredTrailerSubsystem::GroundFn() const
{
	const UWorld* World = GetWorld();
	return [World](double X, double Y)
	{
		return FMath::Max(ExploredGround::HeightAtMeters(World, X, Y), FIslandLayout::SeaLevel);
	};
}

void UExploredTrailerSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld())
	{
		return;
	}
	FString Path;
	FParse::Value(FCommandLine::Get(), TEXT("ExploredTrailer="), Path);
	OutputDir = FPaths::ProjectSavedDir() / TEXT("Trailer") / TEXT("frames");
	FParse::Value(FCommandLine::Get(), TEXT("TrailerDir="), OutputDir);
	FParse::Value(FCommandLine::Get(), TEXT("TrailerStride="), Stride);
	Stride = FMath::Clamp(Stride, 1, 30);
	FString Error;
	if (!LoadTimeline(Path, Error))
	{
		UE_LOG(LogExplored, Error, TEXT("[Trailer] %s: %s"), *Path, *Error);
		FPlatformMisc::RequestExit(false, TEXT("ExploredTrailer"));
		return;
	}

	// Tiempo fijo: cada tick avanza 1/fps del juego, tarde lo que tarde en dibujarse y guardarse.
	FApp::SetUseFixedTimeStep(true);
	FApp::SetFixedDeltaTime(1.0 / Timeline.Fps);
	ApplyTrailerSettings();
	ScreenshotHandle = UGameViewportClient::OnScreenshotCaptured().AddUObject(this, &UExploredTrailerSubsystem::OnScreenshotCaptured);
	// El módulo de imagen se carga aquí, en el hilo de juego: las compresiones van en tareas.
	TArray64<uint8> Warm;
	FImageUtils::PNGCompressImageArray(1, 1, TArrayView64<const FColor>(&FColor::Black, 1), Warm);

	if (AExploredPlayerController* PC = Cast<AExploredPlayerController>(UGameplayStatics::GetPlayerController(&InWorld, 0)))
	{
		PC->SetUIMode(EExploredUIMode::Playing);
	}
	const FTrailerGroundFn Ground = GroundFn();
	for (const int32 Index : Order)
	{
		MotionIssues.Append(FTrailerModel::CheckMotion(Timeline.Shots[Index], Timeline.Fps, Ground, Limits));
	}
	for (const FString& Issue : MotionIssues)
	{
		UE_LOG(LogExplored, Warning, TEXT("[Trailer] movimiento: %s"), *Issue);
	}
	State = EState::Warmup;
	Timer = -TrailerDetail::WarmupSeconds;
	UE_LOG(LogExplored, Display, TEXT("[Trailer] %d planos de %d, %d fotogramas a %d fps (uno de cada %d) -> %s"), Order.Num(),
		Timeline.Shots.Num(), Timeline.TotalFrames, Timeline.Fps, Stride, *OutputDir);
}

void UExploredTrailerSubsystem::Deinitialize()
{
	UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotHandle);
	// Las tareas de compresión escriben en este objeto: no se destruye con alguna en marcha.
	while (InFlight.load() > 0)
	{
		FPlatformProcess::Sleep(0.01f);
	}
	Super::Deinitialize();
}

void UExploredTrailerSubsystem::ApplyTrailerSettings() const
{
	if (!GEngine)
	{
		return;
	}
	// Solo en el modo tráiler: imagen a resolución nativa con TSR, desenfoque de movimiento suave y sin
	// mensajes de depuración en pantalla. No toca los ajustes guardados del jugador.
	for (const TCHAR* Command : { TEXT("r.ScreenPercentage 100"), TEXT("r.AntiAliasingMethod 4"), TEXT("r.MotionBlur.Amount 0.25"),
		TEXT("r.MotionBlur.Max 2"), TEXT("DisableAllScreenMessages") })
	{
		GEngine->Exec(GetWorld(), Command);
	}
}

bool UExploredTrailerSubsystem::IsWorldSettled() const
{
	if ((GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) || FAssetCompilingManager::Get().GetNumRemainingAssets() > 0)
	{
		return false;
	}
	const UWorldPartitionSubsystem* Partition = GetWorld()->GetSubsystem<UWorldPartitionSubsystem>();
	return !Partition || Partition->IsStreamingCompleted();
}

void UExploredTrailerSubsystem::Tick(float DeltaTime)
{
	if (State == EState::Flushing)
	{
		if (InFlight.load() == 0)
		{
			WriteReportAndExit();
		}
		return;
	}
	if (State == EState::Recording)
	{
		RecordFrame();
		return;
	}
	if (!IsWorldSettled())
	{
		return;
	}
	Timer += DeltaTime;
	if (State == EState::Warmup)
	{
		if (Timer >= 0.0f)
		{
			BeginShot(0);
		}
		return;
	}
	const FTrailerShot& Shot = CurrentShot();
	if (State == EState::Leading)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0); PC && Camera)
		{
			PC->SetViewTarget(Camera);
		}
		if (Timer >= TrailerDetail::BeamLeadSeconds)
		{
			State = EState::Recording;
		}
		return;
	}
	// Asentamiento: la cámara quieta en el primer fotograma del plano (césped, texturas, exposición, TSR).
	ApplySample(Shot, FTrailerModel::Evaluate(Shot, 0, Timeline.Fps, GroundFn()));
	const bool bTexturesReady = IStreamingManager::Get().GetNumWantingResources() == 0;
	if (Timer < SettleSeconds || (!bTexturesReady && Timer < SettleSeconds + TrailerDetail::MaxStreamingWaitSeconds))
	{
		return;
	}
	UE_LOG(LogExplored, Display, TEXT("[Trailer] %s asentado en %.1f s de juego"), *Shot.Name, Timer);
	if (Shot.Kind == ETrailerShotKind::Beam)
	{
		if (UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(GetWorld()))
		{
			Beacon->TryLight();
		}
		State = EState::Leading;
		Timer = 0.0f;
		return;
	}
	if (Shot.Action == TEXT("hud"))
	{
		ClearHudTransient();
	}
	State = EState::Recording;
}

void UExploredTrailerSubsystem::BeginShot(int32 OrderIndex)
{
	OrderIndexNow = OrderIndex;
	LocalFrame = 0;
	Timer = 0.0f;
	MaxStepCm = 0.0f;
	State = EState::Settling;
	const FTrailerShot& Shot = CurrentShot();
	UWorld* World = GetWorld();
	const FTrailerCameraSample First = FTrailerModel::Evaluate(Shot, 0, Timeline.Fps, GroundFn());
	if (!Camera)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Camera = World->SpawnActor<ACameraActor>(First.EyeM * 100.0, FRotator(First.Pitch, First.Yaw, 0.0f), Params);
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
	}
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		PC->SetViewTarget(Camera);
		if (bFading && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StopCameraFade();
			bFading = false;
		}
		if (APawn* Pawn = PC->GetPawn())
		{
			// El streaming y el césped siguen al jugador y a la cámara: el pawn, oculto, va al plano.
			Pawn->SetActorHiddenInGame(true);
			Pawn->SetActorLocation(First.EyeM * 100.0, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	if (UTimeOfDaySubsystem* Time = World->GetSubsystem<UTimeOfDaySubsystem>())
	{
		Time->SetTimeScale(0.0f);
	}
	PrepareGameplay(Shot);
	ApplySample(Shot, First);
	LastEyeCm = First.EyeM * 100.0;
	UE_LOG(LogExplored, Display, TEXT("[Trailer] plano %s (%s): fotogramas %d-%d"), *Shot.Name, FTrailerModel::KindName(Shot.Kind),
		Shot.StartFrame, Shot.StartFrame + Shot.FrameCount - 1);
}

void UExploredTrailerSubsystem::PrepareGameplay(const FTrailerShot& Shot)
{
	UWorld* World = GetWorld();
	AExploredPlayerController* PC = Cast<AExploredPlayerController>(UGameplayStatics::GetPlayerController(World, 0));
	if (Shot.Action == TEXT("hud"))
	{
		PrepareHud(Shot);
		return;
	}
	if (Shot.Kind == ETrailerShotKind::Normal)
	{
		return;
	}
	// Final y haz: la partida en juego con la linterna montada (la baliza se enciende al grabar o antes).
	if (PC && PC->GetUIMode() != EExploredUIMode::Playing)
	{
		PC->ResumeGame();
		PC->SetViewTarget(Camera);
	}
	if (UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(World))
	{
		Beacon->DebugSetStage(EBeaconStage::LinternaMontada);
	}
}

void UExploredTrailerSubsystem::ApplySample(const FTrailerShot& Shot, const FTrailerCameraSample& Sample)
{
	UWorld* World = GetWorld();
	if (UTimeOfDaySubsystem* Time = World->GetSubsystem<UTimeOfDaySubsystem>())
	{
		Time->SetTime(Sample.Day, Sample.Hours);
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	const FRotator Rotation(Sample.Pitch, Sample.Yaw, 0.0f);
	if (Shot.Action == TEXT("hud"))
	{
		// Desde los ojos de Iria: solo gira la vista; su cámara y la interfaz son las del juego.
		if (PC)
		{
			PC->SetControlRotation(Rotation);
		}
	}
	else if (Camera)
	{
		Camera->SetActorLocationAndRotation(Sample.EyeM * 100.0, Rotation);
		Camera->GetCameraComponent()->SetFieldOfView(Sample.Fov);
	}
	if (PC && PC->PlayerCameraManager && (Sample.Black > 0.0f || bFading))
	{
		PC->PlayerCameraManager->SetManualCameraFade(Sample.Black, FLinearColor::Black, false);
		bFading = Sample.Black > 0.0f;
	}
}

void UExploredTrailerSubsystem::RecordFrame()
{
	const FTrailerShot& Shot = CurrentShot();
	if (PendingFrame != INDEX_NONE)
	{
		// El fotograma anterior no se dibujó (la captura llega en el Draw del mismo tick).
		UE_LOG(LogExplored, Warning, TEXT("[Trailer] fotograma %d sin entregar"), PendingFrame);
		LostFrames.Add(PendingFrame);
		PendingFrame = INDEX_NONE;
	}
	if (LocalFrame >= Shot.FrameCount)
	{
		TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetStringField(TEXT("name"), Shot.Name);
		Report->SetNumberField(TEXT("startFrame"), Shot.StartFrame);
		Report->SetNumberField(TEXT("frames"), Shot.FrameCount);
		Report->SetNumberField(TEXT("maxStepCm"), MaxStepCm);
		ShotReports.Add(MakeShared<FJsonValueObject>(Report));
		UE_LOG(LogExplored, Display, TEXT("[Trailer] %s grabado: salto máximo del ojo %.1f cm por fotograma"), *Shot.Name, MaxStepCm);
		if (OrderIndexNow + 1 < Order.Num())
		{
			BeginShot(OrderIndexNow + 1);
		}
		else
		{
			State = EState::Flushing;
		}
		return;
	}
	while (InFlight.load() >= TrailerDetail::MaxInFlight)
	{
		// El juego va a tiempo fijo: esperar aquí no cambia nada de lo que se ve.
		FPlatformProcess::Sleep(0.002f);
	}
	const int32 Global = Shot.StartFrame + LocalFrame;
	const FTrailerCameraSample Sample = FTrailerModel::Evaluate(Shot, LocalFrame, Timeline.Fps, GroundFn());
	ApplySample(Shot, Sample);
	if (Shot.Kind == ETrailerShotKind::Beam)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0); PC && Camera)
		{
			PC->SetViewTarget(Camera);
		}
	}
	if (Shot.Kind == ETrailerShotKind::Finale)
	{
		TrackFinale(Global);
	}
	if (Shot.Action == TEXT("hud"))
	{
		// El cartel «Día N» del HUD sale al volver a pintarse la interfaz: fuera en todo el plano.
		ClearHudTransient();
	}
	const FVector EyeCm = Sample.EyeM * 100.0;
	if (LocalFrame > 0)
	{
		MaxStepCm = FMath::Max(MaxStepCm, static_cast<float>((EyeCm - LastEyeCm).Size()));
	}
	LastEyeCm = EyeCm;
	if (Global % Stride == 0)
	{
		PendingFrame = Global;
		++RequestedFrames;
		FScreenshotRequest::RequestScreenshot(OutputDir / TEXT("pending.png"), Shot.bShowUI, false);
	}
	++LocalFrame;
}

void UExploredTrailerSubsystem::TrackFinale(int32 GlobalFrame)
{
	UWorld* World = GetWorld();
	if (LocalFrame == 0)
	{
		for (TActorIterator<ALighthouseLamp> It(World); It; ++It)
		{
			It->DebugIgnite();
			break;
		}
		FinaleFrames.Add(TEXT("ignite"), GlobalFrame);
	}
	const UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(World);
	const float SinceMistral = Beacon ? Beacon->GetSecondsSinceMistral() : -1.0f;
	if (SinceMistral < 0.0f)
	{
		return;
	}
	const FFinaleTiming Timing = Beacon->FinaleTiming();
	auto Mark = [this, GlobalFrame](const TCHAR* Name)
	{
		if (!FinaleFrames.Contains(Name))
		{
			FinaleFrames.Add(Name, GlobalFrame);
			UE_LOG(LogExplored, Display, TEXT("[Trailer] final: %s en el fotograma %d"), Name, GlobalFrame);
		}
	};
	Mark(TEXT("mistral"));
	if (SinceMistral >= FFinaleSequenceModel::HornAtSeconds(Timing))
	{
		Mark(TEXT("horn"));
	}
	if (SinceMistral >= FFinaleSequenceModel::FadeAtSeconds(Timing))
	{
		Mark(TEXT("fade"));
	}
}

void UExploredTrailerSubsystem::OnScreenshotCaptured(int32 Width, int32 Height, const TArray<FColor>& Colors)
{
	if (PendingFrame == INDEX_NONE)
	{
		return;
	}
	const int32 Frame = PendingFrame;
	PendingFrame = INDEX_NONE;
	const FString File = OutputDir / FTrailerModel::FrameFileName(Frame);
	InFlight.fetch_add(1);
	UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, Frame, File, Width, Height, Pixels = TArray<FColor>(Colors)]()
	{
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Width, Height, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
		const bool bSaved = Png.Num() > 0 && FFileHelper::SaveArrayToFile(Png, *File);
		{
			FScopeLock Lock(&WrittenLock);
			(bSaved ? WrittenFrames : FailedFrames).Add(Frame);
		}
		InFlight.fetch_sub(1);
	});
}

void UExploredTrailerSubsystem::WriteReportAndExit()
{
	State = EState::Off;
	TArray<int32> Expected;
	TArray<int32> Missing;
	TArray<int32> Repeated;
	int32 ExpectedCount = 0;
	{
		FScopeLock Lock(&WrittenLock);
		for (const int32 Index : Order)
		{
			const FTrailerShot& Shot = Timeline.Shots[Index];
			TArray<int32> ShotMissing;
			TArray<int32> ShotRepeated;
			FTrailerModel::FindGaps(WrittenFrames, Shot.StartFrame, Shot.StartFrame + Shot.FrameCount - 1, Stride, ShotMissing, ShotRepeated);
			Missing.Append(ShotMissing);
			Repeated.Append(ShotRepeated);
			for (int32 Frame = Shot.StartFrame; Frame < Shot.StartFrame + Shot.FrameCount; ++Frame)
			{
				ExpectedCount += Frame % Stride == 0 ? 1 : 0;
			}
		}
	}
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("fps"), Timeline.Fps);
	Root->SetNumberField(TEXT("stride"), Stride);
	Root->SetNumberField(TEXT("expected"), ExpectedCount);
	Root->SetNumberField(TEXT("written"), WrittenFrames.Num());
	Root->SetNumberField(TEXT("failed"), FailedFrames.Num());
	Root->SetNumberField(TEXT("lost"), LostFrames.Num());
	auto Numbers = [](const TArray<int32>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Out;
		for (const int32 Value : Values)
		{
			Out.Add(MakeShared<FJsonValueNumber>(Value));
		}
		return Out;
	};
	Root->SetArrayField(TEXT("missing"), Numbers(Missing));
	Root->SetArrayField(TEXT("repeated"), Numbers(Repeated));
	TArray<TSharedPtr<FJsonValue>> Issues;
	for (const FString& Issue : MotionIssues)
	{
		Issues.Add(MakeShared<FJsonValueString>(Issue));
	}
	Root->SetArrayField(TEXT("motionIssues"), Issues);
	Root->SetArrayField(TEXT("shots"), ShotReports);
	TSharedPtr<FJsonObject> Finale = MakeShared<FJsonObject>();
	for (const TPair<FString, int32>& Pair : FinaleFrames)
	{
		Finale->SetNumberField(Pair.Key, Pair.Value);
	}
	Root->SetObjectField(TEXT("finaleFrames"), Finale);
	FString Text;
	FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Text));
	const FString Name = Order.Num() == 1 ? Timeline.Shots[Order[0]].Name : TEXT("grupo");
	FFileHelper::SaveStringToFile(Text, *(OutputDir / FString::Printf(TEXT("report_%s.json"), *Name)), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UE_LOG(LogExplored, Display, TEXT("[Trailer] Terminado: %d de %d fotogramas escritos, %d faltan, %d repetidos, %d perdidos, %d fallidos"),
		WrittenFrames.Num(), ExpectedCount, Missing.Num(), Repeated.Num(), LostFrames.Num(), FailedFrames.Num());
	FPlatformMisc::RequestExit(false, TEXT("ExploredTrailer"));
}
