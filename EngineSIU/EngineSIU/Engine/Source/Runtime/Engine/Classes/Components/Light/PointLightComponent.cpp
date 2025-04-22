#include "PointLightComponent.h"
#include "UObject/Casts.h"

UPointLightComponent::UPointLightComponent()
{
    Radius = 30.f;
    Attenuation = 20.0f;
}

UPointLightComponent::~UPointLightComponent()
{
}

UObject* UPointLightComponent::Duplicate(UObject* InOuter)
{
    UPointLightComponent* NewComponent = Cast<UPointLightComponent>(Super::Duplicate(InOuter));
    if (NewComponent)
    {
        NewComponent->Attenuation = Attenuation;
        NewComponent->Radius = Radius;
    }
    return NewComponent;
}

float UPointLightComponent::GetRadius() const
{
    return Radius;
}

void UPointLightComponent::SetRadius(float InRadius)
{
    Radius = InRadius;
}

float UPointLightComponent::GetAttenuation() const
{
    return Attenuation;
}

void UPointLightComponent::SetAttenuation(float InAttenuation)
{
    Attenuation = InAttenuation;
}
