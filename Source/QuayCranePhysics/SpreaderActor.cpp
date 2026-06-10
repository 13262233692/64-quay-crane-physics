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
    SpreaderFrame->SetLinearDamping(0.3f);
    SpreaderFrame->SetAngularDamping(2.0f);
    SpreaderFrame->SetNotifyRigidBodyCollision(true);

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
    SpreaderFrame->SetLinearDamping(0.3f);
    SpreaderFrame->SetAngularDamping(2.0f);
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

    AContainerActor* ContainerActor = Cast<AContainerActor>(Container);
    if (ContainerActor)
    {
        ContainerActor->SetAttachedState(true);
        UpdateSuspendedMass(ContainerActor->GetCurrentWeight());
    }

    OnContainerLocked(Container);
    UE_LOG(LogQuayCranePhysics, Log, TEXT("Spreader: Container ATTACHED - %s"), *Container->GetName());
}

void ASpreaderActor::HandleContainerDetached()
{
    bIsContainerAttached = false;
    TotalSuspendedMass = SpreaderMass;
    SpreaderFrame->SetMassOverrideInKg(NAME_None, SpreaderMass);
    SpreaderFrame->UpdateMassProperties();

    OnContainerReleased();
    UE_LOG(LogQuayCranePhysics, Log, TEXT("Spreader: Container DETACHED"));
}
