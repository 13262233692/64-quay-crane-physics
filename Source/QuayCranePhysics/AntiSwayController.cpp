#include "AntiSwayController.h"
#include "QuayCranePhysics.h"

UAntiSwayController::UAntiSwayController()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UAntiSwayController::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (AntiSwayMode == EAntiSwayMode::Disabled)
    {
        bIsControllerActive = false;
        CorrectionAccelerationX = 0.0f;
        CorrectionAccelerationY = 0.0f;
        return;
    }

    if (CurrentHoistLength < MinHoistLengthForControl)
    {
        bIsControllerActive = false;
        CorrectionAccelerationX = 0.0f;
        CorrectionAccelerationY = 0.0f;
        return;
    }

    ComputeAdaptiveGains();

    float AbsSwayAngle = FMath::Sqrt(SwayAngleX * SwayAngleX + SwayAngleY * SwayAngleY);

    if (!bIsControllerActive)
    {
        if (AbsSwayAngle > SwayAngleActivationThreshold)
        {
            bIsControllerActive = true;
            UE_LOG(LogQuayCranePhysics, Log, TEXT("AntiSway: ACTIVATED - angle=%.4f rad (%.2f deg)"),
                AbsSwayAngle, FMath::RadiansToDegrees(AbsSwayAngle));
        }
    }
    else
    {
        if (AbsSwayAngle < SwayAngleDeactivationThreshold && FMath::Abs(IntegralX) < 0.01f && FMath::Abs(IntegralY) < 0.01f)
        {
            bIsControllerActive = false;
            CorrectionAccelerationX = 0.0f;
            CorrectionAccelerationY = 0.0f;
            UE_LOG(LogQuayCranePhysics, Log, TEXT("AntiSway: DEACTIVATED - sway within tolerance"));
            return;
        }
    }

    if (!bIsControllerActive)
        return;

    float OutputX = ComputePIDOutput_X(DeltaTime);
    float OutputY = ComputePIDOutput_Y(DeltaTime);

    if (AntiSwayMode == EAntiSwayMode::PositionTarget)
    {
        OutputX += ComputePositionCorrection(0.0f);
    }

    OutputX = FMath::Clamp(OutputX, -MaxCorrectionAcceleration, MaxCorrectionAcceleration);
    OutputY = FMath::Clamp(OutputY, -MaxCorrectionAcceleration * 0.5f, MaxCorrectionAcceleration * 0.5f);

    if (FMath::IsNaN(OutputX)) OutputX = 0.0f;
    if (FMath::IsNaN(OutputY)) OutputY = 0.0f;

    CorrectionAccelerationX = OutputX;
    CorrectionAccelerationY = OutputY;

    TotalSwayAngle = AbsSwayAngle;
    SwayDisplacementCm = CurrentHoistLength * FMath::Tan(TotalSwayAngle);
}

void UAntiSwayController::UpdateSwayGeometry(const FVector& TrolleyLocation, const FVector& SpreaderLocation, float HoistLength)
{
    CurrentHoistLength = FMath::Max(HoistLength, 1.0f);
    ComputeSwayAngles(TrolleyLocation, SpreaderLocation);
}

void UAntiSwayController::ComputeSwayAngles(const FVector& TrolleyLocation, const FVector& SpreaderLocation)
{
    FVector RopeVector = SpreaderLocation - TrolleyLocation;

    float RopeLength = RopeVector.Size();
    if (RopeLength < 1.0f)
    {
        SwayAngleX = 0.0f;
        SwayAngleY = 0.0f;
        return;
    }

    FVector RopeDir = RopeVector / RopeLength;

    FVector VerticalDown = FVector(0.0f, 0.0f, -1.0f);

    float CosAngle = FVector::DotProduct(RopeDir, VerticalDown);
    CosAngle = FMath::Clamp(CosAngle, -1.0f, 1.0f);
    TotalSwayAngle = FMath::Acos(CosAngle);

    SwayAngleX = FMath::Atan2(RopeDir.X, -RopeDir.Z);
    SwayAngleY = FMath::Atan2(RopeDir.Y, -RopeDir.Z);

    if (FMath::IsNaN(SwayAngleX)) SwayAngleX = 0.0f;
    if (FMath::IsNaN(SwayAngleY)) SwayAngleY = 0.0f;
}

