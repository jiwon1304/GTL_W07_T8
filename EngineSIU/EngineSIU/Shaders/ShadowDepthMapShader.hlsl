//#include "ShaderRegisters.hlsl"

struct VS_INPUT_StaticMesh
{
    float3 Position : POSITION;
    float4 Color : COLOR;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float2 UV : TEXCOORD;
    uint MaterialIndex : MATERIAL_INDEX;
};

struct ViewProjTransform
{
    row_major matrix ViewProj;
};

StructuredBuffer<ViewProjTransform> ShadowTransforms : register(t9);

cbuffer ObjectBuffer : register(b9)
{
    row_major matrix WorldMatrix;
};

cbuffer ViewProj : register(b10)
{
    uint Index;
}
    
struct VS_OUPUT
{
    float4 Position : SV_Position;
};

VS_OUPUT mainVS(VS_INPUT_StaticMesh Input)
{
    VS_OUPUT Output;

    Output.Position = float4(Input.Position, 1.0);
    Output.Position = mul(Output.Position, WorldMatrix);
    Output.Position = mul(Output.Position, ShadowTransforms[Index].ViewProj);
    
    return Output;
}
