cbuffer MatrixBuffer {
    matrix proj;
};

struct VertexInput {
    float2 position : POSITION;
    float2 uv : TEXCOORD;
    float4 colour : COLOR;
    float z : ZVALUE;
};

struct PixelInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 colour : COLOR;
};

PixelInput VS(VertexInput input) {
    PixelInput output;
    output.position = mul(float4(input.position, input.z, 1), proj);
    output.uv = input.uv;
    output.colour = input.colour;
    return output;
}