struct VSOutput {
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

cbuffer TransformBuffer : register(b0, space1) {
    float4x4 model;
};

VSOutput main(uint VertexIndex : SV_VertexID) {
    VSOutput output;
    
    // Generate a quad (Triangle Strip)
    float x = float(VertexIndex & 1);
    float y = float((VertexIndex >> 1) & 1);

    float2 pos = float2(x - 0.5, y - 0.5) * 2.0;
    pos *= 0.5; // Scale
    
    output.Position = mul(model, float4(pos, 0.0, 1.0));
    output.UV = float2(x, 1.0 - y);
    
    return output;
}
