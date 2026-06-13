#version 450
layout(location = 0) in vec2 inUV;
layout(location = 0) out float outColor;

layout(set = 0, binding = 0) uniform sampler2D ssaoRawTex;

void main() {
    vec2 texelSize = 1.0 / textureSize(ssaoRawTex, 0);
    float result = 0.0;
    
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            result += texture(ssaoRawTex, inUV + vec2(x, y) * texelSize).r;
        }
    }
    
    outColor = result / 9.0;
}
