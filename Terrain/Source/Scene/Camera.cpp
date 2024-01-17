#include "Camera.h"
#include <glm/gtc/matrix_access.hpp>

Camera::Camera(float fov, float aspectRatio, float nearPlane, float farPlane) : 
	m_FieldOfView(fov), m_AspectRatio(aspectRatio), m_Near(nearPlane), m_Far(farPlane)
{
	m_Projection = glm::perspective(glm::radians(m_FieldOfView), m_AspectRatio, m_Near, m_Far);
}

void Camera::Move(const glm::vec3& direction)
{
	m_Position += direction.x * Right();
	m_Position -= direction.y * Up();
	m_Position -= direction.z * Forward();
}

void Camera::Rotate(const glm::vec2& axis)
{
	const float yawSign = Up().y < 0.0f ? -1.0f : 1.0f;
	m_Yaw += axis.y * yawSign;

	if (glm::abs(m_Pitch + axis.x) < 2.0f)
		m_Pitch += axis.x;
}

glm::mat4 Camera::getProjection() const
{
	return m_Projection;
}

glm::mat4 Camera::getView()
{
	const float yawSign = Up().y < 0 ? -1.0f : 1.0f;
	const glm::vec3 lookAt = m_Position + Forward();
	m_View = glm::lookAt(m_Position, lookAt, glm::vec3{ 0.f, yawSign, 0.f });

	return m_View;
}

void Camera::setFOV(float fov)
{
	m_FieldOfView = fov;
	m_Projection = glm::perspective(glm::radians(m_FieldOfView), m_AspectRatio, m_Near, m_Far);
}

void Camera::setAspectRatio(float ar)
{
	m_AspectRatio = ar;
	m_Projection = glm::perspective(glm::radians(m_FieldOfView), m_AspectRatio, m_Near, m_Far);
}

void Camera::setPlanes(float nearPlane, float farPlane)
{
	m_Near = nearPlane;
	m_Far = farPlane;
	m_Projection = glm::perspective(glm::radians(m_FieldOfView), m_AspectRatio, m_Near, m_Far);
}

glm::vec3 Camera::Right() const
{
	return glm::rotate(Orientation(), glm::vec3(1.0f, 0.0f, 0.0f));
}

glm::vec3 Camera::Up() const
{
	return glm::rotate(Orientation(), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::vec3 Camera::Forward() const
{
	return glm::rotate(Orientation(), glm::vec3(0.0f, 0.0f, -1.0f));
}

glm::quat Camera::Orientation() const
{
	return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
}
