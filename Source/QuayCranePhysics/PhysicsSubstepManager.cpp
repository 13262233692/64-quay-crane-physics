#include "PhysicsSubstepManager.h"
#include "QuayCranePhysics.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Chaos/ChaosEngineInterface.h"

UPhysicsSubstepManager::UPhysicsSubstepManager()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UPhysicsSubstepManager::InitializeComponent()
{
    Super::InitializeComponent();
}

void UPhysicsSubstepManager::BeginPlay()
{
    Super::BeginPlay();
    ApplySolverIterationOverride();
}

void UPhysicsSubstepManager::ApplySolverIterationOverride()
{
    UPhysicsSettings* PhysicsSettings = Cast<UPhysicsSettings>(UPhysicsSettings::StaticClass()->GetDefaultObject());
    if (PhysicsSettings)
    {
        PhysicsSettings->bSubstepping = true;
        PhysicsSettings->MaxSubstepDeltaTime = MaxSubstepDeltaTime;
        PhysicsSettings->MaxSubsteps = MaxSubsteps;

        UE_LOG(LogQuayCranePhysics, Log, TEXT("PhysicsSubstepManager: Substepping ENABLED - MaxDT=%.4f, MaxSteps=%d"),
            MaxSubstepDeltaTime, MaxSubsteps);
    }

    for (const TWeakObjectPtr<UPrimitiveComponent>& BodyPtr : RegisteredBodies)
    {
        if (BodyPtr.IsValid())
        {
            FBodyInstance* BodyInst = BodyPtr->GetBodyInstance();
            if (BodyInst)
            {
                BodyInst->SolverIterationCount = SolverIterations;
                BodyInst->SolverVelocityIterationCount = VelocityIterations;
                BodyInst->UpdatePhysicalMaterials();
            }
        }
    }

    UE_LOG(LogQuayCranePhysics, Log, TEXT("PhysicsSubstepManager: Solver iterations set - Pos=%d, Vel=%d, Joint=%d"),
        SolverIterations, VelocityIterations, JointIterations);
}

void UPhysicsSubstepManager::RegisterPhysicsBody(UPrimitiveComponent* Body)
{
    if (!Body) return;

    RegisteredBodies.Add(Body);

    FBodyInstance* BodyInst = Body->GetBodyInstance();
    if (BodyInst)
    {
        BodyInst->SolverIterationCount = SolverIterations;
        BodyInst->SolverVelocityIterationCount = VelocityIterations;
    }

    Body->SetPhysicsMaxAngularVelocityInDegrees(MaxAngularVelocity);
    Body->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable, FVector::UpVector));
}

void UPhysicsSubstepManager::UnregisterPhysicsBody(UPrimitiveComponent* Body)
{
    if (!Body) return;

    RegisteredBodies.Remove(Body);
}

void UPhysicsSubstepManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsEmergencyFrozen)
    {
        EmergencyFreezeTimer -= DeltaTime;
        if (EmergencyFreezeTimer <= 0.0f)
        {
            ThawAll();
        }
        return;
    }

    UpdateStabilityState(DeltaTime);
}

void UPhysicsSubstepManager::UpdateStabilityState(float DeltaTime)
{
    StabilityTimer += DeltaTime;
    if (StabilityTimer < STABILITY_CHECK_INTERVAL)
        return;

    StabilityTimer = 0.0f;

    DetectExplosion();
}

void UPhysicsSubstepManager::DetectExplosion()
{
    PeakVelocity = 0.0f;
    PeakAngularVelocity = 0.0f;

    for (const TWeakObjectPtr<UPrimitiveComponent>& BodyPtr : RegisteredBodies)
    {
        if (!BodyPtr.IsValid() || !BodyPtr->IsSimulatingPhysics())
            continue;

        FVector LinVel = BodyPtr->GetPhysicsLinearVelocity();
        FVector AngVel = BodyPtr->GetPhysicsAngularVelocityInDegrees();

        float LinSpeed = LinVel.Size();
        float AngSpeed = AngVel.Size();

        PeakVelocity = FMath::Max(PeakVelocity, LinSpeed);
        PeakAngularVelocity = FMath::Max(PeakAngularVelocity, AngSpeed);
    }

    EPhysicsStabilityState PreviousState = StabilityState;

    if (PeakVelocity > ExplosionVelocityThreshold || PeakAngularVelocity > ExplosionAngularThreshold)
    {
        if (FMath::IsNaN(PeakVelocity) || FMath::IsNaN(PeakAngularVelocity))
        {
            StabilityState = EPhysicsStabilityState::Emergency;
            ExplosionCount++;
            EmergencyFreezeAll();
            OnPhysicsExplosionDetected.Broadcast(PeakVelocity, FVector::ZeroVector);
            UE_LOG(LogQuayCranePhysics, Error, TEXT("PHYSICS EXPLOSION DETECTED! NaN detected - Emergency freeze engaged"));
            return;
        }

        StabilityState = EPhysicsStabilityState::Critical;
        ExplosionCount++;
        ClampAllVelocities();
        ApplyStabilizationDamping();
        OnPhysicsExplosionDetected.Broadcast(PeakVelocity, FVector::ZeroVector);
        UE_LOG(LogQuayCranePhysics, Error, TEXT("PHYSICS EXPLOSION DETECTED! PeakVel=%.1f, PeakAngVel=%.1f - Clamping engaged"),
            PeakVelocity, PeakAngularVelocity);
    }
    else if (PeakVelocity > ExplosionVelocityThreshold * 0.5f ||
             PeakAngularVelocity > ExplosionAngularThreshold * 0.5f)
    {
        StabilityState = EPhysicsStabilityState::Warning;
        ClampAllVelocities();
    }
    else
    {
        if (PreviousState != EPhysicsStabilityState::Stable)
        {
            StabilityState = EPhysicsStabilityState::Stable;
            OnPhysicsStabilized.Broadcast();
            UE_LOG(LogQuayCranePhysics, Log, TEXT("Physics stabilized after %d explosions"), ExplosionCount);
        }
        StabilityState = EPhysicsStabilityState::Stable;
    }
}

