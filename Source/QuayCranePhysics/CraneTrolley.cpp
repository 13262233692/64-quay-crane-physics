#include "CraneTrolley.h"
#include "QuayCranePhysics.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

ACraneTrolley::ACraneTrolley()
{
    PrimaryActorTick.bCanEverTick = true;

    TrolleyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrolleyMesh"));
    SetRootComponent(TrolleyMesh);

    TrolleyMesh->SetSimulatePhysics(true);
    TrolleyMesh->SetEnableGravity(true);
    TrolleyMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TrolleyMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    TrolleyMesh->SetMassOverrideInKg(NAME_None, TrolleyMass);
    TrolleyMesh->SetLinearDamping(5.0f);
    TrolleyMesh->SetAngularDamping(50.0f);
    TrolleyMesh->SetNotifyRigidBodyCollision(true);
    TrolleyMesh->SetPhysicsMaxAngularVelocityInDegrees(MaxPhysicsAngularVelocity);

    TrolleyMesh->GetBodyInstance()->bLockRotation = true;
    TrolleyMesh->GetBodyInstance()->bLockXRotation = true;
    TrolleyMesh->GetBodyInstance()->bLockYRotation = true;
    TrolleyMesh->GetBodyInstance()->bLockZRotation = true;
    TrolleyMesh->GetBodyInstance()->SolverIterationCount = 30;
    TrolleyMesh->GetBodyInstance()->SolverVelocityIterationCount = 15;

    for (int32 i = 0; i < 4; ++i)
    {
        USceneComponent* Anchor = CreateDefaultSubobject<USceneComponent>(
            *FString::Printf(TEXT("RopeAnchor_%d"), i));
        Anchor->SetupAttachment(TrolleyMesh);
        WireRopeAnchorPoints.Add(Anchor);
    }
}

void ACraneTrolley::BeginPlay()
{
    Super::BeginPlay();
    SetupRopeAnchorPoints();
    TrolleyMesh->SetMassOverrideInKg(NAME_None, TrolleyMass);
    TrolleyMesh->SetPhysicsMaxAngularVelocityInDegrees(MaxPhysicsAngularVelocity);
}

void ACraneTrolley::SetupRopeAnchorPoints()
{
    float HalfSpreadX = RopeAnchorSpreadX * 0.5f;
    float HalfSpreadY = RopeAnchorSpreadY * 0.5f;

    FVector AnchorOffsets[4] = {
        FVector( HalfSpreadX, -HalfSpreadY, -100.0f),
        FVector( HalfSpreadX,  HalfSpreadY, -100.0f),
        FVector(-HalfSpreadX, -HalfSpreadY, -100.0f),
        FVector(-HalfSpreadX,  HalfSpreadY, -100.0f),
    };

    for (int32 i = 0; i < 4; ++i)
    {
        if (WireRopeAnchorPoints[i])
        {
            WireRopeAnchorPoints[i]->SetRelativeLocation(AnchorOffsets[i]);
        }
    }
}

void ACraneTrolley::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdateTravelMovement(DeltaTime);
    UpdateHoistMovement(DeltaTime);
    ClampPhysicsVelocities();

    TravelPosition = GetActorLocation().X;
    CurrentVelocity = TrolleyMesh->GetPhysicsLinearVelocity();
}

void ACraneTrolley::UpdateTravelMovement(float DeltaTime)
{
    PreviousTravelSpeed = CurrentTravelSpeed;

    if (FMath::Abs(TravelInput) < 0.01f)
    {
        if (FMath::Abs(CurrentTravelSpeed) > 1.0f)
        {
            float EffectiveDecel = Deceleration;
            float SpeedRatio = FMath::Abs(CurrentTravelSpeed) / MaxTravelSpeed;
            EffectiveDecel *= (1.0f + ProgressiveDecelFactor * (1.0f - SpeedRatio));

            float Decel = EffectiveDecel * DeltaTime;
            if (CurrentTravelSpeed > 0.0f)
                CurrentTravelSpeed = FMath::Max(0.0f, CurrentTravelSpeed - Decel);
            else
                CurrentTravelSpeed = FMath::Min(0.0f, CurrentTravelSpeed + Decel);
        }
        else
        {
            CurrentTravelSpeed = 0.0f;
        }
    }
    else
    {
        float TargetSpeed = TravelInput * MaxTravelSpeed;
        float SpeedDelta = Acceleration * DeltaTime;

        if (FMath::Abs(TargetSpeed - CurrentTravelSpeed) < SpeedDelta)
        {
            CurrentTravelSpeed = TargetSpeed;
        }
        else if (TargetSpeed > CurrentTravelSpeed)
        {
            CurrentTravelSpeed += SpeedDelta;
        }
        else
        {
            CurrentTravelSpeed -= SpeedDelta;
        }
    }

    CurrentTravelSpeed = FMath::Clamp(CurrentTravelSpeed, -MaxTravelSpeed, MaxTravelSpeed);

    float CurrentAccel = (DeltaTime > 0.0001f) ?
        (CurrentTravelSpeed - PreviousTravelSpeed) / DeltaTime : 0.0f;

    CurrentJerk = (DeltaTime > 0.0001f) ?
        (CurrentAccel - PreviousAcceleration) / DeltaTime : 0.0f;

    if (FMath::Abs(CurrentJerk) > MaxJerk)
    {
        float JerkSign = FMath::Sign(CurrentJerk);
        float AllowedAccelDelta = MaxJerk * DeltaTime;
        CurrentAccel = PreviousAcceleration + JerkSign * AllowedAccelDelta;
        CurrentTravelSpeed = PreviousTravelSpeed + CurrentAccel * DeltaTime;
        CurrentTravelSpeed = FMath::Clamp(CurrentTravelSpeed, -MaxTravelSpeed, MaxTravelSpeed);
    }

    PreviousAcceleration = CurrentAccel;

    FVector CurrentLoc = GetActorLocation();
    float NewX = CurrentLoc.X + CurrentTravelSpeed * DeltaTime;
    NewX = FMath::Clamp(NewX, TravelRangeMin, TravelRangeMax);

    if (NewX <= TravelRangeMin || NewX >= TravelRangeMax)
    {
        CurrentTravelSpeed = 0.0f;
        PreviousAcceleration = 0.0f;
    }

    FVector TargetVelocity = FVector(CurrentTravelSpeed, 0.0f, 0.0f);

    FVector CurrentPhysVel = TrolleyMesh->GetPhysicsLinearVelocity();
    FVector BlendedVelocity = FMath::Lerp(FVector(CurrentPhysVel.X, 0.0f, 0.0f), TargetVelocity, VelocityBlendAlpha);

    TrolleyMesh->SetPhysicsLinearVelocity(
        FVector(BlendedVelocity.X, CurrentPhysVel.Y, CurrentPhysVel.Z), false);
}

