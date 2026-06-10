#include "QuayCraneActor.h"
#include "QuayCranePhysics.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

AQuayCraneActor::AQuayCraneActor()
{
    PrimaryActorTick.bCanEverTick = true;

    CraneBoom = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CraneBoom"));
    SetRootComponent(CraneBoom);

    CraneBoom->SetSimulatePhysics(false);
    CraneBoom->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CraneBoom->SetMobility(EComponentMobility::Static);

    CraneLegPort = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CraneLegPort"));
    CraneLegPort->SetupAttachment(CraneBoom);

    CraneLegStarboard = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CraneLegStarboard"));
    CraneLegStarboard->SetupAttachment(CraneBoom);

    CraneAFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CraneAFrame"));
    CraneAFrame->SetupAttachment(CraneBoom);

    CranePortalBeam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CranePortalBeam"));
    CranePortalBeam->SetupAttachment(CraneBoom);
}

void AQuayCraneActor::BeginPlay()
{
    Super::BeginPlay();
    SpawnSubActors();

    GetWorldTimerManager().SetTimerForNextTick(this, &AQuayCraneActor::ConnectWireRopeSystem);

    UE_LOG(LogQuayCranePhysics, Log, TEXT("QuayCrane initialized - Boom: %.0f, Portal: %.0f x %.0f"),
        BoomLength, PortalWidth, PortalHeight);
}

void AQuayCraneActor::SpawnSubActors()
{
    FTransform CraneTransform = GetActorTransform();
    FVector BoomBottom = GetActorLocation() + FVector(0, 0, -BoomHeight * 0.5f);
    FVector TrolleySpawnLoc = BoomBottom + FVector(0, 0, 500.0f);
    FVector SpreaderSpawnLoc = TrolleySpawnLoc - FVector(0, 0, InitialHoistLength);

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    Trolley = GetWorld()->SpawnActor<ACraneTrolley>(ACraneTrolley::StaticClass(),
        FTransform(FRotator::ZeroRotator, TrolleySpawnLoc), SpawnParams);

    if (Trolley)
    {
        Trolley->CurrentHoistLength = InitialHoistLength;
        UE_LOG(LogQuayCranePhysics, Log, TEXT("Trolley spawned at: %s"), *TrolleySpawnLoc.ToString());
    }

    Spreader = GetWorld()->SpawnActor<ASpreaderActor>(ASpreaderActor::StaticClass(),
        FTransform(FRotator::ZeroRotator, SpreaderSpawnLoc), SpawnParams);

    if (Spreader)
    {
        UE_LOG(LogQuayCranePhysics, Log, TEXT("Spreader spawned at: %s"), *SpreaderSpawnLoc.ToString());
    }

    WireRopeSystem = GetWorld()->SpawnActor<AWireRopeActor>(AWireRopeActor::StaticClass(),
        FTransform(FRotator::ZeroRotator, GetActorLocation()), SpawnParams);

    if (WireRopeSystem)
    {
        WireRopeSystem->DefaultRestLength = InitialHoistLength;
        UE_LOG(LogQuayCranePhysics, Log, TEXT("WireRopeSystem spawned"));
    }
}

