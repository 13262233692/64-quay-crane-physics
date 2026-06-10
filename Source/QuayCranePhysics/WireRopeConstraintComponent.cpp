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
    UE_LOG(LogQuayCranePhysics, Log, TEXT("Wire rope initialized: %d segments, rest length %.1f, eff stiffness %.1f"),
        ChainSegments, WireRestLength, ComputeEffectiveStiffness());
}

float UWireRopeConstraintComponent::ComputeEffectiveStiffness() const
{
    if (WireRestLength < MIN_WIRE_LENGTH)
        return SpringStiffness * SoftStiffnessScale;

    if (WireRestLength < MinRestLengthForFullStiffness)
    {
        float Ratio = WireRestLength / MinRestLengthForFullStiffness;
        float Scale = SoftStiffnessScale + (1.0f - SoftStiffnessScale) * Ratio;
        return SpringStiffness * Scale;
    }

    return SpringStiffness;
}

void UWireRopeConstraintComponent::ConfigureConstraintStability(
    UPhysicsConstraintComponent* Constraint, float Stiffness, float Damping)
{
    if (!Constraint) return;

    Constraint->ConstraintInstance.SetLinearDriveParams(Stiffness, Damping, ForceLimit);
    Constraint->ConstraintInstance.SetAngularDriveParams(
        Stiffness * 0.1f, Damping * 0.1f, ForceLimit);
}

void UWireRopeConstraintComponent::BuildConstraintChain()
{
    ConstraintChain.Empty();
    RopeSegmentMeshes.Empty();

    AActor* Owner = GetOwner();
    if (!Owner || !AnchorComponent.IsValid() || !LoadComponent.IsValid())
        return;

    float SegmentLength = WireRestLength / static_cast<float>(ChainSegments);
    SegmentLength = FMath::Max(SegmentLength, 50.0f);

    EffectiveStiffness = ComputeEffectiveStiffness();
    bIsInSoftMode = (WireRestLength < MinRestLengthForFullStiffness);

    float SegmentDamping = DampingCoefficient;
    if (bIsInSoftMode)
    {
        SegmentDamping *= SegmentDampingBoostOnShortRope;
    }

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
        SegmentMesh->SetLinearDamping(bIsInSoftMode ? 1.0f * SegmentDampingBoostOnShortRope : 1.0f);
        SegmentMesh->SetAngularDamping(bIsInSoftMode ? 4.0f * SegmentDampingBoostOnShortRope : 4.0f);
        SegmentMesh->SetPhysicsMaxAngularVelocityInDegrees(MaxSegmentAngularVelocity);
        SegmentMesh->RegisterComponent();

        FBodyInstance* BodyInst = SegmentMesh->GetBodyInstance();
        if (BodyInst)
        {
            BodyInst->SolverIterationCount = 30;
            BodyInst->SolverVelocityIterationCount = 15;
            BodyInst->ContactOffset = ContactOffsetOverride;
        }

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
    RootConstraint->ConstraintSetup.LinearLimit.SoftConstraint = true;
    RootConstraint->ConstraintSetup.LinearLimit.Stiffness = EffectiveStiffness;
    RootConstraint->ConstraintSetup.LinearLimit.Damping = SegmentDamping;
    RootConstraint->ConstraintSetup.LinearLimit.Restitution = 0.0f;
    RootConstraint->ConstraintSetup.LinearLimit.ContactDistance = SegmentLength * 0.1f;

    RootConstraint->ConstraintSetup.AngularLimit.Swing1Limit = AngularSwingLimit;
    RootConstraint->ConstraintSetup.AngularLimit.Swing2Limit = AngularSwingLimit;
    RootConstraint->ConstraintSetup.AngularLimit.TwistLimit = AngularTwistLimit;

    RootConstraint->ConstraintSetup.AngularLimit.Swing1Motion = EAngularConstraintMotion::ACM_Limited;
    RootConstraint->ConstraintSetup.AngularLimit.Swing2Motion = EAngularConstraintMotion::ACM_Limited;
    RootConstraint->ConstraintSetup.AngularLimit.TwistMotion = EAngularConstraintMotion::ACM_Limited;
    RootConstraint->ConstraintSetup.AngularLimit.SoftConstraint = true;

    ConfigureConstraintStability(RootConstraint, EffectiveStiffness, SegmentDamping);

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
        SegmentConstraint->ConstraintSetup.LinearLimit.SoftConstraint = true;
        SegmentConstraint->ConstraintSetup.LinearLimit.Stiffness = EffectiveStiffness;
        SegmentConstraint->ConstraintSetup.LinearLimit.Damping = SegmentDamping;
        SegmentConstraint->ConstraintSetup.LinearLimit.Restitution = 0.0f;
        SegmentConstraint->ConstraintSetup.LinearLimit.ContactDistance = SegmentLength * 0.1f;

        SegmentConstraint->ConstraintSetup.AngularLimit.Swing1Limit = AngularSwingLimit;
        SegmentConstraint->ConstraintSetup.AngularLimit.Swing2Limit = AngularSwingLimit;
        SegmentConstraint->ConstraintSetup.AngularLimit.TwistLimit = AngularTwistLimit;

        SegmentConstraint->ConstraintSetup.AngularLimit.Swing1Motion = EAngularConstraintMotion::ACM_Limited;
        SegmentConstraint->ConstraintSetup.AngularLimit.Swing2Motion = EAngularConstraintMotion::ACM_Limited;
        SegmentConstraint->ConstraintSetup.AngularLimit.TwistMotion = EAngularConstraintMotion::ACM_Limited;
        SegmentConstraint->ConstraintSetup.AngularLimit.SoftConstraint = true;

        ConfigureConstraintStability(SegmentConstraint, EffectiveStiffness, SegmentDamping);

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
    EndConstraint->ConstraintSetup.LinearLimit.SoftConstraint = true;
    EndConstraint->ConstraintSetup.LinearLimit.Stiffness = EffectiveStiffness;
    EndConstraint->ConstraintSetup.LinearLimit.Damping = SegmentDamping;
    EndConstraint->ConstraintSetup.LinearLimit.Restitution = 0.0f;
    EndConstraint->ConstraintSetup.LinearLimit.ContactDistance = SegmentLength * 0.1f;

    EndConstraint->ConstraintSetup.AngularLimit.Swing1Limit = AngularSwingLimit;
    EndConstraint->ConstraintSetup.AngularLimit.Swing2Limit = AngularSwingLimit;
    EndConstraint->ConstraintSetup.AngularLimit.TwistLimit = AngularTwistLimit;

    EndConstraint->ConstraintSetup.AngularLimit.Swing1Motion = EAngularConstraintMotion::ACM_Limited;
    EndConstraint->ConstraintSetup.AngularLimit.Swing2Motion = EAngularConstraintMotion::ACM_Limited;
    EndConstraint->ConstraintSetup.AngularLimit.TwistMotion = EAngularConstraintMotion::ACM_Limited;
    EndConstraint->ConstraintSetup.AngularLimit.SoftConstraint = true;

    ConfigureConstraintStability(EndConstraint, EffectiveStiffness, SegmentDamping);

    EndConstraint->SetConstrainedComponents(
        RopeSegmentMeshes.Last(), FVector::ZeroVector,
        LoadComponent.Get(), LoadAttachOffset);

    ConstraintChain.Add(EndConstraint);
}

