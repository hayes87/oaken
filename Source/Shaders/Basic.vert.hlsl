struct VSOutput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

static const float3 positions[3] = {
    float3( 0.0,  0.5, 0.0),
    float3( 0.5, -0.5, 0.0),
    float3(-0.5, -0.5, 0.0)
};

static const float3 colors[3] = {
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0)
};

VSOutput main(uint vertexID : SV_VertexID) {
    VSOutput output;
    output.position = float4(positions[vertexID], 1.0);
    output.color = float4(colors[vertexID], 1.0);
    return output;
}
