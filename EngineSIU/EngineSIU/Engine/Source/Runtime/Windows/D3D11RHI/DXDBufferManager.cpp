#include "DXDBufferManager.h"

#include <codecvt>
#include <locale>

void FDXDBufferManager::Initialize(ID3D11Device* InDXDevice, ID3D11DeviceContext* InDXDeviceContext)
{
    DXDevice = InDXDevice;
    DXDeviceContext = InDXDeviceContext;
    CreateQuadBuffer();
}

void FDXDBufferManager::ReleaseBuffers()
{
    for (auto& Pair : VertexBufferPool)
    {
        if (Pair.Value.VertexBuffer)
        {
            Pair.Value.VertexBuffer->Release();
            Pair.Value.VertexBuffer = nullptr;
        }
    }
    VertexBufferPool.Empty();

    for (auto& Pair : IndexBufferPool)
    {
        if (Pair.Value.IndexBuffer)
        {
            Pair.Value.IndexBuffer->Release();
            Pair.Value.IndexBuffer = nullptr;
        }
    }
    IndexBufferPool.Empty();
}

void FDXDBufferManager::ReleaseConstantBuffer()
{
    for (auto& Pair : ConstantBufferPool)
    {
        if (Pair.Value)
        {
            Pair.Value->Release();
            Pair.Value = nullptr;
        }
    }
    ConstantBufferPool.Empty();
}

void FDXDBufferManager::BindConstantBuffers(const TArray<FString>& Keys, UINT StartSlot, EShaderStage Stage) const
{
    const int Count = Keys.Num();
    TArray<ID3D11Buffer*> Buffers;
    Buffers.Reserve(Count);
    for (const FString& Key : Keys)
    {
        ID3D11Buffer* Buffer = GetConstantBuffer(Key);
        Buffers.Add(Buffer);
    }

    if (Stage == EShaderStage::Vertex)
    {
        DXDeviceContext->VSSetConstantBuffers(StartSlot, Count, Buffers.GetData());
    }
    else if (Stage == EShaderStage::Pixel)
    {
        DXDeviceContext->PSSetConstantBuffers(StartSlot, Count, Buffers.GetData());
    }
}

void FDXDBufferManager::BindConstantBuffer(const FString& Key, UINT StartSlot, EShaderStage Stage) const
{
    ID3D11Buffer* Buffer = GetConstantBuffer(Key);
    if (Stage == EShaderStage::Vertex)
        DXDeviceContext->VSSetConstantBuffers(StartSlot, 1, &Buffer);
    else if (Stage == EShaderStage::Pixel)
        DXDeviceContext->PSSetConstantBuffers(StartSlot, 1, &Buffer);
}

HRESULT FDXDBufferManager::CreateStructuredBuffer(const FString& KeyName, UINT byteWidth, UINT bindFlags, D3D11_USAGE usage, UINT cpuAccessFlags, UINT Stride, UINT numElements)
{
    if (StructuredBufferPool.Contains(KeyName))
    {
        return S_OK;
    }

    if (byteWidth != Stride * numElements)
    {
        UE_LOG(LogLevel::Warning, TEXT("Structuredbuffer is not packed. This can can be intended."));
    }

    if ((bindFlags & D3D11_BIND_SHADER_RESOURCE || bindFlags & D3D11_BIND_UNORDERED_ACCESS) == false)
    {
        UE_LOG(LogLevel::Warning, TEXT("Structuredbuffer creates no view."));
    }
    FStructuredBufferResources Resources;

    byteWidth = Align16(byteWidth);

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = byteWidth;
    desc.Usage = usage;
    desc.BindFlags = bindFlags;
    desc.CPUAccessFlags = cpuAccessFlags;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = Stride;

    ID3D11Buffer* buffer = nullptr;
    HRESULT hr = DXDevice->CreateBuffer(&desc, nullptr, &buffer);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("Error Create Structured Buffer!"));
        return hr;
    }
    Resources.Buffer = buffer;

    ID3D11ShaderResourceView* SRV = nullptr;
    if (bindFlags & D3D11_BIND_SHADER_RESOURCE)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Buffer.NumElements = numElements;
        hr = DXDevice->CreateShaderResourceView(buffer, &srvDesc, &SRV);
        if (FAILED(hr))
        {
            UE_LOG(LogLevel::Error, TEXT("Error Create SRV from Structured Buffer!"));
            return hr;
        }
    }
    Resources.SRV = SRV;

    ID3D11UnorderedAccessView* UAV = nullptr;
    if (bindFlags & D3D11_BIND_UNORDERED_ACCESS)
    {
        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = numElements;

        hr = DXDevice->CreateUnorderedAccessView(buffer, &uavDesc, &UAV);
        if (FAILED(hr))
        {
            UE_LOG(LogLevel::Error, TEXT("Error Create UAV from Structured Buffer!"));
            return hr;
        }
    }
    Resources.UAV = UAV;

    StructuredBufferPool.Add(KeyName, Resources);
    return S_OK;
}

