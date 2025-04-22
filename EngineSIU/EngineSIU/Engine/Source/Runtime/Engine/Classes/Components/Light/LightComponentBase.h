#pragma once
#include "Components/SceneComponent.h"
#include "UObject/ObjectMacros.h"

class ULightComponentBase : public USceneComponent
{
    DECLARE_CLASS(ULightComponentBase, USceneComponent)

public:
    ULightComponentBase();
    virtual ~ULightComponentBase() override;
    virtual UObject* Duplicate(UObject* InOuter) override;

    virtual void TickComponent(float DeltaTime) override;
    virtual int CheckRayIntersection(FVector& rayOrigin, FVector& rayDirection, float& pfNearHitDistance) override;

    FLinearColor GetLightColor() const;
    void SetLightColor(const FLinearColor& InColor);

    float GetIntensity() const;
    void SetIntensity(float InIntensity);

    bool GetCastShadowBoolean() const;
    void SetCastShadowBoolean(bool InCastShadows);

protected:
    FBoundingBox AABB;

public:
    FBoundingBox GetBoundingBox() const { return AABB; }

private:
    FLinearColor LightColor;
    float Intensity;
    bool bCastShadows;
};
