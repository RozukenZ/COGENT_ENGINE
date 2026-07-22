#version 450

layout(set = 0, binding = 0) uniform sampler2D hdrColor;
layout(set = 0, binding = 1) uniform sampler2D bloomTexture;

layout(push_constant) uniform PushConstants {
    float exposure;
    float gamma;
} pc;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

// ACES filmic tonemapping curve
vec3 ACESFilm(vec3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(hdrColor, inUV).rgb;
    vec3 bloom = texture(bloomTexture, inUV).rgb;
    
    // Add bloom
    hdr += bloom; // additive blending
    
    // Exposure tone mapping
    vec3 mapped = vec3(1.0) - exp(-hdr * pc.exposure);
    
    // ACES Tonemapping
    mapped = ACESFilm(hdr * pc.exposure);
    
    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / pc.gamma));
    
    outColor = vec4(mapped, 1.0);
}
