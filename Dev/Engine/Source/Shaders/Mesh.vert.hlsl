struct VSInput {
    float3 Position : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 UV : TEXCOORD2;
    float4 Weights : TEXCOORD3;
    uint4 Joints : TEXCOORD4;
};

struct VSOutput {
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float3 Normal : TEXCOORD1;
};

cbuffer UniformBlock : register(b0, space1) {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4x4 jointMatrices[100];
};

VSOutput main(VSInput input) {
    VSOutput output;
    
    float4x4 skinMatrix = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    
    if (dot(input.Weights, float4(1,1,1,1)) > 0.0) {
        skinMatrix = 
            input.Weights.x * jointMatrices[input.Joints.x] +
            input.Weights.y * jointMatrices[input.Joints.y] +
            input.Weights.z * jointMatrices[input.Joints.z] +
            input.Weights.w * jointMatrices[input.Joints.w];
    }
    
    // Note: mul(vector, matrix) is row-vector * matrix (standard HLSL)
    // But GLM is column-major. 
    // If we send column-major matrices to HLSL, we might need to transpose or use mul(matrix, vector).
    // SDL_GPU / DX12 usually expects row-major in constant buffers unless specified otherwise.
    // Let's assume standard mul(matrix, vector) for column-major if we use #pragma pack_matrix(column_major) or similar.
    // Or just try mul(matrix, vector).
    
    float4 localPos = float4(input.Position, 1.0);
    float4 skinnedPos = mul(skinMatrix, localPos); // Matrix * Vector
    float4 worldPos = mul(model, skinnedPos);
    float4 viewPos = mul(view, worldPos);
    output.Position = mul(proj, viewPos);
    
    output.UV = input.UV;
    
    // Normal
    // Inverse transpose is needed for non-uniform scaling
    // For now, just rotate
    float3 skinnedNormal = mul((float3x3)skinMatrix, input.Normal);
    output.Normal = mul((float3x3)model, skinnedNormal);
    
    return output;
}
