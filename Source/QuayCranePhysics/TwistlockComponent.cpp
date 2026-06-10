#include "TwistlockComponent.h"
#include "QuayCranePhysics.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"

UTwistlockComponent::UTwistlockComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    ContainerObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
}

void UTwistlockComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerRootPrimitive = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());

    if (TwistlockSockets.Num() == 0)
    {
        float HalfWidth = 2438.4f;
        float HalfDepth = 6058.0f;
        float TopZ = 200.0f;

        TwistlockSockets.SetNum(REQUIRED_TWISTLOCK_COUNT);

        TwistlockSockets[0].SocketName = FName(TEXT("TL_FrontLeft"));
        TwistlockSockets[0].LocalOffset = FVector(-HalfDepth, -HalfWidth, TopZ);

        TwistlockSockets[1].SocketName = FName(TEXT("TL_FrontRight"));
        TwistlockSockets[1].LocalOffset = FVector(-HalfDepth, HalfWidth, TopZ);

        TwistlockSockets[2].SocketName = FName(TEXT("TL_RearLeft"));
        TwistlockSockets[2].LocalOffset = FVector(HalfDepth, -HalfWidth, TopZ);

        TwistlockSockets[3].SocketName = FName(TEXT("TL_RearRight"));
        TwistlockSockets[3].LocalOffset = FVector(HalfDepth, HalfWidth, TopZ);
    }
}

void UTwistlockComponent::InitializeTwistlocks(const TArray<FTwistlockSocket>& InSockets)
{
    TwistlockSockets = InSockets;
    EngagedSocketCount = 0;

    for (FTwistlockSocket& Socket : TwistlockSockets)
    {
        Socket.bIsEngaged = false;
        Socket.LockConstraint = nullptr;
    }
}

void UTwistlockComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    for (FTwistlockSocket& Socket : TwistlockSockets)
    {
        Socket.WorldPosition = GetOwner()->GetActorTransform().TransformPosition(Socket.LocalOffset);
    }

    switch (CurrentState)
    {
    case ETwistlockState::Unlocked:
        ScanForContainerCornerCastings();
        CheckProximity();
        break;

    case ETwistlockState::ProximityDetected:
        CheckProximity();
        break;

    case ETwistlockState::Engaging:
        UpdateEngagement(DeltaTime);
        break;

    case ETwistlockState::Locked:
        break;

    case ETwistlockState::Disengaging:
        EngagementTimer -= DeltaTime;
        EngagementProgress = FMath::Clamp(EngagementTimer / EngagementDuration, 0.0f, 1.0f);
        if (EngagementTimer <= 0.0f)
        {
            DestroyLockConstraints();
            AttachedContainer = nullptr;
            EngagedSocketCount = 0;
            for (FTwistlockSocket& Socket : TwistlockSockets)
            {
                Socket.bIsEngaged = false;
            }
            SetState(ETwistlockState::Unlocked);
            OnContainerDetached.Broadcast();
        }
        break;
    }
}

void UTwistlockComponent::ScanForContainerCornerCastings()
{
    DetectedCastingPositions.Empty();

    if (!OwnerRootPrimitive)
        return;

    FTransform OwnerTransform = GetOwner()->GetActorTransform();
    FVector DownDir = -OwnerTransform.GetUnitAxis(EAxis::Z);

    for (int32 i = 0; i < TwistlockSockets.Num(); ++i)
    {
        FVector SocketWorldPos = OwnerTransform.TransformPosition(TwistlockSockets[i].LocalOffset);

        FHitResult HitResult;
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(GetOwner());
        for (AActor* Ignored : IgnoredActors)
        {
            QueryParams.AddIgnoredActor(Ignored);
        }

        FVector TraceEnd = SocketWorldPos + DownDir * ProximityScanRange;

        bool bHit = GetWorld()->LineTraceSingleByObjectType(
            HitResult,
            SocketWorldPos,
            TraceEnd,
            FCollisionObjectQueryParams(ContainerObjectTypes),
            QueryParams);

        if (bHit)
        {
            DetectedCastingPositions.Add(HitResult.ImpactPoint);
        }
    }
}

void UTwistlockComponent::CheckProximity()
{
    if (DetectedCastingPositions.Num() >= REQUIRED_TWISTLOCK_COUNT)
    {
        if (CurrentState == ETwistlockState::Unlocked)
        {
            SetState(ETwistlockState::ProximityDetected);
        }
    }
    else
    {
        if (CurrentState == ETwistlockState::ProximityDetected)
        {
            SetState(ETwistlockState::Unlocked);
        }
    }
}

bool UTwistlockComponent::TryEngageTwistlocks()
{
    if (CurrentState != ETwistlockState::ProximityDetected)
    {
        UE_LOG(LogQuayCranePhysics, Warning, TEXT("TryEngageTwistlocks: Not in proximity state"));
        return false;
    }

    if (!AreAllSocketsAligned())
    {
        UE_LOG(LogQuayCranePhysics, Warning, TEXT("TryEngageTwistlocks: Sockets not aligned within tolerance"));
        return false;
    }

    EngagementTimer = 0.0f;
    EngagementProgress = 0.0f;
    SetState(ETwistlockState::Engaging);

    UE_LOG(LogQuayCranePhysics, Log, TEXT("Twistlock engagement initiated"));
    return true;
}

