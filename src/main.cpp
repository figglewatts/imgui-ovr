#include "GL.h"

#include <iostream>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include "VAO.h"
#include "Shader.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"

// CONSTANTS
const size_t WINDOW_WIDTH = 800;
const size_t WINDOW_HEIGHT = 600;
const size_t GL_VER_MAJOR = 4;
const size_t GL_VER_MINOR = 3;
const float FOV_DEGREES = 60;
const float Z_NEAR = 0.1f;
const float Z_FAR = 100;
const char* glsl_version = "#version 330 core";

// GLOBAL VARIABLES
GLFWwindow* pWindow;

// vao
std::vector<Vertex> verts = 
{
	Vertex(glm::vec3(-0.5f, -0.5f, 0), glm::vec3(), glm::vec2(), glm::vec4(1, 0, 0, 1)),
	Vertex(glm::vec3(0.5f, -0.5f, 0), glm::vec3(), glm::vec2(), glm::vec4(0, 1, 0, 1)),
	Vertex(glm::vec3(0, 0.5f, 0), glm::vec3(), glm::vec2(), glm::vec4(0, 0, 1, 1))
};
std::vector<unsigned> indices = { 0, 1, 2 };
VAO* pVAO;

Shader *pShader;

// camera matrix, translated along z-axis for zoom back
glm::mat4 camera = glm::translate(glm::mat4(1), glm::vec3(0, 0, -5.f));
glm::mat4 projection;

// END GLOBAL VARIABLES

// BEGIN GLFW CALLBACKS

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	projection = glm::perspective(glm::radians(FOV_DEGREES), (float)width / (float)height, Z_NEAR, Z_FAR);
}

// END GLFW CALLBACKS

bool initialize()
{
	if (!glfwInit())
	{
		std::cerr << "Could not init GLFW!" << std::endl;
		return false;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GL_VER_MAJOR);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GL_VER_MINOR);
	pWindow = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "imgui-ovr", nullptr, nullptr);
	if (!pWindow)
	{
		std::cerr << "Could not create GLFW window!" << std::endl;
		return false;
	}

	glfwMakeContextCurrent(pWindow);

	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
	{
		std::cerr << "Failed to initialize GLAD" << std::endl;
		return false;
	}

	glfwSetFramebufferSizeCallback(pWindow, framebuffer_size_callback);

	projection = glm::perspective(glm::radians(FOV_DEGREES), (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, Z_NEAR, Z_FAR);

	return true;
}

void setup_opengl_state()
{
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	glClearColor(0.2f, 0.2f, 0.2f, 1);
}

void init_imgui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui_ImplGlfw_InitForOpenGL(pWindow, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	ImGui::StyleColorsDark();


}

void load_data()
{
	pVAO = new VAO(verts, indices);
	pShader = new Shader("basic", "shaders/basic");
}

void process_input()
{
	if (glfwGetKey(pWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(pWindow, true);
}

void render()
{
	ImGui::ShowDemoWindow();
}

void application_loop()
{
	while (!glfwWindowShouldClose(pWindow))
	{
		process_input();
		
		glClear(GL_COLOR_BUFFER_BIT);

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		render();

		ImGui::Render();

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		
		glfwSwapBuffers(pWindow);
		glfwPollEvents();
	}
}

int main()
{
	if (!initialize())
	{
		return -1;
	}
	setup_opengl_state();
	init_imgui();
	load_data();

	application_loop();

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(pWindow);
	glfwTerminate();
	return 0;
}