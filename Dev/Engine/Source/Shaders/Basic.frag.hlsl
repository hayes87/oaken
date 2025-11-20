struct VSOutput {
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

Texture2D tex : register(t0, space2);
SamplerState samp : register(s0, space2);

float4 main(VSOutput input) : SV_Target {
    return tex.Sample(samp, input.UV);
}