void ACraneTrolley::UpdateHoistMovement(float DeltaTime)
{
    if (FMath::Abs(HoistInput) < 0.01f)
    {
        if (FMath::Abs(CurrentHoistSpeed) > 1.0f)
        {
            float Decel = HoistAcceleration * DeltaTime;
            if (CurrentHoistSpeed > 0.0f)
                CurrentHoistSpeed = FMath::Max(0.0f, CurrentHoistSpeed - Decel);
            else
                CurrentHoistSpeed = FMath::Min(0.0f, CurrentHoistSpeed + Decel);
        }
        else
        {
            CurrentHoistSpeed = 0.0f;
        }
    }
    else
    {
        float TargetSpeed = HoistInput * MaxHoistSpeed;
        float SpeedDelta = HoistAcceleration * DeltaTime;

        if (FMath::Abs(TargetSpeed - CurrentHoistSpeed) < SpeedDelta)
        {
            CurrentHoistSpeed = TargetSpeed;
        }
        else if (TargetSpeed > CurrentHoistSpeed)
        {
            CurrentHoistSpeed += SpeedDelta;
        }
        else
        {
            CurrentHoistSpeed -= SpeedDelta;
        }
    }

    CurrentHoistSpeed = FMath::Clamp(CurrentHoistSpeed, -MaxHoistSpeed, MaxHoistSpeed);
    TargetHoistLength -= CurrentHoistSpeed * DeltaTime;
    TargetHoistLength = FMath::Clamp(TargetHoistLength, MinHoistLength, MaxHoistLength);

    CurrentHoistLength = TargetHoistLength;
}

void ACraneTrolley::ClampPhysicsVelocities()
{
    if (!TrolleyMesh || !TrolleyMesh->IsSimulatingPhysics())
        return;

    FVector LinVel = TrolleyMesh->GetPhysicsLinearVelocity();
    FVector AngVel = TrolleyMesh->GetPhysicsAngularVelocityInDegrees();

    float LinSpeed = LinVel.Size();
    float AngSpeed = AngVel.Size();

    if (FMath::IsNaN(LinSpeed) || FMath::IsNaN(AngSpeed))
    {
        TrolleyMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        TrolleyMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        CurrentTravelSpeed = 0.0f;
        PreviousAcceleration = 0.0f;
        return;
    }

    if (LinSpeed > MaxPhysicsLinearVelocity)
    {
        FVector ClampedVel = LinVel.GetSafeNormal() * MaxPhysicsLinearVelocity;
        TrolleyMesh->SetPhysicsLinearVelocity(ClampedVel, false);
        CurrentTravelSpeed = FMath::Clamp(CurrentTravelSpeed, -MaxPhysicsLinearVelocity, MaxPhysicsLinearVelocity);
    }

    if (AngSpeed > MaxPhysicsAngularVelocity)
    {
        FVector ClampedAngVel = AngVel.GetSafeNormal() * MaxPhysicsAngularVelocity;
        TrolleyMesh->SetPhysicsAngularVelocityInDegrees(ClampedAngVel, false);
    }
}

void ACraneTrolley::SetTravelInput(float InputValue)
{
    TravelInput = FMath::Clamp(InputValue, -1.0f, 1.0f);
}

void ACraneTrolley::SetHoistInput(float InputValue)
{
    HoistInput = FMath::Clamp(InputValue, -1.0f, 1.0f);
}

void ACraneTrolley::EmergencyStop()
{
    TravelInput = 0.0f;
    HoistInput = 0.0f;
    CurrentTravelSpeed = 0.0f;
    CurrentHoistSpeed = 0.0f;
    PreviousAcceleration = 0.0f;
    CurrentJerk = 0.0f;

    if (TrolleyMesh && TrolleyMesh->IsSimulatingPhysics())
    {
        FVector CurrentVel = TrolleyMesh->GetPhysicsLinearVelocity();
        TrolleyMesh->SetPhysicsLinearVelocity(FVector(0.0f, CurrentVel.Y, CurrentVel.Z), false);
        TrolleyMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }
}

TArray<FVector> ACraneTrolley::GetWireRopeAnchorWorldPositions() const
{
    TArray<FVector> Positions;
    for (USceneComponent* Anchor : WireRopeAnchorPoints)
    {
        if (Anchor)
        {
            Positions.Add(Anchor->GetComponentLocation());
        }
    }
    return Positions;
}

float ACraneTrolley::GetTargetHoistLength() const
{
    return TargetHoistLength;
}
