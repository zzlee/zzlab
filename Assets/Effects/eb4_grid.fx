// eb4.fx

#include "zzlab.fx"

// transformations
float4x4 MATRIX_MVP;
 
// texture
texture MainTex;
//texture HorBlend;
//texture VerBlend;

sampler Sampler = sampler_state 
{ 
    Texture = (MainTex);
    MipFilter = LINEAR;
    MagFilter = LINEAR;
    MinFilter = LINEAR;

    AddressU = CLAMP;
    AddressV = CLAMP;
};

/*
sampler SamplerH = sampler_state 
{ 
    Texture = (HorBlend);
    Mipfilter = LINEAR;

    AddressU = CLAMP;
    AddressV = CLAMP;
};

sampler SamplerV = sampler_state 
{ 
    Texture = (VerBlend);
    Mipfilter = LINEAR;

    AddressU = CLAMP;
    AddressV = CLAMP;
};
*/

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
    Out.UV0 = vs_in.UV0;

    return Out;
}
 
//---------------------------------------------------------------
 
float4 PS(VS_OUTPUT ps_in) : COLOR
{
    return tex2D(Sampler, ps_in.UV0);
    //return float4(1, 0, 0, 1);
}

//--------------------------------------------------------------- 
technique eb4
{
    pass P0
    {
        FillMode = SOLID;
        Lighting = FALSE;
        ZEnable = FALSE;
        ZWriteEnable = FALSE;
        CullMode = None;

        Sampler[0] = (Sampler);
        //Sampler[1] = (SamplerH);
        //Sampler[2] = (SamplerV);

        MultiSampleAntialias = TRUE;

        // shaders
        VertexShader = compile vs_1_1 VS();
        PixelShader  = compile ps_2_0 PS(); 
    }
}