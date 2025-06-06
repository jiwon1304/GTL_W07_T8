#include "ShadowPass.h"
#include "Renderer.h"
#include <UObject/UObjectIterator.h>
#include <UObject/Casts.h>
#include "Components/Light/LightComponent.h"
#include "Components/Light/DirectionalLightComponent.h"
#include "Components/Light/PointLightComponent.h"
#include "Components/Light/SpotLightComponent.h"
#include "RendererHelpers.h"
#include "UnrealClient.h"
#include "UnrealEd/EditorViewportClient.h"
#include "Runtime/Windows/D3D11RHI/DXDShaderManager.h"
#include "Runtime/Windows/D3D11RHI/DXDBufferManager.h"
#include "Runtime/Windows/D3D11RHI/GraphicDevice.h"
#include "Runtime/Core/Math/JungleMath.h"
#include "Engine/Classes/Components/Material/Material.h"
#include "Components/StaticMeshComponent.h"
#include "BaseGizmos/GizmoBaseComponent.h"
#include "Engine/EditorEngine.h"


ID3D11Texture2D* FShadowPass::ShadowMapTexture;
ID3D11Texture2D* FShadowPass::ShadowMapTextureVSM;
ID3D11Texture2D* FShadowPass::ShadowMapDepthVSM;
TArray<ID3D11DepthStencilView*> FShadowPass::ShadowMapDSV;
TArray<ID3D11DepthStencilView*> FShadowPass::ShadowMapDSVVSM;
TArray<ID3D11RenderTargetView*> FShadowPass::ShadowMapRTV;
ID3D11ShaderResourceView* FShadowPass::ShadowMapSRV;
ID3D11ShaderResourceView* FShadowPass::ShadowMapSRVVSM;
TMap<ULightComponentBase*, TArray<uint32>> FShadowPass::IndicesMap;
D3D11_VIEWPORT FShadowPass::ShadowMapViewport;
ID3D11SamplerState* FShadowPass::ShadowMapSampler;
ID3D11SamplerState* FShadowPass::ShadowMapSamplerVSM;
EShadowFilterMethod FShadowPass::CurrentShadowFilterMode = EShadowFilterMethod::NONE;

void FShadowPass::Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage)
{
    BufferManager = InBufferManager;
    Graphics = InGraphics;
    ShaderManager = InShaderManage;

    CreateShader();
    CreateTexture(TextureSize, NumShadowMaps);
    CreateBuffer(NumShadowMaps);
}

void FShadowPass::PrepareRender()
{
    for (const auto iter : TObjectRange<UStaticMeshComponent>())
    {
        if (!Cast<UGizmoBaseComponent>(iter) && iter->GetWorld() == GEngine->ActiveWorld)
        {
            StaticMeshComponents.Add(iter);
        }
    }
}

