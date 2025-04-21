#pragma once
#define _TCHAR_DEFINED
#include "Define.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include "Container/String.h"
#include "Container/Array.h"
#include "Container/Map.h"
#include "Engine/Texture.h"
#include "GraphicDevice.h"
#include "UserInterface/Console.h"

// ShaderStage 열거형
enum class EShaderStage
{
    Vertex,
    Pixel,
    Compute
};
enum class EShaderViewType
{
    SRV,
    UAV,
};
struct QuadVertex
{
    float Position[3];
    float TexCoord[2];
};
struct FStructuredBufferResources
{
    ID3D11Buffer* Buffer;
    ID3D11ShaderResourceView* SRV;
    ID3D11UnorderedAccessView* UAV;
};

class FDXDBufferManager
{
public:
    QuadVertex Q;

    FDXDBufferManager() = default;
    void Initialize(ID3D11Device* DXDevice, ID3D11DeviceContext* DXDeviceContext);

    // 템플릿을 활용한 버텍스 버퍼 생성 (정적/동적)
    template<typename T>
    HRESULT CreateVertexBuffer(const FString& KeyName, const TArray<T>& vertices, FVertexInfo& OutVertexInfo);
    template<typename T>
    HRESULT CreateVertexBuffer(const FWString& KeyName, const TArray<T>& vertices, FVertexInfo& OutVertexInfo);

    template<typename T>
    HRESULT CreateIndexBuffer(const FString& KeyName, const TArray<T>& indices, FIndexInfo& OutIndexInfo);
    template<typename T>
    HRESULT CreateIndexBuffer(const FWString& KeyName, const TArray<T>& indices, FIndexInfo& OutIndexInfo);

    template<typename T>
    HRESULT CreateDynamicVertexBuffer(const FString& KeyName, const TArray<T>& vertices, FVertexInfo& OutVertexInfo);

    // 템플릿 헬퍼 함수: 내부에서 버퍼 생성 로직 통합
    template<typename T>
    HRESULT CreateVertexBufferInternal(const FString& KeyName, const TArray<T>& vertices, FVertexInfo& OutVertexInfo,
        D3D11_USAGE usage, UINT cpuAccessFlags);

    template<typename T>
    HRESULT CreateVertexBufferInternal(const FWString& KeyName, const TArray<T>& vertices, FVertexInfo& OutVertexInfo,
        D3D11_USAGE usage, UINT cpuAccessFlags);

    HRESULT CreateUnicodeTextBuffer(const FWString& Text, FBufferInfo& OutBufferInfo, float BitmapWidth, float BitmapHeight, float ColCount, float RowCount);

    void SetStartUV(wchar_t hangul, FVector2D& UVOffset);
    
    void ReleaseBuffers();
    void ReleaseConstantBuffer();

    template<typename T>
    HRESULT CreateBufferGeneric(const FString& KeyName, T* data, UINT byteWidth, UINT bindFlags, D3D11_USAGE usage, UINT cpuAccessFlags);

    template<typename T>
    void UpdateConstantBuffer(const FString& key, const T& data) const;

    template<typename T>
    void UpdateDynamicVertexBuffer(const FString& KeyName, const TArray<T>& vertices) const;

    void BindConstantBuffers(const TArray<FString>& Keys, UINT StartSlot, EShaderStage Stage) const;
    void BindConstantBuffer(const FString& Key, UINT StartSlot, EShaderStage Stage) const;

   HRESULT CreateStructuredBuffer(const FString& KeyName, UINT byteWidth, UINT bindFlags, D3D11_USAGE usage, UINT cpuAccessFlags, UINT Stride, UINT numElements);

    template<typename T>
    void UpdateStructuredBuffer(const FString& KeyName, const TArray<T>& data);
    void BindStructuredBuffer(const FString& Key, UINT StartSlot, EShaderStage Stage, EShaderViewType ViewType) const;

    template<typename T>
    static void SafeRelease(T*& comObject);


    FVertexInfo GetVertexBuffer(const FString& InName) const;
    FIndexInfo GetIndexBuffer(const FString& InName) const;
    FVertexInfo GetTextVertexBuffer(const FWString& InName) const;
    FIndexInfo GetTextIndexBuffer(const FWString& InName) const;
    ID3D11Buffer* GetConstantBuffer(const FString& InName) const;
    ID3D11Buffer* GetStructuredBuffer(const FString& InName) const;
    ID3D11ShaderResourceView* GetStructuredBufferSRV(const FString& InName) const;
    ID3D11UnorderedAccessView* GetStructuredBufferUAV(const FString& InName) const;