void UAntiSwayController::ComputeAdaptiveGains()
{
    if (!bAdaptiveGainScaling || ReferenceHoistLength <= 0.0f)
    {
        EffectiveKp = Kp;
        EffectiveKd = Kd;
        return;
    }

    float LengthRatio = CurrentHoistLength / ReferenceHoistLength;
    float SqrtLengthRatio = FMath::Sqrt(LengthRatio);

    EffectiveKp = Kp * LengthRatio;
    EffectiveKd = Kd * SqrtLengthRatio;

    if (CurrentHoistLength < 3000.0f)
    {
        float ShortRopeScale = CurrentHoistLength / 3000.0f;
        EffectiveKd *= ShortRopeScale;
    }
}

float UAntiSwayController::ComputePIDOutput_X(float DeltaTime)
{
    if (DeltaTime < 0.0001f)
        return CorrectionAccelerationX;

    RawDerivativeX = (SwayAngleX - PrevSwayAngleX) / DeltaTime;

    FilteredDerivativeX = DerivativeFilterAlpha * RawDerivativeX +
        (1.0f - DerivativeFilterAlpha) * FilteredDerivativeX;

    if (FMath::IsNaN(FilteredDerivativeX))
        FilteredDerivativeX = 0.0f;

    float ErrorX = SwayAngleX;

    IntegralX += ErrorX * DeltaTime;

    IntegralX *= (1.0f - IntegralDecayRate * DeltaTime);

    IntegralX = FMath::Clamp(IntegralX, -IntegralMax, IntegralMax);

    float P = EffectiveKp * ErrorX;
    float I = Ki * IntegralX;
    float D = EffectiveKd * FilteredDerivativeX;

    float Output = P + I + D;

    PrevSwayAngleX = SwayAngleX;

    return Output;
}

float UAntiSwayController::ComputePIDOutput_Y(float DeltaTime)
{
    if (DeltaTime < 0.0001f)
        return CorrectionAccelerationY;

    RawDerivativeY = (SwayAngleY - PrevSwayAngleY) / DeltaTime;

    FilteredDerivativeY = DerivativeFilterAlpha * RawDerivativeY +
        (1.0f - DerivativeFilterAlpha) * FilteredDerivativeY;

    if (FMath::IsNaN(FilteredDerivativeY))
        FilteredDerivativeY = 0.0f;

    float ErrorY = SwayAngleY;

    IntegralY += ErrorY * DeltaTime;

    IntegralY *= (1.0f - IntegralDecayRate * DeltaTime);

    IntegralY = FMath::Clamp(IntegralY, -IntegralMax, IntegralMax);

    float P = EffectiveKp * ErrorY;
    float I = Ki * IntegralY;
    float D = EffectiveKd * FilteredDerivativeY;

    float Output = P + I + D;

    PrevSwayAngleY = SwayAngleY;

    return Output;
}

float UAntiSwayController::ComputePositionCorrection(float TrolleyX)
{
    float PositionError = PositionTargetX - TrolleyX;
    float AbsError = FMath::Abs(PositionError);

    if (AbsError < PositionTargetArrivalThreshold)
    {
        float ArrivalRatio = AbsError / PositionTargetArrivalThreshold;
        return PositionKp * PositionError * ArrivalRatio;
    }

    return PositionKp * PositionError;
}

FVector UAntiSwayController::GetCorrectionAcceleration() const
{
    return FVector(CorrectionAccelerationX, CorrectionAccelerationY, 0.0f);
}

void UAntiSwayController::ResetController()
{
    IntegralX = 0.0f;
    IntegralY = 0.0f;
    PrevSwayAngleX = 0.0f;
    PrevSwayAngleY = 0.0f;
    FilteredDerivativeX = 0.0f;
    FilteredDerivativeY = 0.0f;
    RawDerivativeX = 0.0f;
    RawDerivativeY = 0.0f;
    CorrectionAccelerationX = 0.0f;
    CorrectionAccelerationY = 0.0f;
    bHasPrevAngle = false;
    bIsControllerActive = false;

    UE_LOG(LogQuayCranePhysics, Log, TEXT("AntiSway: Controller RESET"));
}

void UAntiSwayController::SetAntiSwayEnabled(bool bEnabled)
{
    if (!bEnabled)
    {
        AntiSwayMode = EAntiSwayMode::Disabled;
        ResetController();
    }
    else
    {
        AntiSwayMode = EAntiSwayMode::SwayDamping;
    }
}

float UAntiSwayController::GetSwayDisplacementCm() const
{
    return SwayDisplacementCm;
}

float UAntiSwayController::GetPendulumPeriod() const
{
    if (CurrentHoistLength <= 0.0f)
        return 0.0f;
    return 2.0f * PI * FMath::Sqrt(CurrentHoistLength / GRAVITY);
}