void AQuayCraneActor::ConnectWireRopeSystem()
{
    if (!Trolley || !Spreader || !WireRopeSystem)
    {
        UE_LOG(LogQuayCranePhysics, Error, TEXT("ConnectWireRopeSystem: Missing sub-actors"));
        return;
    }

    UPrimitiveComponent* TrolleyRoot = Cast<UPrimitiveComponent>(Trolley->GetRootComponent());
    UPrimitiveComponent* SpreaderRoot = Cast<UPrimitiveComponent>(Spreader->GetRootComponent());

    if (!TrolleyRoot || !SpreaderRoot)
    {
        UE_LOG(LogQuayCranePhysics, Error, TEXT("ConnectWireRopeSystem: Missing root primitives"));
        return;
    }

    TArray<FVector> TrolleyAnchors;
    TArray<FVector> SpreaderAttaches;

    float HalfSpreadX = 2000.0f;
    float HalfSpreadY = 2000.0f;

    TrolleyAnchors.Add(FVector( HalfSpreadX, -HalfSpreadY, -100.0f));
    TrolleyAnchors.Add(FVector( HalfSpreadX,  HalfSpreadY, -100.0f));
    TrolleyAnchors.Add(FVector(-HalfSpreadX, -HalfSpreadY, -100.0f));
    TrolleyAnchors.Add(FVector(-HalfSpreadX,  HalfSpreadY, -100.0f));

    SpreaderAttaches.Add(FVector( HalfSpreadX, -HalfSpreadY, 750.0f));
    SpreaderAttaches.Add(FVector( HalfSpreadX,  HalfSpreadY, 750.0f));
    SpreaderAttaches.Add(FVector(-HalfSpreadX, -HalfSpreadY, 750.0f));
    SpreaderAttaches.Add(FVector(-HalfSpreadX,  HalfSpreadY, 750.0f));

    WireRopeSystem->ConnectTrolleyToSpreader(
        TrolleyRoot, SpreaderRoot,
        TrolleyAnchors, SpreaderAttaches,
        InitialHoistLength);

    UE_LOG(LogQuayCranePhysics, Log, TEXT("Wire rope system connected: 4 ropes, %.0f mm hoist"), InitialHoistLength);
}

void AQuayCraneActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdateOperationMode(DeltaTime);
    UpdateHoistSync();

    if (Spreader)
    {
        TotalWireTension = (WireRopeSystem) ? WireRopeSystem->TotalTension : 0.0f;
        CurrentPendulumAngle = Spreader->PendulumAngleX;
        bIsContainerAttached = Spreader->bIsContainerAttached;
        CurrentContainer = Cast<AContainerActor>(Spreader->TwistlockSystem ? Spreader->TwistlockSystem->AttachedContainer : nullptr);
    }

    if (bAutoSequenceActive)
    {
        ProcessAutoSequence(DeltaTime);
    }

    CurrentHoistLength = (Trolley) ? Trolley->CurrentHoistLength : InitialHoistLength;
}

void AQuayCraneActor::UpdateOperationMode(float DeltaTime)
{
    if (!Trolley) return;

    bool bIsTraveling = FMath::Abs(Trolley->CurrentTravelSpeed) > 10.0f;
    bool bIsHoisting = FMath::Abs(Trolley->CurrentHoistSpeed) > 10.0f;

    if (bIsContainerAttached)
    {
        if (bIsTraveling)
            OperationMode = ECraneOperationMode::Transporting;
        else if (bIsHoisting && Trolley->CurrentHoistSpeed > 0)
            OperationMode = ECraneOperationMode::Lifting;
        else
            OperationMode = ECraneOperationMode::Idle;
    }
    else
    {
        if (bIsTraveling)
            OperationMode = ECraneOperationMode::Traveling;
        else if (bIsHoisting && Trolley->CurrentHoistSpeed > 0)
            OperationMode = ECraneOperationMode::Lowering;
        else if (bIsHoisting && Trolley->CurrentHoistSpeed < 0)
            OperationMode = ECraneOperationMode::Hoisting;
        else if (Spreader && Spreader->TwistlockSystem &&
                 Spreader->TwistlockSystem->CurrentState == ETwistlockState::ProximityDetected)
            OperationMode = ECraneOperationMode::Locking;
        else
            OperationMode = ECraneOperationMode::Idle;
    }
}

void AQuayCraneActor::UpdateHoistSync()
{
    if (!Trolley || !WireRopeSystem) return;

    float TargetLength = Trolley->GetTargetHoistLength();
    WireRopeSystem->UpdateHoistLength(TargetLength);
}

void AQuayCraneActor::ScanForNearbyContainers()
{
    NearbyContainers.Empty();

    TArray<AActor*> FoundContainers;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AContainerActor::StaticClass(), FoundContainers);

    FVector CraneLocation = GetActorLocation();

    for (AActor* Actor : FoundContainers)
    {
        AContainerActor* Container = Cast<AContainerActor>(Actor);
        if (Container && !Container->bIsAttachedToSpreader)
        {
            float Distance = FVector::Dist(CraneLocation, Container->GetActorLocation());
            if (Distance < CONTAINER_SCAN_RADIUS)
            {
                NearbyContainers.Add(Container);
            }
        }
    }
}

