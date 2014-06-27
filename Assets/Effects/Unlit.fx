// Unlit.fx

#include "zzlab.fx"

uniform float4x4 MATRIX_MVP;
 
uniform float4 Diffuse;
uniform texture MainTex;

sampler MainSampler = sampler_state 
{ 
    Texture = (MainTex);
    Mipfilter = LINEAR;

    AddressU = CLAMP;
    AddressV = CLAMP;
};

//-------------------------------------------------------------- 
struct VS_IN
{
    float3 Position: POSITION;
    float2 UV0 : TEXCOORD;
};
 
struct VS_OUTPUT
{
    float4 Position : POSITION;
    float2 UV0 : TEXCOORD;
};
 
VS_OUTPUT VS(VS_IN IN)
{
    VS_OUTPUT OUT;

    OUT.Position = mul(float4(IN.Position, 1), MATRIX_MVP);
    OUT.UV0 = IN.UV0;

    return OUT;
}
 
//---------------------------------------------------------------
 
float4 PS(VS_OUTPUT IN) : COLOR
{
    return tex2D(MainSampler, IN.UV0) * Diffuse;
}

//--------------------------------------------------------------- 
technique Unlit
{
    pass 
    {
        FillMode = SOLID;
        Lighting = FALSE;
        ZEnable = FALSE;
        ZWriteEnable = FALSE;

        Sampler[0] = (MainSampler);

        // shaders
        VertexShader = compile vs_1_1 VS();
        PixelShader  = compile ps_2_0 PS(); 
    }
}