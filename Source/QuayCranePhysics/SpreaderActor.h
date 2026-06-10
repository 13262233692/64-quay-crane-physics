#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TwistlockComponent.h"
#include "SpreaderActor.generated.h"

UCLASS()
class QUAYCRANEPHYSICS_API ASpreaderActor : public AActor
{
    GENERATED_BODY()

public:
    ASpreaderActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spreader|Components")
    UStaticMeshComponent* SpreaderFrame;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spreader|Components")
    UTwistlockComponent* TwistlockSystem;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spreader|Components")
    TArray<USceneComponent*> WireRopeAttachPoints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spreader|Properties")
    float SpreaderMass = 10000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spreader|Properties")
    float SpreaderLength = 12192.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spreader|Properties")
    float SpreaderWidth = 2438.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spreader|Properties")
    float SpreaderHeight = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spreader|RopeAttach")
    float RopeAttachSpreadX = 2000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spreader|RopeAttach")
    float RopeAttachSpreadY = 2000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spreader|AntiExplosion")
    float MaxLinearVelocity = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spreader|AntiExplosion")
    float MaxAngularVelocity = 720.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spreader|AntiExplosion")
    float LockDampingDuration = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spreader|AntiExplosion")
    float LockAngularDampingBoost = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spreader|AntiExplosion")
    float LockLinearDampingBoost = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spreader|AntiExplosion")
    float StabilizationAngularMaxVel = 360.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spreader|State")
    float PendulumAngleX = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spreader|State")
    float PendulumAngleY = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spreader|State")
    FVector PendulumVelocity = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spreader|State")
    bool bIsContainerAttached = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spreader|State")
    float TotalSuspendedMass = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spreader|State")
    bool bIsLockDamping = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spreader|State")
    float LockDampingTimer = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spreader|State")
    float BaseLinearDamping = 0.3f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spreader|State")
    float BaseAngularDamping = 2.0f;

    UFUNCTION(BlueprintCallable, Category = "Spreader")
    void EngageTwistlocks();

    UFUNCTION(BlueprintCallable, Category = "Spreader")
    void DisengageTwistlocks();

    UFUNCTION(BlueprintCallable, Category = "Spreader")
    void EmergencyRelease();

    UFUNCTION(BlueprintCallable, Category = "Spreader")
    TArray<FVector> GetWireRopeAttachWorldPositions() const;

    UFUNCTION(BlueprintCallable, Category = "Spreader")
    float CalculatePendulumPeriod() const;

    UFUNCTION(BlueprintCallable, Category = "Spreader")
    void UpdateSuspendedMass(float ContainerMass);

    UFUNCTION(BlueprintImplementableEvent, Category = "Spreader")
    void OnContainerLocked(AActor* Container);

    UFUNCTION(BlueprintImplementableEvent, Category = "Spreader")
    void OnContainerReleased();

protected:
    void SetupSpreaderPhysics();
    void SetupRopeAttachPoints();
    void CalculatePendulumState();
    void BindTwistlockEvents();
    void ClampPhysicsVelocities();
    void UpdateLockDamping(float DeltaTime);

    UFUNCTION()
    void HandleContainerAttached(AActor* Container);

    UFUNCTION()
    void HandleContainerDetached();

private:
    static constexpr float GRAVITY = 980.665f;
};
