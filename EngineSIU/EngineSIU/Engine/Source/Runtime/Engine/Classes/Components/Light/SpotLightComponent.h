#pragma once
#include "LightComponent.h"

class USpotLightComponent :public ULightComponent
{

    DECLARE_CLASS(USpotLightComponent, ULightComponent)
public:
    USpotLightComponent();
    virtual ~USpotLightComponent();

    virtual UObject* Duplicate(UObject* InOuter) override;

    FVector GetDirection();

    float GetRadius() const;
    void SetRadius(float InRadius);

    float GetInnerRad() const;
    void SetInnerRad(float InInnerCos);

    float GetOuterRad() const;
    void SetOuterRad(float InOuterCos);

    float GetInnerDegree() const;
    void SetInnerDegree(float InInnerDegree);

    float GetOuterDegree() const;
    void SetOuterDegree(float InOuterDegree);

    float GetAttenuation() const;
    void SetAttenuation(float InAttenuation);
private:
    FSpotLightInfo SpotLightInfo;

    float InnerRad;
    float OuterRad;
    float Radius;
    float Attenuation;
};

