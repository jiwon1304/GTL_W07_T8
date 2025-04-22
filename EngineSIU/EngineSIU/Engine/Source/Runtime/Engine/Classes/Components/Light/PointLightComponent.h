#pragma once
#include "LightComponent.h"

class UPointLightComponent :public ULightComponentBase
{

    DECLARE_CLASS(UPointLightComponent, ULightComponentBase)
public:
    UPointLightComponent();
    virtual ~UPointLightComponent() override;

    UObject* Duplicate(UObject* InOuter);

    const FPointLightInfo& GetPointLightInfo() const;
    void SetPointLightInfo(const FPointLightInfo& InPointLightInfo);

    float GetRadius() const;
    void SetRadius(float InRadius);

    FLinearColor GetLightColor() const;
    void SetLightColor(const FLinearColor& InColor);


    float GetIntensity() const;
    void SetIntensity(float InIntensity);

    int GetType() const;
    void SetType(int InType);

private:
    FPointLightInfo PointLightInfo;
};


