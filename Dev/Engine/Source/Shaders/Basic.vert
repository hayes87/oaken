#version 450

layout(location = 0) out vec2 outUV;

void main() {
    // Generate a quad (Triangle Strip)
    // 0: -0.5, -0.5
    // 1:  0.5, -0.5
    // 2: -0.5,  0.5
    // 3:  0.5,  0.5
    
    float x = float(gl_VertexIndex & 1);
    float y = float((gl_VertexIndex >> 1) & 1);

    vec2 pos = vec2(x - 0.5, y - 0.5) * 2.0;
    // Scale to make it visible (0.5 size)
    pos *= 0.5;
    
    gl_Position = vec4(pos, 0.0, 1.0);
    outUV = vec2(x, 1.0 - y);
}
