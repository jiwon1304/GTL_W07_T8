#include "LightComponent.h"
#include "UObject/Casts.h"

ULightComponent::ULightComponent()
{
    ShadowResolutionScale = 0;
    ShadowBias = 0.0001;
    ShadowSlopeBias = 0.0001;
    ShadowSharpen = 0;
}

ULightComponent::~ULightComponent()
{
  
}

UObject* ULightComponent::Duplicate(UObject* InOuter)
{
    ULightComponent* NewComponent = Cast<ULightComponent>(Super::Duplicate(InOuter));
    if (NewComponent)
    {
        NewComponent->ShadowResolutionScale = ShadowResolutionScale;
        NewComponent->ShadowBias = ShadowBias;
        NewComponent->ShadowSlopeBias = ShadowSlopeBias;
        NewComponent->ShadowSharpen = ShadowSharpen;
    }
    return NewComponent;
}

float ULightComponent::GetShadowResolutionScale() const
{
    return ShadowResolutionScale;
}

void ULightComponent::SetShadowResolutionScale(float InShadowResolution)
{
    ShadowResolutionScale = InShadowResolution;
}

float ULightComponent::GetShadowBias() const
{
    return ShadowBias;
}

void ULightComponent::SetShadowBias(float InBias)
{
    ShadowBias = InBias;
}

float ULightComponent::GetShadowSlopeBias() const
{
    return ShadowSlopeBias;
}

void ULightComponent::SetShadowSlopeBias(float InSlopeBias)
{
    ShadowSlopeBias = InSlopeBias;
}

float ULightComponent::GetShadowSharpen() const
{
    return ShadowSharpen;
}

void ULightComponent::SetShadowSharpen(float InSharpness)
{
    ShadowSharpen = InSharpness;
}
