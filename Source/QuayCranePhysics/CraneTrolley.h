#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CraneTrolley.generated.h"

UCLASS()
class QUAYCRANEPHYSICS_API ACraneTrolley : public AActor
{
    GENERATED_BODY()

public:
    ACraneTrolley();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trolley|Components")
    UStaticMeshComponent* TrolleyMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trolley|Components")
    TArray<USceneComponent*> WireRopeAnchorPoints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trolley|Movement")
    float MaxTravelSpeed = 2400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trolley|Movement")
    float Acceleration = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trolley|Movement")
    float Deceleration = 1200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trolley|Movement")
    float TravelRangeMin = -60000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trolley|Movement")
    float TravelRangeMax = 60000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trolley|Hoist")
    float MaxHoistSpeed = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trolley|Hoist")
    float HoistAcceleration = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trolley|Hoist")
    float MinHoistLength = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trolley|Hoist")
    float MaxHoistLength = 40000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trolley|Physics")
    float TrolleyMass = 25000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trolley|RopeAnchors")
    float RopeAnchorSpreadX = 2000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trolley|RopeAnchors")
    float RopeAnchorSpreadY = 2000.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trolley|State")
    float CurrentTravelSpeed = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trolley|State")
    float CurrentHoistSpeed = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trolley|State")
    float CurrentHoistLength = 15000.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trolley|State")
    float TravelPosition = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trolley|State")
    FVector CurrentVelocity = FVector::ZeroVector;

    UFUNCTION(BlueprintCallable, Category = "Trolley")
    void SetTravelInput(float InputValue);

    UFUNCTION(BlueprintCallable, Category = "Trolley")
    void SetHoistInput(float InputValue);

    UFUNCTION(BlueprintCallable, Category = "Trolley")
    void EmergencyStop();

    UFUNCTION(BlueprintCallable, Category = "Trolley")
    TArray<FVector> GetWireRopeAnchorWorldPositions() const;

    UFUNCTION(BlueprintCallable, Category = "Trolley")
    float GetTargetHoistLength() const;

protected:
    void UpdateTravelMovement(float DeltaTime);
    void UpdateHoistMovement(float DeltaTime);
    void SetupRopeAnchorPoints();

private:
    float TravelInput = 0.0f;
    float HoistInput = 0.0f;
    float TargetHoistLength = 15000.0f;
};
