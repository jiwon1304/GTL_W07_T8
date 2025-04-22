#include "DirectionalLightComponent.h"
#include "Components/SceneComponent.h"
#include "Math/Rotator.h"
#include "Math/Quat.h"
#include "UObject/Casts.h"

UDirectionalLightComponent::UDirectionalLightComponent()
{
}

UDirectionalLightComponent::~UDirectionalLightComponent()
{
}

UObject* UDirectionalLightComponent::Duplicate(UObject* InOuter)
{
    UDirectionalLightComponent* NewComponent = Cast<UDirectionalLightComponent>(Super::Duplicate(InOuter));
    return NewComponent;
}

FVector UDirectionalLightComponent::GetDirection()  
{
    FRotator rotator = GetWorldRotation();
    FVector WorldDown= rotator.ToQuaternion().RotateVector(-GetUpVector());
    return WorldDown;  
}
