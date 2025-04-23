
#define MAX_LIGHTS 16 

#define MAX_DIRECTIONAL_LIGHT 16
#define MAX_POINT_LIGHT 16
#define MAX_SPOT_LIGHT 16
#define MAX_AMBIENT_LIGHT 16

#define POINT_LIGHT         1
#define SPOT_LIGHT          2
#define DIRECTIONAL_LIGHT   3
#define AMBIENT_LIGHT       4

struct FAmbientLightInfo
{
    float4 AmbientColor;
};

struct FDirectionalLightInfo
{
    float4 LightColor;

    float3 Direction;
    float Intensity;
    
    // [TEMP] 추후 들어오는 값에 따라 바뀔 수 있음
    int ShadowMapIndex; // 텍스처 배열 시작 인덱스
    int CastsShadows;
    
    float ShadowResolutionScale;
    float ShadowBias;
    float ShadowSlopeBias;
    float ShadowSharpen;
    
    float2 Padding;
};

struct FPointLightInfo
{
    float4 LightColor;

    float3 Position;
    float Radius;

    int Type;
    float Intensity;
    float Attenuation;
    
    // [TEMP] 추후 들어오는 값에 따라 바뀔 수 있음
    int ShadowMapIndex; // 텍스처 배열 시작 인덱스
    int CastsShadows;
    
    float ShadowResolutionScale;
    float ShadowBias;
    float ShadowSlopeBias;
    float ShadowSharpen;
    
    float3 Padding;
};

struct FSpotLightInfo
{
    float4 LightColor;

    float3 Position;
    float Radius;

    float3 Direction;
    float Intensity;

    int Type;
    float InnerRad;
    float OuterRad;
    float Attenuation;
    
    // [TEMP] 추후 들어오는 값에 따라 바뀔 수 있음
    int ShadowMapIndex; // 텍스처 배열 시작 인덱스
    int CastsShadows;
    
    float ShadowResolutionScale;
    float ShadowBias;
    float ShadowSlopeBias;
    float ShadowSharpen;
    
    float2 Padding;
};

// PoissonSample points
static const float2 PoissonSamples[32] =
{
    float2(-0.975402, -0.071138), float2(-0.920347, -0.411421),
    float2(-0.883908, 0.217872), float2(-0.884518, 0.568041),
    float2(-0.811945, 0.905914), float2(-0.446474, -0.891119),
    float2(-0.366831, -0.460642), float2(-0.344744, 0.137777),
    float2(-0.325663, 0.731335), float2(-0.107038, -0.796387),
    float2(-0.148229, -0.243912), float2(-0.165593, 0.544225),
    float2(-0.112812, 0.838630), float2(0.098818, -0.601404),
    float2(0.246710, -0.315761), float2(0.255873, 0.135252),
    float2(0.249166, 0.676046), float2(0.405266, 0.652483),
    float2(0.476016, 0.155594), float2(0.490905, -0.441405),
    float2(0.795899, -0.462350), float2(0.698856, -0.152325),
    float2(0.593628, 0.195342), float2(0.726165, 0.665112),
    float2(0.998631, 0.047394), float2(0.955596, 0.372224),
    float2(0.911023, -0.326685), float2(0.814097, 0.293714),
    float2(0.879595, 0.475554), float2(0.936791, -0.138803),
    float2(0.506965, -0.761049), float2(0.322733, -0.925850)
};

struct MatrixTransposed
{
    row_major matrix TransposedMat;
};

StructuredBuffer<MatrixTransposed> LigthViewProjection : register(t9);

// [TEMP] ShadowMap Slot / Comparison Sampler Slot 미정
Texture2DArray<float> ShadowMapArray : register(t8);
SamplerComparisonState ShadowSampler : register(s8);

// [TEMP] VSM용 ShadowMap / Sampler 바인딩 - 미구현
Texture2DArray<float2> VSMShadowMapArray : register(t10);
SamplerState VSMSampler : register(s10);

cbuffer Lighting : register(b0)
{
    int DirectionalLightsCount;
    int PointLightsCount;
    int SpotLightsCount;
    int AmbientLightsCount;
};

cbuffer ShadowConfigurations : register(b8)
{
    int ShadowFilterMode;
    float3 Padding;
};

StructuredBuffer<FAmbientLightInfo> AmbientLights : register(t20);
StructuredBuffer<FDirectionalLightInfo> DirectionalLights : register(t21);
StructuredBuffer<FSpotLightInfo> SpotLights : register(t22);
StructuredBuffer<FPointLightInfo> PointLights : register(t23);

