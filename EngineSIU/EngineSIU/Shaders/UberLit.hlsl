#ifndef UBERLIT_HLSL
#define UBERLIT_HLSL

//#define USE_NORMAL_MAP
//#define USE_SPECULAR_MAP

#define DIRECTIONAL_LIGHT   0
#define POINT_LIGHT         1
#define SPOT_LIGHT          2

#define MAX_LIGHTS 16

// Light.hlsl
struct LIGHT
{
    float3 m_cDiffuse;
    float pad2;

    float3 m_cSpecular;
    float pad3;

    float3 m_vPosition;
    float m_fFalloff; // 스팟라이트의 감쇄 인자

    float3 m_vDirection;
    float pad4;

    float m_fAttenuation; // 거리 기반 감쇄 계수
    int m_bEnable;
    int m_nType;
    float m_fIntensity; // 광원 강도
    
    float m_fAttRadius; // 감쇄 반경 (Attenuation Radius)
    float3 LightPad;
};

struct Material
{
    float3 DiffuseColor; // 확산 색상
    float TransparencyScalar; // 투명도
    
    float3 AmbientColor; // 주변광 색상
    float DensityScalar; // 밀도
    
    float3 SpecularColor; // 반사 색상
    float SpecularPower; // 반사 강도 지수
    
    float3 EmissiveColor; // 발광 색상
    float padding; // 패딩
};

cbuffer cbLights : register(b0)
{
    LIGHT gLights[MAX_LIGHTS];
    float4 gcGlobalAmbientLight;
    int gnLights;
    float3 padCB;
}

cbuffer cbCamera : register(b1)
{
    row_major float4x4 gView; // 뷰 행렬
    row_major float4x4 gProjection; // 투영 행렬
    float3 gCameraPosition; // 카메라 위치
    float gPad1; // 패딩
}

cbuffer cbObject : register(b2)
{
    row_major float4x4 gWorld;
    row_major float4x4 gWorldInvTranspose;
    float4 gUUID;
    int gIsSelected;
    int gIsUnlit;
    int gPad2;
    int gPad3;
}

cbuffer cbMaterial : register(b3)
{
    Material gMaterial; // 재질 정보
}

Texture2D gDiffuseMap : register(t0);

#ifdef USE_NORMAL_MAP
Texture2D gNormalMap : register(t1);
#endif

#ifdef USE_SPECULAR_MAP
Texture2D gSpecularMap : register(t2);
#endif

SamplerState gSampler : register(s0);


struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 texcoord : TEXCOORD;
    float4 color : COLOR;
    int materialIndex : MATERIAL_INDEX;
};

struct PS_INPUT
{
    float4 position : SV_POSITION; // 화면 좌표
    float3 worldPos : TEXCOORD0; // 월드 위치
    float3 normal : NORMAL; // 노멀
    
#ifdef USE_NORMAL_MAP
    float3 tangent : TANGENT; // 탄젠트
    float3 binormal : BINORMAL; // 바이노멀
#endif
    
    float2 texcoord : TEXCOORD1; // UV
    float4 color : COLOR; // 정점 색상 (Gouraud 쉐이딩용)
    int materialIndex : MATERIAL_INDEX; // 머티리얼 인덱스
};

struct PS_OUTPUT
{
    float4 color : SV_Target0;
    float4 uuid : SV_Target1;
};

float3 ComputeLambertShading(float3 normal, float3 lightDir, float3 lightColor, float3 diffuseColor)
{
    float NdotL = max(dot(normal, lightDir), 0.0);
    return lightColor * diffuseColor * NdotL;
}

float3 ComputeBlinnPhongShading(float3 normal, float3 lightDir, float3 viewDir, float3 lightColor,
                               float3 diffuseColor, float3 specularColor, float specularPower)
{
    float NdotL = max(dot(normal, lightDir), 0.0);
    float3 diffuse = lightColor * diffuseColor * NdotL;
    
    if (NdotL > 0.0) // 반사광은 조명이 표면을 비출 때만 계산
    {
        float3 halfVector = normalize(lightDir + viewDir);
        float NdotH = max(dot(normal, halfVector), 0.0);
        float3 specular = lightColor * specularColor * pow(NdotH, specularPower * 4);
        return diffuse + specular;
    }
    
    return diffuse;
}

