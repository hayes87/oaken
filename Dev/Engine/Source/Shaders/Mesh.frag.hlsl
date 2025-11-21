struct PSInput {
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float3 Normal : TEXCOORD1;
};

float4 main(PSInput input) : SV_TARGET {
    float3 lightDir = normalize(float3(1.0, 1.0, 1.0));
    float diff = max(dot(normalize(input.Normal), lightDir), 0.0);
    float3 diffuse = diff * float3(1.0, 1.0, 1.0);
    float3 ambient = float3(0.1, 0.1, 0.1);
    
    return float4(ambient + diffuse, 1.0);
}