// PCF
float FilterPCF(
    float2 ShadowUV,
    float TexelSize,
    float DepthReceiver,
    int ShadowIndex,
    int KernelSize = 3)
{
    float ShadowFactor = 0.0;
    const float KernelOffset = (KernelSize % 2 == 0) ? 0.5 : 0.0;
    const int HalfKernel = KernelSize / 2;
    
    for (int x = 0; x < KernelSize; ++x)
    {
        for (int y = 0; y < KernelSize; ++y)
        {
            float2 SampleOffset = float2(
                (x - (KernelSize - 1) / 2.0 + KernelOffset) * TexelSize,
                (y - (KernelSize - 1) / 2.0 + KernelOffset) * TexelSize
            );
            
            ShadowFactor += ShadowMapArray.SampleCmpLevelZero(
                ShadowSampler,
                float3(ShadowUV + SampleOffset, ShadowIndex),
                DepthReceiver
            );
        }
    }
    
    float NormalizationFactor = (KernelSize % 2 == 0) ?
        (KernelSize * KernelSize - 1) :
        (KernelSize * KernelSize);
        
    return ShadowFactor / NormalizationFactor;
}

// Poisson Filter
float FilterPoisson(
    float2 ShadowUV,
    float TexelSize,
    float DepthRecevier,
    int ShadowIndex,
    float Spread = 2.0,
    int SampleCount = 32)
{
    float ShadowFactor = 0.0;
    
    float2 SeedUV = ShadowUV * 1000.0;
    float RandomAngle = frac(sin(dot(SeedUV, float2(12.9898, 78.233))) * 43758.5453) * 6.2832;
    float2x2 RotMatrix = float2x2(cos(RandomAngle), -sin(RandomAngle),
                                  sin(RandomAngle), cos(RandomAngle));

    for (int i = 0; i < SampleCount; ++i)
    {
        float2 offset = mul(PoissonSamples[i], RotMatrix) * TexelSize * Spread;
        
        ShadowFactor += ShadowMapArray.SampleCmpLevelZero(
            ShadowSampler,
            float3(ShadowUV + offset, ShadowIndex),
            DepthRecevier
        );
    }
    return ShadowFactor / SampleCount;
}

// VSM - VSM용 2Channel depthmap 필요
float ChebyshevUpperBound(float2 Moments, float DepthRecevier)
{
    // One-tailed inequality valid if t > Moments.x
    float p = (DepthRecevier <= Moments.x);
    
    // Compute variance.
    float Variance = Moments.y - (Moments.x * Moments.x);
    Variance = max(Variance, 0.00001);
    
    // Compute probabilistic upper bound.
    float d = DepthRecevier - Moments.x;
    float pMax = Variance / (Variance + d * d);
    
    // Reduce light bleeding
    pMax = smoothstep(0.0, 1.0, pMax);
    
    return max(p, pMax);
    return p ? 1.0 : (1.0 - pMax);
}

float FilterVSM(float2 ShadowUV, float DepthRecevier, int ShadowIndex)
{
    float2 Moments = VSMShadowMapArray.Sample(VSMSampler, float3(ShadowUV, ShadowIndex)).rg;
    return ChebyshevUpperBound(Moments, DepthRecevier);
}

// Shadow 생성 여부 검사 Factor 1에 가까울수록 밝음
float CalculateShadowFactor(
    float4x4 LightViewProjection,
    int ShadowIndex,
    float3 WorldPos,
    float3 Normal,
    float3 LightDir,
    float BaseBias,
    float SlopeBias,
    float Sharpness
)
{
    // World to Light perspective
    float4 LightSpacePos = mul(float4(WorldPos, 1.0), LightViewProjection);
    LightSpacePos.xyz /= LightSpacePos.w;

    float2 ShadowUV = LightSpacePos.xy * 0.5 + 0.5;
    ShadowUV.y = 1.0 - ShadowUV.y;

    // Dynamic shadow bias
    float NdotL = dot(Normal, LightDir);
    float SlopeFactor = sqrt(1 - NdotL * NdotL);
    float Bias = BaseBias + SlopeBias * SlopeFactor;
    float DepthReceiver = LightSpacePos.z - Bias;

    // Texelsize - 정방형으로 가정함
    uint Width, Height, Elements;
    ShadowMapArray.GetDimensions(Width, Height, Elements);
    const float TexelSize = 1.0 / Width;
    
    float ShadowFactor;
    
    float AdjustedSharpness = saturate(Sharpness);
   
    // Filtering 적용
    if (any(ShadowUV < 0.0 || ShadowUV > 1.0)) 
        return 1.0f; // 텍스쳐 경계 바깥 Lit

    [branch]
    switch (ShadowFilterMode)
    {
        case 0:
        {
            ShadowFactor = ShadowMapArray.SampleCmpLevelZero(
                ShadowSampler,
                float3(ShadowUV, ShadowIndex),
                DepthReceiver
            );
                break;
            }
        case 1: // PCF
        {
            int KernelSize = lerp(9.0f, 1.0f, AdjustedSharpness);
            ShadowFactor = FilterPCF(ShadowUV, TexelSize, DepthReceiver, ShadowIndex, KernelSize);
            break;
        }
        case 2: // Poisson
        {
            float Spread = lerp(3.0, 0.0, AdjustedSharpness);
            int SampleCount = lerp(32, 4, AdjustedSharpness);
            ShadowFactor = FilterPoisson(ShadowUV, TexelSize, DepthReceiver, ShadowIndex, Spread, SampleCount);
            break;
        }
        case 3: // VSM
        {
            ShadowFactor = FilterVSM(ShadowUV, DepthReceiver, ShadowIndex);
            break;
        }
        default: // Default Hard Shadow
        {
            ShadowFactor = ShadowMapArray.SampleCmpLevelZero(
                ShadowSampler,
                float3(ShadowUV, ShadowIndex),
                DepthReceiver
            );
        }
    }

    return ShadowFactor;
}