void FDXDBufferManager::BindStructuredBuffer(const FString& Key, UINT StartSlot, EShaderStage Stage, EShaderViewType ViewType) const
{
   if (ViewType == EShaderViewType::SRV)
   {
       if (ID3D11ShaderResourceView* SRV = GetStructuredBufferSRV(Key))
       {
           if (Stage == EShaderStage::Vertex)
           {
               DXDeviceContext->VSSetShaderResources(StartSlot, 1, &SRV);
           }
           else if (Stage == EShaderStage::Pixel)
           {
               DXDeviceContext->PSSetShaderResources(StartSlot, 1, &SRV);
           }
           else if (Stage == EShaderStage::Compute)
           {
               DXDeviceContext->CSSetShaderResources(StartSlot, 1, &SRV);
           }
       }
   }
   else if (ViewType == EShaderViewType::UAV)
   {
       if (ID3D11UnorderedAccessView* UAV = GetStructuredBufferUAV(Key))
       {
           if (Stage == EShaderStage::Compute)
           {
               DXDeviceContext->CSSetUnorderedAccessViews(StartSlot, 1, &UAV, nullptr);
           }
           else
           {
               UE_LOG(LogLevel::Error, TEXT("Cannot bind UAV to vertex / pixel shader!"));
           }
       }
   }
}

FVertexInfo FDXDBufferManager::GetVertexBuffer(const FString& InName) const
{
    if (VertexBufferPool.Contains(InName))
        return VertexBufferPool[InName];
    return FVertexInfo();
}

FIndexInfo FDXDBufferManager::GetIndexBuffer(const FString& InName) const
{
    if (IndexBufferPool.Contains(InName))
        return IndexBufferPool[InName];
    return FIndexInfo();
}

FVertexInfo FDXDBufferManager::GetTextVertexBuffer(const FWString& InName) const
{
    if (TextAtlasVertexBufferPool.Contains(InName))
        return TextAtlasVertexBufferPool[InName];

    return FVertexInfo();
}

FIndexInfo FDXDBufferManager::GetTextIndexBuffer(const FWString& InName) const
{
    if (TextAtlasIndexBufferPool.Contains(InName))
        return TextAtlasIndexBufferPool[InName];

    return FIndexInfo();
}


ID3D11Buffer* FDXDBufferManager::GetConstantBuffer(const FString& InName) const
{
    if (ConstantBufferPool.Contains(InName))
        return ConstantBufferPool[InName];

    return nullptr;
}

ID3D11Buffer* FDXDBufferManager::GetStructuredBuffer(const FString& InName) const
{
    if (StructuredBufferPool.Contains(InName))
        return StructuredBufferPool[InName].Buffer;

    return nullptr;
}

ID3D11ShaderResourceView* FDXDBufferManager::GetStructuredBufferSRV(const FString& InName) const
{
    if (StructuredBufferPool.Contains(InName))
        return StructuredBufferPool[InName].SRV;

    return nullptr;
}

ID3D11UnorderedAccessView* FDXDBufferManager::GetStructuredBufferUAV(const FString& InName) const
{
    if (StructuredBufferPool.Contains(InName))
        return StructuredBufferPool[InName].UAV;

    return nullptr;
}

void FDXDBufferManager::CreateQuadBuffer()
{
    TArray<QuadVertex> Vertices =
    {
        { {-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f} },
        { { 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f} },
        { { 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f} },
        { {-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f} },
    };

    FVertexInfo VertexInfo;
    CreateVertexBuffer(TEXT("QuadBuffer"), Vertices, VertexInfo);

    TArray<short> Indices =
    {
        0, 1, 2,
        0, 2, 3
    };

    FIndexInfo IndexInfo;
    CreateIndexBuffer(TEXT("QuadBuffer"), Indices, IndexInfo);
}

void FDXDBufferManager::GetQuadBuffer(FVertexInfo& OutVertexInfo, FIndexInfo& OutIndexInfo)
{
    OutVertexInfo = GetVertexBuffer(TEXT("QuadBuffer"));
    OutIndexInfo = GetIndexBuffer(TEXT("QuadBuffer"));
}

void FDXDBufferManager::GetTextBuffer(const FWString& Text, FVertexInfo& OutVertexInfo, FIndexInfo& OutIndexInfo)
{
    OutVertexInfo = GetTextVertexBuffer(Text);
    OutIndexInfo = GetTextIndexBuffer(Text);
}


