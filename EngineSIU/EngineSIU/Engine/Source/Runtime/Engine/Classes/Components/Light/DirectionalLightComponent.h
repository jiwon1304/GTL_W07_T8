#pragma once
#include "LightComponent.h"
#include "UObject/ObjectMacros.h"

class UDirectionalLightComponent : public ULightComponent
{
    DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)

public:
    UDirectionalLightComponent();
    virtual ~UDirectionalLightComponent() override;

    virtual UObject* Duplicate(UObject* InOuter) override;

    FVector GetDirection();

private:
    FDirectionalLightInfo DirectionalLightInfo;
};

