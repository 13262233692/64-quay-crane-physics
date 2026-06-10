#include "SpreaderActor.h"
#include "QuayCranePhysics.h"
#include "ContainerActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"

ASpreaderActor::ASpreaderActor()
{
    PrimaryActorTick.bCanEverTick = true;

    SpreaderFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpreaderFrame"));
    SetRootComponent(SpreaderFrame);

    SpreaderFrame->SetSimulatePhysics(true);
    SpreaderFrame->SetEnableGravity(true);
    SpreaderFrame->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    SpreaderFrame->SetCollisionProfileName(TEXT("PhysicsActor"));
    SpreaderFrame->SetMassOverrideInKg(NAME_None, SpreaderMass);
    SpreaderFrame->SetLinearDamping(BaseLinearDamping);
    SpreaderFrame->SetAngularDamping(BaseAngularDamping);
    SpreaderFrame->SetNotifyRigidBodyCollision(true);
    SpreaderFrame->SetPhysicsMaxAngularVelocityInDegrees(MaxAngularVelocity);

    FBodyInstance* BodyInst = SpreaderFrame->GetBodyInstance();
    if (BodyInst)
    {
        BodyInst->SolverIterationCount = 30;
        BodyInst->SolverVelocityIterationCount = 15;
    }

    TwistlockSystem = CreateDefaultSubobject<UTwistlockComponent>(TEXT("TwistlockSystem"));

    for (int32 i = 0; i < 4; ++i)
    {
        USceneComponent* AttachPoint = CreateDefaultSubobject<USceneComponent>(
            *FString::Printf(TEXT("RopeAttachPoint_%d"), i));
        AttachPoint->SetupAttachment(SpreaderFrame);
        WireRopeAttachPoints.Add(AttachPoint);
    }
}

void ASpreaderActor::BeginPlay()
{
    Super::BeginPlay();
    SetupSpreaderPhysics();
    SetupRopeAttachPoints();
    BindTwistlockEvents();
}

void ASpreaderActor::SetupSpreaderPhysics()
{
    SpreaderFrame->SetMassOverrideInKg(NAME_None, SpreaderMass);
    SpreaderFrame->SetLinearDamping(BaseLinearDamping);
    SpreaderFrame->SetAngularDamping(BaseAngularDamping);
    SpreaderFrame->SetPhysicsMaxAngularVelocityInDegrees(MaxAngularVelocity);
    SpreaderFrame->UpdateMassProperties();
    TotalSuspendedMass = SpreaderMass;
}

void ASpreaderActor::SetupRopeAttachPoints()
{
    float HalfSpreadX = RopeAttachSpreadX * 0.5f;
    float HalfSpreadY = RopeAttachSpreadY * 0.5f;

    FVector AttachOffsets[4] = {
        FVector( HalfSpreadX, -HalfSpreadY, SpreaderHeight * 0.5f),
        FVector( HalfSpreadX,  HalfSpreadY, SpreaderHeight * 0.5f),
        FVector(-HalfSpreadX, -HalfSpreadY, SpreaderHeight * 0.5f),
        FVector(-HalfSpreadX,  HalfSpreadY, SpreaderHeight * 0.5f),
    };

    for (int32 i = 0; i < 4; ++i)
    {
        if (WireRopeAttachPoints[i])
        {
            WireRopeAttachPoints[i]->SetRelativeLocation(AttachOffsets[i]);
        }
    }
}

void ASpreaderActor::BindTwistlockEvents()
{
    if (TwistlockSystem)
    {
        TwistlockSystem->OnContainerAttached.AddDynamic(this, &ASpreaderActor::HandleContainerAttached);
        TwistlockSystem->OnContainerDetached.AddDynamic(this, &ASpreaderActor::HandleContainerDetached);
    }
}

void ASpreaderActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    CalculatePendulumState();
    ClampPhysicsVelocities();
    UpdateLockDamping(DeltaTime);
}

void ASpreaderActor::CalculatePendulumState()
{
    FVector SpreaderVelocity = SpreaderFrame->GetPhysicsLinearVelocity();
    PendulumVelocity = SpreaderVelocity;

    float VelX = SpreaderVelocity.X;
    float VelY = SpreaderVelocity.Y;

    PendulumAngleX = FMath::Atan2(VelX, FMath::Sqrt(GRAVITY * 15000.0f)) * 180.0f / PI;
    PendulumAngleY = FMath::Atan2(VelY, FMath::Sqrt(GRAVITY * 15000.0f)) * 180.0f / PI;
}

