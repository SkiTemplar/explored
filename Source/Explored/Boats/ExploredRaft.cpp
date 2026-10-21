#include "Boats/ExploredRaft.h"

#include "CollisionQueryParams.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/SoftObjectPath.h"

#include "Core/ExploredAssetFallback.h"
#include "Crafting/RecipeBookModel.h"
#include "Explored.h"
#include "Items/ExploredIslandStateSubsystem.h"
#include "Items/ExploredSurvivorComponent.h"
#include "Items/IslandDataLoader.h"
#include "Narrative/BeaconSubsystem.h"
#include "Ocean/ExploredOcean.h"
#include "Player/ExploredCharacter.h"
#include "Player/SwimComponent.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Survival/IslandBodyComponent.h"
#include "UI/ExploredHUD.h"
#include "Weather/ExploredWeatherSubsystem.h"
#include "WorldGen/IslandPropsLoader.h"
#include "WorldGen/IslandPropsModel.h"

#define LOCTEXT_NAMESPACE "ExploredRaft"

namespace ExploredRaftDetail
{
	const FName LogItem(TEXT("tronco"));
	const FName PaddleItem(TEXT("remo"));
	const FName RaftRecipe(TEXT("balsa"));
	const FName IsletProp(TEXT("carguero_casco"));
	const TCHAR* const RaftMeshPath = TEXT("/Game/Art/K2_Estructuras/SM_Balsa_01.SM_Balsa_01");
	const TCHAR* const LogMeshPath = TEXT("/Game/Art/K3_Objetos/SM_Tronco_01.SM_Tronco_01");
	const TCHAR* const CylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	/** Tamaño de la balsa de cubos (m): 3 × 2,2 × 0,3 (island_props.json «balsa_sitio»). */
	const FVector RaftSizeMeters(3.0, 2.2, 0.3);
	/** Altura del asiento sobre la cubierta: el centro de la cápsula de Iria (cm). */
	constexpr float SeatHeightCm = 110.0f;
	constexpr int32 RaftInputPriority = 10;
	constexpr float ProbeTopCm = 10000.0f;
	constexpr float ProbeDepthCm = 5000.0f;

	double AbsHours(const UObject* Context)
	{
		const UWorld* World = Context ? Context->GetWorld() : nullptr;
		const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
		return Time ? FStormClockModel::ToAbsoluteHours(Time->GetDay(), Time->GetHours()) : 0.0;
	}

	FStormClockModel Storm(const UObject* Context)
	{
		const UWorld* World = Context ? Context->GetWorld() : nullptr;
		const UExploredWeatherSubsystem* Weather = World ? World->GetSubsystem<UExploredWeatherSubsystem>() : nullptr;
		return Weather ? Weather->GetStorm() : FStormClockModel();
	}

	void Say(const UObject* Context, const FText& Text)
	{
		const APlayerController* PC = UGameplayStatics::GetPlayerController(Context, 0);
		if (AExploredHUD* HUD = PC ? Cast<AExploredHUD>(PC->GetHUD()) : nullptr)
		{
			HUD->ShowSubtitle(Text, 4.0f);
		}
	}

	FText BoardMessage(ERaftBoard Result)
	{
		switch (Result)
		{
		case ERaftBoard::NoPaddle: return LOCTEXT("NeedPaddle", "Sin remo no voy a ningún sitio.");
		case ERaftBoard::SeaClosed: return LOCTEXT("SeaClosedNoBoard", "El mar se cierra. A tierra.");
		default: return FText::GetEmpty();
		}
	}

	UInputAction* MakeAction(UObject* Outer, FName Name)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = EInputActionValueType::Axis1D;
		return Action;
	}

	void MapKey(UInputMappingContext* Context, UInputAction* Action, FKey Key, bool bNegate = false)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
		}
	}

	UStaticMesh* LoadMesh(const TCHAR* Path, const TCHAR* Fallback, bool& bOutFinal)
	{
		UStaticMesh* Mesh = ExploredAssets::LoadMeshIfPresent(FSoftObjectPath(Path));
		bOutFinal = Mesh != nullptr;
		return Mesh ? Mesh : ExploredAssets::LoadMeshOrFallback(FSoftObjectPath(Fallback), Fallback);
	}
}

