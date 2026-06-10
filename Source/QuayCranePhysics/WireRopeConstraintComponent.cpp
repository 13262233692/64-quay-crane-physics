#include "WireRopeConstraintComponent.h"
#include "QuayCranePhysics.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Chaos/ChaosEngineInterface.h"

UWireRopeConstraintComponent::UWireRopeConstraintComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UWireRopeConstraintComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UWireRopeConstraintComponent::InitializeRope(
    UPrimitiveComponent* InAnchor,
    UPrimitiveComponent* InLoad,
    const FVector& InAnchorOffset,
    const FVector& InLoadOffset)
{
    if (!InAnchor || !InLoad)
    {
        UE_LOG(LogQuayCranePhysics, Error, TEXT("InitializeRope: Null anchor or load component"));
        return;
    }

    AnchorComponent = InAnchor;
    LoadComponent = InLoad;
    AnchorAttachOffset = InAnchorOffset;
    LoadAttachOffset = InLoadOffset;

    BuildConstraintChain();
    UE_LOG(LogQuayCranePhysics, Log, TEXT("Wire rope initialized: %d segments, rest length %.1f"),
        ChainSegments, WireRestLength);
}

void UWireRopeConstraintComponent::BuildConstraintChain()
{
    ConstraintChain.Empty();
    RopeSegmentMeshes.Empty();

    AActor* Owner = GetOwner();
    if (!Owner || !AnchorComponent.IsValid() || !LoadComponent.IsValid())
        return;

    float SegmentLength = WireRestLength / static_cast<float>(ChainSegments);

    for (int32 i = 0; i < ChainSegments; ++i)
    {
        UStaticMeshComponent* SegmentMesh = NewObject<UStaticMeshComponent>(Owner,
            *FString::Printf(TEXT("RopeSegment_%d_%s"), i, *GetName()));
        SegmentMesh->SetupAttachment(Owner->GetRootComponent());
        SegmentMesh->SetMobility(EComponentMobility::Movable);
        SegmentMesh->SetSimulatePhysics(true);
        SegmentMesh->SetEnableGravity(true);
        SegmentMesh->SetMassOverrideInKg(NAME_None, ROPE_SEGMENT_MASS);
        SegmentMesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
        SegmentMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
        SegmentMesh->SetLinearDamping(0.5f);
        SegmentMesh->SetAngularDamping(2.0f);
        SegmentMesh->RegisterComponent();

        RopeSegmentMeshes.Add(SegmentMesh);
    }

    UPhysicsConstraintComponent* RootConstraint = NewObject<UPhysicsConstraintComponent>(Owner,
        *FString::Printf(TEXT("RopeRootConstraint_%s"), *GetName()));
    RootConstraint->SetupAttachment(Owner->GetRootComponent());
    RootConstraint->RegisterComponent();

    RootConstraint->ConstraintSetup.LinearLimit.XMotion = ELinearConstraintMotion::LCM_Limited;
    RootConstraint->ConstraintSetup.LinearLimit.YMotion = ELinearConstraintMotion::LCM_Limited;
    RootConstraint->ConstraintSetup.LinearLimit.ZMotion = ELinearConstraintMotion::LCM_Limited;
    RootConstraint->ConstraintSetup.LinearLimit.Limit = SegmentLength;

    RootConstraint->ConstraintSetup.AngularLimit.Swing1Limit = AngularSwingLimit;
    RootConstraint->ConstraintSetup.AngularLimit.Swing2Limit = AngularSwingLimit;
    RootConstraint->ConstraintSetup.AngularLimit.TwistLimit = AngularTwistLimit;

    RootConstraint->ConstraintSetup.AngularLimit.Swing1Motion = EAngularConstraintMotion::ACM_Limited;
    RootConstraint->ConstraintSetup.AngularLimit.Swing2Motion = EAngularConstraintMotion::ACM_Limited;
    RootConstraint->ConstraintSetup.AngularLimit.TwistMotion = EAngularConstraintMotion::ACM_Limited;

    RootConstraint->ConstraintInstance.SetLinearDriveParams(SpringStiffness, DampingCoefficient, ForceLimit);
    RootConstraint->ConstraintInstance.SetAngularDriveParams(SpringStiffness * 0.1f, DampingCoefficient * 0.1f, ForceLimit);

    RootConstraint->SetConstrainedComponents(
        AnchorComponent.Get(), AnchorAttachOffset,
        RopeSegmentMeshes[0], FVector::ZeroVector);

    ConstraintChain.Add(RootConstraint);

    for (int32 i = 0; i < ChainSegments - 1; ++i)
    {
        UPhysicsConstraintComponent* SegmentConstraint = NewObject<UPhysicsConstraintComponent>(Owner,
            *FString::Printf(TEXT("RopeSegmentConstraint_%d_%s"), i, *GetName()));
        SegmentConstraint->SetupAttachment(Owner->GetRootComponent());
        SegmentConstraint->RegisterComponent();

        SegmentConstraint->ConstraintSetup.LinearLimit.XMotion = ELinearConstraintMotion::LCM_Limited;
        SegmentConstraint->ConstraintSetup.LinearLimit.YMotion = ELinearConstraintMotion::LCM_Limited;
        SegmentConstraint->ConstraintSetup.LinearLimit.ZMotion = ELinearConstraintMotion::LCM_Limited;
        SegmentConstraint->ConstraintSetup.LinearLimit.Limit = SegmentLength;

        SegmentConstraint->ConstraintSetup.AngularLimit.Swing1Limit = AngularSwingLimit;
        SegmentConstraint->ConstraintSetup.AngularLimit.Swing2Limit = AngularSwingLimit;
        SegmentConstraint->ConstraintSetup.AngularLimit.TwistLimit = AngularTwistLimit;

        SegmentConstraint->ConstraintSetup.AngularLimit.Swing1Motion = EAngularConstraintMotion::ACM_Limited;
        SegmentConstraint->ConstraintSetup.AngularLimit.Swing2Motion = EAngularConstraintMotion::ACM_Limited;
        SegmentConstraint->ConstraintSetup.AngularLimit.TwistMotion = EAngularConstraintMotion::ACM_Limited;

        SegmentConstraint->ConstraintInstance.SetLinearDriveParams(SpringStiffness, DampingCoefficient, ForceLimit);
        SegmentConstraint->ConstraintInstance.SetAngularDriveParams(SpringStiffness * 0.1f, DampingCoefficient * 0.1f, ForceLimit);

        SegmentConstraint->SetConstrainedComponents(
            RopeSegmentMeshes[i], FVector::ZeroVector,
            RopeSegmentMeshes[i + 1], FVector::ZeroVector);

        ConstraintChain.Add(SegmentConstraint);
    }

    UPhysicsConstraintComponent* EndConstraint = NewObject<UPhysicsConstraintComponent>(Owner,
        *FString::Printf(TEXT("RopeEndConstraint_%s"), *GetName()));
    EndConstraint->SetupAttachment(Owner->GetRootComponent());
    EndConstraint->RegisterComponent();

    EndConstraint->ConstraintSetup.LinearLimit.XMotion = ELinearConstraintMotion::LCM_Limited;
    EndConstraint->ConstraintSetup.LinearLimit.YMotion = ELinearConstraintMotion::LCM_Limited;
    EndConstraint->ConstraintSetup.LinearLimit.ZMotion = ELinearConstraintMotion::LCM_Limited;
    EndConstraint->ConstraintSetup.LinearLimit.Limit = SegmentLength;

    EndConstraint->ConstraintSetup.AngularLimit.Swing1Limit = AngularSwingLimit;
    EndConstraint->ConstraintSetup.AngularLimit.Swing2Limit = AngularSwingLimit;
    EndConstraint->ConstraintSetup.AngularLimit.TwistLimit = AngularTwistLimit;

    EndConstraint->ConstraintSetup.AngularLimit.Swing1Motion = EAngularConstraintMotion::ACM_Limited;
    EndConstraint->ConstraintSetup.AngularLimit.Swing2Motion = EAngularConstraintMotion::ACM_Limited;
    EndConstraint->ConstraintSetup.AngularLimit.TwistMotion = EAngularConstraintMotion::ACM_Limited;

    EndConstraint->ConstraintInstance.SetLinearDriveParams(SpringStiffness, DampingCoefficient, ForceLimit);
    EndConstraint->ConstraintInstance.SetAngularDriveParams(SpringStiffness * 0.1f, DampingCoefficient * 0.1f, ForceLimit);

    EndConstraint->SetConstrainedComponents(
        RopeSegmentMeshes.Last(), FVector::ZeroVector,
        LoadComponent.Get(), LoadAttachOffset);

    ConstraintChain.Add(EndConstraint);
}