void UWireRopeConstraintComponent::ApplySoftConstraintMode(float DeltaTime)
{
    bool bShouldBeSoft = (WireRestLength < MinRestLengthForFullStiffness);

    if (bShouldBeSoft != bIsInSoftMode)
    {
        bIsInSoftMode = bShouldBeSoft;
        EffectiveStiffness = ComputeEffectiveStiffness();

        float TargetDamping = bIsInSoft ? DampingCoefficient * SegmentDampingBoostOnShortRope : DampingCoefficient;

        for (UPhysicsConstraintComponent* Constraint : ConstraintChain)
        {
            if (Constraint)
            {
                ConfigureConstraintStability(Constraint, EffectiveStiffness, TargetDamping);
            }
        }

        for (UStaticMeshComponent* Segment : RopeSegmentMeshes)
        {
            if (Segment)
            {
                Segment->SetLinearDamping(bIsInSoftMode ? 1.0f * SegmentDampingBoostOnShortRope : 1.0f);
                Segment->SetAngularDamping(bIsInSoftMode ? 4.0f * SegmentDampingBoostOnShortRope : 4.0f);
            }
        }

        UE_LOG(LogQuayCranePhysics, Log, TEXT("WireRope soft mode %s - EffStiffness=%.1f, RestLen=%.1f"),
            bIsInSoftMode ? TEXT("ENGAGED") : TEXT("DISENGAGED"), EffectiveStiffness, WireRestLength);
    }
}