float CalculateAttenuation(float Distance, float AttenuationFactor, float Radius)
{
    if (Distance > Radius)
    {
        return 0.0;
    }

    float Falloff = 1.0 / (1.0 + AttenuationFactor * Distance * Distance);
    float SmoothFactor = (1.0 - (Distance / Radius)); // 부드러운 falloff

    return Falloff * SmoothFactor;
}

float CalculateSpotEffect(float3 LightDir, float3 SpotDir, float InnerRadius, float OuterRadius, float SpotFalloff)
{
    float Dot = dot(-LightDir, SpotDir); // [-1,1]
    
    float SpotEffect = smoothstep(cos(OuterRadius / 2), cos(InnerRadius / 2), Dot);
    
    return SpotEffect * pow(max(Dot, 0.0), SpotFalloff);
}

float CalculateDiffuse(float3 WorldNormal, float3 LightDir)
{
    return max(dot(WorldNormal, LightDir), 0.0);
}

float CalculateSpecular(float3 WorldNormal, float3 ToLightDir, float3 ViewDir, float Shininess, float SpecularStrength = 0.5)
{
#ifdef LIGHTING_MODEL_GOURAUD
    float3 ReflectDir = reflect(-ToLightDir, WorldNormal);
    float Spec = pow(max(dot(ViewDir, ReflectDir), 0.0), Shininess);
#else
    float3 HalfDir = normalize(ToLightDir + ViewDir); // Blinn-Phong
    float Spec = pow(max(dot(WorldNormal, HalfDir), 0.0), Shininess);
#endif
    return Spec * SpecularStrength;
}

// WorldViewPosition parameter type float -> float3 변경, 왜인지 몰라 주석 남겨둠
float4 PointLight(int Index, float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor)
{
    FPointLightInfo LightInfo = PointLights[Index];
    
    float3 ToLight = LightInfo.Position - WorldPosition;
    float Distance = length(ToLight);
    
    float Attenuation = CalculateAttenuation(Distance, LightInfo.Attenuation, LightInfo.Radius);
    if (Attenuation <= 0.0)
    {
        return float4(0.f, 0.f, 0.f, 0.f);
    }
    
    float3 LightDir = normalize(ToLight);
    float DiffuseFactor = CalculateDiffuse(WorldNormal, LightDir);
    
    // Check Shadow
    float ShadowFactor = 1.f;
    
    if (LightInfo.CastsShadows)
    {
        for (int i = 0; i < 6; ++i)
        {
            ShadowFactor += CalculateShadowFactor(
                LigthViewProjection[LightInfo.ShadowMapIndex + i].TransposedMat,
                LightInfo.ShadowMapIndex + i,
                WorldPosition,
                WorldNormal,
                LightDir,
                LightInfo.ShadowBias,
                LightInfo.ShadowSlopeBias,
                LightInfo.ShadowSharpen
            );
        }
        ShadowFactor /= 6.0;
    }
    
    
#ifdef LIGHTING_MODEL_LAMBERT
    float3 Lit = (DiffuseFactor * DiffuseColor) * ShadowFactor * LightInfo.LightColor.rgb;
#else
    float3 ViewDir = normalize(WorldViewPosition - WorldPosition);
    float SpecularFactor = CalculateSpecular(WorldNormal, LightDir, ViewDir, Material.SpecularScalar);
    float3 Lit = ((DiffuseFactor * DiffuseColor) + (SpecularFactor * Material.SpecularColor)) * ShadowFactor * LightInfo.LightColor.rgb;
#endif
    
    return float4(Lit * Attenuation * LightInfo.Intensity, 1.0);
}

