struct PixelInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 colour : COLOR;
};

Texture2D diffuse_texture;

SamplerState Sampler0 {
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

float4 PS(PixelInput input) : SV_Target {
    float4 colour = diffuse_texture.Sample(Sampler0, input.uv) * input.colour;
    if (colour.a < 0.1f)
        discard;
    return colour;
}