void UPhysicsSubstepManager::ClampAllVelocities()
{
    for (const TWeakObjectPtr<UPrimitiveComponent>& BodyPtr : RegisteredBodies)
    {
        if (BodyPtr.IsValid() && BodyPtr->IsSimulatingPhysics())
        {
            ClampBodyVelocities(BodyPtr.Get());
        }
    }
}

void UPhysicsSubstepManager::ClampBodyVelocities(UPrimitiveComponent* Body)
{
    if (!Body || !Body->IsSimulatingPhysics()) return;

    FVector LinVel = Body->GetPhysicsLinearVelocity();
    FVector AngVel = Body->GetPhysicsAngularVelocityInDegrees();

    float LinSpeed = LinVel.Size();
    float AngSpeed = AngVel.Size();

    if (FMath::IsNaN(LinSpeed) || FMath::IsNaN(AngSpeed))
    {
        Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
        Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        UE_LOG(LogQuayCranePhysics, Error, TEXT("NaN velocity detected and zeroed on %s"), *Body->GetName());
        return;
    }

    if (LinSpeed > MaxLinearVelocity)
    {
        FVector ClampedVel = LinVel.GetSafeNormal() * MaxLinearVelocity;
        Body->SetPhysicsLinearVelocity(ClampedVel, false);
    }

    if (AngSpeed > MaxAngularVelocity)
    {
        FVector ClampedAngVel = AngVel.GetSafeNormal() * MaxAngularVelocity;
        Body->SetPhysicsAngularVelocityInDegrees(ClampedAngVel, false);
    }
}

void UPhysicsSubstepManager::ApplyStabilizationDamping()
{
    for (const TWeakObjectPtr<UPrimitiveComponent>& BodyPtr : RegisteredBodies)
    {
        if (!BodyPtr.IsValid() || !BodyPtr->IsSimulatingPhysics())
            continue;

        float CurrentDamping = BodyPtr->GetLinearDamping();
        float CurrentAngDamping = BodyPtr->GetAngularDamping();

        BodyPtr->SetLinearDamping(CurrentDamping * StabilizationDampingMultiplier);
        BodyPtr->SetAngularDamping(CurrentAngDamping * StabilizationDampingMultiplier);
    }
}

void UPhysicsSubstepManager::EmergencyFreezeAll()
{
    bIsEmergencyFrozen = true;
    EmergencyFreezeTimer = EmergencyFreezeDuration;

    for (const TWeakObjectPtr<UPrimitiveComponent>& BodyPtr : RegisteredBodies)
    {
        if (BodyPtr.IsValid() && BodyPtr->IsSimulatingPhysics())
        {
            BodyPtr->SetPhysicsLinearVelocity(FVector::ZeroVector, false);
            BodyPtr->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector, false);
        }
    }

    UE_LOG(LogQuayCranePhysics, Warning, TEXT("EMERGENCY PHYSICS FREEZE - All bodies zeroed for %.2fs"), EmergencyFreezeDuration);
}

void UPhysicsSubstepManager::ThawAll()
{
    bIsEmergencyFrozen = false;

    for (const TWeakObjectPtr<UPrimitiveComponent>& BodyPtr : RegisteredBodies)
    {
        if (BodyPtr.IsValid() && BodyPtr->IsSimulatingPhysics())
        {
            BodyPtr->SetLinearDamping(BodyPtr->GetLinearDamping() / StabilizationDampingMultiplier);
            BodyPtr->SetAngularDamping(BodyPtr->GetAngularDamping() / StabilizationDampingMultiplier);
        }
    }

    StabilityState = EPhysicsStabilityState::Warning;
    UE_LOG(LogQuayCranePhysics, Log, TEXT("Physics THAWED - Resuming with stabilization damping"));
}

void UPhysicsSubstepManager::SetClampingLimits(float InMaxLinearVel, float InMaxAngularVel, float InMaxForce)
{
    MaxLinearVelocity = InMaxLinearVel;
    MaxAngularVelocity = InMaxAngularVel;
    MaxForcePerSubstep = InMaxForce;
}

void UPhysicsSubstepManager::HandleSubstepTick(float DeltaTime)
{
    ClampAllVelocities();
}
