#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AntiSwayController.generated.h"

UENUM(BlueprintType)
enum class EAntiSwayMode : uint8
{
    Disabled,
    SwayDamping,
    PositionTarget
};

UCLASS(ClassGroup = (Physics), meta = (BlueprintSpawnableComponent))
class QUAYCRANEPHYSICS_API UAntiSwayController : public UActorComponent
{
    GENERATED_BODY()

public:
    UAntiSwayController();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiSway|PID")
    float Kp = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiSway|PID")
    float Ki = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiSway|PID")
    float Kd = 4500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiSway|PID")
    float IntegralMax = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiSway|PID")
    float IntegralDecayRate = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiSway|PID")
    float DerivativeFilterAlpha = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiSway|Limits")
    float MaxCorrectionAcceleration = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiSway|Limits")
    float SwayAngleActivationThreshold = 0.002f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiSway|Limits")
    float SwayAngleDeactivationThreshold = 0.0005f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiSway|Adaptive")
    float ReferenceHoistLength = 15000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiSway|Adaptive")
    bool bAdaptiveGainScaling = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiSway|Adaptive")
    float MinHoistLengthForControl = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiSway|Mode")
    EAntiSwayMode AntiSwayMode = EAntiSwayMode::SwayDamping;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiSway|Target")
    float PositionTargetX = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiSway|Target")
    float PositionTargetArrivalThreshold = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AntiSway|Target")
    float PositionKp = 50.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AntiSway|State")
    float SwayAngleX = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AntiSway|State")
    float SwayAngleY = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AntiSway|State")
    float SwayAngularVelocityX = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AntiSway|State")
    float SwayAngularVelocityY = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AntiSway|State")
    float CorrectionAccelerationX = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AntiSway|State")
    float CorrectionAccelerationY = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AntiSway|State")
    float CurrentHoistLength = 15000.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AntiSway|State")
    float EffectiveKp = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AntiSway|State")
    float EffectiveKd = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AntiSway|State")
    bool bIsControllerActive = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AntiSway|State")
    float SwayDisplacementCm = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AntiSway|State")
    float TotalSwayAngle = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AntiSway|State")
    float FilteredDerivativeX = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AntiSway|State")
    float FilteredDerivativeY = 0.0f;

    UFUNCTION(BlueprintCallable, Category = "AntiSway")
    void UpdateSwayGeometry(const FVector& TrolleyLocation, const FVector& SpreaderLocation, float HoistLength);

    UFUNCTION(BlueprintCallable, Category = "AntiSway")
    FVector GetCorrectionAcceleration() const;

    UFUNCTION(BlueprintCallable, Category = "AntiSway")
    void ResetController();

    UFUNCTION(BlueprintCallable, Category = "AntiSway")
    void SetAntiSwayEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "AntiSway")
    float GetSwayDisplacementCm() const;

    UFUNCTION(BlueprintCallable, Category = "AntiSway")
    float GetPendulumPeriod() const;

protected:
    void ComputeSwayAngles(const FVector& TrolleyLocation, const FVector& SpreaderLocation);
    void ComputeAdaptiveGains();
    float ComputePIDOutput_X(float DeltaTime);
    float ComputePIDOutput_Y(float DeltaTime);
    float ComputePositionCorrection(float TrolleyX);

private:
    float IntegralX = 0.0f;
    float IntegralY = 0.0f;
    float PrevSwayAngleX = 0.0f;
    float PrevSwayAngleY = 0.0f;
    float RawDerivativeX = 0.0f;
    float RawDerivativeY = 0.0f;
    bool bHasPrevAngle = false;

    static constexpr float GRAVITY = 980.665f;
};
