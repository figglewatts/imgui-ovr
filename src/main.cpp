#include "GL.h"

#include <iostream>
#include "Camera.h"
#include "imgui.h"
#include "VR.h"
#include "imgui_impl_ovr.h"
#include "imgui_impl_glfw.h"

// CONSTANTS
const size_t WINDOW_WIDTH = 800;
const size_t WINDOW_HEIGHT = 600;
const size_t GL_VER_MAJOR = 4;
const size_t GL_VER_MINOR = 3;
const float FOV_DEGREES = 60;
const float Z_NEAR = 0.1f;
const float Z_FAR = 100;

// GLOBAL VARIABLES
GLFWwindow* pWindow;
glm::mat4 uiModelMatrix;

// camera matrix, translated along z-axis for zoom back
Camera camera;

// END GLOBAL VARIABLES

// BEGIN GLFW CALLBACKS

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	VR::set_screen(width, height);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
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
	glfwSetKeyCallback(pWindow, key_callback);

	uiModelMatrix = glm::translate(glm::mat4(1), glm::vec3(-1.f, 0, -1.f)) * glm::rotate(glm::mat4(1), glm::radians(30.f), glm::vec3(0, 1, 0));

	return true;
}

void setup_opengl_state()
{
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	glClearColor(0.2f, 0.2f, 0.2f, 1);

	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_ADD);

	glEnable(GL_DEPTH_TEST);

	glUseProgram(0);
}

void init_imgui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	// load a nice big sans-serif font for easy legibility in VR
	io.Fonts->AddFontFromFileTTF("fnt/Roboto-Medium.ttf", 24.f);

	ImGuiStyle* style = &ImGui::GetStyle();
	style->ScrollbarSize = 30.f; // make scrollbar bigger for easier selection
	style->GrabMinSize = 30.f; // make sure grab section doesn't get too thin


	
	ImGui_ImplOvr_Init(VR::vrSession, &VR::frameIndex);

	ImGui_ImplGlfw_InitForOpenGL(pWindow, false);
}

void process_input()
{
	if (glfwGetKey(pWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(pWindow, true);
}

void render()
{
	ImGui_ImplOvr_RenderGUIQuad(VR::currentProjection, VR::currentView, uiModelMatrix);
	ImGui_ImplOvr_RenderControllerLine(VR::currentProjection, VR::currentView);
}

void render_gui()
{
	ImGui::ShowTestWindow();
}

void application_loop()
{
	while (!glfwWindowShouldClose(pWindow))
	{
		process_input();

		// Start the Dear ImGui frame
		ImGui_ImplGlfw_NewFrame();
		ImGui_ImplOvr_NewFrame(uiModelMatrix);
		ImGui::NewFrame();

		render_gui();

		ImGui_ImplOvr_Update();

		// only need to render GUI once, not for each eye
		ImGui::Render();
		ImGui_ImplOvr_RenderDrawData(ImGui::GetDrawData());
		    
		VR::begin_frame();

		for (int eye = 0; eye < 2; eye++)
		{	
			VR::begin_eye(eye);
			
			render();

			VR::end_eye(eye);
		}

		VR::end_frame();
		
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

	if (!VR::init(WINDOW_WIDTH, WINDOW_HEIGHT))
	{
		return -1;
	}
	VR::pCamera = &camera;

	init_imgui();

	camera.pos.z = 0.1f;

	application_loop();

	// Cleanup
	ImGui_ImplOvr_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(pWindow);
	glfwTerminate();
	return 0;
}