void FShadowPass::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    UpdatePerspectiveShadowMap(Viewport);

    ShaderManager->SetVertexShaderAndInputLayout(VertexShaderBufferKey, Graphics->DeviceContext);

    if (CurrentShadowFilterMode == EShadowFilterMethod::VSM) {
        ShaderManager->SetPixelShader(PixelShaderBufferKey, Graphics->DeviceContext);
    }
    else
    {
        ShaderManager->SetPixelShaderNull(Graphics->DeviceContext);
    }

    BufferManager->BindConstantBuffer(ShadowConfigBufferKey, 8, EShaderStage::Vertex);
    BufferManager->BindConstantBuffer(ShadowConfigBufferKey, 8, EShaderStage::Pixel);

    BufferManager->BindStructuredBuffer(TransformDataBufferKey, TransformSRVSlot, EShaderStage::Vertex, EShaderViewType::SRV); // 실제 matrix
    BufferManager->BindConstantBuffer(ViewProjTransformBufferKey, ViewProjTransformCBSlot, EShaderStage::Vertex); // index만
    BufferManager->BindConstantBuffer(WorldTransformBufferKey, WorldTransformCBSlot, EShaderStage::Vertex); // worldmatrix

    BufferManager->BindStructuredBuffer(TransformDataBufferKey, TransformSRVSlot, EShaderStage::Pixel, EShaderViewType::SRV);
    BufferManager->BindConstantBuffer(ViewProjTransformBufferKey, ViewProjTransformCBSlot, EShaderStage::Pixel);
    BufferManager->BindConstantBuffer(WorldTransformBufferKey, WorldTransformCBSlot, EShaderStage::Pixel);

    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Graphics->DeviceContext->RSSetViewports(1, &ShadowMapViewport);
    UsedShadowMaps = 0;
    UsedShadowMapsForDir = 0;
    UsedShadowMapsForPoint = 0;
    UsedShadowMapsForSpot = 0;

    for (const auto& Pair : IndicesMap)
    {
        ULightComponentBase* Light = Pair.Key;
        const TArray<uint32>& Indices = Pair.Value; // transform(view proj) 동시에, texture2d까지

        // 사용 예
        for (uint32 Index : Indices)
        {
            if (UsedShadowMaps >= NumShadowMaps)
            {
                Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
                return;
            }
            UsedShadowMaps++;
            if (Light->IsA<UDirectionalLightComponent>())
            {
                UsedShadowMapsForDir++;
            }
            else if (Light->IsA<UPointLightComponent>())
            {
                UsedShadowMapsForPoint++;
            }
            else if (Light->IsA<USpotLightComponent>())
            {
                UsedShadowMapsForSpot++;
            }

            BufferManager->UpdateConstantBuffer(ViewProjTransformBufferKey, Index);
            if (CurrentShadowFilterMode == EShadowFilterMethod::VSM)
            {
                ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
                //// 픽셀 셰이더 슬롯 10 언바인드
                Graphics->DeviceContext->PSSetShaderResources(10, 1, nullSRVs);
                // VSM: RTV 바인딩 및 클리어
                FLOAT clearColor[4] = { 1.0f, 1.0f, 0.0f, 1.0f };
                Graphics->DeviceContext->ClearRenderTargetView(
                    ShadowMapRTV[Index],
                    clearColor
                );
                Graphics->DeviceContext->ClearDepthStencilView(
                    ShadowMapDSVVSM[Index],
                    D3D11_CLEAR_DEPTH,
                    1.0f,
                    0
                );
                Graphics->DeviceContext->OMSetRenderTargets(
                    1,
                    &ShadowMapRTV[Index],
                    ShadowMapDSVVSM[Index]
                );
            }
            else
            {
                Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, ShadowMapDSV[Index]);
                Graphics->DeviceContext->ClearDepthStencilView(ShadowMapDSV[Index], D3D11_CLEAR_DEPTH, 1.0f, 0);
            }
            // OMSetrendertargets에서 DSV연결
            // draw -> texture2d에 그려짐
            // ... Do something with Index ...

            for (UStaticMeshComponent* Comp : StaticMeshComponents)
            {
                if (!Comp || !Comp->GetStaticMesh())
                {
                    continue;
                }

                OBJ::FStaticMeshRenderData* RenderData = Comp->GetStaticMesh()->GetRenderData();
                if (RenderData == nullptr)
                {
                    continue;
                }

                UEditorEngine* Engine = Cast<UEditorEngine>(GEngine);

                FMatrix WorldMatrix = Comp->GetWorldMatrix();

                BufferManager->UpdateConstantBuffer(WorldTransformBufferKey, WorldMatrix);

                RenderPrimitive(RenderData, Comp->GetStaticMesh()->GetMaterials(), Comp->GetOverrideMaterials(), Comp->GetselectedSubMeshIndex());
            }
        }
    }
    ID3D11RenderTargetView* nullRTV = nullptr;
    ID3D11DepthStencilView* nullDSV = nullptr;
    Graphics->DeviceContext->OMSetRenderTargets(0, &nullRTV, nullDSV);
    Graphics->DeviceContext->OMSetRenderTargets(1, &nullRTV, nullDSV);
}

void FShadowPass::ClearRenderArr()
{
    StaticMeshComponents.Empty();
}

