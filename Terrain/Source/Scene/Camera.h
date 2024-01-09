#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>


class Camera
{
public:
	Camera(float fov, float aspectRatio, float nearPlane, float farPlane);

	void Move(const glm::vec3& direction);
	void Rotate(const glm::vec2& axis);

	glm::mat4 getProjection() const;
	glm::mat4 getView();

	void setFOV(float fov);
	void setAspectRatio(float ar);
	void setPlanes(float nearPlane, float farPlane);

private:
	glm::vec3 Right() const;
	glm::vec3 Up() const;
	glm::vec3 Forward() const;
	glm::quat Orientation() const;

private:
	glm::mat4 m_Projection{ 1.0f };
	glm::mat4 m_View{ 1.0f };

	glm::vec3 m_Position{ 0.0f, 0.0f, 10.0f };

	float m_FieldOfView;
	float m_AspectRatio;

	float m_Near, m_Far;

	float m_Pitch = 0.0f;
	float m_Yaw = 0.0f;
};
