#include "WireRopeActor.h"
#include "QuayCranePhysics.h"
#include "Components/StaticMeshComponent.h"

AWireRopeActor::AWireRopeActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;

    RopeTensions.SetNum(ROPE_COUNT);
    RopeLengths.SetNum(ROPE_COUNT);
    RopeBrokenStates.SetNum(ROPE_COUNT);

    for (int32 i = 0; i < ROPE_COUNT; ++i)
    {
        RopeTensions[i] = 0.0f;
        RopeLengths[i] = 0.0f;
        RopeBrokenStates[i] = false;
    }
}

void AWireRopeActor::BeginPlay()
{
    Super::BeginPlay();
    CreateWireRopes();
}

void AWireRopeActor::CreateWireRopes()
{
    WireRopes.Empty();

    for (int32 i = 0; i < ROPE_COUNT; ++i)
    {
        UWireRopeConstraintComponent* Rope = NewObject<UWireRopeConstraintComponent>(this,
            *FString::Printf(TEXT("WireRope_%d"), i));
        Rope->RegisterComponent();

        Rope->SpringStiffness = DefaultStiffness;
        Rope->DampingCoefficient = DefaultDamping;
        Rope->WireRestLength = DefaultRestLength;
        Rope->BreakThreshold = DefaultBreakThreshold;
        Rope->bEnableBreaking = true;
        Rope->ChainSegments = SegmentsPerRope;
        Rope->AngularSwingLimit = AngularSwingLimit;
        Rope->AngularTwistLimit = AngularTwistLimit;
        Rope->MaxTensionClamp = PerRopeMaxTensionClamp;

        WireRopes.Add(Rope);
    }
}

void AWireRopeActor::ComputeSolverSafeStiffness()
{
    float BaseStiffness = DefaultStiffness;

    if (LastKnownHoistLength < ShortHoistLengthThreshold)
    {
        float Ratio = LastKnownHoistLength / ShortHoistLengthThreshold;
        float Scale = ShortHoistStiffnessScale + (1.0f - ShortHoistStiffnessScale) * Ratio;
        BaseStiffness *= Scale;
    }

    if (bIsInEmergencyRelief)
    {
        BaseStiffness *= EmergencyStiffnessReductionFactor;
    }

    CurrentEffectiveStiffness = BaseStiffness;

    for (UWireRopeConstraintComponent* Rope : WireRopes)
    {
        if (Rope && !Rope->bIsBroken)
        {
            Rope->SetSpringDamperParams(CurrentEffectiveStiffness, Rope->DampingCoefficient, Rope->WireRestLength);
        }
    }
}

void AWireRopeActor::CheckEmergencyTensionRelief()
{
    if (TotalTension > EmergencyTensionReliefThreshold && !bIsInEmergencyRelief)
    {
        bIsInEmergencyRelief = true;
        ComputeSolverSafeStiffness();

        for (UWireRopeConstraintComponent* Rope : WireRopes)
        {
            if (Rope && !Rope->bIsBroken)
            {
                Rope->DampingCoefficient = DefaultDamping * 3.0f;
            }
        }

        UE_LOG(LogQuayCranePhysics, Error, TEXT("EMERGENCY TENSION RELIEF: TotalTension=%.1f > Threshold=%.1f, Stiffness reduced to %.1f"),
            TotalTension, EmergencyTensionReliefThreshold, CurrentEffectiveStiffness);
    }
    else if (bIsInEmergencyRelief && TotalTension < EmergencyTensionReliefThreshold * 0.5f)
    {
        bIsInEmergencyRelief = false;
        ComputeSolverSafeStiffness();

        for (UWireRopeConstraintComponent* Rope : WireRopes)
        {
            if (Rope && !Rope->bIsBroken)
            {
                Rope->DampingCoefficient = DefaultDamping;
            }
        }

        UE_LOG(LogQuayCranePhysics, Log, TEXT("Emergency tension relief DISENGAGED - tension stabilized at %.1f"), TotalTension);
    }

    if (TotalTension > GlobalMaxTension)
    {
        for (UWireRopeConstraintComponent* Rope : WireRopes)
        {
            if (Rope && !Rope->bIsBroken)
            {
                Rope->CurrentTension = FMath::Min(Rope->CurrentTension, GlobalMaxTension / static_cast<float>(ROPE_COUNT));
            }
        }
    }
}

void AWireRopeActor::ConnectTrolleyToSpreader(
    UPrimitiveComponent* TrolleyRoot,
    UPrimitiveComponent* SpreaderRoot,
    const TArray<FVector>& TrolleyAnchorOffsets,
    const TArray<FVector>& SpreaderAttachOffsets,
    float HoistLength)
{
    if (!TrolleyRoot || !SpreaderRoot)
    {
        UE_LOG(LogQuayCranePhysics, Error, TEXT("ConnectTrolleyToSpreader: Null root components"));
        return;
    }

    CachedTrolleyRoot = TrolleyRoot;
    CachedSpreaderRoot = SpreaderRoot;
    CachedTrolleyOffsets = TrolleyAnchorOffsets;
    CachedSpreaderOffsets = SpreaderAttachOffsets;
    LastKnownHoistLength = HoistLength;

    float RopeLength = HoistLength / static_cast<float>(ROPE_COUNT);
    RopeLength = FMath::Max(RopeLength, 50.0f);

    ComputeSolverSafeStiffness();

    for (int32 i = 0; i < ROPE_COUNT && i < WireRopes.Num(); ++i)
    {
        if (WireRopes[i])
        {
            FVector TrolleyOffset = (i < TrolleyAnchorOffsets.Num()) ? TrolleyAnchorOffsets[i] : FVector::ZeroVector;
            FVector SpreaderOffset = (i < SpreaderAttachOffsets.Num()) ? SpreaderAttachOffsets[i] : FVector::ZeroVector;

            WireRopes[i]->SetSpringDamperParams(CurrentEffectiveStiffness, DefaultDamping, RopeLength);
            WireRopes[i]->InitializeRope(TrolleyRoot, SpreaderRoot, TrolleyOffset, SpreaderOffset);
        }
    }

    UE_LOG(LogQuayCranePhysics, Log, TEXT("Wire ropes connected: HoistLength=%.1f, RopeLength=%.1f, EffStiffness=%.1f"),
        HoistLength, RopeLength, CurrentEffectiveStiffness);
}

