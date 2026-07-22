#version 450

layout(location = 0) in vec3 nearPoint;
layout(location = 1) in vec3 farPoint;
layout(location = 2) in mat4 fragView;
layout(location = 6) in mat4 fragProj;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outPosition;

vec4 grid(vec3 fragPos3D, float scale) {
    vec2 coord = fragPos3D.xy * scale; // Vulkan is Z-up here for world? No, in CogentEngine Z is up. 
    // Wait, let's look at Camera::Camera constructor. "Z is Up in Vulkan (custom coordinate)".
    // So the floor plane is XY plane (Z=0).
    // Let's verify: In CogentEngine.cpp, sun is (0.5, -1.0, -0.5) maybe Y is up? 
    // Camera::worldUp = vec3(0.0f, 0.0f, 1.0f); -> Z is UP!
    // So floor is XY plane!
    // If floor is XY, then fragPos3D.z is height!
    // But wait, the standard usually uses Y as up. If worldUp is (0,0,1), then Z is up.
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    float minimumx = min(derivative.x, 1.0);
    float minimumy = min(derivative.y, 1.0);
    vec4 color = vec4(0.3, 0.3, 0.3, 1.0 - min(line, 1.0));
    
    // Y axis (green) - along X=0
    if(fragPos3D.x > -0.1 * minimumx && fragPos3D.x < 0.1 * minimumx) {
        color = vec4(0.0, 1.0, 0.0, color.a);
    }
    // X axis (red) - along Y=0
    if(fragPos3D.y > -0.1 * minimumy && fragPos3D.y < 0.1 * minimumy) {
        color = vec4(1.0, 0.0, 0.0, color.a);
    }
    return color;
}

float computeDepth(vec3 pos) {
    vec4 clip_space_pos = fragProj * fragView * vec4(pos.xyz, 1.0);
    return (clip_space_pos.z / clip_space_pos.w);
}

void main() {
    // We are on the Z=0 plane (if Z is up)
    // Ray equation: P = nearPoint + t * (farPoint - nearPoint)
    // P.z = 0 -> nearPoint.z + t * (farPoint.z - nearPoint.z) = 0
    // t = -nearPoint.z / (farPoint.z - nearPoint.z)
    
    float t = -nearPoint.z / (farPoint.z - nearPoint.z);
    
    if (t < 0.0) discard; // Above horizon (looking up)
    
    vec3 fragPos3D = nearPoint + t * (farPoint - nearPoint);
    
    vec4 gridColor = grid(fragPos3D, 1.0) + grid(fragPos3D, 10.0) * 0.5;
    
    // Fade out in distance
    float linearDepth = computeDepth(fragPos3D);
    float fading = max(0.0, (1.0 - linearDepth));
    gridColor.a *= fading;
    
    // Sharp discard for crisp lines (since we write to GBuffer without blending)
    if (gridColor.a < 0.3) discard;
    
    outAlbedo = vec4(gridColor.rgb, 1.0);
    outNormal = vec4(0.0, 0.0, 1.0, 1.0); // Z is up
    outPosition = vec4(fragPos3D, 1.0);
    
    gl_FragDepth = computeDepth(fragPos3D);
}
