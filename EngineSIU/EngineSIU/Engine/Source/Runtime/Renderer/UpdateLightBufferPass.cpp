#include "Define.h"
#include "UObject/Casts.h"
#include "UpdateLightBufferPass.h"
#include "D3D11RHI/DXDBufferManager.h"
#include "D3D11RHI/GraphicDevice.h"
#include "D3D11RHI/DXDShaderManager.h"
#include "Components/Light/LightComponent.h"
#include "Components/Light/PointLightComponent.h"
#include "Components/Light/SpotLightComponent.h"
#include "Components/Light/DirectionalLightComponent.h"
#include "Components/Light/AmbientLightComponent.h"
#include "Engine/EditorEngine.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectIterator.h"
#include "ShadowPass.h"

//------------------------------------------------------------------------------
// 생성자/소멸자
//------------------------------------------------------------------------------
FUpdateLightBufferPass::FUpdateLightBufferPass()
    : BufferManager(nullptr)
    , Graphics(nullptr)
    , ShaderManager(nullptr)
{
}

FUpdateLightBufferPass::~FUpdateLightBufferPass()
{
}

void FUpdateLightBufferPass::Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager)
{
    BufferManager = InBufferManager;
    Graphics = InGraphics;
    ShaderManager = InShaderManager;

    // Ambient Light Buffer
    BufferManager->CreateStructuredBuffer(
        AmbientLightBufferKey,
        sizeof(FAmbientLightInfo) * MAX_AMBIENT_LIGHT,
        D3D11_BIND_SHADER_RESOURCE,
        D3D11_USAGE_DYNAMIC,
        D3D11_CPU_ACCESS_WRITE,
        sizeof(FAmbientLightInfo),
        MAX_AMBIENT_LIGHT
    );

    BufferManager->CreateStructuredBuffer(
        DirectionalLightBufferKey,
        sizeof(FDirectionalLightInfo) * MAX_DIRECTIONAL_LIGHT,
        D3D11_BIND_SHADER_RESOURCE,
        D3D11_USAGE_DYNAMIC,
        D3D11_CPU_ACCESS_WRITE,
        sizeof(FDirectionalLightInfo),
        MAX_DIRECTIONAL_LIGHT
    );

    BufferManager->CreateStructuredBuffer(
        SpotLightBufferKey,
        sizeof(FSpotLightInfo) * MAX_SPOT_LIGHT,
        D3D11_BIND_SHADER_RESOURCE,
        D3D11_USAGE_DYNAMIC,
        D3D11_CPU_ACCESS_WRITE,
        sizeof(FSpotLightInfo),
        MAX_SPOT_LIGHT
    );

    BufferManager->CreateStructuredBuffer(
        PointLightBufferKey,
        sizeof(FPointLightInfo) * MAX_POINT_LIGHT,
        D3D11_BIND_SHADER_RESOURCE,
        D3D11_USAGE_DYNAMIC,
        D3D11_CPU_ACCESS_WRITE,
        sizeof(FPointLightInfo),
        MAX_POINT_LIGHT
    );
}

void FUpdateLightBufferPass::PrepareRender()
{
    for (const auto iter : TObjectRange<ULightComponentBase>())
    {
        if (iter->GetWorld() == GEngine->ActiveWorld)
        {
            if (UPointLightComponent* PointLight = Cast<UPointLightComponent>(iter))
            {
                PointLights.Add(PointLight);
            }
            else if (USpotLightComponent* SpotLight = Cast<USpotLightComponent>(iter))
            {
                SpotLights.Add(SpotLight);
            }
            else if (UDirectionalLightComponent* DirectionalLight = Cast<UDirectionalLightComponent>(iter))
            {
                DirectionalLights.Add(DirectionalLight);
            }
            // Begin Test
            else if (UAmbientLightComponent* AmbientLight = Cast<UAmbientLightComponent>(iter))
            {
                AmbientLights.Add(AmbientLight);
            }
            // End Test
        }
    }
}

void FUpdateLightBufferPass::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    UpdateLightBuffer();
}

void FUpdateLightBufferPass::ClearRenderArr()
{
    PointLights.Empty();
    SpotLights.Empty();
    DirectionalLights.Empty();
    AmbientLights.Empty();
}


