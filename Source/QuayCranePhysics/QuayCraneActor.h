#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CraneTrolley.h"
#include "SpreaderActor.h"
#include "WireRopeActor.h"
#include "ContainerActor.h"
#include "QuayCraneActor.generated.h"

UENUM(BlueprintType)
enum class ECraneOperationMode : uint8
{
    Idle,
    Traveling,
    Hoisting,
    Lowering,
    Locking,
    Lifting,
    Transporting,
    Placing,
    Unlocking
};

UCLASS()
class QUAYCRANEPHYSICS_API AQuayCraneActor : public AActor
{
    GENERATED_BODY()

public:
    AQuayCraneActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crane|Structure")
    UStaticMeshComponent* CraneBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crane|Structure")
    UStaticMeshComponent* CraneLegPort;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crane|Structure")
    UStaticMeshComponent* CraneLegStarboard;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crane|Structure")
    UStaticMeshComponent* CraneAFrame;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crane|Structure")
    UStaticMeshComponent* CranePortalBeam;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crane|SubActors")
    ACraneTrolley* Trolley;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crane|SubActors")
    ASpreaderActor* Spreader;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crane|SubActors")
    AWireRopeActor* WireRopeSystem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crane|Dimensions")
    float BoomLength = 70000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crane|Dimensions")
    float BoomHeight = 50000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crane|Dimensions")
    float PortalWidth = 30000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crane|Dimensions")
    float PortalHeight = 40000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crane|Dimensions")
    float LegWidth = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crane|Dimensions")
    float AFrameHeight = 15000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crane|Hoist")
    float InitialHoistLength = 15000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crane|Physics")
    float CraneStructureMass = 100000.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crane|State")
    ECraneOperationMode OperationMode = ECraneOperationMode::Idle;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crane|State")
    float CurrentHoistLength = 15000.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crane|State")
    float TotalWireTension = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crane|State")
    float CurrentPendulumAngle = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crane|State")
    bool bIsContainerAttached = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crane|State")
    AContainerActor* CurrentContainer = nullptr;

    UFUNCTION(BlueprintCallable, Category = "Crane|Control")
    void SetTrolleyTravel(float Input);

    UFUNCTION(BlueprintCallable, Category = "Crane|Control")
    void SetHoistMovement(float Input);

    UFUNCTION(BlueprintCallable, Category = "Crane|Control")
    void EngageTwistlocks();

    UFUNCTION(BlueprintCallable, Category = "Crane|Control")
    void DisengageTwistlocks();

    UFUNCTION(BlueprintCallable, Category = "Crane|Control")
    void EmergencyStop();

    UFUNCTION(BlueprintCallable, Category = "Crane|Control")
    void SpawnContainerAtLocation(const FVector& Location, EContainerSize Size);

    UFUNCTION(BlueprintCallable, Category = "Crane|Control")
    void AutoLiftSequence();

    UFUNCTION(BlueprintCallable, Category = "Crane|Control")
    void AutoPlaceSequence();

    UFUNCTION(BlueprintCallable, Category = "Crane|Control")
    void CancelAutoSequence();

    UFUNCTION(BlueprintCallable, Category = "Crane|Debug")
    void DrawDebugPhysicsState();

protected:
    void SpawnSubActors();
    void ConnectWireRopeSystem();
    void UpdateOperationMode(float DeltaTime);
    void UpdateHoistSync();
    void ScanForNearbyContainers();
    void ProcessAutoSequence(float DeltaTime);

    UPROPERTY()
    TArray<AContainerActor*> NearbyContainers;

    UPROPERTY()
    AContainerActor* TargetContainer = nullptr;

private:
    float AutoSequenceTimer = 0.0f;
    float AutoSequencePhase = 0.0f;
    bool bAutoSequenceActive = false;
    FVector AutoSequenceTargetLocation = FVector::ZeroVector;

    static constexpr float CONTAINER_SCAN_RADIUS = 20000.0f;
    static constexpr float GRAVITY = 980.665f;
};
