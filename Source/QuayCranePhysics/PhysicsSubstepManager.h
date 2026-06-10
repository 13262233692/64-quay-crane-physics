#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PhysicsSubstepManager.generated.h"

UENUM(BlueprintType)
enum class EPhysicsStabilityState : uint8
{
    Stable,
    Warning,
    Critical,
    Emergency
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPhysicsExplosionDetected, float, Magnitude, FVector, Epicenter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPhysicsStabilized);

UCLASS(ClassGroup=(QuayCrane), meta=(BlueprintSpawnableComponent))
class QUAYCRANEPHYSICS_API UPhysicsSubstepManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UPhysicsSubstepManager();

    virtual void InitializeComponent() override;
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Substep|Config")
    float MaxSubstepDeltaTime = 0.001f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Substep|Config")
    int32 MaxSubsteps = 16;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Substep|Solver")
    int32 SolverIterations = 30;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Substep|Solver")
    int32 VelocityIterations = 15;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Substep|Solver")
    int32 PositionIterations = 15;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Substep|Solver")
    int32 JointIterations = 15;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Substep|Clamping")
    float MaxLinearVelocity = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Substep|Clamping")
    float MaxAngularVelocity = 720.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Substep|Clamping")
    float MaxForcePerSubstep = 50000000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Substep|Explosion")
    float ExplosionVelocityThreshold = 10000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Substep|Explosion")
    float ExplosionAngularThreshold = 3600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Substep|Explosion")
    float StabilizationDampingMultiplier = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Substep|Explosion")
    float EmergencyFreezeDuration = 0.1f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Substep|State")
    EPhysicsStabilityState StabilityState = EPhysicsStabilityState::Stable;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Substep|State")
    float PeakVelocity = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Substep|State")
    float PeakAngularVelocity = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Substep|State")
    int32 ExplosionCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Substep|State")
    bool bIsEmergencyFrozen = false;

    UPROPERTY(BlueprintAssignable, Category = "Substep|Events")
    FOnPhysicsExplosionDetected OnPhysicsExplosionDetected;

    UPROPERTY(BlueprintAssignable, Category = "Substep|Events")
    FOnPhysicsStabilized OnPhysicsStabilized;

    UFUNCTION(BlueprintCallable, Category = "Substep")
    void RegisterPhysicsBody(UPrimitiveComponent* Body);

    UFUNCTION(BlueprintCallable, Category = "Substep")
    void UnregisterPhysicsBody(UPrimitiveComponent* Body);

    UFUNCTION(BlueprintCallable, Category = "Substep")
    void EmergencyFreezeAll();

    UFUNCTION(BlueprintCallable, Category = "Substep")
    void ThawAll();

    UFUNCTION(BlueprintCallable, Category = "Substep")
    void ApplySolverIterationOverride();

    UFUNCTION(BlueprintCallable, Category = "Substep")
    void ClampAllVelocities();

    UFUNCTION(BlueprintCallable, Category = "Substep")
    void ApplyStabilizationDamping();

    UFUNCTION(BlueprintCallable, Category = "Substep")
    void SetClampingLimits(float InMaxLinearVel, float InMaxAngularVel, float InMaxForce);

private:
    UPROPERTY()
    TArray<TWeakObjectPtr<UPrimitiveComponent>> RegisteredBodies;

    FPhysicsSubstepCallbackData SubstepCallbackData;
    FDelegateHandle SubstepDelegateHandle;

    float EmergencyFreezeTimer = 0.0f;
    float StabilityTimer = 0.0f;

    static constexpr float STABILITY_CHECK_INTERVAL = 0.05f;

    void OnPhysicsSubstep(float DeltaTime, FBodyInstance* BodyInstance);
    void DetectExplosion();
    void UpdateStabilityState(float DeltaTime);
    void ClampBodyVelocities(UPrimitiveComponent* Body);

    UFUNCTION()
    void HandleSubstepTick(float DeltaTime);
};