HRESULT FDXDBufferManager::CreateUnicodeTextBuffer(const FWString& Text, FBufferInfo& OutBufferInfo,
    float BitmapWidth, float BitmapHeight, float ColCount, float RowCount)
{
    if (TextAtlasBufferPool.Contains(Text))
    {
        OutBufferInfo = TextAtlasBufferPool[Text];
        return S_OK;
    }

    TArray<FVertexTexture> Vertices;

    // 각 글자에 대한 기본 쿼드 크기 (폭과 높이)
    const float quadWidth = 2.0f;
    const float quadHeight = 2.0f;

    // 전체 텍스트의 너비
    float totalTextWidth = quadWidth * Text.size();
    // 텍스트의 중앙으로 정렬하기 위한 오프셋
    float centerOffset = totalTextWidth / 2.0f;

    float CellWidth = float(BitmapWidth) / ColCount; // 컬럼별 셀 폭
    float CellHeight = float(BitmapHeight) / RowCount; // 행별 셀 높이

    float nTexelUOffset = CellWidth / BitmapWidth;
    float nTexelVOffset = CellHeight / BitmapHeight;

    for (int i = 0; i < Text.size(); i++)
    {
        // 각 글자에 대해 기본적인 사각형 좌표 설정 (원점은 -1.0f부터 시작)
        FVertexTexture leftUP = { -1.0f,  1.0f, 0.0f, 0.0f, 0.0f };
        FVertexTexture rightUP = { 1.0f,  1.0f, 0.0f, 1.0f, 0.0f };
        FVertexTexture leftDown = { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f };
        FVertexTexture rightDown = { 1.0f, -1.0f, 0.0f, 1.0f, 1.0f };

        // UV 좌표 관련 보정 (nTexel 오프셋 적용)
        rightUP.u *= nTexelUOffset;
        leftDown.v *= nTexelVOffset;
        rightDown.u *= nTexelUOffset;
        rightDown.v *= nTexelVOffset;

        // 각 글자의 x 좌표에 대해 오프셋 적용 (중앙 정렬을 위해 centerOffset 만큼 빼줌)
        float xOffset = quadWidth * i - centerOffset;
        leftUP.x += xOffset;
        rightUP.x += xOffset;
        leftDown.x += xOffset;
        rightDown.x += xOffset;

        FVector2D UVOffset;
        SetStartUV(Text[i], UVOffset);

        leftUP.u += (nTexelUOffset * UVOffset.X);
        leftUP.v += (nTexelVOffset * UVOffset.Y);
        rightUP.u += (nTexelUOffset * UVOffset.X);
        rightUP.v += (nTexelVOffset * UVOffset.Y);
        leftDown.u += (nTexelUOffset * UVOffset.X);
        leftDown.v += (nTexelVOffset * UVOffset.Y);
        rightDown.u += (nTexelUOffset * UVOffset.X);
        rightDown.v += (nTexelVOffset * UVOffset.Y);

        // 각 글자의 쿼드를 두 개의 삼각형으로 생성
        Vertices.Add(leftUP);
        Vertices.Add(rightUP);
        Vertices.Add(leftDown);
        Vertices.Add(rightUP);
        Vertices.Add(rightDown);
        Vertices.Add(leftDown);
    }

    CreateVertexBuffer(Text, Vertices, OutBufferInfo.VertexInfo);
    TextAtlasBufferPool[Text] = OutBufferInfo;

    return S_OK;
}

void FDXDBufferManager::SetStartUV(wchar_t hangul, FVector2D& UVOffset)
{
    //대문자만 받는중
    int StartU = 0;
    int StartV = 0;
    int offset = -1;

    if (hangul == L' ') {
        UVOffset = FVector2D(0, 0);  // Space는 특별히 UV 좌표를 (0,0)으로 설정
        offset = 0;
        return;
    }
    else if (hangul >= L'A' && hangul <= L'Z') {

        StartU = 11;
        StartV = 0;
        offset = hangul - L'A'; // 대문자 위치
    }
    else if (hangul >= L'a' && hangul <= L'z') {
        StartU = 37;
        StartV = 0;
        offset = (hangul - L'a'); // 소문자는 대문자 다음 위치
    }
    else if (hangul >= L'0' && hangul <= L'9') {
        StartU = 1;
        StartV = 0;
        offset = (hangul - L'0'); // 숫자는 소문자 다음 위치
    }
    else if (hangul >= L'가' && hangul <= L'힣')
    {
        StartU = 63;
        StartV = 0;
        offset = hangul - L'가'; // 대문자 위치
    }

    if (offset == -1)
    {
        UE_LOG(LogLevel::Warning, "Text Error");
    }

    int offsetV = (offset + StartU) / 106;
    int offsetU = (offset + StartU) % 106;

    UVOffset = FVector2D(offsetU, StartV + offsetV);

}