void AQuayCraneActor::SetTrolleyTravel(float Input)
{
    if (Trolley)
    {
        Trolley->SetTravelInput(Input);
    }
}

void AQuayCraneActor::SetHoistMovement(float Input)
{
    if (Trolley)
    {
        Trolley->SetHoistInput(Input);
    }
}

void AQuayCraneActor::EngageTwistlocks()
{
    if (Spreader)
    {
        Spreader->EngageTwistlocks();
    }
}

void AQuayCraneActor::DisengageTwistlocks()
{
    if (Spreader)
    {
        Spreader->DisengageTwistlocks();
    }
}

void AQuayCraneActor::EmergencyStop()
{
    if (Trolley)
    {
        Trolley->EmergencyStop();
    }
    if (Spreader)
    {
        Spreader->EmergencyRelease();
    }
}

void AQuayCraneActor::SpawnContainerAtLocation(const FVector& Location, EContainerSize Size)
{
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AContainerActor* NewContainer = GetWorld()->SpawnActor<AContainerActor>(
        AContainerActor::StaticClass(),
        FTransform(FRotator::ZeroRotator, Location),
        SpawnParams);

    if (NewContainer)
    {
        NewContainer->ContainerSize = Size;
        NewContainer->bIsLoaded = true;
        UE_LOG(LogQuayCranePhysics, Log, TEXT("Container spawned at: %s"), *Location.ToString());
    }
}

void AQuayCraneActor::AutoLiftSequence()
{
    if (!Spreader || !Trolley) return;

    bAutoSequenceActive = true;
    AutoSequencePhase = 0.0f;
    AutoSequenceTimer = 0.0f;
    OperationMode = ECraneOperationMode::Locking;

    UE_LOG(LogQuayCranePhysics, Log, TEXT("Auto-lift sequence initiated"));
}

void AQuayCraneActor::AutoPlaceSequence()
{
    if (!Spreader || !Trolley || !bIsContainerAttached) return;

    bAutoSequenceActive = true;
    AutoSequencePhase = 10.0f;
    AutoSequenceTimer = 0.0f;
    OperationMode = ECraneOperationMode::Placing;

    UE_LOG(LogQuayCranePhysics, Log, TEXT("Auto-place sequence initiated"));
}

void AQuayCraneActor::CancelAutoSequence()
{
    bAutoSequenceActive = false;
    AutoSequencePhase = 0.0f;
    EmergencyStop();
    UE_LOG(LogQuayCranePhysics, Log, TEXT("Auto sequence CANCELLED"));
}