void AWireRopeActor::UpdateHoistLength(float NewHoistLength)
{
    LastKnownHoistLength = NewHoistLength;
    float RopeLength = NewHoistLength / static_cast<float>(ROPE_COUNT);
    RopeLength = FMath::Max(RopeLength, 50.0f);

    ComputeSolverSafeStiffness();

    for (UWireRopeConstraintComponent* Rope : WireRopes)
    {
        if (Rope && !Rope->bIsBroken)
        {
            Rope->SetSpringDamperParams(CurrentEffectiveStiffness, Rope->DampingCoefficient, RopeLength);
        }
    }
}

void AWireRopeActor::SetSpringDamperParams(float Stiffness, float Damping)
{
    DefaultStiffness = Stiffness;
    DefaultDamping = Damping;
    ComputeSolverSafeStiffness();

    for (UWireRopeConstraintComponent* Rope : WireRopes)
    {
        if (Rope && !Rope->bIsBroken)
        {
            Rope->SetSpringDamperParams(CurrentEffectiveStiffness, DefaultDamping, Rope->WireRestLength);
        }
    }
}

void AWireRopeActor::RepairAllRopes()
{
    bIsInEmergencyRelief = false;

    for (int32 i = 0; i < WireRopes.Num(); ++i)
    {
        if (WireRopes[i])
        {
            WireRopes[i]->RepairRope();
            RopeBrokenStates[i] = false;
        }
    }

    if (CachedTrolleyRoot.IsValid() && CachedSpreaderRoot.IsValid())
    {
        ConnectTrolleyToSpreader(
            CachedTrolleyRoot.Get(),
            CachedSpreaderRoot.Get(),
            CachedTrolleyOffsets,
            CachedSpreaderOffsets,
            LastKnownHoistLength);
    }
}

float AWireRopeActor::GetRopeTension(EWireRopePosition Position) const
{
    int32 Index = static_cast<int32>(Position);
    if (Index >= 0 && Index < RopeTensions.Num())
    {
        return RopeTensions[Index];
    }
    return 0.0f;
}

bool AWireRopeActor::IsRopeBroken(EWireRopePosition Position) const
{
    int32 Index = static_cast<int32>(Position);
    if (Index >= 0 && Index < RopeBrokenStates.Num())
    {
        return RopeBrokenStates[Index];
    }
    return false;
}

void AWireRopeActor::UpdatePhysicsForLoad(float TotalLoadMass)
{
    float GravityForce = TotalLoadMass * 980.665f;
    float PerRopeForce = GravityForce / static_cast<float>(ROPE_COUNT);

    float SafetyFactor = 2.5f;
    float NewBreakThreshold = PerRopeForce * SafetyFactor * 10.0f;
    NewBreakThreshold = FMath::Min(NewBreakThreshold, PerRopeMaxTensionClamp);

    ComputeSolverSafeStiffness();

    for (UWireRopeConstraintComponent* Rope : WireRopes)
    {
        if (Rope)
        {
            Rope->BreakThreshold = NewBreakThreshold;
            Rope->MaxTensionClamp = PerRopeMaxTensionClamp;
            Rope->SpringStiffness = CurrentEffectiveStiffness;
        }
    }
}

TArray<FVector> AWireRopeActor::GetRopeSegmentPositions(int32 RopeIndex) const
{
    TArray<FVector> Positions;

    if (RopeIndex >= 0 && RopeIndex < WireRopes.Num() && WireRopes[RopeIndex])
    {
        for (UStaticMeshComponent* Segment : WireRopes[RopeIndex]->RopeSegmentMeshes)
        {
            if (Segment)
            {
                Positions.Add(Segment->GetComponentLocation());
            }
        }
    }

    return Positions;
}

void AWireRopeActor::UpdateTensionMetrics()
{
    TotalTension = 0.0f;
    MaxTension = 0.0f;
    MinTension = TNumericLimits<float>::Max();

    for (int32 i = 0; i < WireRopes.Num() && i < ROPE_COUNT; ++i)
    {
        if (WireRopes[i])
        {
            RopeTensions[i] = WireRopes[i]->CurrentTension;
            RopeLengths[i] = WireRopes[i]->CurrentWireLength;
            RopeBrokenStates[i] = WireRopes[i]->bIsBroken;

            if (FMath::IsNaN(RopeTensions[i]))
            {
                RopeTensions[i] = 0.0f;
            }

            TotalTension += RopeTensions[i];
            MaxTension = FMath::Max(MaxTension, RopeTensions[i]);
            MinTension = FMath::Min(MinTension, RopeTensions[i]);
        }
    }

    if (MinTension == TNumericLimits<float>::Max())
    {
        MinTension = 0.0f;
    }

    TotalTension = FMath::Clamp(TotalTension, 0.0f, GlobalMaxTension);
}

void AWireRopeActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UpdateTensionMetrics();
    CheckEmergencyTensionRelief();
}