float4 SpotLight(int Index, float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor)
{
    FSpotLightInfo LightInfo = SpotLights[Index];
    
    float3 ToLight = LightInfo.Position - WorldPosition;
    float Distance = length(ToLight);
    
    float Attenuation = CalculateAttenuation(Distance, LightInfo.Attenuation, LightInfo.Radius);
    if (Attenuation <= 0.0)
    {
        return float4(0.0, 0.0, 0.0, 0.0);
    }
    
    float3 LightDir = normalize(ToLight);
    float SpotlightFactor = CalculateSpotEffect(LightDir, normalize(LightInfo.Direction), LightInfo.InnerRad, LightInfo.OuterRad, LightInfo.Attenuation);
    if (SpotlightFactor <= 0.0)
    {
        return float4(0.0, 0.0, 0.0, 0.0);
    }
    
    float DiffuseFactor = CalculateDiffuse(WorldNormal, LightDir);
    
    // Check Shadow
    float ShadowFactor = 1.0;
    
    if (LightInfo.CastsShadows)
    {
        ShadowFactor = CalculateShadowFactor(
            LigthViewProjection[LightInfo.ShadowMapIndex].TransposedMat,
            LightInfo.ShadowMapIndex,
            WorldPosition,
            WorldNormal,
            LightDir,
            LightInfo.ShadowBias,
            LightInfo.ShadowSlopeBias,
            LightInfo.ShadowSharpen
        );
    }
     
#ifdef LIGHTING_MODEL_LAMBERT
    float3 Lit = DiffuseFactor * DiffuseColor * ShadowFactor * LightInfo.LightColor.rgb;
#else
    float3 ViewDir = normalize(WorldViewPosition - WorldPosition);
    float SpecularFactor = CalculateSpecular(WorldNormal, LightDir, ViewDir, Material.SpecularScalar);
    float3 Lit = ((DiffuseFactor * DiffuseColor) + (SpecularFactor * Material.SpecularColor)) * ShadowFactor * LightInfo.LightColor.rgb;
#endif
    
    return float4(Lit * Attenuation * SpotlightFactor * LightInfo.Intensity, 1.0);
}

float4 DirectionalLight(int nIndex, float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor)
{
    FDirectionalLightInfo LightInfo = DirectionalLights[nIndex];
    
    float3 LightDir = normalize(-LightInfo.Direction);
    float3 ViewDir = normalize(WorldViewPosition - WorldPosition);
    float DiffuseFactor = CalculateDiffuse(WorldNormal, LightDir);
    
    // Check Shadow
    float ShadowFactor = 1.0;
    
    if (LightInfo.CastsShadows)
    {
        ShadowFactor = CalculateShadowFactor(
            LigthViewProjection[LightInfo.ShadowMapIndex].TransposedMat,
            LightInfo.ShadowMapIndex,
            WorldPosition,
            WorldNormal,
            LightDir,
            LightInfo.ShadowBias,
            LightInfo.ShadowSlopeBias,
            LightInfo.ShadowSharpen
        );
    }

#ifdef LIGHTING_MODEL_LAMBERT
    float3 Lit = DiffuseFactor * DiffuseColor * ShadowFactor * LightInfo.LightColor.rgb;
#else
    float SpecularFactor = CalculateSpecular(WorldNormal, LightDir, ViewDir, Material.SpecularScalar);
    float3 Lit = ((DiffuseFactor * DiffuseColor) + (SpecularFactor * Material.SpecularColor)) * LightInfo.LightColor.rgb;
#endif
    return float4(Lit * LightInfo.Intensity * ShadowFactor, 1.0);
}

float4 Lighting(float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor)
{
    float4 FinalColor = float4(0.0, 0.0, 0.0, 0.0);
    
    // 다소 비효율적일 수도 있음.
    [unroll(MAX_POINT_LIGHT)]
    for (int i = 0; i < PointLightsCount; i++)
    {
        FinalColor += PointLight(i, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor);
    }
    [unroll(MAX_SPOT_LIGHT)]
    for (int j = 0; j < SpotLightsCount; j++)
    {
        FinalColor += SpotLight(j, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor);
    }
    [unroll(MAX_DIRECTIONAL_LIGHT)]
    for (int k = 0; k < DirectionalLightsCount; k++)
    {
        FinalColor += DirectionalLight(k, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor);
    }
    [unroll(MAX_AMBIENT_LIGHT)]
    for (int l = 0; l < AmbientLightsCount; l++)
    {
        FinalColor += float4(AmbientLights[l].AmbientColor.rgb, 0.0);
        FinalColor.a = 1.0;
    }
    
    return FinalColor;
}
