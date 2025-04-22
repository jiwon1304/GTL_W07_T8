#pragma once
#include "LightComponent.h"

class UAmbientLightComponent : public ULightComponent
{
    DECLARE_CLASS(UAmbientLightComponent, ULightComponent)

public:
    UAmbientLightComponent();
    virtual ~UAmbientLightComponent() override;

    virtual UObject* Duplicate(UObject* InOuter) override;

private:
    FAmbientLightInfo AmbientLightInfo;
};