float3 DirectionalLight(int lightIndex, float3 normal, float3 viewDir, float3 diffuseColor, float3 specularColor, float specularPower)
{
    LIGHT light = gLights[lightIndex];
    
    if (light.m_bEnable == 0)
        return float3(0, 0, 0);
        
    float3 lightDir = normalize(-light.m_vDirection);
    
#ifdef LIGHTING_MODEL_LAMBERT
    return ComputeLambertShading(normal, lightDir, light.m_cDiffuse, diffuseColor) * light.m_fIntensity;
#elif defined(LIGHTING_MODEL_BLINN_PHONG)
    return ComputeBlinnPhongShading(normal, lightDir, viewDir, light.m_cDiffuse, 
                                     diffuseColor, specularColor, specularPower) * light.m_fIntensity;
#else
    return float3(0, 0, 0);
#endif
}

float3 PointLight(int lightIndex, float3 position, float3 normal, float3 viewDir,
                 float3 diffuseColor, float3 specularColor, float specularPower)
{
    LIGHT light = gLights[lightIndex];
    
    if (light.m_bEnable == 0)
        return float3(0, 0, 0);
        
    float3 lightVec = light.m_vPosition - position;
    float distance = length(lightVec);
    
    if (distance > light.m_fAttRadius)
        return float3(0, 0, 0);
        
    float3 lightDir = normalize(lightVec);
    float3 result = float3(0, 0, 0);
    
#ifdef LIGHTING_MODEL_LAMBERT
    result = ComputeLambertShading(normal, lightDir, light.m_cDiffuse, diffuseColor);
#elif defined(LIGHTING_MODEL_BLINN_PHONG)
    result = ComputeBlinnPhongShading(normal, lightDir, viewDir, light.m_cDiffuse, diffuseColor, specularColor, specularPower);
#endif
    
    float attenuation = 1.0 / (1.0 + light.m_fAttenuation * distance * distance);
    
    return result * attenuation * light.m_fIntensity;
}

float3 SpotLight(int lightIndex, float3 position, float3 normal, float3 viewDir, float3 diffuseColor, float3 specularColor, float specularPower)
{
    LIGHT light = gLights[lightIndex];
    
    if (light.m_bEnable == 0)
        return float3(0, 0, 0);
        
    float3 lightVec = light.m_vPosition - position;
    float distance = length(lightVec);
    
    if (distance > light.m_fAttRadius)
        return float3(0, 0, 0);
        
    float3 lightDir = normalize(lightVec);
    float3 result = float3(0, 0, 0);
    
#ifdef LIGHTING_MODEL_LAMBERT
    result = ComputeLambertShading(normal, lightDir, light.m_cDiffuse, diffuseColor);
#elif defined(LIGHTING_MODEL_BLINN_PHONG)
    result = ComputeBlinnPhongShading(normal, lightDir, viewDir, light.m_cDiffuse, diffuseColor, specularColor, specularPower);
#endif
    
    float attenuation = 1.0 / (1.0 + light.m_fAttenuation * distance * distance);
    
    float spotFactor = pow(max(dot(-lightDir, normalize(light.m_vDirection)), 0.0), light.m_fFalloff);
    
    return result * attenuation * spotFactor * light.m_fIntensity;
}

float3 ComputeLighting(float3 position, float3 normal, float3 diffuseColor, float3 specularColor, float specularPower)
{
    float3 totalLighting = gcGlobalAmbientLight.rgb * gMaterial.AmbientColor;
    float3 viewDir = normalize(gCameraPosition - position);
    
    [unroll(MAX_LIGHTS)]
    for (int i = 0; i < gnLights; i++)
    {
        if (gLights[i].m_bEnable)
        {
            if (gLights[i].m_nType == DIRECTIONAL_LIGHT)
            {
                totalLighting += DirectionalLight(i, normal, viewDir, diffuseColor, specularColor, specularPower);
            }
            else if (gLights[i].m_nType == POINT_LIGHT)
            {
                totalLighting += PointLight(i, position, normal, viewDir, diffuseColor, specularColor, specularPower);
            }
            else if (gLights[i].m_nType == SPOT_LIGHT)
            {
                totalLighting += SpotLight(i, position, normal, viewDir, diffuseColor, specularColor, specularPower);
            }

        }
    }
    
    return totalLighting;
}