bool FShadowPass::UpdateShadowMap(uint32 InTextureSize, uint32 InNumMaps)
{

    if (InTextureSize == 0)
    {
        InTextureSize = TextureSize;
    }
    if (InNumMaps == 0)
    {
        InNumMaps = NumShadowMaps;
    }
    uint64 TotalSize = InTextureSize * InTextureSize * InNumMaps;

    if (InTextureSize & (InTextureSize - 1) != 0)
    {
        UE_LOG(LogLevel::Error, "Texture size must be power of 2");
        return false;
    }

    if (TotalSize > MaxSize)
    {
        UE_LOG(LogLevel::Error, "Maximum size exceeded");
        return false;
    }
    TextureSize = InTextureSize;
    NumShadowMaps = InNumMaps;
    ReleaseTexture();
    CreateTexture(TextureSize, NumShadowMaps);

    return true;
}

void FShadowPass::SetShadowFilterMode(EShadowFilterMethod InFilterMode)
{
    CurrentShadowFilterMode = InFilterMode;

    FShadowConfigurations Settings;
    Settings.FilterMode = static_cast<int>(CurrentShadowFilterMode);

    BufferManager->UpdateConstantBuffer(
        ShadowConfigBufferKey,
        Settings
    );
}

HRESULT FShadowPass::CreateShader()
{
    D3D11_INPUT_ELEMENT_DESC StaticMeshLayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"MATERIAL_INDEX", 0, DXGI_FORMAT_R32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    D3D11_SAMPLER_DESC SamplerDesc;
    ZeroMemory(&SamplerDesc, sizeof(SamplerDesc));
    SamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    SamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    SamplerDesc.MinLOD = 0;
    SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    SamplerDesc.BorderColor[0] = 1.0f;
    SamplerDesc.BorderColor[1] = 1.0f;
    SamplerDesc.BorderColor[2] = 1.0f;
    SamplerDesc.BorderColor[3] = 1.0f;

    ID3D11SamplerState* pShadowSampler;
    Graphics->Device->CreateSamplerState(&SamplerDesc, &ShadowMapSampler);

    D3D11_SAMPLER_DESC vsmSamplerDesc = {};
    vsmSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    vsmSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    vsmSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    vsmSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    vsmSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    vsmSamplerDesc.MinLOD = 0;
    vsmSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    ID3D11SamplerState* vsmSampler = nullptr;
    Graphics->Device->CreateSamplerState(&vsmSamplerDesc, &ShadowMapSamplerVSM);

    ShaderManager->AddPixelShader(PixelShaderBufferKey, L"Shaders/ShadowDepthMapShader.hlsl", "mainPS");
    return ShaderManager->AddVertexShaderAndInputLayout(VertexShaderBufferKey, L"Shaders/ShadowDepthMapShader.hlsl", "mainVS", StaticMeshLayoutDesc, ARRAYSIZE(StaticMeshLayoutDesc));
}

