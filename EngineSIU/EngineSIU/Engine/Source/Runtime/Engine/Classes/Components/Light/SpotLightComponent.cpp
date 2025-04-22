#include "SpotLightComponent.h"
#include "Math/Rotator.h"
#include "Math/Quat.h"
#include "UObject/Casts.h"

USpotLightComponent::USpotLightComponent()
{
    InnerRad = 0.2618;
    OuterRad = 0.5236;
    Attenuation = 20.0f;
    Radius = 30.0f;
}

USpotLightComponent::~USpotLightComponent()
{
}

UObject* USpotLightComponent::Duplicate(UObject* InOuter)
{
    USpotLightComponent* NewComponent = Cast<USpotLightComponent>(Super::Duplicate(InOuter));
    if (NewComponent)
    {
        NewComponent->Attenuation = Attenuation;
        NewComponent->Radius = Radius;
        NewComponent->InnerRad = InnerRad;
        NewComponent->OuterRad = OuterRad;
    }
    return NewComponent;
}

FVector USpotLightComponent::GetDirection()
{
    FRotator rotator = GetWorldRotation();
    FVector WorldForward = rotator.ToQuaternion().RotateVector(GetForwardVector());
    return WorldForward;
}

float USpotLightComponent::GetRadius() const
{
    return Radius;
}

void USpotLightComponent::SetRadius(float InRadius)
{
    Radius = InRadius;
}

float USpotLightComponent::GetInnerRad() const
{
    return InnerRad;
}

void USpotLightComponent::SetInnerRad(float InInnerCos)
{
    InnerRad = InInnerCos;
}

float USpotLightComponent::GetOuterRad() const
{
    return OuterRad;
}

void USpotLightComponent::SetOuterRad(float InOuterCos)
{
    OuterRad = InOuterCos;
}

float USpotLightComponent::GetInnerDegree() const
{
    return InnerRad * (180.0f / PI);
}

void USpotLightComponent::SetInnerDegree(float InInnerDegree)
{
    InnerRad = InInnerDegree * (PI / 180.0f);
}

float USpotLightComponent::GetOuterDegree() const
{
    return OuterRad * (180 / PI);
}

void USpotLightComponent::SetOuterDegree(float InOuterDegree)
{
    OuterRad = InOuterDegree * (PI / 180.0f);
}

float USpotLightComponent::GetAttenuation() const
{
    return Attenuation;
}

void USpotLightComponent::SetAttenuation(float InAttenuation)
{
    Attenuation = InAttenuation;
}