#ifdef USE_NORMAL_MAP
float3 ApplyNormalMap(float3 normal, float3 tangent, float3 binormal, float2 texCoord)
{
    float3 normalMap = gNormalMap.Sample(gSampler, texCoord).xyz * 2.0 - 1.0;
    float3x3 TBN = float3x3(tangent, binormal, normal);
    return normalize(mul(normalMap, TBN));
}
#endif

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    float4 worldPosition = mul(float4(input.position, 1.0), gWorld);
    output.worldPos = worldPosition.xyz;
    output.position = mul(worldPosition, gView);
    output.position = mul(output.position, gProjection);
    
    output.normal = normalize(mul(input.normal, (float3x3) gWorldInvTranspose));
    
#ifdef USE_NORMAL_MAP
    output.tangent = normalize(mul(input.tangent, (float3x3) gWorld));
    output.binormal = cross(output.normal, output.tangent);
#endif
    
    output.texcoord = input.texcoord;
    output.materialIndex = input.materialIndex;

#ifdef LIGHTING_MODEL_GOURAUD
    if (gIsUnlit == 0)
    {
        float3 diffuseColor = gMaterial.DiffuseColor;
        
        if (input.texcoord.x > 0 || input.texcoord.y > 0)
        {
            diffuseColor *= gDiffuseMap.SampleLevel(gSampler, input.texcoord, 0).rgb;
        }
        
        // Gouraud: VS에서 라이팅 계산함
        float3 specularColor = gMaterial.SpecularColor;
#ifdef USE_SPECULAR_MAP
        if (input.texcoord.x > 0 || input.texcoord.y > 0)
        {
            float specularIntensity = gSpecularMap.SampleLevel(gSampler, input.texcoord, 0).r;
            specularColor *= specularIntensity;
        }
#endif
        
        float3 lighting = ComputeLighting(
            output.worldPos,
            output.normal,
            diffuseColor,
            specularColor,
            gMaterial.SpecularPower
        );
        
        output.color = float4(saturate(lighting), 1.0);
    }
    else
    {
        output.color = input.color;
    }
#else
    output.color = input.color;
#endif
    
    return output;
}

PS_OUTPUT mainPS(PS_INPUT input)
{
    PS_OUTPUT output;
    output.uuid = gUUID;
    
#ifdef USE_NORMAL_MAP
    float3 normal = ApplyNormalMap(input.normal, input.tangent, input.binormal, input.texcoord);
#else
    float3 normal = input.normal;
#endif
    
    float3 diffuseColor = gMaterial.DiffuseColor;
    float4 texColor = gDiffuseMap.Sample(gSampler, input.texcoord);
    
    bool hasTexture = any(texColor.rgb != float3(0, 0, 0));
    if (hasTexture)
    {
        diffuseColor *= texColor.rgb;
    }
    
    float3 specularColor = gMaterial.SpecularColor;
    
    
    if (gIsUnlit == 0)
    {
#ifdef LIGHTING_MODEL_GOURAUD
        output.color = float4(diffuseColor * input.color.rgb, 1.0);
#elif defined(LIGHTING_MODEL_LAMBERT) || defined(LIGHTING_MODEL_BLINN_PHONG)
        float3 lighting = ComputeLighting(
            input.worldPos,
            normal,
            diffuseColor,
            specularColor,
            gMaterial.SpecularPower
        );
        
        output.color = float4(saturate(lighting), 1.0);
#else
        // UNLIT
        output.color = float4(diffuseColor, 1.0);
#endif
    }
    else
    {
        output.color = float4(diffuseColor, 1.0);
    }
    
    // emissive가 SM_PS.hlsl의 FMaterial에는 있는데, 실제 구현부는 없음.
    //output.color.rgb += gMaterial.EmissiveColor;
    
    // 선택 효과
    if (gIsSelected == 1)
    {
        output.color += float4(0.2, 0.2, 0.2, 0.0);
    }
    
    return output;
}

#endif