void AQuayCraneActor::ProcessAutoSequence(float DeltaTime)
{
    AutoSequenceTimer += DeltaTime;

    if (AutoSequencePhase < 5.0f)
    {
        if (AutoSequencePhase < 1.0f)
        {
            if (Spreader && Spreader->TwistlockSystem &&
                Spreader->TwistlockSystem->CurrentState == ETwistlockState::ProximityDetected)
            {
                EngageTwistlocks();
                AutoSequencePhase = 1.0f;
                AutoSequenceTimer = 0.0f;
            }
            else
            {
                SetHoistMovement(-0.3f);
            }
        }
        else if (AutoSequencePhase < 2.0f)
        {
            if (AutoSequenceTimer > 1.5f)
            {
                AutoSequencePhase = 2.0f;
                AutoSequenceTimer = 0.0f;
            }
        }
        else if (AutoSequencePhase < 3.0f)
        {
            SetHoistMovement(0.8f);
            if (AutoSequenceTimer > 3.0f)
            {
                SetHoistMovement(0.0f);
                AutoSequencePhase = 3.0f;
                AutoSequenceTimer = 0.0f;
                OperationMode = ECraneOperationMode::Transporting;
            }
        }
        else
        {
            SetTrolleyTravel(0.5f);
            if (AutoSequenceTimer > 5.0f)
            {
                SetTrolleyTravel(0.0f);
                bAutoSequenceActive = false;
                OperationMode = ECraneOperationMode::Idle;
            }
        }
    }
    else
    {
        if (AutoSequencePhase < 11.0f)
        {
            SetTrolleyTravel(-0.5f);
            if (AutoSequenceTimer > 5.0f)
            {
                SetTrolleyTravel(0.0f);
                AutoSequencePhase = 11.0f;
                AutoSequenceTimer = 0.0f;
            }
        }
        else if (AutoSequencePhase < 12.0f)
        {
            SetHoistMovement(-0.5f);
            if (AutoSequenceTimer > 4.0f)
            {
                SetHoistMovement(0.0f);
                AutoSequencePhase = 12.0f;
                AutoSequenceTimer = 0.0f;
            }
        }
        else if (AutoSequencePhase < 13.0f)
        {
            DisengageTwistlocks();
            if (AutoSequenceTimer > 1.0f)
            {
                AutoSequencePhase = 13.0f;
                AutoSequenceTimer = 0.0f;
            }
        }
        else
        {
            SetHoistMovement(0.8f);
            if (AutoSequenceTimer > 3.0f)
            {
                SetHoistMovement(0.0f);
                bAutoSequenceActive = false;
                OperationMode = ECraneOperationMode::Idle;
            }
        }
    }
}

void AQuayCraneActor::DrawDebugPhysicsState()
{
    if (!Trolley || !Spreader || !WireRopeSystem) return;

    FVector TrolleyLoc = Trolley->GetActorLocation();
    FVector SpreaderLoc = Spreader->GetActorLocation();

    DrawDebugLine(GetWorld(), TrolleyLoc, SpreaderLoc, FColor::Yellow, false, 0.1f, 0, 3.0f);

    TArray<FVector> TrolleyAnchors = Trolley->GetWireRopeAnchorWorldPositions();
    TArray<FVector> SpreaderAttaches = Spreader->GetWireRopeAttachWorldPositions();

    for (int32 i = 0; i < 4 && i < TrolleyAnchors.Num() && i < SpreaderAttaches.Num(); ++i)
    {
        FColor RopeColor = FColor::Green;
        if (WireRopeSystem->IsRopeBroken(static_cast<EWireRopePosition>(i)))
        {
            RopeColor = FColor::Red;
        }
        else
        {
            float Tension = WireRopeSystem->GetRopeTension(static_cast<EWireRopePosition>(i));
            float TensionRatio = FMath::Clamp(Tension / WireRopeSystem->DefaultBreakThreshold, 0.0f, 1.0f);
            RopeColor = FColor::MakeRedToGreenColorFromScalar(1.0f - TensionRatio);
        }

        DrawDebugLine(GetWorld(), TrolleyAnchors[i], SpreaderAttaches[i], RopeColor, false, 0.1f, 0, 2.0f);

        for (int32 j = 0; j < WireRopeSystem->GetRopeSegmentPositions(i).Num() - 1; ++j)
        {
            TArray<FVector> SegPos = WireRopeSystem->GetRopeSegmentPositions(i);
            if (j + 1 < SegPos.Num())
            {
                DrawDebugLine(GetWorld(), SegPos[j], SegPos[j + 1], RopeColor, false, 0.1f, 0, 1.5f);
            }
        }
    }

    if (bIsContainerAttached && CurrentContainer)
    {
        DrawDebugBox(GetWorld(), CurrentContainer->GetActorLocation(),
            CurrentContainer->GetContainerDimensions() * 0.5f,
            FColor::Cyan, false, 0.1f, 0, 2.0f);
    }

    if (Spreader->TwistlockSystem)
    {
        for (const FTwistlockSocket& Socket : Spreader->TwistlockSystem->TwistlockSockets)
        {
            FColor SocketColor = Socket.bIsEngaged ? FColor::Red : FColor::Green;
            DrawDebugSphere(GetWorld(), Socket.WorldPosition, 20.0f, 8, SocketColor, false, 0.1f, 0, 1.0f);
        }
    }
}
