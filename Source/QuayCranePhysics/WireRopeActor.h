#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WireRopeConstraintComponent.h"
#include "WireRopeActor.generated.h"

UENUM(BlueprintType)
enum class EWireRopePosition : uint8
{
    FrontLeft,
    FrontRight,
    RearLeft,
    RearRight
};

UCLASS()
class QUAYCRANEPHYSICS_API AWireRopeActor : public AActor
{
    GENERATED_BODY()

public:
    AWireRopeActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WireRope|Components")
    TArray<UWireRopeConstraintComponent*> WireRopes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|Physics")
    float DefaultStiffness = 500000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|Physics")
    float DefaultDamping = 50000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|Physics")
    float DefaultBreakThreshold = 8000000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|Physics")
    int32 SegmentsPerRope = 6;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|Physics")
    float DefaultRestLength = 3000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|Physics")
    float AngularSwingLimit = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireRope|Physics")
    float AngularTwistLimit = 5.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WireRope|State")
    TArray<float> RopeTensions;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WireRope|State")
    TArray<float> RopeLengths;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WireRope|State")
    TArray<bool> RopeBrokenStates;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WireRope|State")
    float TotalTension = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WireRope|State")
    float MaxTension = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WireRope|State")
    float MinTension = 0.0f;

    UFUNCTION(BlueprintCallable, Category = "WireRope")
    void ConnectTrolleyToSpreader(
        UPrimitiveComponent* TrolleyRoot,
        UPrimitiveComponent* SpreaderRoot,
        const TArray<FVector>& TrolleyAnchorOffsets,
        const TArray<FVector>& SpreaderAttachOffsets,
        float HoistLength);

    UFUNCTION(BlueprintCallable, Category = "WireRope")
    void UpdateHoistLength(float NewHoistLength);

    UFUNCTION(BlueprintCallable, Category = "WireRope")
    void SetSpringDamperParams(float Stiffness, float Damping);

    UFUNCTION(BlueprintCallable, Category = "WireRope")
    void RepairAllRopes();

    UFUNCTION(BlueprintCallable, Category = "WireRope")
    float GetRopeTension(EWireRopePosition Position) const;

    UFUNCTION(BlueprintCallable, Category = "WireRope")
    bool IsRopeBroken(EWireRopePosition Position) const;

    UFUNCTION(BlueprintCallable, Category = "WireRope")
    void UpdatePhysicsForLoad(float TotalLoadMass);

    UFUNCTION(BlueprintCallable, Category = "WireRope")
    TArray<FVector> GetRopeSegmentPositions(int32 RopeIndex) const;

protected:
    void CreateWireRopes();
    void UpdateTensionMetrics();

private:
    static constexpr int32 ROPE_COUNT = 4;

    UPROPERTY()
    TWeakObjectPtr<UPrimitiveComponent> CachedTrolleyRoot;

    UPROPERTY()
    TWeakObjectPtr<UPrimitiveComponent> CachedSpreaderRoot;

    UPROPERTY()
    TArray<FVector> CachedTrolleyOffsets;

    UPROPERTY()
    TArray<FVector> CachedSpreaderOffsets;

    float LastKnownHoistLength = 3000.0f;
};
