#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

class Camera {
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    // Euler Angles
    float yaw;
    float pitch;

    // Camera options
    float movementSpeed;
    float mouseSensitivity;

    Camera(glm::vec3 startPosition = glm::vec3(0.0f, 0.0f, 3.0f)) {
        position = startPosition;
        worldUp = glm::vec3(0.0f, 0.0f, 1.0f); // Z is Up in Vulkan (custom coordinate)
        yaw = -90.0f;
        pitch = 0.0f;
        movementSpeed = 2.5f;
        mouseSensitivity = 0.1f;
        updateCameraVectors();
    }

    glm::mat4 getViewMatrix() {
        return glm::lookAt(position, position + front, up);
    }

    void processKeyboard(GLFWwindow* window, float deltaTime) {
        float velocity = movementSpeed * deltaTime;
        
        // W A S D Movement
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            position += front * velocity;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            position -= front * velocity;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            position -= right * velocity;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            position += right * velocity;
            
        // Fly Up/Down (Space/Ctrl)
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            position += worldUp * velocity;
        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
            position -= worldUp * velocity;
    }

    void processMouseMovement(float xoffset, float yoffset) {
        xoffset *= mouseSensitivity;
        yoffset *= mouseSensitivity;

        yaw   += xoffset;
        pitch += yoffset;

        // Constraint agar tidak leher patah
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        updateCameraVectors();
    }

private:
    void updateCameraVectors() {
        // Hitung vektor depan baru
        glm::vec3 newFront;
        newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        newFront.y = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        newFront.z = sin(glm::radians(pitch));
        front = glm::normalize(newFront);

        // Hitung Right dan Up vector
        right = glm::normalize(glm::cross(front, worldUp));  
        up    = glm::normalize(glm::cross(right, front));
    }
};