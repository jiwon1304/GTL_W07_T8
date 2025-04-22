#pragma once
#include "LightComponent.h"

class UPointLightComponent :public ULightComponent
{

    DECLARE_CLASS(UPointLightComponent, ULightComponent)
public:
    UPointLightComponent();
    virtual ~UPointLightComponent() override;

    virtual UObject* Duplicate(UObject* InOuter) override;

    float GetRadius() const;
    void SetRadius(float InRadius);

    float GetAttenuation() const;
    void SetAttenuation(float InAttenuation);

private:
    FPointLightInfo PointLightInfo;

    float Attenuation;
    float Radius;
};