void UWireRopeConstraintComponent::ClampSegmentVelocities()
{
    for (UStaticMeshComponent* Segment : RopeSegmentMeshes)
    {
        if (!Segment || !Segment->IsSimulatingPhysics())
            continue;

        FVector LinVel = Segment->GetPhysicsLinearVelocity();
        FVector AngVel = Segment->GetPhysicsAngularVelocityInDegrees();

        float LinSpeed = LinVel.Size();
        float AngSpeed = AngVel.Size();

        if (FMath::IsNaN(LinSpeed) || FMath::IsNaN(AngSpeed))
        {
            Segment->SetPhysicsLinearVelocity(FVector::ZeroVector);
            Segment->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
            continue;
        }

        if (LinSpeed > MaxSegmentLinearVelocity)
        {
            Segment->SetPhysicsLinearVelocity(LinVel.GetSafeNormal() * MaxSegmentLinearVelocity, false);
        }

        if (AngSpeed > MaxSegmentAngularVelocity)
        {
            Segment->SetPhysicsAngularVelocityInDegrees(AngVel.GetSafeNormal() * MaxSegmentAngularVelocity, false);
        }
    }

    if (LoadComponent.IsValid() && LoadComponent->IsSimulatingPhysics())
    {
        FVector LoadLinVel = LoadComponent->GetPhysicsLinearVelocity();
        float LoadLinSpeed = LoadLinVel.Size();

        if (FMath::IsNaN(LoadLinSpeed))
        {
            LoadComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
            LoadComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        }
        else if (LoadLinSpeed > MaxSegmentLinearVelocity)
        {
            LoadComponent->SetPhysicsLinearVelocity(LoadLinVel.GetSafeNormal() * MaxSegmentLinearVelocity, false);
        }
    }
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

    if (RopeExtensionRatio > MaxExtensionRatio)
    {
        RopeExtensionRatio = MaxExtensionRatio;
        if (LoadComponent->IsSimulatingPhysics())
        {
            FVector Direction = Delta.GetSafeNormal();
            FVector TargetPos = AnchorWorldPos + Direction * (WireRestLength * MaxExtensionRatio);
            FVector Correction = TargetPos - LoadWorldPos;
            LoadComponent->SetPhysicsLinearVelocity(
                LoadComponent->GetPhysicsLinearVelocity() + Correction * 0.5f, false);
        }
    }

    CurrentTension = FMath::Min(CalculateTension(), MaxTensionClamp);

    if (FMath::IsNaN(CurrentTension))
    {
        CurrentTension = 0.0f;
        UE_LOG(LogQuayCranePhysics, Error, TEXT("WireRope: NaN tension detected and zeroed"));
    }

    if (bEnableBreaking && CurrentTension > BreakThreshold)
    {
        BreakRope();
        return;
    }

    EffectiveStiffness = ComputeEffectiveStiffness();

    for (UPhysicsConstraintComponent* Constraint : ConstraintChain)
    {
        if (Constraint)
        {
            ConfigureConstraintStability(Constraint, EffectiveStiffness, DampingCoefficient);
        }
    }
}

float UWireRopeConstraintComponent::CalculateTension() const
{
    if (CurrentWireLength <= WireRestLength)
        return 0.0f;

    float ClampedExtension = FMath::Min(
        CurrentWireLength - WireRestLength,
        WireRestLength * (MaxExtensionRatio - 1.0f));

    if (!AnchorComponent.IsValid() || !LoadComponent.IsValid())
        return 0.0f;

    FVector AnchorWorldPos = AnchorComponent->GetComponentLocation() + AnchorAttachOffset;
    FVector LoadWorldPos = LoadComponent->GetComponentLocation() + LoadAttachOffset;
    FVector Direction = (LoadWorldPos - AnchorWorldPos).GetSafeNormal();

    if (Direction.IsNearlyZero())
        return 0.0f;

    FVector RelativeVelocity = LoadComponent->GetPhysicsLinearVelocity() - AnchorComponent->GetPhysicsLinearVelocity();
    float VelocityAlongRope = FVector::DotProduct(RelativeVelocity, Direction);

    float EffectiveK = ComputeEffectiveStiffness();

    float SpringForce = EffectiveK * ClampedExtension;
    float DampingForce = DampingCoefficient * VelocityAlongRope;

    float TotalForce = SpringForce + DampingForce;

    TotalForce = FMath::Clamp(TotalForce, 0.0f, MaxTensionClamp);

    return TotalForce;
}

void UWireRopeConstraintComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    ApplySoftConstraintMode(DeltaTime);
    UpdateSpringDamperForces(DeltaTime);
    ClampSegmentVelocities();
}

void UWireRopeConstraintComponent::SetSpringDamperParams(float InStiffness, float InDamping, float InWireLength)
{
    SpringStiffness = InStiffness;
    DampingCoefficient = InDamping;
    WireRestLength = FMath::Max(InWireLength, MIN_WIRE_LENGTH);
    EffectiveStiffness = ComputeEffectiveStiffness();
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
            Constraint->ConstraintInstance.SetLinearDriveParams(EffectiveStiffness, DampingCoefficient, ForceLimit);
        }
    }

    if (AnchorComponent.IsValid() && LoadComponent.IsValid())
    {
        BuildConstraintChain();
    }

    UE_LOG(LogQuayCranePhysics, Log, TEXT("Wire rope repaired."));
}