    void GetQuadBuffer(FVertexInfo& OutVertexInfo, FIndexInfo& OutIndexInfo);
    void GetTextBuffer(const FWString& Text, FVertexInfo& OutVertexInfo, FIndexInfo& OutIndexInfo);
    void CreateQuadBuffer();
private:
    // 16바이트 정렬
    inline UINT Align16(UINT size) { return (size + 15) & ~15; }
private:
    ID3D11Device* DXDevice = nullptr;
    ID3D11DeviceContext* DXDeviceContext = nullptr;

    TMap<FString, FVertexInfo> VertexBufferPool;
    TMap<FString, FIndexInfo> IndexBufferPool;
    TMap<FString, ID3D11Buffer*> ConstantBufferPool;
    TMap<FString, FStructuredBufferResources> StructuredBufferPool;

    TMap<FWString, FBufferInfo> TextAtlasBufferPool;
    TMap<FWString, FVertexInfo> TextAtlasVertexBufferPool;
    TMap<FWString, FIndexInfo> TextAtlasIndexBufferPool;
};

// 템플릿 함수 구현부

template<typename T>
HRESULT FDXDBufferManager::CreateVertexBufferInternal(const FString& KeyName, const TArray<T>& vertices, FVertexInfo& OutVertexInfo,
    D3D11_USAGE usage, UINT cpuAccessFlags)
{
    if (!KeyName.IsEmpty() && VertexBufferPool.Contains(KeyName))
    {
        OutVertexInfo = VertexBufferPool[KeyName];
        return S_OK;
    }
    uint32_t Stride = sizeof(T);
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = usage;
    bufferDesc.ByteWidth = Stride * vertices.Num();
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = cpuAccessFlags;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices.GetData();

    ID3D11Buffer* NewBuffer = nullptr;
    HRESULT hr = DXDevice->CreateBuffer(&bufferDesc, &initData, &NewBuffer);
    if (FAILED(hr))
        return hr;

    OutVertexInfo.NumVertices = static_cast<uint32>(vertices.Num());
    OutVertexInfo.VertexBuffer = NewBuffer;
    OutVertexInfo.Stride = Stride;
    VertexBufferPool.Add(KeyName, OutVertexInfo);

    return S_OK;
}
template<typename T>
HRESULT FDXDBufferManager::CreateIndexBuffer(const FString& KeyName, const TArray<T>& indices, FIndexInfo& OutIndexInfo)
{
    if (!KeyName.IsEmpty() && IndexBufferPool.Contains(KeyName))
    {
        OutIndexInfo = IndexBufferPool[KeyName];
        return S_OK;
    }

    D3D11_BUFFER_DESC indexBufferDesc = {};
    indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    indexBufferDesc.ByteWidth = indices.Num() * sizeof(uint32);
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexInitData = {};
    indexInitData.pSysMem = indices.GetData();

    ID3D11Buffer* NewBuffer = nullptr;
    HRESULT hr = DXDevice->CreateBuffer(&indexBufferDesc, &indexInitData, &NewBuffer);
    if (FAILED(hr))
        return hr;

    OutIndexInfo.NumIndices = static_cast<uint32>(indices.Num());
    OutIndexInfo.IndexBuffer = NewBuffer;
    IndexBufferPool.Add(KeyName, FIndexInfo(static_cast<uint32>(indices.Num()), NewBuffer));


    return S_OK;
}

template<typename T>
HRESULT FDXDBufferManager::CreateVertexBuffer(const FString& KeyName, const TArray<T>& vertices, FVertexInfo& OutVertexInfo)
{
    return CreateVertexBufferInternal(KeyName, vertices, OutVertexInfo, D3D11_USAGE_DEFAULT, 0);
}