// ============================================================================
// AExploredRaftSite
// ============================================================================

AExploredRaftSite::AExploredRaftSite()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
}

void AExploredRaftSite::BeginPlay()
{
	Super::BeginPlay();
	const TSharedRef<const FIslandProps> Props = ExploredIslandProps::Get();
	if (const FIslandProp* Islet = Props->FindProp(ExploredRaftDetail::IsletProp))
	{
		IsletCenter = IslandPropsPlacement::WorldLocation(*Islet, 0.0f);
		bHasIslet = true;
	}
	RefreshFromIslandState();
}

void AExploredRaftSite::OnConfigured(const FIslandProp& Prop, const FIslandPropType& Type)
{
	if (bHasIslet)
	{
		const FVector2D Delta(IsletCenter - GetActorLocation());
		RouteYawDeg = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
	}
	RefreshFromIslandState();
}

void AExploredRaftSite::RefreshFromIslandState()
{
	if (bHasIslet)
	{
		const FVector2D Delta(IsletCenter - GetActorLocation());
		RouteYawDeg = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
	}
	const UExploredIslandStateSubsystem* Island = UExploredIslandStateSubsystem::Get(this);
	const ExploredSaveStates::FRaftSaveData Saved = Island ? Island->GetRaft() : ExploredSaveStates::FRaftSaveData();
	Site = Saved.Site;
	UpdateLogVisuals();
	if (Site.bBuilt)
	{
		EnsureRaft();
		if (AExploredRaft* Existing = Raft.Get(); Existing && Saved.bHasPose && !Existing->IsOccupied())
		{
			Existing->PlaceAt(Saved.Location, Saved.YawDeg);
		}
	}
	else if (AExploredRaft* Existing = Raft.Get())
	{
		Existing->Destroy();
		Raft.Reset();
	}
}

void AExploredRaftSite::UpdateLogVisuals()
{
	bool bFinal = false;
	UStaticMesh* LogMesh = ExploredRaftDetail::LoadMesh(ExploredRaftDetail::LogMeshPath, ExploredRaftDetail::CylinderPath, bFinal);
	const int32 Visible = Site.bBuilt ? 0 : Site.LogsPlaced;
	while (LogMeshes.Num() < Visible)
	{
		UStaticMeshComponent* Log = NewObject<UStaticMeshComponent>(this);
		Log->SetupAttachment(GetRootComponent());
		Log->SetStaticMesh(LogMesh);
		Log->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		const int32 Index = LogMeshes.Num();
		// Troncos tumbados uno al lado de otro sobre el sitio (2,6 m × 0,35 m de diámetro).
		Log->SetRelativeLocation(FVector(0.0, -75.0 + 50.0 * Index, 40.0));
		Log->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
		Log->SetRelativeScale3D(bFinal ? FVector(1.0) : FVector(0.35, 0.35, 2.6));
		Log->RegisterComponent();
		LogMeshes.Add(Log);
	}
	while (LogMeshes.Num() > Visible)
	{
		if (UStaticMeshComponent* Log = LogMeshes.Pop())
		{
			Log->DestroyComponent();
		}
	}
}

