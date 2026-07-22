#version 450

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    mat4 prevView; 
    mat4 prevProj;
    vec3 viewPos; 
    float time;
    float deltaTime;
} ubo;

layout(location = 0) out vec3 nearPoint;
layout(location = 1) out vec3 farPoint;
layout(location = 2) out mat4 fragView;
layout(location = 6) out mat4 fragProj;

// Quad covering the entire screen
vec3 gridPlane[6] = vec3[](
    vec3(1, 1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0),
    vec3(-1, -1, 0), vec3(1, 1, 0), vec3(1, -1, 0)
);

vec3 unprojectPoint(float x, float y, float z, mat4 view, mat4 proj) {
    mat4 viewInv = inverse(view);
    mat4 projInv = inverse(proj);
    vec4 unprojectedPoint =  viewInv * projInv * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

void main() {
    vec3 p = gridPlane[gl_VertexIndex];
    nearPoint = unprojectPoint(p.x, p.y, 0.0, ubo.view, ubo.proj); // z=0 is near plane (Vulkan)
    farPoint = unprojectPoint(p.x, p.y, 1.0, ubo.view, ubo.proj);  // z=1 is far plane (Vulkan)
    
    fragView = ubo.view;
    fragProj = ubo.proj;
    
    gl_Position = vec4(p, 1.0); // Render quad directly to clip space
}
