#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>

// CONSTANTS
const size_t WINDOW_WIDTH = 800;
const size_t WINDOW_HEIGHT = 600;

// GLOBAL VARIABLES
GLFWwindow* pWindow;

// BEGIN GLFW CALLBACKS

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

// END GLFW CALLBACKS

bool initialize()
{
	if (!glfwInit())
	{
		std::cerr << "Could not init GLFW!" << std::endl;
		return false;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
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

	return true;
}

void setup_opengl_state()
{
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
}

void process_input()
{
	if (glfwGetKey(pWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(pWindow, true);
}

void application_loop()
{
	while (!glfwWindowShouldClose(pWindow))
	{
		process_input();
		
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

	application_loop();

	glfwDestroyWindow(pWindow);
	glfwTerminate();
	return 0;
}