FVector AExploredRaftSite::FindLaunchLocation() const
{
	const FVector Start = GetActorLocation();
	const FVector2D Dir = FRaftModel::Forward(RouteYawDeg);
	const UWorld* World = GetWorld();
	float WaterZ = 0.0f;
	for (TActorIterator<AExploredOcean> It(World); It; ++It)
	{
		WaterZ = It->GetActorLocation().Z;
		break;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RaftLaunch), false, this);
	for (int32 Step = 0; Step <= 80; ++Step)
	{
		const FVector Probe = Start + FVector(Dir.X, Dir.Y, 0.0) * (100.0 * Step);
		FHitResult Hit;
		const bool bHit = World && World->LineTraceSingleByChannel(Hit, FVector(Probe.X, Probe.Y, ExploredRaftDetail::ProbeTopCm),
			FVector(Probe.X, Probe.Y, -ExploredRaftDetail::ProbeDepthCm), ECC_WorldStatic, Params);
		if (!bHit || Hit.ImpactPoint.Z < WaterZ - 50.0)
		{
			return FVector(Probe.X, Probe.Y, WaterZ) + FVector(Dir.X, Dir.Y, 0.0) * 150.0;
		}
	}
	return Start + FVector(Dir.X, Dir.Y, 0.0) * 1000.0;
}

void AExploredRaftSite::EnsureRaft()
{
	if (Raft.IsValid() || !GetWorld())
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Launch = FindLaunchLocation();
	AExploredRaft* Spawned = GetWorld()->SpawnActor<AExploredRaft>(AExploredRaft::StaticClass(), Launch, FRotator(0.0, RouteYawDeg, 0.0), Params);
	if (!Spawned)
	{
		UE_LOG(LogExplored, Warning, TEXT("[Balsa] No se pudo crear la balsa"));
		return;
	}
	Spawned->PlaceAt(Launch, RouteYawDeg);
	Spawned->SetCurrent(FRaftModel::LateralCurrent(RouteYawDeg, Tuning));
	Raft = Spawned;
	UE_LOG(LogExplored, Display, TEXT("[Balsa] Balsa en el agua en %s, rumbo %.0f°"), *Launch.ToCompactString(), RouteYawDeg);
}

FVector AExploredRaftSite::GetShoreLocation() const
{
	// Medio metro hacia tierra desde el sitio, a la altura del suelo + la cápsula.
	const FVector2D Back = -FRaftModel::Forward(RouteYawDeg);
	const FVector Base = GetActorLocation() + FVector(Back.X, Back.Y, 0.0) * 300.0;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RaftShore), false, this);
	if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, FVector(Base.X, Base.Y, ExploredRaftDetail::ProbeTopCm),
		FVector(Base.X, Base.Y, -ExploredRaftDetail::ProbeDepthCm), ECC_WorldStatic, Params))
	{
		return Hit.ImpactPoint + FVector(0.0, 0.0, 120.0);
	}
	return Base + FVector(0.0, 0.0, 200.0);
}

void AExploredRaftSite::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	if (Site.bBuilt)
	{
		return;
	}
	if (FRaftModel::CanTie(Site, Tuning) == ERaftSiteResult::Ok)
	{
		OutVerbs.Add(LOCTEXT("TieRaft", "Atar balsa"));
		return;
	}
	OutVerbs.Add(FText::Format(LOCTEXT("PlaceLog", "Dejar tronco ({0}/{1})"), FText::AsNumber(Site.LogsPlaced), FText::AsNumber(Tuning.LogsNeeded)));
}

bool AExploredRaftSite::CanInteract_Implementation(AActor* InInstigator) const
{
	const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	return !Site.bBuilt && Survivor && !Survivor->IsBusy();
}

void AExploredRaftSite::Interact_Implementation(AActor* InInstigator)
{
	if (FRaftModel::CanTie(Site, Tuning) == ERaftSiteResult::Ok)
	{
		Tie(InInstigator);
	}
	else
	{
		PlaceLog(InInstigator);
	}
}

