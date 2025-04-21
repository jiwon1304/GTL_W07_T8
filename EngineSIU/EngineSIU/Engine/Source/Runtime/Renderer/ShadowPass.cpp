#include "ShadowPass.h"
#include "Renderer.h"
#include <UObject/UObjectIterator.h>
#include <UObject/Casts.h>
#include "Components/Light/LightComponent.h"
#include "Components/Light/DirectionalLightComponent.h"
#include "Components/Light/PointLightComponent.h"
#include "Components/Light/SpotLightComponent.h"
#include "RendererHelpers.h"
#include "Define.h"
#include "UnrealClient.h"
#include "UnrealEd/EditorViewportClient.h"
#include "Runtime/Windows/D3D11RHI/DXDShaderManager.h"
#include "Runtime/Windows/D3D11RHI/DXDBufferManager.h"
#include "Runtime/Windows/D3D11RHI/GraphicDevice.h"
#include "Runtime/Core/Math/JungleMath.h"


void FShadowPass::Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage)
{
    BufferManager = InBufferManager;
    Graphics = InGraphics;
    ShaderManager = InShaderManage;

    CreateTexture(TextureSize, NumShadowMaps);
    CreateBuffer(NumShadowMaps);
}

void FShadowPass::PrepareRender()
{
}

void FShadowPass::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    UpdatePerspectiveShadowMap(Viewport);
    
    for (const auto& Pair : IndicesMap)
    {
        ULightComponentBase* Light = Pair.Key;
        const TArray<uint32>& Indices = Pair.Value;

        // 사용 예
        for (uint32 Index : Indices)
        {
            BufferManager->BindStructuredBuffer(TransformBufferKey, 9, EShaderStage::Vertex, EShaderViewType::SRV);
            // rtv bind
            // draw
            // ... Do something with Index ...
        }
    }
}

void FShadowPass::ClearRenderArr()
{
}

/// <summary>
/// Shadow map을 저장할 배열을 생성합니다.
/// </summary>
/// <param name="TextureSize"></param> 정방형 Texture의 크기입니다. (TextureSize * TextureSize)
/// <param name="NumMaps"></param> 배열의 길이입니다.
HRESULT FShadowPass::CreateTexture(uint32 TextureSize, uint32 NumMaps)
{
    if (NumMaps > 4096)
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

    HRESULT hr = Graphics->Device->CreateTexture2D(&texDesc, nullptr, &ShadowMapTexture);
    if (FAILED(hr)) { 
        return hr;
    }

    // 2. DepthStencilView 생성
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT; // Typeless의 실질적인 Depth format
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
    dsvDesc.Texture2DArray.MipSlice = 0;
    dsvDesc.Texture2DArray.FirstArraySlice = 0;
    dsvDesc.Texture2DArray.ArraySize = NumMaps;

    hr = Graphics->Device->CreateDepthStencilView(ShadowMapTexture, &dsvDesc, &ShadowMapDSV);
    if (FAILED(hr)) {
        return hr;
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

    // ShadowMap 텍스처는 내부적으로 관리되므로 참조 해제
    ShadowMapTexture->Release();
}

HRESULT FShadowPass::CreateBuffer(uint32 NumTransforms)
{
    return BufferManager->CreateStructuredBuffer(TransformBufferKey, sizeof(FMatrix) * NumTransforms, D3D11_BIND_SHADER_RESOURCE,
        D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE, sizeof(FMatrix), NumTransforms);
}

void FShadowPass::UpdateShadowMap(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
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
            FVector eye = CameraPosition - DirLight->GetDirection() * 128;
            FVector target = CameraPosition;
            FVector up = FVector::UpVector;
            FMatrix View = JungleMath::CreateViewMatrix(eye, target, up);

            FMatrix Proj = JungleMath::CreateOrthoProjectionMatrix(100.0, 100.0, 0.1f, 100.f); // 파라미터 받아서값 조절할 수 있게 만들기

            IndicesMap[LightComponent].Add(Index);
            Transforms.Add(View * Proj);
            Index++;
        }
        // index 부족하면 동적으로 늘리기
        // point spot은 나중에 작성
    }

    BufferManager->UpdateStructuredBuffer(TransformBufferKey, Transforms);
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
