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

cbuffer ShadowConfigurations : register(b8)
{
    int ShadowFilterMode;
    float3 Padding;
};

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

struct PS_OUTPUT
{
    float2 Moments : SV_Target; // R32G32
};

VS_OUPUT mainVS(VS_INPUT_StaticMesh Input)
{
    VS_OUPUT Output;

    Output.Position = float4(Input.Position, 1.0);
    Output.Position = mul(Output.Position, WorldMatrix);
    Output.Position = mul(Output.Position, ShadowTransforms[Index].ViewProj);
    
    //Output.Position.z /= 10;
    return Output;
}

PS_OUTPUT mainPS(VS_OUPUT Input)
{
    PS_OUTPUT Output;
    
    float depth = Input.Position.z / Input.Position.w;
    
    float dx = ddx(depth);
    float dy = ddy(depth);
    Output.Moments = float2(depth, depth * depth + 0.25 * (dx * dx + dy * dy));
    
    return Output;
}