void AExploredRaftSite::PlaceLog(AActor* InInstigator)
{
	UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	if (!Survivor)
	{
		return;
	}
	const bool bHolding = Survivor->GetHeldHeavy() == ExploredRaftDetail::LogItem;
	FRaftSiteState Next;
	const ERaftSiteResult Result = FRaftModel::PlaceLog(Site, bHolding, Next, Tuning);
	if (Result == ERaftSiteResult::NoLog)
	{
		Survivor->ShowMessage(LOCTEXT("NeedLog", "Hacen falta troncos: se traen de uno en uno."));
		return;
	}
	FBackpackState Pack;
	if (Result != ERaftSiteResult::Ok
		|| FBackpackModel::Remove(Survivor->GetBackpack(), ExploredRaftDetail::LogItem, 1, Pack) != EBackpackResult::Ok)
	{
		return;
	}
	Survivor->ApplyBackpack(Pack);
	Site = Next;
	UpdateLogVisuals();
	SaveToIslandState();
	UE_LOG(LogExplored, Display, TEXT("[Balsa] Tronco %d/%d en el sitio"), Site.LogsPlaced, Tuning.LogsNeeded);
}

void AExploredRaftSite::Tie(AActor* InInstigator)
{
	UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	const FIslandData& Data = ExploredIslandData::Get();
	const FRecipeDef* Recipe = Data.Recipes.Find(ExploredRaftDetail::RaftRecipe);
	if (!Survivor || !Recipe)
	{
		return;
	}
	const TMap<FName, int32> Station = { { ExploredRaftDetail::LogItem, Site.LogsPlaced } };
	const ECraftResult Check = FRecipeBook::Check(*Recipe, Survivor->GetBackpack(), ECraftPlace::RaftSite, Station);
	if (Check != ECraftResult::Ok)
	{
		Survivor->ReportCraft(Check);
		return;
	}
	const TWeakObjectPtr<AExploredRaftSite> WeakThis(this);
	const TWeakObjectPtr<AActor> WeakInstigator(InInstigator);
	Survivor->StartTimedAction(Recipe->Seconds, [WeakThis, WeakInstigator]()
	{
		if (AExploredRaftSite* Self = WeakThis.Get())
		{
			Self->FinishTie(WeakInstigator.Get());
		}
	});
}

void AExploredRaftSite::FinishTie(AActor* InInstigator)
{
	UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(InInstigator);
	const FRecipeDef* Recipe = ExploredIslandData::Get().Recipes.Find(ExploredRaftDetail::RaftRecipe);
	if (!Survivor || !Recipe)
	{
		return;
	}
	const TMap<FName, int32> Station = { { ExploredRaftDetail::LogItem, Site.LogsPlaced } };
	FBackpackState Pack;
	const ECraftResult Result = FRecipeBook::Apply(*Recipe, Survivor->GetBackpack(), ExploredIslandData::Get().Backpack,
		ECraftPlace::RaftSite, Station, Pack);
	FRaftSiteState Next;
	if (Result != ECraftResult::Ok || FRaftModel::Tie(Site, Next, Tuning) != ERaftSiteResult::Ok)
	{
		Survivor->ReportCraft(Result);
		return;
	}
	Survivor->ApplyBackpack(Pack);
	Site = Next;
	UpdateLogVisuals();
	EnsureRaft();
	SaveToIslandState();
	ExploredRaftDetail::Say(this, LOCTEXT("RaftDone", "Cuatro troncos y dos cuerdas. Flota."));
	UE_LOG(LogExplored, Display, TEXT("[Balsa] Balsa atada"));
}

void AExploredRaftSite::DebugBuild()
{
	Site.LogsPlaced = Tuning.LogsNeeded;
	Site.bBuilt = true;
	UpdateLogVisuals();
	EnsureRaft();
	SaveToIslandState();
}

void AExploredRaftSite::SaveToIslandState() const
{
	UExploredIslandStateSubsystem* Island = UExploredIslandStateSubsystem::Get(this);
	if (!Island)
	{
		return;
	}
	ExploredSaveStates::FRaftSaveData Data;
	Data.Site = Site;
	if (const AExploredRaft* Existing = Raft.Get())
	{
		Data.bHasPose = true;
		Data.Location = Existing->GetActorLocation();
		Data.YawDeg = static_cast<float>(Existing->GetActorRotation().Yaw);
	}
	Island->SetRaft(Data);
}