void UWireRopeConstraintComponent::UpdateSpringDamperForces(float DeltaTime)
{
    if (!AnchorComponent.IsValid() || !LoadComponent.IsValid() || bIsBroken)
        return;

    FVector AnchorWorldPos = AnchorComponent->GetComponentLocation() + AnchorAttachOffset;
    FVector LoadWorldPos = LoadComponent->GetComponentLocation() + LoadAttachOffset;

    FVector Delta = LoadWorldPos - AnchorWorldPos;
    CurrentWireLength = Delta.Size();
    RopeExtensionRatio = CurrentWireLength / FMath::Max(WireRestLength, MIN_WIRE_LENGTH);

    CurrentTension = CalculateTension();

    if (bEnableBreaking && CurrentTension > BreakThreshold)
    {
        BreakRope();
        return;
    }

    for (UPhysicsConstraintComponent* Constraint : ConstraintChain)
    {
        if (Constraint)
        {
            Constraint->ConstraintInstance.SetLinearDriveParams(SpringStiffness, DampingCoefficient, ForceLimit);
            Constraint->ConstraintInstance.SetAngularDriveParams(
                SpringStiffness * 0.1f, DampingCoefficient * 0.1f, ForceLimit);
        }
    }
}

float UWireRopeConstraintComponent::CalculateTension() const
{
    if (CurrentWireLength <= WireRestLength)
        return 0.0f;

    float Extension = CurrentWireLength - WireRestLength;

    if (!AnchorComponent.IsValid() || !LoadComponent.IsValid())
        return 0.0f;

    FVector AnchorWorldPos = AnchorComponent->GetComponentLocation() + AnchorAttachOffset;
    FVector LoadWorldPos = LoadComponent->GetComponentLocation() + LoadAttachOffset;
    FVector Direction = (LoadWorldPos - AnchorWorldPos).GetSafeNormal();

    FVector RelativeVelocity = LoadComponent->GetPhysicsLinearVelocity() - AnchorComponent->GetPhysicsLinearVelocity();
    float VelocityAlongRope = FVector::DotProduct(RelativeVelocity, Direction);

    float SpringForce = SpringStiffness * Extension;
    float DampingForce = DampingCoefficient * VelocityAlongRope;

    return FMath::Max(SpringForce + DampingForce, 0.0f);
}

void UWireRopeConstraintComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UpdateSpringDamperForces(DeltaTime);
}

void UWireRopeConstraintComponent::SetSpringDamperParams(float InStiffness, float InDamping, float InWireLength)
{
    SpringStiffness = InStiffness;
    DampingCoefficient = InDamping;
    WireRestLength = FMath::Max(InWireLength, MIN_WIRE_LENGTH);
}

void UWireRopeConstraintComponent::BreakRope()
{
    if (bIsBroken)
        return;

    bIsBroken = true;
    CurrentTension = 0.0f;

    for (UPhysicsConstraintComponent* Constraint : ConstraintChain)
    {
        if (Constraint)
        {
            Constraint->BreakConstraint();
        }
    }

    UE_LOG(LogQuayCranePhysics, Warning, TEXT("Wire rope BROKEN! Tension exceeded threshold."));
}

void UWireRopeConstraintComponent::RepairRope()
{
    if (!bIsBroken)
        return;

    bIsBroken = false;

    for (UPhysicsConstraintComponent* Constraint : ConstraintChain)
    {
        if (Constraint)
        {
            Constraint->ConstraintInstance.SetLinearDriveParams(SpringStiffness, DampingCoefficient, ForceLimit);
        }
    }

    if (AnchorComponent.IsValid() && LoadComponent.IsValid())
    {
        BuildConstraintChain();
    }

    UE_LOG(LogQuayCranePhysics, Log, TEXT("Wire rope repaired."));
}
