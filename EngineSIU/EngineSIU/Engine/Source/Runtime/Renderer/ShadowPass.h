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

    static TArray<uint32> GetShadowMapIndex(ULightComponentBase* InLightComponent) { return IndicesMap[InLightComponent]; }

    void SetShadowFilterMode(EShadowFilterMethod InFilterMode);
private:
    HRESULT CreateShader();

    HRESULT CreateTexture(uint32 TextureSize, uint32 NumMaps);

    HRESULT CreateBuffer(uint32 NumTransforms);

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

    uint32 TextureSize = 1024; // initial value
    uint32 NumShadowMaps = 128; // initial value

    FWString VertexShaderBufferKey = L"ShadowPassDepthRenderShader";
    FString TransformDataBufferKey = "ShadowTransformDataBufferKey";
    FString ViewProjTransformBufferKey = "ShadowViewProjTransformBufferKey"; // view->proj의 structuredbuffer index
    FString WorldTransformBufferKey = "ShadowWorldTransformBufferKey"; // staticmesh render할때 필요한 world trnasform
    FString ShadowConfigBufferKey = "ShadowConfigurations";

    static EShadowFilterMethod CurrentShadowFilterMode;
public:
    static ID3D11Texture2D* ShadowMapTexture;
    static TArray<ID3D11DepthStencilView*> ShadowMapDSV;
    static ID3D11ShaderResourceView* ShadowMapSRV;
    static D3D11_VIEWPORT ShadowMapViewport;
    static ID3D11SamplerState* ShadowMapSampler;

    static TMap<ULightComponentBase*, TArray<uint32>> IndicesMap; // LightComponentBase -> ShadowMaps/Transforms를 접근할때 광원에 해당하는 그림자맵의 index
};

