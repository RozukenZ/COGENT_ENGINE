#version 450

// INPUT: Must match C++ Vertex struct (pos, color, normal, texCoord)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;

// OUTPUT to Fragment Shader
layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec2 fragTexCoord;

// UBO Camera (Set 0) - Must match C++ CameraUBO struct exactly
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    mat4 prevView; 
    mat4 prevProj;
    vec3 viewPos; 
    float time;
    float deltaTime;
} ubo;

layout(push_constant) uniform PushConsts {
    mat4 model;
    vec4 color;
} push;

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    
    fragPos = worldPos.xyz;
    fragColor = push.color.rgb * inColor; // Use push constant color * vertex color
    fragNormal = mat3(push.model) * inNormal;
    fragTexCoord = inTexCoord;

    gl_Position = ubo.proj * ubo.view * worldPos;
}