#pragma once
#include <array>
#include <glm/glm.hpp>

namespace Cogent::Math {

    struct Plane {
        glm::vec3 normal;
        float distance;

        void normalize() {
            float length = glm::length(normal);
            normal /= length;
            distance /= length;
        }

        // Returns distance from point to plane. Positive = inside/front.
        float getSignedDistance(const glm::vec3& point) const {
            return glm::dot(normal, point) + distance;
        }
    };

    struct AABB {
        glm::vec3 min;
        glm::vec3 max;
        
        glm::vec3 getCenter() const { return (min + max) * 0.5f; }
        glm::vec3 getExtent() const { return (max - min) * 0.5f; }
    };

    class Frustum {
    public:
        enum Side { LEFT = 0, RIGHT, TOP, BOTTOM, BACK, FRONT };
        std::array<Plane, 6> planes;

        void update(const glm::mat4& viewProj) {
            // Extraction (Gribb/Hartmann method)
            auto& m = viewProj;

            planes[LEFT].normal.x = m[0][3] + m[0][0];
            planes[LEFT].normal.y = m[1][3] + m[1][0];
            planes[LEFT].normal.z = m[2][3] + m[2][0];
            planes[LEFT].distance = m[3][3] + m[3][0];

            planes[RIGHT].normal.x = m[0][3] - m[0][0];
            planes[RIGHT].normal.y = m[1][3] - m[1][0];
            planes[RIGHT].normal.z = m[2][3] - m[2][0];
            planes[RIGHT].distance = m[3][3] - m[3][0];

            planes[BOTTOM].normal.x = m[0][3] + m[0][1];
            planes[BOTTOM].normal.y = m[1][3] + m[1][1];
            planes[BOTTOM].normal.z = m[2][3] + m[2][1];
            planes[BOTTOM].distance = m[3][3] + m[3][1];

            planes[TOP].normal.x = m[0][3] - m[0][1];
            planes[TOP].normal.y = m[1][3] - m[1][1];
            planes[TOP].normal.z = m[2][3] - m[2][1];
            planes[TOP].distance = m[3][3] - m[3][1];

            planes[BACK].normal.x = m[0][3] + m[0][2];
            planes[BACK].normal.y = m[1][3] + m[1][2];
            planes[BACK].normal.z = m[2][3] + m[2][2];
            planes[BACK].distance = m[3][3] + m[3][2];

            planes[FRONT].normal.x = m[0][3] - m[0][2];
            planes[FRONT].normal.y = m[1][3] - m[1][2];
            planes[FRONT].normal.z = m[2][3] - m[2][2];
            planes[FRONT].distance = m[3][3] - m[3][2];

            for (auto& plane : planes) {
                plane.normalize();
            }
        }

        bool checkAABB(const AABB& box) const {
            // Check box against all 6 planes
            for (const auto& plane : planes) {
                glm::vec3 positiveVertex = box.min;
                if (plane.normal.x >= 0) positiveVertex.x = box.max.x;
                if (plane.normal.y >= 0) positiveVertex.y = box.max.y;
                if (plane.normal.z >= 0) positiveVertex.z = box.max.z;

                if (plane.getSignedDistance(positiveVertex) < 0) {
                    return false; // Outside
                }
            }
            return true;
        }
    };
}