void UTwistlockComponent::UpdateEngagement(float DeltaTime)
{
    EngagementTimer += DeltaTime;
    EngagementProgress = FMath::Clamp(EngagementTimer / EngagementDuration, 0.0f, 1.0f);

    if (EngagementTimer >= EngagementDuration)
    {
        CreateLockConstraints();
        EngagedSocketCount = REQUIRED_TWISTLOCK_COUNT;

        for (FTwistlockSocket& Socket : TwistlockSockets)
        {
            Socket.bIsEngaged = true;
        }

        SetState(ETwistlockState::Locked);

        if (AttachedContainer)
        {
            OnContainerAttached.Broadcast(AttachedContainer);
        }

        UE_LOG(LogQuayCranePhysics, Log, TEXT("Twistlock ENGAGED - Container locked: %s"),
            AttachedContainer ? *AttachedContainer->GetName() : TEXT("None"));
    }
}

void UTwistlockComponent::CreateLockConstraints()
{
    AActor* Owner = GetOwner();
    if (!Owner || !AttachedContainer)
        return;

    UPrimitiveComponent* SpreaderRoot = Cast<UPrimitiveComponent>(Owner->GetRootComponent());
    UPrimitiveComponent* ContainerRoot = Cast<UPrimitiveComponent>(AttachedContainer->GetRootComponent());

    if (!SpreaderRoot || !ContainerRoot)
        return;

    for (int32 i = 0; i < TwistlockSockets.Num(); ++i)
    {
        if (TwistlockSockets[i].LockConstraint)
            continue;

        UPhysicsConstraintComponent* LockConstraint = NewObject<UPhysicsConstraintComponent>(Owner,
            *FString::Printf(TEXT("TwistlockConstraint_%d"), i));
        LockConstraint->RegisterComponent();

        LockConstraint->ConstraintSetup.LinearLimit.XMotion = ELinearConstraintMotion::LCM_Locked;
        LockConstraint->ConstraintSetup.LinearLimit.YMotion = ELinearConstraintMotion::LCM_Locked;
        LockConstraint->ConstraintSetup.LinearLimit.ZMotion = ELinearConstraintMotion::LCM_Locked;

        LockConstraint->ConstraintSetup.AngularLimit.Swing1Motion = EAngularConstraintMotion::ACM_Locked;
        LockConstraint->ConstraintSetup.AngularLimit.Swing2Motion = EAngularConstraintMotion::ACM_Locked;
        LockConstraint->ConstraintSetup.AngularLimit.TwistMotion = EAngularConstraintMotion::ACM_Locked;

        LockConstraint->ConstraintInstance.SetLinearDriveParams(LockStiffness, LockDamping, LockBreakForce);
        LockConstraint->ConstraintInstance.SetAngularDriveParams(LockStiffness, LockDamping, LockBreakForce);

        FVector ContainerLocalOffset = FVector::ZeroVector;
        if (i < DetectedCastingPositions.Num())
        {
            ContainerLocalOffset = AttachedContainer->GetActorTransform().InverseTransformPosition(DetectedCastingPositions[i]);
        }

        LockConstraint->SetConstrainedComponents(
            SpreaderRoot, TwistlockSockets[i].LocalOffset,
            ContainerRoot, ContainerLocalOffset);

        TwistlockSockets[i].LockConstraint = LockConstraint;
    }
}

void UTwistlockComponent::DestroyLockConstraints()
{
    for (FTwistlockSocket& Socket : TwistlockSockets)
    {
        if (Socket.LockConstraint)
        {
            Socket.LockConstraint->BreakConstraint();
            Socket.LockConstraint->DestroyComponent();
            Socket.LockConstraint = nullptr;
        }
        Socket.bIsEngaged = false;
    }
}

bool UTwistlockComponent::AreAllSocketsAligned() const
{
    int32 AlignedCount = 0;

    for (int32 i = 0; i < TwistlockSockets.Num(); ++i)
    {
        FVector CastingWorldPos;
        if (IsSocketAlignedWithCasting(i, CastingWorldPos))
        {
            AlignedCount++;
        }
    }

    return AlignedCount >= REQUIRED_TWISTLOCK_COUNT;
}

bool UTwistlockComponent::IsSocketAlignedWithCasting(int32 SocketIndex, FVector& OutCastingWorldPos) const
{
    if (SocketIndex >= TwistlockSockets.Num() || SocketIndex >= DetectedCastingPositions.Num())
        return false;

    FVector SocketWorldPos = GetOwner()->GetActorTransform().TransformPosition(TwistlockSockets[SocketIndex].LocalOffset);
    FVector CastingPos = DetectedCastingPositions[SocketIndex];
    OutCastingWorldPos = CastingPos;

    FVector Delta = CastingPos - SocketWorldPos;
    float Distance = Delta.Size();

    if (Distance > EngagementRange)
        return false;

    if (Distance <= PositionTolerance)
        return true;

    return false;
}

void UTwistlockComponent::DisengageTwistlocks()
{
    if (CurrentState != ETwistlockState::Locked)
        return;

    EngagementTimer = EngagementDuration;
    SetState(ETwistlockState::Disengaging);
    UE_LOG(LogQuayCranePhysics, Log, TEXT("Twistlock disengagement initiated"));
}

void UTwistlockComponent::ForceDisengage()
{
    DestroyLockConstraints();
    AttachedContainer = nullptr;
    EngagedSocketCount = 0;
    EngagementProgress = 0.0f;
    SetState(ETwistlockState::Unlocked);
    OnContainerDetached.Broadcast();
    UE_LOG(LogQuayCranePhysics, Warning, TEXT("Twistlock FORCE disengaged"));
}

void UTwistlockComponent::SetState(ETwistlockState NewState)
{
    ETwistlockState OldState = CurrentState;
    CurrentState = NewState;

    if (OldState != NewState)
    {
        OnTwistlockStateChanged.Broadcast(NewState, OldState);
    }
}
