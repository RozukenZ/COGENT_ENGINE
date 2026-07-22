#pragma once
#include <vector>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include "../Core/Math/Frustum.hpp"

namespace Cogent::Geometry {

    struct Meshlet {
        uint32_t vertexCount = 0;
        uint32_t triangleCount = 0;
        uint32_t vertices[64]; // Maps local index -> global index
        uint8_t indices[126 * 3]; // Maps triangle vertex -> local index (0-63)

        // Bounding Volume for Cluster Culling / Frustum Culling
        Math::AABB aabb;
        // Cone cone; // Normal cone for backface culling (future)
    };

    class MeshletBuilder {
    public:
        // Proper Meshlet Builder: Vertex Reuse & Hardware Limits
        // Max Vertices: 64
        // Max Primitives: 126 (NV Mesh Shader Limit is 126 triangles, 64 verts usually)
        static std::vector<Meshlet> build(const std::vector<uint32_t>& indices, const std::vector<glm::vec3>& positions) {
            std::vector<Meshlet> meshlets;
            
            const uint32_t MAX_VERTS = 64;
            const uint32_t MAX_PRIMS = 124; // Safe limit

            Meshlet currentMeshlet{};
            resetMeshlet(currentMeshlet);

            // Temporary map to track if a GLOBAL vertex is already in LOCAL meshlet
            // Key: Global Index, Value: Local Index (0-63)
            // Reset every meshlet.
            // Using a simple array/vector loop for lookups because N<=64 is small enough for linear scan or small map.
            // A fixed size array lookup is faster if global indices are small, but they aren't.
            // Let's use linear scan on currentMeshlet.vertices for simplicity & no allocs.
            
            size_t totalTriangles = indices.size() / 3;

            for (size_t i = 0; i < totalTriangles; i++) {
                uint32_t idx0 = indices[i * 3 + 0];
                uint32_t idx1 = indices[i * 3 + 1];
                uint32_t idx2 = indices[i * 3 + 2];

                // Check which vertices are already in the meshlet
                uint8_t localIdx0 = getLocalIndex(currentMeshlet, idx0);
                uint8_t localIdx1 = getLocalIndex(currentMeshlet, idx1);
                uint8_t localIdx2 = getLocalIndex(currentMeshlet, idx2);

                uint32_t newVerticesCount = 0;
                if (localIdx0 == 0xFF) newVerticesCount++;
                if (localIdx1 == 0xFF) newVerticesCount++;
                if (localIdx2 == 0xFF) newVerticesCount++;

                // Does it fit?
                if (currentMeshlet.vertexCount + newVerticesCount > MAX_VERTS || currentMeshlet.triangleCount + 1 > MAX_PRIMS) {
                    // Push and Reset
                    meshlets.push_back(currentMeshlet);
                    resetMeshlet(currentMeshlet);

                    // Re-evaluate for new meshlet (guaranteed to fit 1 triangle)
                    localIdx0 = addVertex(currentMeshlet, idx0, positions[idx0]);
                    localIdx1 = addVertex(currentMeshlet, idx1, positions[idx1]);
                    localIdx2 = addVertex(currentMeshlet, idx2, positions[idx2]);
                } else {
                    // Add Vertices if not present
                    if (localIdx0 == 0xFF) localIdx0 = addVertex(currentMeshlet, idx0, positions[idx0]);
                    if (localIdx1 == 0xFF) localIdx1 = addVertex(currentMeshlet, idx1, positions[idx1]);
                    if (localIdx2 == 0xFF) localIdx2 = addVertex(currentMeshlet, idx2, positions[idx2]);
                }

                // Add Primitives
                currentMeshlet.indices[currentMeshlet.triangleCount * 3 + 0] = localIdx0;
                currentMeshlet.indices[currentMeshlet.triangleCount * 3 + 1] = localIdx1;
                currentMeshlet.indices[currentMeshlet.triangleCount * 3 + 2] = localIdx2;
                currentMeshlet.triangleCount++;
            }

            // Push last partial meshlet
            if (currentMeshlet.triangleCount > 0) {
                meshlets.push_back(currentMeshlet);
            }

            return meshlets;
        }

    private:
        static void resetMeshlet(Meshlet& m) {
            m.vertexCount = 0;
            m.triangleCount = 0;
            // AABB reset
            m.aabb.min = glm::vec3(FLT_MAX);
            m.aabb.max = glm::vec3(-FLT_MAX);
        }

        static uint8_t getLocalIndex(const Meshlet& m, uint32_t globalIdx) {
            for (uint32_t i = 0; i < m.vertexCount; i++) {
                if (m.vertices[i] == globalIdx) {
                    return static_cast<uint8_t>(i);
                }
            }
            return 0xFF; // Not found
        }

        static uint8_t addVertex(Meshlet& m, uint32_t globalIdx, const glm::vec3& pos) {
            m.vertices[m.vertexCount] = globalIdx;
            
            // Update Bounds
            m.aabb.min = glm::min(m.aabb.min, pos);
            m.aabb.max = glm::max(m.aabb.max, pos);

            return static_cast<uint8_t>(m.vertexCount++);
        }
    };
}