bool AExploredRaftSite::IsPlayerOnIslet(const APawn& Pawn) const
{
	return bHasIslet && FVector::Dist2D(Pawn.GetActorLocation(), IsletCenter) <= IsletRadiusMeters * 100.0;
}

void AExploredRaftSite::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	WatchClock();
	// La posición de la balsa se guarda con la partida (queda donde se dejó).
	if (Raft.IsValid())
	{
		SaveToIslandState();
	}
}

void AExploredRaftSite::WatchClock()
{
	const double Now = ExploredRaftDetail::AbsHours(this);
	const FStormClockModel Storm = ExploredRaftDetail::Storm(this);
	const double Closed = Storm.GetConfig().RaftLastHour;
	if (LastSeenHours >= 0.0 && LastSeenHours < Closed && Now >= Closed && Now < Closed + 1.0)
	{
		ExploredRaftDetail::Say(this, LOCTEXT("SeaClosing", "El mar se cierra."));
		UE_LOG(LogExplored, Display, TEXT("[Balsa] El mar se cierra (%.2f h)"), Now);
	}
	LastSeenHours = Now;

	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!bReturning && Pawn && FRaftModel::ShouldForceReturn(Now, Storm.GetConfig().ForcedReturnHour, IsPlayerOnIslet(*Pawn),
		Storm.GetConfig().RaftReopenHour))
	{
		ForceReturn();
	}
}

void AExploredRaftSite::ForceReturn()
{
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	UIslandBodyComponent* Body = UIslandBodyComponent::FindOn(Pawn);
	if (!Body || Body->IsFainting())
	{
		return;
	}
	bReturning = true;
	UE_LOG(LogExplored, Display, TEXT("[Balsa] 18:45 con Iria en el islote: cruce forzoso a Playa Norte"));
	const TWeakObjectPtr<AExploredRaftSite> WeakThis(this);
	Body->PlayBlackout([WeakThis]()
	{
		AExploredRaftSite* Self = WeakThis.Get();
		APawn* InnerPawn = Self ? UGameplayStatics::GetPlayerPawn(Self, 0) : nullptr;
		if (!Self || !InnerPawn)
		{
			return;
		}
		if (AExploredRaft* Existing = Self->Raft.Get())
		{
			if (Existing->IsOccupied())
			{
				Existing->Leave();
			}
			// Varada en la orilla: no se vuelve a usar hasta que reabre el mar (D3 06:00).
			Existing->PlaceAt(Self->FindLaunchLocation(), Self->RouteYawDeg);
		}
		InnerPawn->TeleportTo(Self->GetShoreLocation(), InnerPawn->GetActorRotation(), false, true);
		if (UIslandBodyComponent* InnerBody = UIslandBodyComponent::FindOn(InnerPawn))
		{
			InnerBody->RestoreBars(FRaftModel::ForcedReturnBars(InnerBody->GetBars(), Self->Tuning), InnerBody->GetLastShelterId());
		}
		Self->bReturning = false;
	}, LOCTEXT("ForcedReturn", "Cruzas como puedes. Playa Norte."));
}

// ============================================================================
// AExploredRaft
// ============================================================================

AExploredRaft::AExploredRaft()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Hull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
	Hull->SetupAttachment(Root);
	Hull->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Hull->SetCanEverAffectNavigation(false);
	Seat = CreateDefaultSubobject<USceneComponent>(TEXT("Seat"));
	Seat->SetupAttachment(Root);
	Seat->SetRelativeLocation(FVector(-40.0, 0.0, ExploredRaftDetail::SeatHeightCm));
}

