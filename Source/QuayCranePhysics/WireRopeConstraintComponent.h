#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "WireRopeConstraintComponent.generated.h"

UCLASS(ClassGroup=(QuayCrane), meta=(BlueprintSpawnableComponent))
class QUAYCRANEPHYSICS_API UWireRopeConstraintComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWireRopeConstraintComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "WireRope")
    void InitializeRope(UPrimitiveComponent* InAnchor, UPrimitiveComponent* InLoad, const FVector& InAnchorOffset, const FVector& InLoadOffset);

    UFUNCTION(BlueprintCallable, Category = "WireRope")
    void SetSpringDamperParams(float InStiffness, float InDamping, float InWireLength);

    UFUNCTION(BlueprintCallable, Category = "WireRope")
    void BreakRope();

    UFUNCTION(BlueprintCallable, Category = "WireRope")
    void RepairRope();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|SpringDamper")
    float SpringStiffness = 500000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|SpringDamper")
    float DampingCoefficient = 50000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|SpringDamper")
    float WireRestLength = 3000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|SpringDamper")
    float ForceLimit = 5000000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|Breaking")
    float BreakThreshold = 8000000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|Breaking")
    bool bEnableBreaking = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|Constraint")
    float LinearLimitSize = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|Constraint")
    float AngularSwingLimit = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|Constraint")
    float AngularTwistLimit = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|Constraint")
    int32 ChainSegments = 6;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|AntiExplosion")
    float MaxTensionClamp = 12000000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|AntiExplosion")
    float MaxExtensionRatio = 1.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|AntiExplosion")
    float MinRestLengthForFullStiffness = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|AntiExplosion")
    float SoftStiffnessScale = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|AntiExplosion")
    float MaxSegmentLinearVelocity = 8000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|AntiExplosion")
    float MaxSegmentAngularVelocity = 1440.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|AntiExplosion")
    float SegmentDampingBoostOnShortRope = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|AntiExplosion")
    float LinearDriveSoftness = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|AntiExplosion")
    float AngularDriveSoftness = 0.001f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|AntiExplosion")
    float ContactOffsetOverride = 5.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WireRope|State")
    float CurrentTension = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WireRope|State")
    bool bIsBroken = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WireRope|State")
    float CurrentWireLength = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WireRope|State")
    float RopeExtensionRatio = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WireRope|State")
    float EffectiveStiffness = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WireRope|State")
    bool bIsInSoftMode = false;

    UPROPERTY()
    TArray<UPhysicsConstraintComponent*> ConstraintChain;

    UPROPERTY()
    TArray<UStaticMeshComponent*> RopeSegmentMeshes;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY()
    TWeakObjectPtr<UPrimitiveComponent> AnchorComponent;

    UPROPERTY()
    TWeakObjectPtr<UPrimitiveComponent> LoadComponent;

    FVector AnchorAttachOffset = FVector::ZeroVector;
    FVector LoadAttachOffset = FVector::ZeroVector;

    void BuildConstraintChain();
    void UpdateSpringDamperForces(float DeltaTime);
    float CalculateTension() const;
    void ClampSegmentVelocities();
    void ApplySoftConstraintMode(float DeltaTime);
    float ComputeEffectiveStiffness() const;
    void ConfigureConstraintStability(UPhysicsConstraintComponent* Constraint, float Stiffness, float Damping);

    UPROPERTY()
    UStaticMesh* RopeSegmentMeshAsset;

    static constexpr float ROPE_SEGMENT_MASS = 50.0f;
    static constexpr float MIN_WIRE_LENGTH = 100.0f;
};
