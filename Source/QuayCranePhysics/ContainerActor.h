#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ContainerActor.generated.h"

UENUM(BlueprintType)
enum class EContainerSize : uint8
{
    TwentyFoot,
    FortyFoot,
    FortyFiveFoot
};

UCLASS()
class QUAYCRANEPHYSICS_API AContainerActor : public AActor
{
    GENERATED_BODY()

public:
    AContainerActor();

    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Container|Components")
    UStaticMeshComponent* ContainerMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Container|Components")
    TArray<USceneComponent*> CornerCastingComponents;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container|Properties")
    EContainerSize ContainerSize = EContainerSize::FortyFoot;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container|Properties")
    float ContainerMass = 30000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container|Properties")
    float GrossMass = 34000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container|Properties")
    bool bIsLoaded = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Container|State")
    bool bIsLifted = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Container|State")
    bool bIsAttachedToSpreader = false;

    UFUNCTION(BlueprintCallable, Category = "Container")
    FVector GetContainerDimensions() const;

    UFUNCTION(BlueprintCallable, Category = "Container")
    TArray<FVector> GetCornerCastingWorldPositions() const;

    UFUNCTION(BlueprintCallable, Category = "Container")
    void SetLiftedState(bool bLifted);

    UFUNCTION(BlueprintCallable, Category = "Container")
    void SetAttachedState(bool bAttached);

    UFUNCTION(BlueprintCallable, Category = "Container")
    float GetCurrentWeight() const;

protected:
    void SetupContainerPhysics();
    void CreateCornerCastingPositions();

private:
    static constexpr float TWENTY_FOOT_LENGTH = 6058.0f;
    static constexpr float FORTY_FOOT_LENGTH = 12192.0f;
    static constexpr float FORTY_FIVE_FOOT_LENGTH = 13716.0f;
    static constexpr float CONTAINER_WIDTH = 2438.0f;
    static constexpr float CONTAINER_HEIGHT = 2591.0f;
    static constexpr float CORNER_CASTING_SIZE = 178.0f;
};
