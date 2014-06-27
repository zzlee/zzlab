// Unlit_YUV.fx

#include "zzlab.fx"

// transformations
float4x4 MATRIX_MVP;
 
uniform float4 Diffuse;

// texture
texture MainTex_Y;
texture MainTex_U;
texture MainTex_V;

sampler SamplerY = sampler_state 
{ 
    Texture = (MainTex_Y);
    Mipfilter = LINEAR;
    MagFilter = LINEAR;
    MinFilter = LINEAR;

    AddressU = CLAMP;
    AddressV = CLAMP;
};

sampler SamplerU = sampler_state 
{ 
    Texture = (MainTex_U);
    Mipfilter = LINEAR;
    MagFilter = LINEAR;
    MinFilter = LINEAR;

    AddressU = CLAMP;
    AddressV = CLAMP;
};

sampler SamplerV = sampler_state 
{ 
    Texture = (MainTex_V);
    Mipfilter = LINEAR;
    MagFilter = LINEAR;
    MinFilter = LINEAR;

    AddressU = CLAMP;
    AddressV = CLAMP;
};

//-------------------------------------------------------------- 
struct VS_IN
{
    float3 Position: POSITION;
    float2 UV0 : TEXCOORD0;
};
 
struct VS_OUTPUT
{
    float4 Position : POSITION;
    float2 UV0 : TEXCOORD0;
};
 
VS_OUTPUT VS(VS_IN vs_in)
{
    VS_OUTPUT Out;

    Out.Position = mul(float4(vs_in.Position, 1), MATRIX_MVP);
    Out.UV0 = vs_in.UV0 * float2(1, 1);

    return Out;
}
 
//---------------------------------------------------------------
 
float4 PS(VS_OUTPUT ps_in) : COLOR
{
    return float4(yuv2rgb(
        tex2D(SamplerY, ps_in.UV0).r,
        tex2D(SamplerU, ps_in.UV0).r, 
        tex2D(SamplerV, ps_in.UV0).r), 1) * Diffuse;
}

//--------------------------------------------------------------- 
technique Unlit_YUV
{
    pass P0
    {
        FillMode = SOLID;
        Lighting = FALSE;
        ZEnable = FALSE;
        ZWriteEnable = FALSE;

        Sampler[0] = (SamplerY);
        Sampler[1] = (SamplerU);
        Sampler[2] = (SamplerV);

        // shaders
        VertexShader = compile vs_1_1 VS();
        PixelShader  = compile ps_2_0 PS(); 
    }
}