/// <summary>
/// Shadow map을 저장할 배열을 생성합니다.
/// </summary>
/// <param name="TextureSize"></param> 정방형 Texture의 크기입니다. (TextureSize * TextureSize)
/// <param name="NumMaps"></param> 배열의 길이입니다.
HRESULT FShadowPass::CreateTexture(uint32 TextureSize, uint32 NumMaps)
{
    if (NumMaps > 1024)
    {
        UE_LOG(LogLevel::Error, TEXT("Texture2DArray의 최대 크기 4096를 넘었습니다."));
        return S_FALSE;
    }

    NumShadowMaps = NumMaps;
    // 1. Depth Texture 생성
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = TextureSize;
    texDesc.Height = TextureSize;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = NumMaps;
    texDesc.Format = DXGI_FORMAT_R32_TYPELESS; // Typeless에서 변경했는데 될지 모름...
    texDesc.SampleDesc.Count = 1;
    texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.MiscFlags = 0;
    texDesc.CPUAccessFlags = 0;

    // VSM Depth
    D3D11_TEXTURE2D_DESC depthDescVSM = {};
    depthDescVSM.Width = TextureSize;
    depthDescVSM.Height = TextureSize;
    depthDescVSM.MipLevels = 1;
    depthDescVSM.ArraySize = NumMaps;
    depthDescVSM.Format = DXGI_FORMAT_D32_FLOAT;
    depthDescVSM.SampleDesc.Count = 1;
    depthDescVSM.Usage = D3D11_USAGE_DEFAULT;
    depthDescVSM.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    // VSM Texture
    D3D11_TEXTURE2D_DESC texDescVSM = {};
    texDescVSM.Width = TextureSize;
    texDescVSM.Height = TextureSize;
    texDescVSM.MipLevels = 1;
    texDescVSM.ArraySize = NumMaps;
    texDescVSM.Format = DXGI_FORMAT_R16G16_TYPELESS;
    texDescVSM.SampleDesc.Count = 1;
    texDescVSM.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texDescVSM.Usage = D3D11_USAGE_DEFAULT;
    texDescVSM.MiscFlags = 0;
    texDescVSM.CPUAccessFlags = 0;

    HRESULT hr = Graphics->Device->CreateTexture2D(&texDesc, nullptr, &ShadowMapTexture);
    if (FAILED(hr)) {
        return hr;
    }

    Graphics->Device->CreateTexture2D(&depthDescVSM, nullptr, &ShadowMapDepthVSM);

    // VSM Texture
    Graphics->Device->CreateTexture2D(&texDescVSM, nullptr, &ShadowMapTextureVSM);

    // 2. DepthStencilView 생성
    for (int i = 0; i < NumMaps; ++i)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT; // Typeless의 실질적인 Depth format
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.MipSlice = 0;
        dsvDesc.Texture2DArray.FirstArraySlice = i;
        dsvDesc.Texture2DArray.ArraySize = 1;

        ID3D11DepthStencilView* DSV;
        hr = Graphics->Device->CreateDepthStencilView(ShadowMapTexture, &dsvDesc, &DSV);
        if (FAILED(hr)) {
            return hr;
        }
        ShadowMapDSV.Add(DSV);
    }

    // VSM DSV
    for (int i = 0; i < NumMaps; ++i) {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.MipSlice = 0;
        dsvDesc.Texture2DArray.FirstArraySlice = i;
        dsvDesc.Texture2DArray.ArraySize = 1;

        ID3D11DepthStencilView* DSV;
        Graphics->Device->CreateDepthStencilView(ShadowMapDepthVSM, &dsvDesc, &DSV);
        ShadowMapDSVVSM.Add(DSV);
    }

    // VSM RTVs
    for (int i = 0; i < NumMaps; ++i)
    {
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.MipSlice = 0;
        rtvDesc.Texture2DArray.FirstArraySlice = i;
        rtvDesc.Texture2DArray.ArraySize = 1;

        ID3D11RenderTargetView* RTV;
        hr = Graphics->Device->CreateRenderTargetView(ShadowMapTextureVSM, &rtvDesc, &RTV);
        if (FAILED(hr)) {
            return hr;
        }
        ShadowMapRTV.Add(RTV);
    }

    // 3. ShaderResourceView 생성
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT; // Typeless와 호환되는 SRV 포맷
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.ArraySize = NumMaps;

    hr = Graphics->Device->CreateShaderResourceView(ShadowMapTexture, &srvDesc, &ShadowMapSRV);
    if (FAILED(hr)) {
        return hr;
    }

    // SRV for VSM
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDescVSM = {};
    srvDescVSM.Format = DXGI_FORMAT_R16G16_FLOAT; // 2-Channel format
    srvDescVSM.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDescVSM.Texture2DArray.MostDetailedMip = 0;
    srvDescVSM.Texture2DArray.MipLevels = 1;
    srvDescVSM.Texture2DArray.FirstArraySlice = 0;
    srvDescVSM.Texture2DArray.ArraySize = NumMaps;

    hr = Graphics->Device->CreateShaderResourceView(ShadowMapTextureVSM, &srvDescVSM, &ShadowMapSRVVSM);
    if (FAILED(hr)) {
        return hr;
    }

    // ShadowMap 텍스처는 내부적으로 관리되므로 참조 해제
    ShadowMapTexture->Release();
    //ShadowMapTextureVSM->Release();

    ShadowMapViewport = { 0 };
    ShadowMapViewport.Width = static_cast<float>(TextureSize);
    ShadowMapViewport.Height = static_cast<float>(TextureSize);
    ShadowMapViewport.MaxDepth = 1.0f;
    ShadowMapViewport.MinDepth = 0.f;
}

