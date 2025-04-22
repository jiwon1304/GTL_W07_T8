#pragma once
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "Math/Matrix.h"
#define MAX_AMBIENT_LIGHT 16
#define MAX_DIRECTIONAL_LIGHT 16
#define MAX_POINT_LIGHT 16
#define MAX_SPOT_LIGHT 16

struct FAmbientLightInfo
{
    FLinearColor AmbientColor;         // RGB + alpha
};

struct FDirectionalLightInfo
{
    FLinearColor LightColor;         // RGB + alpha

    FVector Direction;   // 정규화된 광선 방향 (월드 공간 기준)
    float   Intensity;   // 밝기

    int     ShadowMapIndex; 
    int     CastShadow; // 그림자 생성 여부
    float   Pad1;
    float   Pad2;
};

struct FPointLightInfo
{
    FLinearColor LightColor;         // RGB + alpha

    FVector Position;    // 월드 공간 위치
    float   Radius;      // 감쇠가 0이 되는 거리

    int     Type;        // 라이트 타입 구분용 (예: 1 = Point)
    float   Intensity;   // 밝기
    float   Attenuation;
    int     ShadowMapIndex;

    int     CastShadow;
    FVector Padding;
};

struct FSpotLightInfo
{
    FLinearColor LightColor;         // RGB + alpha

    FVector Position;       // 월드 공간 위치
    float   Radius;         // 감쇠 거리

    FVector Direction;      // 빛이 향하는 방향 (normalize)
    float   Intensity;      // 밝기

    int     Type;           // 라이트 타입 구분용 (예: 2 = Spot)
    float   InnerRad; // cos(inner angle)
    float   OuterRad; // cos(outer angle)
    float   Attenuation;

    int     ShadowMapIndex;
    int     CastShadow;
    FVector2D Padding;
};

struct FLightInfoBuffer
{
    int PointLightsCount;
    int SpotLightsCount;
    int AmbientLightsCount;
};
