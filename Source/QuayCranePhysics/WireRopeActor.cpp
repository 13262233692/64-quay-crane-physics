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

        WireRopes.Add(Rope);
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

    for (int32 i = 0; i < ROPE_COUNT && i < WireRopes.Num(); ++i)
    {
        if (WireRopes[i])
        {
            FVector TrolleyOffset = (i < TrolleyAnchorOffsets.Num()) ? TrolleyAnchorOffsets[i] : FVector::ZeroVector;
            FVector SpreaderOffset = (i < SpreaderAttachOffsets.Num()) ? SpreaderAttachOffsets[i] : FVector::ZeroVector;

            WireRopes[i]->SetSpringDamperParams(DefaultStiffness, DefaultDamping, RopeLength);
            WireRopes[i]->InitializeRope(TrolleyRoot, SpreaderRoot, TrolleyOffset, SpreaderOffset);
        }
    }

    UE_LOG(LogQuayCranePhysics, Log, TEXT("Wire ropes connected: HoistLength=%.1f, RopeLength=%.1f"),
        HoistLength, RopeLength);
}

void AWireRopeActor::UpdateHoistLength(float NewHoistLength)
{
    LastKnownHoistLength = NewHoistLength;
    float RopeLength = NewHoistLength / static_cast<float>(ROPE_COUNT);

    for (UWireRopeConstraintComponent* Rope : WireRopes)
    {
        if (Rope && !Rope->bIsBroken)
        {
            Rope->SetSpringDamperParams(Rope->SpringStiffness, Rope->DampingCoefficient, RopeLength);
        }
    }
}

void AWireRopeActor::SetSpringDamperParams(float Stiffness, float Damping)
{
    for (UWireRopeConstraintComponent* Rope : WireRopes)
    {
        if (Rope && !Rope->bIsBroken)
        {
            Rope->SetSpringDamperParams(Stiffness, Damping, Rope->WireRestLength);
        }
    }
}

void AWireRopeActor::RepairAllRopes()
{
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
    float NewStiffness = FMath::Max(DefaultStiffness, GravityForce * 0.5f);

    for (UWireRopeConstraintComponent* Rope : WireRopes)
    {
        if (Rope)
        {
            Rope->BreakThreshold = NewBreakThreshold;
            Rope->SpringStiffness = NewStiffness;
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

            TotalTension += RopeTensions[i];
            MaxTension = FMath::Max(MaxTension, RopeTensions[i]);
            MinTension = FMath::Min(MinTension, RopeTensions[i]);
        }
    }

    if (MinTension == TNumericLimits<float>::Max())
    {
        MinTension = 0.0f;
    }
}

void AWireRopeActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UpdateTensionMetrics();
}
