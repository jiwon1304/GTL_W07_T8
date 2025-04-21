#pragma once
#include "IRenderPass.h"

#define _TCHAR_DEFINED
#include <d3d11.h>
#include <d3dcompiler.h>

#include "Container/Array.h"
#include "Container/Map.h"

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

private:
    HRESULT CreateTexture(uint32 TextureSize, uint32 NumMaps);

    HRESULT CreateBuffer(uint32 NumTransforms);

    void UpdateShadowMap(const std::shared_ptr<FEditorViewportClient>& Viewport);

    void UpdatePerspectiveShadowMap(const std::shared_ptr<FEditorViewportClient>& Viewport);

    void AddTargetLight(ULightComponentBase* InLightComponent);

    void UpdateCascadedShadowMap(const std::shared_ptr<FEditorViewportClient>& Viewport);

    class ID3D11VertexShader* VertexShader;

    class ID3D11PixelShader* PixelShader;

    class ID3D11InputLayout* InputLayout;

    class FDXDBufferManager* BufferManager;

    class FGraphicsDevice* Graphics;

    class FDXDShaderManager* ShaderManager;

    uint32 TextureSize = 1024; // initial value
    uint32 NumShadowMaps = 128; // initial value
    ID3D11Texture2D* ShadowMapTexture = nullptr;
    ID3D11DepthStencilView* ShadowMapDSV = nullptr;
    ID3D11ShaderResourceView* ShadowMapSRV = nullptr;

    FString TransformBufferKey = "TransformBufferKey";
    
    TMap<ULightComponentBase*, TArray<uint32>> IndicesMap; // LightComponentBase -> ShadowMaps/Transforms를 접근할때 광원에 해당하는 그림자맵의 index
    

};