void AExploredRaft::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// La malla se carga aquí y no en el constructor: lo que carga un constructor (CDO) al arrancar
	// el motor queda fuera del recolector, y entonces M_Toon ya no se puede rehacer en el editor
	// (build_materials.py: «Assertion failed: !IsRooted()»).
	bool bFinal = false;
	UStaticMesh* Mesh = ExploredRaftDetail::LoadMesh(ExploredRaftDetail::RaftMeshPath, ExploredAssets::FallbackCubePath, bFinal);
	Hull->SetStaticMesh(Mesh);
	// Cubo del motor de 1 m centrado: se escala a 3 × 2,2 × 0,3 m y se sube para que la base quede en el pivote.
	Hull->SetRelativeScale3D(bFinal ? FVector(1.0) : ExploredRaftDetail::RaftSizeMeters);
	Hull->SetRelativeLocation(bFinal ? FVector::ZeroVector : FVector(0.0, 0.0, ExploredRaftDetail::RaftSizeMeters.Z * 50.0));
}

float AExploredRaft::WaterHeightAt(const FVector& Location) const
{
	for (TActorIterator<AExploredOcean> It(GetWorld()); It; ++It)
	{
		return It->GetWaterHeightAt(Location);
	}
	return 0.0f;
}

bool AExploredRaft::ProbeGround(const FVector2D& PositionM, float& OutGroundZ) const
{
	const FVector At(PositionM.X * 100.0, PositionM.Y * 100.0, 0.0);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RaftGround), false, this);
	if (const AActor* Rider = Occupant.Get())
	{
		Params.AddIgnoredActor(Rider);
	}
	FHitResult Hit;
	if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, FVector(At.X, At.Y, ExploredRaftDetail::ProbeTopCm),
		FVector(At.X, At.Y, -ExploredRaftDetail::ProbeDepthCm), ECC_WorldStatic, Params))
	{
		OutGroundZ = static_cast<float>(Hit.ImpactPoint.Z);
		return true;
	}
	return false;
}

void AExploredRaft::PlaceAt(const FVector& Location, float YawDeg)
{
	Motion = FRaftMotion();
	Motion.PositionM = FVector2D(Location.X, Location.Y) / 100.0;
	Motion.YawDeg = YawDeg;
	BaseZ = WaterHeightAt(Location);
	bGrounded = false;
	SetActorLocationAndRotation(FVector(Location.X, Location.Y, BaseZ), FRotator(0.0, YawDeg, 0.0));
}

void AExploredRaft::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyMotion(DeltaSeconds);
}

void AExploredRaft::ApplyMotion(float DeltaSeconds)
{
	FRaftControls Step = Controls;
	Step.bHasPaddle = IsOccupied() && OccupantHasPaddle();
	if (!IsOccupied())
	{
		Step.Forward = 0.0f;
		Step.Turn = 0.0f;
	}
	// Mira el fondo un poco por delante de la proa: si toca, encalla (y aún puede girar para salir).
	const FVector2D Ahead = Motion.PositionM + FRaftModel::Forward(Motion.YawDeg) * 1.6;
	float GroundZ = -1.0e6f;
	const FVector AheadCm(Ahead.X * 100.0, Ahead.Y * 100.0, 0.0);
	const float WaterAhead = WaterHeightAt(AheadCm);
	bGrounded = ProbeGround(Ahead, GroundZ) && GroundZ > WaterAhead - DraftCm;
	Step.bGrounded = bGrounded;
	// Sin ocupante la balsa no deriva: se queda donde se dejó (más legible que buscarla).
	const FVector2D Current = IsOccupied() ? CurrentMps : FVector2D::ZeroVector;
	Motion = FRaftModel::Step(Motion, Step, bGrounded ? FVector2D::ZeroVector : Current, DeltaSeconds, Tuning);

	const FVector Here(Motion.PositionM.X * 100.0, Motion.PositionM.Y * 100.0, 0.0);
	const float Water = WaterHeightAt(Here);
	const FVector2D Fwd = FRaftModel::Forward(Motion.YawDeg);
	const FVector2D Right = FRaftModel::Forward(Motion.YawDeg + 90.0f);
	const float SlopeF = (WaterHeightAt(Here + FVector(Fwd.X, Fwd.Y, 0.0) * 150.0) - WaterHeightAt(Here - FVector(Fwd.X, Fwd.Y, 0.0) * 150.0)) / 300.0f;
	const float SlopeR = (WaterHeightAt(Here + FVector(Right.X, Right.Y, 0.0) * 110.0) - WaterHeightAt(Here - FVector(Right.X, Right.Y, 0.0) * 110.0)) / 220.0f;
	const FVector2D Tilt = FRaftModel::StableTilt(SlopeF, SlopeR, Tuning);
	// Altura amortiguada: sigue el agua sin saltos (tampoco vuelca).
	BaseZ = FMath::FInterpTo(BaseZ, Water, DeltaSeconds, 3.0f);
	SetActorLocationAndRotation(FVector(Here.X, Here.Y, BaseZ), FRotator(Tilt.X, Motion.YawDeg, -Tilt.Y));
}

