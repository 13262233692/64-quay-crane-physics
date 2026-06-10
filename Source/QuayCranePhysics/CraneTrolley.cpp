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

    TrolleyMesh->GetBodyInstance()->bLockRotation = true;
    TrolleyMesh->GetBodyInstance()->bLockXRotation = true;
    TrolleyMesh->GetBodyInstance()->bLockYRotation = true;
    TrolleyMesh->GetBodyInstance()->bLockZRotation = true;

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

    TravelPosition = GetActorLocation().X;
    CurrentVelocity = TrolleyMesh->GetPhysicsLinearVelocity();
}

void ACraneTrolley::UpdateTravelMovement(float DeltaTime)
{
    if (FMath::Abs(TravelInput) < 0.01f)
    {
        if (FMath::Abs(CurrentTravelSpeed) > 1.0f)
        {
            float Decel = Deceleration * DeltaTime;
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

    FVector CurrentLoc = GetActorLocation();
    float NewX = CurrentLoc.X + CurrentTravelSpeed * DeltaTime;
    NewX = FMath::Clamp(NewX, TravelRangeMin, TravelRangeMax);

    if (NewX <= TravelRangeMin || NewX >= TravelRangeMax)
    {
        CurrentTravelSpeed = 0.0f;
    }

    FVector ForceDir = FVector(CurrentTravelSpeed, 0.0f, 0.0f);
    TrolleyMesh->SetPhysicsLinearVelocity(ForceDir, false);
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

    if (TrolleyMesh && TrolleyMesh->IsSimulatingPhysics())
    {
        TrolleyMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
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