void ASpreaderActor::ClampPhysicsVelocities()
{
    if (!SpreaderFrame || !SpreaderFrame->IsSimulatingPhysics())
        return;

    FVector LinVel = SpreaderFrame->GetPhysicsLinearVelocity();
    FVector AngVel = SpreaderFrame->GetPhysicsAngularVelocityInDegrees();

    float LinSpeed = LinVel.Size();
    float AngSpeed = AngVel.Size();

    if (FMath::IsNaN(LinSpeed) || FMath::IsNaN(AngSpeed))
    {
        SpreaderFrame->SetPhysicsLinearVelocity(FVector::ZeroVector);
        SpreaderFrame->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        return;
    }

    float EffectiveMaxLinVel = MaxLinearVelocity;
    float EffectiveMaxAngVel = bIsContainerAttached ? StabilizationAngularMaxVel : MaxAngularVelocity;

    if (LinSpeed > EffectiveMaxLinVel)
    {
        SpreaderFrame->SetPhysicsLinearVelocity(LinVel.GetSafeNormal() * EffectiveMaxLinVel, false);
    }

    if (AngSpeed > EffectiveMaxAngVel)
    {
        SpreaderFrame->SetPhysicsAngularVelocityInDegrees(AngVel.GetSafeNormal() * EffectiveMaxAngVel, false);
    }
}

void ASpreaderActor::UpdateLockDamping(float DeltaTime)
{
    if (!bIsLockDamping)
        return;

    LockDampingTimer -= DeltaTime;

    if (LockDampingTimer > 0.0f)
    {
        float T = LockDampingTimer / LockDampingDuration;
        float CurrentLinDamping = BaseLinearDamping + LockLinearDampingBoost * T;
        float CurrentAngDamping = BaseAngularDamping + LockAngularDampingBoost * T;

        SpreaderFrame->SetLinearDamping(CurrentLinDamping);
        SpreaderFrame->SetAngularDamping(CurrentAngDamping);
    }
    else
    {
        bIsLockDamping = false;
        SpreaderFrame->SetLinearDamping(BaseLinearDamping);
        SpreaderFrame->SetAngularDamping(BaseAngularDamping);
    }
}

void ASpreaderActor::EngageTwistlocks()
{
    if (TwistlockSystem)
    {
        TwistlockSystem->TryEngageTwistlocks();
    }
}

void ASpreaderActor::DisengageTwistlocks()
{
    if (TwistlockSystem)
    {
        TwistlockSystem->DisengageTwistlocks();
    }
}

void ASpreaderActor::EmergencyRelease()
{
    if (TwistlockSystem)
    {
        TwistlockSystem->ForceDisengage();
    }
}

TArray<FVector> ASpreaderActor::GetWireRopeAttachWorldPositions() const
{
    TArray<FVector> Positions;
    for (USceneComponent* AttachPoint : WireRopeAttachPoints)
    {
        if (AttachPoint)
        {
            Positions.Add(AttachPoint->GetComponentLocation());
        }
    }
    return Positions;
}

float ASpreaderActor::CalculatePendulumPeriod() const
{
    float WireLength = 15000.0f;
    return 2.0f * PI * FMath::Sqrt(WireLength / GRAVITY);
}

void ASpreaderActor::UpdateSuspendedMass(float ContainerMass)
{
    TotalSuspendedMass = SpreaderMass + ContainerMass;

    float TotalMass = TotalSuspendedMass;
    SpreaderFrame->SetMassOverrideInKg(NAME_None, TotalMass);
    SpreaderFrame->UpdateMassProperties();

    UE_LOG(LogQuayCranePhysics, Log, TEXT("Suspended mass updated: %.1f kg (Spreader: %.1f + Container: %.1f)"),
        TotalMass, SpreaderMass, ContainerMass);
}

void ASpreaderActor::HandleContainerAttached(AActor* Container)
{
    bIsContainerAttached = true;

    bIsLockDamping = true;
    LockDampingTimer = LockDampingDuration;

    SpreaderFrame->SetLinearDamping(BaseLinearDamping + LockLinearDampingBoost);
    SpreaderFrame->SetAngularDamping(BaseAngularDamping + LockAngularDampingBoost);

    AContainerActor* ContainerActor = Cast<AContainerActor>(Container);
    if (ContainerActor)
    {
        ContainerActor->SetAttachedState(true);

        if (ContainerActor->ContainerMesh)
        {
            ContainerActor->ContainerMesh->SetPhysicsMaxAngularVelocityInDegrees(StabilizationAngularMaxVel);

            FBodyInstance* ContainerBody = ContainerActor->ContainerMesh->GetBodyInstance();
            if (ContainerBody)
            {
                ContainerBody->SolverIterationCount = 30;
                ContainerBody->SolverVelocityIterationCount = 15;
            }
        }

        UpdateSuspendedMass(ContainerActor->GetCurrentWeight());
    }

    OnContainerLocked(Container);
    UE_LOG(LogQuayCranePhysics, Log, TEXT("Spreader: Container ATTACHED - %s (lock damping engaged)"), *Container->GetName());
}

void ASpreaderActor::HandleContainerDetached()
{
    bIsContainerAttached = false;
    TotalSuspendedMass = SpreaderMass;
    SpreaderFrame->SetMassOverrideInKg(NAME_None, SpreaderMass);
    SpreaderFrame->SetLinearDamping(BaseLinearDamping);
    SpreaderFrame->SetAngularDamping(BaseAngularDamping);
    SpreaderFrame->SetPhysicsMaxAngularVelocityInDegrees(MaxAngularVelocity);
    SpreaderFrame->UpdateMassProperties();

    bIsLockDamping = false;

    OnContainerReleased();
    UE_LOG(LogQuayCranePhysics, Log, TEXT("Spreader: Container DETACHED"));
}