bool AExploredRaft::OccupantHasPaddle() const
{
	const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(Occupant.Get());
	return Survivor && Survivor->GetItemCount(ExploredRaftDetail::PaddleItem) > 0;
}

APlayerController* AExploredRaft::GetOccupantController() const
{
	const AExploredCharacter* Character = Occupant.Get();
	return Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
}

void AExploredRaft::GetContextVerbs_Implementation(TArray<FText>& OutVerbs) const
{
	if (!IsOccupied())
	{
		OutVerbs.Add(LOCTEXT("BoardRaft", "Subir a la balsa"));
	}
}

bool AExploredRaft::CanInteract_Implementation(AActor* InInstigator) const
{
	return !IsOccupied() && Cast<AExploredCharacter>(InInstigator) != nullptr;
}

void AExploredRaft::Interact_Implementation(AActor* InInstigator)
{
	AExploredCharacter* Character = Cast<AExploredCharacter>(InInstigator);
	const UExploredSurvivorComponent* Survivor = UExploredSurvivorComponent::FindOn(Character);
	FRaftSiteState Built;
	Built.LogsPlaced = Tuning.LogsNeeded;
	Built.bBuilt = true;
	const ERaftBoard Check = FRaftModel::CanBoard(Built, Survivor && Survivor->GetItemCount(ExploredRaftDetail::PaddleItem) > 0,
		ExploredRaftDetail::AbsHours(this), ExploredRaftDetail::Storm(this).GetConfig().RaftLastHour,
		ExploredRaftDetail::Storm(this).GetConfig().RaftReopenHour);
	if (Check != ERaftBoard::Ok)
	{
		ExploredRaftDetail::Say(this, ExploredRaftDetail::BoardMessage(Check));
		return;
	}
	Board(Character);
}

bool AExploredRaft::Board(AExploredCharacter* Character)
{
	if (!Character || IsOccupied())
	{
		return false;
	}
	Occupant = Character;
	Controls = FRaftControls();
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
		Movement->SetComponentTickEnabled(false);
	}
	if (USwimComponent* Swim = Character->GetSwimComponent())
	{
		Swim->SetComponentTickEnabled(false);
	}
	Character->SetActorEnableCollision(false);
	Character->AttachToComponent(Seat, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld,
		EAttachmentRule::KeepWorld, false));
	if (APlayerController* PC = GetOccupantController())
	{
		BuildInputAssets();
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(MappingContext, ExploredRaftDetail::RaftInputPriority);
		}
		EnableInput(PC);
		BindInput();
	}
	if (UBeaconSubsystem* Beacon = UBeaconSubsystem::Get(this))
	{
		Beacon->TriggerThought(EIriaThought::BoardRaft);
	}
	UE_LOG(LogExplored, Display, TEXT("[Balsa] Iria sube a la balsa en %s"), *GetActorLocation().ToCompactString());
	return true;
}

