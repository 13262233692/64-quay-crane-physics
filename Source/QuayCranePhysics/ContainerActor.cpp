#include "ContainerActor.h"
#include "QuayCranePhysics.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

AContainerActor::AContainerActor()
{
    PrimaryActorTick.bCanEverTick = false;

    ContainerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ContainerMesh"));
    SetRootComponent(ContainerMesh);

    ContainerMesh->SetSimulatePhysics(true);
    ContainerMesh->SetEnableGravity(true);
    ContainerMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    ContainerMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    ContainerMesh->SetLinearDamping(0.2f);
    ContainerMesh->SetAngularDamping(1.0f);
    ContainerMesh->SetMassOverrideInKg(NAME_None, ContainerMass);
    ContainerMesh->SetNotifyRigidBodyCollision(true);

    for (int32 i = 0; i < 8; ++i)
    {
        USceneComponent* CornerCasting = CreateDefaultSubobject<USceneComponent>(
            *FString::Printf(TEXT("CornerCasting_%d"), i));
        CornerCasting->SetupAttachment(ContainerMesh);
        CornerCastingComponents.Add(CornerCasting);
    }
}

void AContainerActor::BeginPlay()
{
    Super::BeginPlay();
    SetupContainerPhysics();
    CreateCornerCastingPositions();
}

void AContainerActor::SetupContainerPhysics()
{
    ContainerMesh->SetMassOverrideInKg(NAME_None, bIsLoaded ? GrossMass : ContainerMass);
    ContainerMesh->SetLinearDamping(0.2f);
    ContainerMesh->SetAngularDamping(1.0f);
    ContainerMesh->UpdateMassProperties();
}

void AContainerActor::CreateCornerCastingPositions()
{
    FVector Dimensions = GetContainerDimensions();
    float HalfLength = Dimensions.X * 0.5f;
    float HalfWidth = Dimensions.Y * 0.5f;
    float HalfHeight = Dimensions.Z * 0.5f;
    float CastingInset = CORNER_CASTING_SIZE;

    struct FCornerDef
    {
        float X;
        float Y;
        float Z;
    };

    FCornerDef TopCorners[4] = {
        { HalfLength - CastingInset, -HalfWidth + CastingInset,  HalfHeight},
        { HalfLength - CastingInset,  HalfWidth - CastingInset,  HalfHeight},
        {-HalfLength + CastingInset, -HalfWidth + CastingInset,  HalfHeight},
        {-HalfLength + CastingInset,  HalfWidth - CastingInset,  HalfHeight},
    };

    FCornerDef BottomCorners[4] = {
        { HalfLength - CastingInset, -HalfWidth + CastingInset, -HalfHeight},
        { HalfLength - CastingInset,  HalfWidth - CastingInset, -HalfHeight},
        {-HalfLength + CastingInset, -HalfWidth + CastingInset, -HalfHeight},
        {-HalfLength + CastingInset,  HalfWidth - CastingInset, -HalfHeight},
    };

    for (int32 i = 0; i < 4; ++i)
    {
        CornerCastingComponents[i]->SetRelativeLocation(FVector(TopCorners[i].X, TopCorners[i].Y, TopCorners[i].Z));
    }
    for (int32 i = 0; i < 4; ++i)
    {
        CornerCastingComponents[i + 4]->SetRelativeLocation(FVector(BottomCorners[i].X, BottomCorners[i].Y, BottomCorners[i].Z));
    }
}

FVector AContainerActor::GetContainerDimensions() const
{
    switch (ContainerSize)
    {
    case EContainerSize::TwentyFoot:
        return FVector(TWENTY_FOOT_LENGTH, CONTAINER_WIDTH, CONTAINER_HEIGHT);
    case EContainerSize::FortyFiveFoot:
        return FVector(FORTY_FIVE_FOOT_LENGTH, CONTAINER_WIDTH, CONTAINER_HEIGHT);
    case EContainerSize::FortyFoot:
    default:
        return FVector(FORTY_FOOT_LENGTH, CONTAINER_WIDTH, CONTAINER_HEIGHT);
    }
}

TArray<FVector> AContainerActor::GetCornerCastingWorldPositions() const
{
    TArray<FVector> Positions;
    for (USceneComponent* Casting : CornerCastingComponents)
    {
        if (Casting)
        {
            Positions.Add(Casting->GetComponentLocation());
        }
    }
    return Positions;
}

void AContainerActor::SetLiftedState(bool bLifted)
{
    bIsLifted = bLifted;
    if (bLifted)
    {
        ContainerMesh->SetLinearDamping(0.5f);
        ContainerMesh->SetAngularDamping(2.0f);
    }
    else
    {
        ContainerMesh->SetLinearDamping(0.2f);
        ContainerMesh->SetAngularDamping(1.0f);
    }
}

void AContainerActor::SetAttachedState(bool bAttached)
{
    bIsAttachedToSpreader = bAttached;
    SetLiftedState(bAttached);
}

float AContainerActor::GetCurrentWeight() const
{
    return bIsLoaded ? GrossMass : ContainerMass;
}
