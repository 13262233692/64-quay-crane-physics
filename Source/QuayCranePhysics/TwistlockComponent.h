#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "TwistlockComponent.generated.h"

UENUM(BlueprintType)
enum class ETwistlockState : uint8
{
    Unlocked,
    ProximityDetected,
    Engaging,
    Locked,
    Disengaging
};

USTRUCT(BlueprintType)
struct FTwistlockSocket
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName SocketName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector LocalOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator LocalRotation = FRotator::ZeroRotator;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bIsEngaged = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FVector WorldPosition = FVector::ZeroVector;

    UPROPERTY()
    UPhysicsConstraintComponent* LockConstraint = nullptr;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTwistlockStateChanged, ETwistlockState, NewState, ETwistlockState, OldState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FContainerAttached, AActor*, Container);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FContainerDetached);

UCLASS(ClassGroup=(QuayCrane), meta=(BlueprintSpawnableComponent))
class QUAYCRANEPHYSICS_API UTwistlockComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTwistlockComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "Twistlock")
    void InitializeTwistlocks(const TArray<FTwistlockSocket>& InSockets);

    UFUNCTION(BlueprintCallable, Category = "Twistlock")
    bool TryEngageTwistlocks();

    UFUNCTION(BlueprintCallable, Category = "Twistlock")
    void DisengageTwistlocks();

    UFUNCTION(BlueprintCallable, Category = "Twistlock")
    void ForceDisengage();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Twistlock|Tolerance")
    float PositionTolerance = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Twistlock|Tolerance")
    float AngularToleranceDeg = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Twistlock|Tolerance")
    float EngagementRange = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Twistlock|Tolerance")
    float ProximityScanRange = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Twistlock|Constraint")
    float LockBreakForce = 10000000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Twistlock|Constraint")
    float LockStiffness = 10000000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Twistlock|Constraint")
    float LockDamping = 500000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Twistlock|Constraint")
    float SnapSpeed = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Twistlock|Constraint")
    float EngagementDuration = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Twistlock|Detection")
    TArray<TEnumAsByte<EObjectTypeQuery>> ContainerObjectTypes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Twistlock|Detection")
    TArray<AActor*> IgnoredActors;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Twistlock|State")
    ETwistlockState CurrentState = ETwistlockState::Unlocked;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Twistlock|State")
    TArray<FTwistlockSocket> TwistlockSockets;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Twistlock|State")
    AActor* AttachedContainer = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Twistlock|State")
    int32 EngagedSocketCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Twistlock|State")
    float EngagementProgress = 0.0f;

    UPROPERTY(BlueprintAssignable, Category = "Twistlock|Events")
    FTwistlockStateChanged OnTwistlockStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Twistlock|Events")
    FContainerAttached OnContainerAttached;

    UPROPERTY(BlueprintAssignable, Category = "Twistlock|Events")
    FContainerDetached OnContainerDetached;

protected:
    virtual void BeginPlay() override;

private:
    void SetState(ETwistlockState NewState);

    void ScanForContainerCornerCastings();
    void CheckProximity();
    void UpdateEngagement(float DeltaTime);
    void CreateLockConstraints();
    void DestroyLockConstraints();

    bool AreAllSocketsAligned() const;
    bool IsSocketAlignedWithCasting(int32 SocketIndex, FVector& OutCastingWorldPos) const;

    UPROPERTY()
    TArray<FVector> DetectedCastingPositions;

    UPROPERTY()
    UPrimitiveComponent* OwnerRootPrimitive = nullptr;

    float EngagementTimer = 0.0f;

    static constexpr int32 REQUIRED_TWISTLOCK_COUNT = 4;
};