HRESULT FShadowPass::CreateBuffer(uint32 NumTransforms)
{
    HRESULT hr;
    hr = BufferManager->CreateBufferGeneric<uint32>(ViewProjTransformBufferKey, nullptr, sizeof(uint32), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
    if (FAILED(hr)) {
        return hr;
    }

    hr = BufferManager->CreateBufferGeneric<FMatrix>(WorldTransformBufferKey, nullptr, sizeof(FMatrix), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
    if (FAILED(hr)) {
        return hr;
    }

    hr = BufferManager->CreateBufferGeneric<FShadowConfigurations>(ShadowConfigBufferKey, nullptr, sizeof(FShadowConfigurations), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
    if (FAILED(hr)) {
        return hr;
    }

    return BufferManager->CreateStructuredBuffer(TransformDataBufferKey, sizeof(FMatrix) * NumTransforms, D3D11_BIND_SHADER_RESOURCE,
        D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE, sizeof(FMatrix), NumTransforms);
}

void FShadowPass::ReleaseTexture()
{
    if (FShadowPass::ShadowMapDSV[0])
    {
        for (auto& DSV : ShadowMapDSV)
        {
            DSV->Release();
            DSV = nullptr;
        }
        ShadowMapDSV.Empty();
    }
    if (FShadowPass::ShadowMapDSVVSM[0])
    {
        for (auto& DSV : ShadowMapDSVVSM)
        {
            DSV->Release();
            DSV = nullptr;
        }
        ShadowMapDSVVSM.Empty();
    }
    if (FShadowPass::ShadowMapRTV[0])
    {
        for (auto& RTV : ShadowMapRTV)
        {
            RTV->Release();
            RTV = nullptr;
        }
        ShadowMapRTV.Empty();
    }
    if (FShadowPass::ShadowMapSRV)
    {
        FShadowPass::ShadowMapSRV->Release();
        FShadowPass::ShadowMapSRV = nullptr;
    }
    if (FShadowPass::ShadowMapSRVVSM)
    {
        FShadowPass::ShadowMapSRVVSM->Release();
        FShadowPass::ShadowMapSRVVSM = nullptr;
    }
}

void FShadowPass::UpdatePerspectiveShadowMap(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    IndicesMap.Empty();

    TArray<FMatrix> Transforms;
    uint32 Index = 0;

    for (ULightComponentBase* LightComponent : TObjectRange<ULightComponentBase>())
    {
        // map의 크기를 AABB로 해서 최적의 값 찾기(near far position)
        if (UDirectionalLightComponent* DirLight = Cast<UDirectionalLightComponent>(LightComponent))
        {
            FVector CameraPosition = Viewport->GetCameraLocation();
            // 일단은 Component의 위치를 기준으로 함(확인하기 편하게)
            //FVector eye = CameraPosition - DirLight->GetDirection() * 64;
            FVector eye = DirLight->GetWorldLocation();
            FVector target = eye + DirLight->GetDirection();
            FVector up = FVector::UpVector;

            if (abs(DirLight->GetDirection().Dot(up)) > 1 - FLT_EPSILON)
            {
                up = FVector::RightVector;
            }

            FMatrix View = JungleMath::CreateViewMatrix(eye, target, up);

            FMatrix Proj = JungleMath::CreateOrthoProjectionMatrix(30.0, 30.0, 0.1f, 300.f); // 파라미터 받아서값 조절할 수 있게 만들기
            //FMatrix Proj = JungleMath::CreateProjectionMatrix(150, 1, 0.1, 30.f);

            IndicesMap[LightComponent].Add(Index);
            Transforms.Add(View * Proj);
            Index++;
        }

        static const FVector directions[6] = {
            FVector(1, 0, 0),  // +X
            FVector(0, 1, 0),  // +Y
            FVector(0, 0, 1),  // +Z
            FVector(-1, 0, 0), // -X
            FVector(0, -1, 0), // -Y
            FVector(0, 0, -1)  // -Z
        };

        if (UPointLightComponent* PointLight = Cast<UPointLightComponent>(LightComponent))
        {
            for (int side = 0; side < 6; side++)
            {
                FVector eye = PointLight->GetWorldLocation();
                FVector target = eye + directions[side];
                FVector up = FVector::UpVector;

                if (side == 2 || side == 5)
                {
                    up = FVector::RightVector;
                }

                FMatrix View = JungleMath::CreateViewMatrix(eye, target, up);

                FMatrix Proj = JungleMath::CreateProjectionMatrix(FMath::DegreesToRadians(90), 1, 0.1, 30.f);

                IndicesMap[LightComponent].Add(Index);
                Transforms.Add(View * Proj);
                Index++;
            }
        }

        if (USpotLightComponent* SpotLight = Cast<USpotLightComponent>(LightComponent))
        {
            FVector eye = SpotLight->GetWorldLocation();
            FVector target = eye + SpotLight->GetDirection();
            FVector up = FVector::UpVector;

            if (abs(SpotLight->GetDirection().Dot(up)) > 1 - FLT_EPSILON)
            {
                up = FVector::RightVector;
            }

            FMatrix View = JungleMath::CreateViewMatrix(eye, target, up);

            float rad = SpotLight->GetOuterRad();
            FMatrix Proj = JungleMath::CreateProjectionMatrix(rad, 1, 0.1f, SpotLight->GetRadius()); // 파라미터 받아서값 조절할 수 있게 만들기

            IndicesMap[LightComponent].Add(Index);
            Transforms.Add(View * Proj);
            Index++;

        }


        // index 부족하면 동적으로 늘리기
        // point spot은 나중에 작성
    }

    BufferManager->UpdateStructuredBuffer(TransformDataBufferKey, Transforms);
}

//void FShadowPass::AddTargetLight(ULightComponentBase* InLightComponent)
//{
//    if (IndicesMap.Contains(InLightComponent)) {
//        UE_LOG(LogLevel::Warning, TEXT("이미 등록된 LightComponent입니다."));
//        return;
//    }
//    
//
//    IndicesMap[InLightComponent] = TArray<uint32>();
//    if (InLightComponent->IsA<UDirectionalLightComponent>())
//    {
//        // 하나만 필요하므로
//        // 이렇게해도 충돌 안나나?
//        IndicesMap[InLightComponent].Add(IndicesMap.Num());
//    }
//}

void FShadowPass::UpdateCascadedShadowMap(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
}


void FShadowPass::RenderPrimitive(OBJ::FStaticMeshRenderData* RenderData, TArray<FStaticMaterial*> Materials, TArray<UMaterial*> OverrideMaterials, int SelectedSubMeshIndex) const
{
    UINT Stride = sizeof(FStaticMeshVertex);
    UINT Offset = 0;

    Graphics->DeviceContext->IASetVertexBuffers(0, 1, &RenderData->VertexBuffer, &Stride, &Offset);

    if (RenderData->IndexBuffer)
    {
        Graphics->DeviceContext->IASetIndexBuffer(RenderData->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    }

    if (RenderData->MaterialSubsets.Num() == 0)
    {
        Graphics->DeviceContext->DrawIndexed(RenderData->Indices.Num(), 0, 0);
        return;
    }

    for (int SubMeshIndex = 0; SubMeshIndex < RenderData->MaterialSubsets.Num(); SubMeshIndex++)
    {
        uint32 StartIndex = RenderData->MaterialSubsets[SubMeshIndex].IndexStart;
        uint32 IndexCount = RenderData->MaterialSubsets[SubMeshIndex].IndexCount;
        Graphics->DeviceContext->DrawIndexed(IndexCount, StartIndex, 0);
    }
}
