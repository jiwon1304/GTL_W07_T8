#include "AmbientLightComponent.h"
#include "UObject/Casts.h"

UAmbientLightComponent::UAmbientLightComponent()
{
    SetLightColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f));
    //AmbientLightInfo.AmbientColor = FLinearColor(1.f, 1.f, 1.f, 1.f);
}

UAmbientLightComponent::~UAmbientLightComponent()
{
}

UObject* UAmbientLightComponent::Duplicate(UObject* InOuter)
{
    UAmbientLightComponent* NewComponent = Cast<UAmbientLightComponent>(Super::Duplicate(InOuter));
    return NewComponent;
}