void AExploredRaft::Leave()
{
	AExploredCharacter* Character = Occupant.Get();
	if (APlayerController* PC = GetOccupantController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(MappingContext);
		}
		DisableInput(PC);
	}
	Occupant.Reset();
	Controls = FRaftControls();
	if (!Character)
	{
		return;
	}
	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	// Por la proa si hay tierra cerca (desembarcar en la orilla); si no, por la borda, al agua.
	const FVector2D Fwd = FRaftModel::Forward(Motion.YawDeg);
	FVector Exit = GetActorLocation() + GetActorRightVector() * 180.0 + FVector(0.0, 0.0, 120.0);
	for (const float Reach : { 2.6f, 3.6f, 4.6f })
	{
		float GroundZ = 0.0f;
		const FVector2D Probe = Motion.PositionM + Fwd * Reach;
		if (ProbeGround(Probe, GroundZ) && GroundZ > WaterHeightAt(FVector(Probe.X * 100.0, Probe.Y * 100.0, 0.0)) - 20.0f)
		{
			Exit = FVector(Probe.X * 100.0, Probe.Y * 100.0, GroundZ + 110.0);
			break;
		}
	}
	Character->SetActorLocationAndRotation(Exit, FRotator(0.0, Motion.YawDeg, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
	Character->SetActorEnableCollision(true);
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->SetComponentTickEnabled(true);
		Movement->SetMovementMode(MOVE_Falling);
	}
	if (USwimComponent* Swim = Character->GetSwimComponent())
	{
		Swim->SetComponentTickEnabled(true);
	}
	UE_LOG(LogExplored, Display, TEXT("[Balsa] Iria baja de la balsa en %s"), *Exit.ToCompactString());
}

void AExploredRaft::BuildInputAssets()
{
	if (MappingContext)
	{
		return;
	}
	using namespace ExploredRaftDetail;
	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Raft"));
	RowAction = MakeAction(this, TEXT("IA_RaftRow"));
	TurnAction = MakeAction(this, TEXT("IA_RaftTurn"));
	LeaveAction = MakeAction(this, TEXT("IA_RaftLeave"));
	LeaveAction->ValueType = EInputActionValueType::Boolean;
	MapKey(MappingContext, RowAction, EKeys::W);
	MapKey(MappingContext, TurnAction, EKeys::D);
	MapKey(MappingContext, TurnAction, EKeys::A, true);
	// E también es «interactuar» en el contexto del personaje; con más prioridad, aquí baja de la balsa.
	MapKey(MappingContext, LeaveAction, EKeys::E);
}

void AExploredRaft::BindInput()
{
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (bInputBound || !Input)
	{
		return;
	}
	Input->BindAction(RowAction, ETriggerEvent::Triggered, this, &AExploredRaft::HandleRow);
	Input->BindAction(RowAction, ETriggerEvent::Completed, this, &AExploredRaft::HandleRowReleased);
	Input->BindAction(TurnAction, ETriggerEvent::Triggered, this, &AExploredRaft::HandleTurn);
	Input->BindAction(TurnAction, ETriggerEvent::Completed, this, &AExploredRaft::HandleTurnReleased);
	Input->BindAction(LeaveAction, ETriggerEvent::Started, this, &AExploredRaft::HandleLeave);
	bInputBound = true;
}

void AExploredRaft::HandleRow(const FInputActionValue& Value)
{
	Controls.Forward = FMath::Clamp(Value.Get<float>(), 0.0f, 1.0f);
}

void AExploredRaft::HandleRowReleased(const FInputActionValue&)
{
	Controls.Forward = 0.0f;
}

void AExploredRaft::HandleTurn(const FInputActionValue& Value)
{
	Controls.Turn = FMath::Clamp(Value.Get<float>(), -1.0f, 1.0f);
}

void AExploredRaft::HandleTurnReleased(const FInputActionValue&)
{
	Controls.Turn = 0.0f;
}

void AExploredRaft::HandleLeave(const FInputActionValue&)
{
	Leave();
}

#undef LOCTEXT_NAMESPACE