void FUpdateLightBufferPass::UpdateLightBuffer() const
{
    TArray<FAmbientLightInfo> AmbientData;
    TArray<FDirectionalLightInfo> DirectionalData;
    TArray<FPointLightInfo> PointData;
    TArray<FSpotLightInfo> SpotData;

    for (auto Light : AmbientLights)
    {
        if (AmbientData.Num() < MAX_AMBIENT_LIGHT)
        {
            FAmbientLightInfo Info;
            Info.LightColor = Light->GetLightColor();
            AmbientData.Add(Info);
        }
    }

    for (auto Light : DirectionalLights)
    {
        if (DirectionalData.Num() < MAX_DIRECTIONAL_LIGHT)
        {
            FDirectionalLightInfo Info;
            Info.LightColor = Light->GetLightColor();
            Info.Intensity = Light->GetIntensity();
            Info.Direction = Light->GetDirection();
            Info.ShadowMapIndex = FShadowPass::GetShadowMapIndex(Light)[0];
            Info.CastShadow = Light->GetCastShadowBoolean() ? 1 : 0;
            Info.ShadowResolutionScale = Light->GetShadowResolutionScale();
            Info.ShadowBias = Light->GetShadowBias();
            Info.ShadowSlopeBias = Light->GetShadowSlopeBias();
            Info.ShadowSharpen = Light->GetShadowSharpen();
            DirectionalData.Add(Info);
        }
    }

    for (auto Light : SpotLights)
    {
        if (SpotLights.Num() < MAX_SPOT_LIGHT)
        {
            FSpotLightInfo Info;
            Info.LightColor = Light->GetLightColor();
            Info.Intensity = Light->GetIntensity();
            Info.Position = Light->GetWorldLocation();
            Info.Direction = Light->GetDirection();
            Info.Radius = Light->GetRadius();
            Info.Attenuation = Light->GetAttenuation();
            Info.InnerRad = Light->GetInnerRad();
            Info.OuterRad = Light->GetOuterRad();
            Info.Type = ELightType::SPOT_LIGHT;
            Info.ShadowMapIndex = FShadowPass::GetShadowMapIndex(Light)[0];
            Info.CastShadow = Light->GetCastShadowBoolean() ? 1 : 0;
            Info.ShadowResolutionScale = Light->GetShadowResolutionScale();
            Info.ShadowBias = Light->GetShadowBias();
            Info.ShadowSlopeBias = Light->GetShadowSlopeBias();
            Info.ShadowSharpen = Light->GetShadowSharpen();
            SpotData.Add(Info);
        }
    }
   
    for (auto Light : PointLights)
    {
        if (PointLights.Num() < MAX_POINT_LIGHT)
        {
            FPointLightInfo Info;
            Info.LightColor = Light->GetLightColor();
            Info.Intensity = Light->GetIntensity();
            Info.Position = Light->GetWorldLocation();
            Info.Radius = Light->GetRadius();
            Info.Attenuation = Light->GetAttenuation();
            Info.Type = ELightType::POINT_LIGHT;
            Info.ShadowMapIndex = FShadowPass::GetShadowMapIndex(Light)[0];
            Info.CastShadow = Light->GetCastShadowBoolean() ? 1 : 0;
            Info.ShadowResolutionScale = Light->GetShadowResolutionScale();
            Info.ShadowBias = Light->GetShadowBias();
            Info.ShadowSlopeBias = Light->GetShadowSlopeBias();
            Info.ShadowSharpen = Light->GetShadowSharpen();
            PointData.Add(Info);
        }
    }

    BufferManager->UpdateStructuredBuffer(
        AmbientLightBufferKey,
        AmbientData
    );

    BufferManager->UpdateStructuredBuffer(
        DirectionalLightBufferKey,
        DirectionalData
    );

    BufferManager->UpdateStructuredBuffer(
        SpotLightBufferKey,
        SpotData
    );
    BufferManager->UpdateStructuredBuffer(
        PointLightBufferKey,
        PointData
    );

    BufferManager->BindStructuredBuffer(AmbientLightBufferKey, 20, EShaderStage::Vertex, EShaderViewType::SRV);
    BufferManager->BindStructuredBuffer(DirectionalLightBufferKey, 21, EShaderStage::Vertex, EShaderViewType::SRV);
    BufferManager->BindStructuredBuffer(SpotLightBufferKey, 22, EShaderStage::Vertex, EShaderViewType::SRV);
    BufferManager->BindStructuredBuffer(PointLightBufferKey, 23, EShaderStage::Vertex, EShaderViewType::SRV);

    BufferManager->BindStructuredBuffer(AmbientLightBufferKey, 20, EShaderStage::Pixel, EShaderViewType::SRV);
    BufferManager->BindStructuredBuffer(DirectionalLightBufferKey, 21, EShaderStage::Pixel, EShaderViewType::SRV);
    BufferManager->BindStructuredBuffer(SpotLightBufferKey, 22, EShaderStage::Pixel, EShaderViewType::SRV);
    BufferManager->BindStructuredBuffer(PointLightBufferKey, 23, EShaderStage::Pixel, EShaderViewType::SRV);

    FLightInfoBuffer Counts;
    Counts.AmbientLightsCount = AmbientData.Num();
    Counts.DirectionalLightsCount = DirectionalData.Num();
    Counts.PointLightsCount = PointData.Num();
    Counts.SpotLightsCount = SpotData.Num();

    BufferManager->UpdateConstantBuffer(
        FString("FLightInfoBuffer"),
        Counts
    );
}
