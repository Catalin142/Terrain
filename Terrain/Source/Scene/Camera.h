#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <array>

struct CameraRenderMatrices
{
	glm::mat4 Projection{ 1.0f };
	glm::mat4 View{ 1.0f };
};

struct Frustum
{
	glm::vec4 Top;
	glm::vec4 Bottom;

	glm::vec4 Right;
	glm::vec4 Left;

	glm::vec4 Far;
	glm::vec4 Near;
};

class Camera
{
public:
	Camera(float fov, float aspectRatio, float nearPlane, float farPlane);

	void Move(const glm::vec3& direction);
	void setPosition(const glm::vec3& position);
	void Rotate(const glm::vec2& axis);

	glm::mat4 getProjection() const;
	glm::mat4 getView();
	CameraRenderMatrices getRenderMatrices() const { return m_CameraRenderMatrices; }

	void updateMatrices();

	void setFOV(float fov);
	void setAspectRatio(float ar);
	void setPlanes(float nearPlane, float farPlane);

	const glm::vec3& getPosition() const { return m_Position; }

	glm::vec3 Right() const;
	glm::vec3 Up() const;
	glm::vec3 Forward() const;
	glm::quat Orientation() const;

	float getYaw() { return m_Yaw; }

	const std::array<glm::vec4, 6>& getFrustum() const { return planes; }

	void setFocalPoint(const glm::vec3& focalPoint);

private:
	CameraRenderMatrices m_CameraRenderMatrices;

	glm::vec3 m_Position{ 0.0f, -10.0f, 0.0f };

	enum side { LEFT = 0, RIGHT = 1, TOP = 2, BOTTOM = 3, BACK = 4, FRONT = 5 };
	std::array<glm::vec4, 6> planes;

	float m_FieldOfView;
	float m_AspectRatio;

	float m_Near, m_Far;

	float m_Pitch = 0.0f;
	float m_Yaw = 0.0f;

	glm::vec3 m_FocalPoint{ 0.0f };
	bool m_FollowPoint = false;
};
