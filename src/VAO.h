#pragma once

#include <vector>
#include <glm/glm.hpp>

struct Vertex
{
	glm::tvec3<float> Position;
	glm::tvec3<float> Normal;
	glm::tvec2<float> TexCoord;
	glm::tvec4<float> Color;

	Vertex()
		: Position()
		, Normal()
		, TexCoord()
		, Color() {}
	Vertex(glm::vec3 pos, glm::vec3 norm = glm::vec3(0), glm::vec2 uv = glm::vec2(0),
		glm::vec4 col = glm::vec4(1))
		: Position(pos), Normal(norm), TexCoord(uv), Color(col) { }
};

class VAO
{
private:
	std::vector<Vertex> _verts;
	std::vector<unsigned> _indices;
	unsigned _vao = 0;
	unsigned _vbo = 0;
	unsigned _ebo = 0;


public:
	VAO(std::vector<Vertex> verts, std::vector<unsigned> indices);
	~VAO();

	const std::vector<Vertex>& verts() const { return _verts; }
	std::vector<Vertex>& verts() { return _verts; }
	const std::vector<unsigned>& indices() const { return _indices; }
	std::vector<unsigned>& indices() { return _indices; }
	void bind() const;
	void unbind() const;
	void render() const;
};