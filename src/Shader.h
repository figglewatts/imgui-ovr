#pragma once
#include <string>
#include <glm/glm.hpp>
#include "GL.h"

class Shader
{
private:
	static const unsigned INFOLOG_BUFF_LEN = 512;

	unsigned _vertHandle = 0;
	unsigned _fragHandle = 0;
	unsigned _progHandle = 0;
	std::string _name;

	void compileAndLink(const std::string& path);
	static GLuint loadSource(const std::string& path, GLenum type);
	bool checkCompileErr(GLuint shader) const;
	bool checkLinkErr(GLuint program) const;
	unsigned getLocation(const std::string& name) const;

public:
	Shader(std::string name, const std::string& path);
	Shader(const Shader& other) = delete;
	Shader& operator=(const Shader& other) = delete;
	~Shader();

	unsigned handle() const { return this->_progHandle; }
	void bind() const;
	void unbind() const;

	void setUniform(const std::string& name, bool value) const;
	void setUniform(const std::string& name, int value) const;
	void setUniform(const std::string& name, float value) const;
	void setUniform(const std::string& name, glm::mat4 value, bool transpose) const;
	void setUniform(const std::string& name, glm::mat3 value, bool transpose) const;
	void setUniform(const std::string& name, glm::vec2 value) const;
	void setUniform(const std::string& name, glm::vec3 value) const;
	void setUniform(const std::string& name, glm::vec4 value) const;

};