// FWString 전용 버텍스 버퍼 생성 (내부)
template<typename T>
HRESULT FDXDBufferManager::CreateVertexBufferInternal(const FWString& KeyName, const TArray<T>& vertices, FVertexInfo& OutVertexInfo,
    D3D11_USAGE usage, UINT cpuAccessFlags)
{
    if (!KeyName.empty() && TextAtlasVertexBufferPool.Contains(KeyName))
    {
        OutVertexInfo = TextAtlasVertexBufferPool[KeyName];
        return S_OK;
    }
    uint32_t Stride = sizeof(T);
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = usage;
    bufferDesc.ByteWidth = Stride * vertices.Num();
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = cpuAccessFlags;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices.GetData();

    ID3D11Buffer* NewBuffer = nullptr;
    HRESULT hr = DXDevice->CreateBuffer(&bufferDesc, &initData, &NewBuffer);
    if (FAILED(hr))
        return hr;

    OutVertexInfo.NumVertices = static_cast<uint32>(vertices.Num());
    OutVertexInfo.VertexBuffer = NewBuffer;
    OutVertexInfo.Stride = Stride;
    TextAtlasVertexBufferPool.Add(KeyName, OutVertexInfo);

    return S_OK;
}

// FWString 전용 인덱스 버퍼 생성
template<typename T>
HRESULT FDXDBufferManager::CreateIndexBuffer(const FWString& KeyName, const TArray<T>& indices, FIndexInfo& OutIndexInfo)
{
    if (!KeyName.empty() && TextAtlasIndexBufferPool.Contains(KeyName))
    {
        OutIndexInfo = TextAtlasIndexBufferPool[KeyName];
        return S_OK;
    }

    D3D11_BUFFER_DESC indexBufferDesc = {};
    indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    indexBufferDesc.ByteWidth = indices.Num() * sizeof(uint32);
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexInitData = {};
    indexInitData.pSysMem = indices.GetData();

    ID3D11Buffer* NewBuffer = nullptr;
    HRESULT hr = DXDevice->CreateBuffer(&indexBufferDesc, &indexInitData, &NewBuffer);
    if (FAILED(hr))
        return hr;

    OutIndexInfo.NumIndices = static_cast<uint32>(indices.Num());
    OutIndexInfo.IndexBuffer = NewBuffer;
    TextAtlasIndexBufferPool.Add(KeyName, FIndexInfo(static_cast<uint32>(indices.Num()), NewBuffer));

    return S_OK;
}

template<typename T>
HRESULT FDXDBufferManager::CreateVertexBuffer(const FWString& KeyName, const TArray<T>& vertices, FVertexInfo& OutVertexInfo)
{
    return CreateVertexBufferInternal(KeyName, vertices, OutVertexInfo, D3D11_USAGE_DEFAULT, 0);
}


template<typename T>
HRESULT FDXDBufferManager::CreateDynamicVertexBuffer(const FString& KeyName, const TArray<T>& vertices, FVertexInfo& OutVertexInfo)
{
    return CreateVertexBufferInternal(KeyName, vertices, OutVertexInfo, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
}

template<typename T>
HRESULT FDXDBufferManager::CreateBufferGeneric(const FString& KeyName, T* data, UINT byteWidth, UINT bindFlags, D3D11_USAGE usage, UINT cpuAccessFlags)
{
    if (ConstantBufferPool.Contains(KeyName))
    {
        return S_OK;
    }
    
    byteWidth = Align16(byteWidth);

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = byteWidth;
    desc.Usage = usage;
    desc.BindFlags = bindFlags;
    desc.CPUAccessFlags = cpuAccessFlags;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = data;

    ID3D11Buffer* buffer = nullptr;
    HRESULT hr = DXDevice->CreateBuffer(&desc, data ? &initData : nullptr, &buffer);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("Error Create Constant Buffer!"));
        return hr;
    }

    ConstantBufferPool.Add(KeyName, buffer);
    return S_OK;
}

template<typename T>
void FDXDBufferManager::UpdateConstantBuffer(const FString& key, const T& data) const
{
    ID3D11Buffer* buffer = GetConstantBuffer(key);
    if (!buffer)
    {
        UE_LOG(LogLevel::Error, TEXT("UpdateConstantBuffer 호출: 키 %s에 해당하는 buffer가 없습니다."), *key);
        return;
    }

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = DXDeviceContext->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("Buffer Map 실패, HRESULT: 0x%X"), hr);
        return;
    }

    memcpy(mappedResource.pData, &data, sizeof(T));
    DXDeviceContext->Unmap(buffer, 0);
}

