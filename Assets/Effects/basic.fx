// transformations
float4x4 MATRIX_M;
float4x4 MATRIX_V;
float4x4 MATRIX_P;
float4x4 MATRIX_MV;
float4x4 MATRIX_VP;
float4x4 MATRIX_MVP;
 
// texture
texture MainTex;

sampler Sampler0 = sampler_state 
{ 
    Texture = (MainTex);
    Mipfilter = LINEAR;

    AddressU = CLAMP;
    AddressV = CLAMP;
};

//-------------------------------------------------------------- 
struct VS_IN
{
    float3 Pos    : POSITION;
    float4 Colour : COLOR0;
    float2 UV0 : TEXCOORD0;
};
 
struct VS_OUTPUT
{
    float4 Pos    : POSITION;
    float4 Colour : COLOR0;
    float2 UV0 : TEXCOORD0;
};
 
VS_OUTPUT VS( VS_IN vs_in )
{
    VS_OUTPUT Out = (VS_OUTPUT)0;
 
    //Out.Pos = mul(float4(vs_in.Pos, 1), mul(mul(MATRIX_M, MATRIX_V), MATRIX_P));
    Out.Pos = mul(float4(vs_in.Pos, 1), MATRIX_MVP);
    Out.Colour = vs_in.Colour;                                // diffuse colour 
    Out.UV0 = vs_in.UV0;
    return Out;
}
 
//---------------------------------------------------------------
 
float4 PS( VS_OUTPUT ps_in ) : COLOR
{
    return tex2D(Sampler0, ps_in.UV0) * ps_in.Colour;
}

//--------------------------------------------------------------- 
technique BasicShader
{
    pass P0
    {
        FillMode = SOLID;
        Lighting = FALSE;
        CullMode = NONE;
        ZEnable = FALSE;
        ZWriteEnable = FALSE;
        AlphaBlendEnable = TRUE;
        SrcBlend = ONE;
        DestBlend = ONE;
        BlendOp = ADD;

        Sampler[0] = (Sampler0);

        // shaders
        VertexShader = compile vs_1_1 VS();
        PixelShader  = compile ps_2_0 PS(); 
    }
}