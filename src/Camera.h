#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Camera
{
	glm::vec3 pos;
	glm::quat rot = glm::quat(1, 0, 0, 0);

	glm::mat4 matrix() const { return glm::toMat4(rot) * glm::translate(glm::mat4(1), -pos); }
};