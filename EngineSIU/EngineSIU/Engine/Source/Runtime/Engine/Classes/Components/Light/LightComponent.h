#pragma once
#include "LightComponentBase.h"
#include "UObject/ObjectMacros.h"

class ULightComponent : public ULightComponentBase
{
    DECLARE_CLASS(ULightComponent, ULightComponentBase)

public:
    ULightComponent();
    ~ULightComponent();

    virtual UObject* Duplicate(UObject* InOuter) override;

    float GetShadowResolutionScale() const;
    void SetShadowResolutionScale(float InShadowResolution);

    float GetShadowBias() const;
    void SetShadowBias(float InBias);

    float GetShadowSlopeBias() const;
    void SetShadowSlopeBias(float InSlopeBias);

    float GetShadowSharpen() const;
    void SetShadowSharpen(float InSharpness);


private:
    // Currently not use resolution member
    float ShadowResolutionScale;
    float ShadowBias;
    float ShadowSlopeBias;
    float ShadowSharpen;
};