template<typename T>
void FDXDBufferManager::UpdateDynamicVertexBuffer(const FString& KeyName, const TArray<T>& vertices) const
{
    if (!VertexBufferPool.Contains(KeyName))
    {
        UE_LOG(LogLevel::Error, TEXT("UpdateDynamicVertexBuffer 호출: 키 %s에 해당하는 버텍스 버퍼가 없습니다."), *KeyName);
        return;
    }
    FVertexInfo vbInfo = VertexBufferPool[KeyName];

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = DXDeviceContext->Map(vbInfo.VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("VertexBuffer Map 실패, HRESULT: 0x%X"), hr);
        return;
    }

    memcpy(mapped.pData, vertices.GetData(), sizeof(T) * vertices.Num());
    DXDeviceContext->Unmap(vbInfo.VertexBuffer, 0);
}

/*
* 예시
    D3D11_BUFFER_DESC BufferDesc;
    BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    BufferDesc.ByteWidth = sizeof(FCone) * NumCones;
    BufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    BufferDesc.StructureByteStride = sizeof(FCone);
*/
//template<typename T>
//inline HRESULT FDXDBufferManager::CreateStructuredBuffer(const FString& KeyName,
//    UINT byteWidth, UINT bindFlags, D3D11_USAGE usage, UINT cpuAccessFlags, const TArray<T>& data)
//{
//    if (StructuredBufferPool.Contains(KeyName))
//    {
//        return S_OK;
//    }
//    
//    FStructuredBufferResources Resources;
//
//    byteWidth = Align16(byteWidth);
//
//    D3D11_BUFFER_DESC desc = {};
//    desc.ByteWidth = byteWidth;
//    desc.Usage = usage;
//    desc.BindFlags = bindFlags;
//    desc.CPUAccessFlags = cpuAccessFlags;
//    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
//    desc.StructureByteStride = sizeof(T);
//
//    D3D11_SUBRESOURCE_DATA initData = {};
//    initData.pSysMem = data.GetData();
//
//    ID3D11Buffer* buffer = nullptr;
//    HRESULT hr = DXDevice->CreateBuffer(&desc, data.Num() != 0 ? &initData : nullptr, &buffer);
//    if (FAILED(hr))
//    {
//        UE_LOG(LogLevel::Error, TEXT("Error Create Constant Buffer!"));
//        return hr;
//    }
//    Resources.Buffer = buffer;
//
//    if (bindFlags | D3D11_BIND_SHADER_RESOURCE)
//    {
//        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
//        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
//        srvDesc.Buffer.FirstElement = 0;
//        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
//        srvDesc.Buffer.NumElements = MaxNumPointLight;
//    }
//
//    StructuredBufferPool.Add(KeyName, buffer);
//    return S_OK;
//}

template<typename T>
inline void FDXDBufferManager::UpdateStructuredBuffer(const FString& KeyName, const TArray<T>& data)
{
    if (!StructuredBufferPool.Contains(KeyName))
    {
        UE_LOG(LogLevel::Error, TEXT("UpdateStructuredBuffer 호출: 키 %s에 해당하는 버텍스 버퍼가 없습니다."), *KeyName);
        return;
    }

    // 버퍼 크기 확인
    ID3D11Buffer* Buffer = StructuredBufferPool[KeyName].Buffer;
    D3D11_BUFFER_DESC Desc;
    Buffer->GetDesc(&Desc);
    uint32 BufferNum = Desc.ByteWidth / sizeof(T);
    
    if (data.Num() > BufferNum)
    {
        UE_LOG(LogLevel::Warning, TEXT("UpdateStructuredBuffer 호출: 업데이트하려는 배열의 크기가 버퍼의 크기보다 큽니다. %d개 중 %d만 업데이트합니다."), *KeyName, data.Num(), BufferNum);
    }
    // 생성 시보다 적은 개수가 들어옴
    else
    {
        BufferNum = data.Num();
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = DXDeviceContext->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("StructuedBuffer Map 실패, HRESULT: 0x%X"), hr);
        return;
    }

    memcpy(mapped.pData, data.GetData(), sizeof(T) * BufferNum);
    DXDeviceContext->Unmap(Buffer, 0);
}

template<typename T>
void FDXDBufferManager::SafeRelease(T*& comObject)
{
    if (comObject)
    {
        comObject->Release();
        comObject = nullptr;
    }
}

