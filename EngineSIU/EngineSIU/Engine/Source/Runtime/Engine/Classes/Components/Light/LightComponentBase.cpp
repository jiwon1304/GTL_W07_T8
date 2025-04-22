#include "LightComponent.h"
#include "UObject/Casts.h"

ULightComponentBase::ULightComponentBase()
{
    AABB.max = { 1.f,1.f,0.1f };
    AABB.min = { -1.f,-1.f,-0.1f };

    Intensity = 1000.0f;
    LightColor = FLinearColor(1, 1, 1, 1);
    bCastShadows = true;
}

ULightComponentBase::~ULightComponentBase()
{

}

UObject* ULightComponentBase::Duplicate(UObject* InOuter)
{
    ThisClass* NewComponent = Cast<ThisClass>(Super::Duplicate(InOuter));

    NewComponent->AABB = AABB;
    NewComponent->LightColor = LightColor;
    NewComponent->Intensity = Intensity;
    NewComponent->bCastShadows = bCastShadows;

    return NewComponent;
}

void ULightComponentBase::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);
}

int ULightComponentBase::CheckRayIntersection(FVector& rayOrigin, FVector& rayDirection, float& pfNearHitDistance)
{
    bool res = AABB.Intersect(rayOrigin, rayDirection, pfNearHitDistance);
    return res;
}

FLinearColor ULightComponentBase::GetLightColor() const
{
    return LightColor;
}

void ULightComponentBase::SetLightColor(const FLinearColor& InColor)
{
    LightColor = InColor;
}

float ULightComponentBase::GetIntensity() const
{
    return Intensity;
}

void ULightComponentBase::SetIntensity(float InIntensity)
{
    Intensity = InIntensity;
}

bool ULightComponentBase::GetCastShadowBoolean() const
{
    return bCastShadows;
}

void ULightComponentBase::SetCastShadowBoolean(bool InCastShadows)
{
    bCastShadows = InCastShadows;
}

