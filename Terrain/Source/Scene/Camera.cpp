#include "Camera.h"
#include <glm/gtc/matrix_access.hpp>

Camera::Camera(float fov, float aspectRatio, float nearPlane, float farPlane) : 
	m_FieldOfView(fov), m_AspectRatio(aspectRatio), m_Near(nearPlane), m_Far(farPlane)
{
	m_CameraRenderMatrices.Projection = glm::perspective(glm::radians(m_FieldOfView), m_AspectRatio, m_Near, m_Far);
}

void Camera::Move(const glm::vec3& direction)
{
	m_Position += direction.x * Right();
	m_Position -= direction.y * Up();
	m_Position -= direction.z * Forward();
}

void Camera::setPosition(const glm::vec3& position)
{
	m_Position = position;
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
	return m_CameraRenderMatrices.Projection;
}

glm::mat4 Camera::getView()
{
	const float yawSign = Up().y < 0 ? -1.0f : 1.0f;
	const glm::vec3 lookAt = m_Position + Forward();
	
	if (!m_FollowPoint)
		m_CameraRenderMatrices.View = glm::lookAt(m_Position, lookAt, glm::vec3{ 0.0f, yawSign, 0.0f });
	else
	{
		glm::vec3 direction = glm::normalize(m_FocalPoint - m_Position);
		m_CameraRenderMatrices.View = glm::lookAt(m_Position, m_Position + glm::normalize(direction), glm::vec3{ 0.0f, 1.0f, 0.0f });
	}
	return m_CameraRenderMatrices.View;
}

void Camera::updateMatrices()
{
	getView();

	glm::mat4 matrix = getProjection() * getView();

	planes[LEFT].x = matrix[0].w + matrix[0].x;
	planes[LEFT].y = matrix[1].w + matrix[1].x;
	planes[LEFT].z = matrix[2].w + matrix[2].x;
	planes[LEFT].w = matrix[3].w + matrix[3].x;

	planes[RIGHT].x = matrix[0].w - matrix[0].x;
	planes[RIGHT].y = matrix[1].w - matrix[1].x;
	planes[RIGHT].z = matrix[2].w - matrix[2].x;
	planes[RIGHT].w = matrix[3].w - matrix[3].x;

	planes[TOP].x = matrix[0].w - matrix[0].y;
	planes[TOP].y = matrix[1].w - matrix[1].y;
	planes[TOP].z = matrix[2].w - matrix[2].y;
	planes[TOP].w = matrix[3].w - matrix[3].y;

	planes[BOTTOM].x = matrix[0].w + matrix[0].y;
	planes[BOTTOM].y = matrix[1].w + matrix[1].y;
	planes[BOTTOM].z = matrix[2].w + matrix[2].y;
	planes[BOTTOM].w = matrix[3].w + matrix[3].y;

	planes[BACK].x = matrix[0].w + matrix[0].z;
	planes[BACK].y = matrix[1].w + matrix[1].z;
	planes[BACK].z = matrix[2].w + matrix[2].z;
	planes[BACK].w = matrix[3].w + matrix[3].z;

	planes[FRONT].x = matrix[0].w - matrix[0].z;
	planes[FRONT].y = matrix[1].w - matrix[1].z;
	planes[FRONT].z = matrix[2].w - matrix[2].z;
	planes[FRONT].w = matrix[3].w - matrix[3].z;

	for (auto i = 0; i < planes.size(); i++)
	{
		float length = sqrtf(planes[i].x * planes[i].x + planes[i].y * planes[i].y + planes[i].z * planes[i].z);
		planes[i] /= length;
	}
}

void Camera::setFOV(float fov)
{
	m_FieldOfView = fov;
	m_CameraRenderMatrices.Projection = glm::perspective(glm::radians(m_FieldOfView), m_AspectRatio, m_Near, m_Far);
}

void Camera::setAspectRatio(float ar)
{
	m_AspectRatio = ar;
	m_CameraRenderMatrices.Projection = glm::perspective(glm::radians(m_FieldOfView), m_AspectRatio, m_Near, m_Far);
}

void Camera::setPlanes(float nearPlane, float farPlane)
{
	m_Near = nearPlane;
	m_Far = farPlane;
	m_CameraRenderMatrices.Projection = glm::perspective(glm::radians(m_FieldOfView), m_AspectRatio, m_Near, m_Far);
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

void Camera::setFocalPoint(const glm::vec3& focalPoint)
{
	m_FocalPoint = focalPoint;
	m_FollowPoint = true;
}
