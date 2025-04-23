#pragma once
#include "IRenderPass.h"

#define _TCHAR_DEFINED
#include <d3d11.h>
#include <d3dcompiler.h>

#include "Container/Array.h"
#include "Container/Map.h"

#include "Define.h"

enum class EShadowFilterMethod : uint8
{
    NONE = 0,
    PCF = 1,
    POISSON = 2,
    VSM = 3,
};

struct FShadowConfigurations
{
    int FilterMode;
    FVector Padding;
};

class ULightComponentBase;

class FShadowPass :
    public IRenderPass
{
public:
    virtual ~FShadowPass() {}

    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage);

    virtual void PrepareRender();

    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport);

    virtual void ClearRenderArr();

    static ID3D11ShaderResourceView* GetShadowMapSRV() { return ShadowMapSRV; }

    static TArray<uint32> GetShadowMapIndex(ULightComponentBase* InLightComponent)
    {
        if (IndicesMap.Contains(InLightComponent))
        {
            return IndicesMap[InLightComponent];
        }
        TArray<uint32> e;
        e.Add(-1);
        return e;
    }

    bool UpdateShadowMap(uint32 InTextureSize, uint32 InNumMaps);

    uint64 GetAllocatedTextureMapSize() { return 4 * TextureSize * TextureSize * (2 * NumShadowMaps + 1); }

    uint32 GetNumUsedTextureMap() { return UsedShadowMaps; }

    uint32 GetNumUsedTextureMapDir() { return UsedShadowMapsForDir; }
    uint32 GetNumUsedTextureMapSpot() { return UsedShadowMapsForPoint; }
    uint32 GetNumUsedTextureMapPoint() { return UsedShadowMapsForSpot; }
    uint32 GetTextureSize() { return TextureSize; }
    uint32 GetNumShadowMaps() { return NumShadowMaps; }


    void SetShadowFilterMode(EShadowFilterMethod InFilterMode);
private:
    HRESULT CreateShader();

    HRESULT CreateTexture(uint32 TextureSize, uint32 NumMaps);

    HRESULT CreateBuffer(uint32 NumTransforms);

    void ReleaseTexture();

    void UpdateShadowMap(const std::shared_ptr<FEditorViewportClient>& Viewport);

    void UpdatePerspectiveShadowMap(const std::shared_ptr<FEditorViewportClient>& Viewport);

    void AddTargetLight(ULightComponentBase* InLightComponent);

    void UpdateCascadedShadowMap(const std::shared_ptr<FEditorViewportClient>& Viewport);

    void RenderPrimitive(OBJ::FStaticMeshRenderData* RenderData, TArray<class FStaticMaterial*> Materials, TArray<class UMaterial*> OverrideMaterials, int SelectedSubMeshIndex) const;

    class ID3D11VertexShader* VertexShader;

    class ID3D11PixelShader* PixelShader;

    class ID3D11InputLayout* InputLayout;

    class FDXDBufferManager* BufferManager;

    class FGraphicsDevice* Graphics;

    class FDXDShaderManager* ShaderManager;
public:
    static const uint32 ShadowMapSRVSlot = 8;
    static const uint32 TransformSRVSlot = 9;
    static const uint32 ViewProjTransformCBSlot = 10;
    static const uint32 WorldTransformCBSlot = 9;

private:
    TArray<class UStaticMeshComponent*> StaticMeshComponents;

    const uint32 MaxSize = 536870912; // 2048 2048 128
    uint32 TextureSize = 1024; // initial value
    uint32 NumShadowMaps = 128; // initial value
    uint32 UsedShadowMaps = 0;
    uint32 UsedShadowMapsForDir = 0;
    uint32 UsedShadowMapsForPoint = 0;
    uint32 UsedShadowMapsForSpot = 0;

    FWString VertexShaderBufferKey = L"ShadowPassDepthRenderShader";
    FWString PixelShaderBufferKey = L"ShadowPassDepthRenderShader";
    FString TransformDataBufferKey = "ShadowTransformDataBufferKey";
    FString ViewProjTransformBufferKey = "ShadowViewProjTransformBufferKey"; // view->proj의 structuredbuffer index
    FString WorldTransformBufferKey = "ShadowWorldTransformBufferKey"; // staticmesh render할때 필요한 world trnasform
    FString ShadowConfigBufferKey = "ShadowConfigurations";

    static EShadowFilterMethod CurrentShadowFilterMode;
public:
    static ID3D11Texture2D* ShadowMapTexture;
    static ID3D11Texture2D* ShadowMapTextureVSM;
    static ID3D11Texture2D* ShadowMapDepthVSM;
    static TArray<ID3D11DepthStencilView*> ShadowMapDSV;
    static TArray<ID3D11DepthStencilView*> ShadowMapDSVVSM;
    static TArray<ID3D11RenderTargetView*> ShadowMapRTV;
    static ID3D11ShaderResourceView* ShadowMapSRV;
    static ID3D11ShaderResourceView* ShadowMapSRVVSM;
    static D3D11_VIEWPORT ShadowMapViewport;
    static ID3D11SamplerState* ShadowMapSampler;
    static ID3D11SamplerState* ShadowMapSamplerVSM;

    static TMap<ULightComponentBase*, TArray<uint32>> IndicesMap; // LightComponentBase -> ShadowMaps/Transforms를 접근할때 광원에 해당하는 그림자맵의 index
};

