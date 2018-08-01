// ImGui Renderer for: Oculus Rift CV1 with OpenGL3+
// This needs to be used along with a Platform Binding (e.g. GLFW, SDL, Win32, custom..)
// (Note: Glad is used as the OpenGL platform binding, but can be replaced with whatever works best for you)

#include "imgui_impl_ovr.h"

#include <imgui.h>
#include <glad/glad.h> // OpenGL bindings

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <LibOVR/OVR_CAPI.h> // Oculus SDK

// TODO: onscreen keyboard solution?

// TODO: set left/right controller

// Handle of the texture used for fonts
static GLuint g_FontTexture = 0;

// Shader program handles for drawing ImGui elements
static GLuint g_ShaderHandle = 0, g_VertHandle = 0, g_FragHandle = 0;

// Shader program handles for drawing GUI render texture on a quad
static GLuint g_QuadShaderHandle = 0, g_QuadVertHandle = 0, g_QuadFragHandle = 0;

static GLuint g_LineShaderHandle = 0, g_LineVertHandle = 0, g_LineFragHandle = 0;

// Handles of GUI render texture and GUI FBO
static GLuint g_GuiTexture = 0, g_GuiFBO = 0;

// Handles of the GUI render quad VAO, VBO, and EBO
static GLuint g_QuadVao = 0, g_QuadVbo = 0, g_QuadEbo = 0;

// Handles of the controller pointer line VAO and VBO
static GLuint g_LineVao = 0, g_LineVbo = 0;

// Set to controller position on mouse position update (ImGui_ImplOvr_UpdateMousePos()).
// Used for drawing controller pointer line.
static glm::vec3 g_LineStart;

// Set to GUI quad intersection point on mouse position update (ImGui_ImplOvr_UpdateMousePos()).
// Used for drawing controller pointer line.
static glm::vec3 g_LineEnd;

// Set to true when the controller pointer is intersecting with the GUI quad. Used to enable/disable
// controller pointer line rendering.
static bool g_MouseOverUI = false;

// RGB color used to set controller pointer line color as shader uniform.
// User-configurable via ImGui_ImplOvr_SetControllerLineColor(glm::vec3 color).
static glm::vec3 g_LineColor = { 1, 0, 0 };

// const string indicating GLSL version to use for shaders
static const std::string g_GlslVersionString = "#version 330 core\n";

// ImGui shader uniform locations
static int g_AttribLocationTex = 0, g_AttribLocationProjMtx = 0;

// ImGui vertex attribute locations
static int g_AttribLocationPosition = 0, g_AttribLocationUV = 0, g_AttribLocationColor = 0;

// GUI render quad uniform locations
static int g_QuadAttribLocationTex = 0, g_QuadAttribLocationProjMtx = 0, g_QuadAttribLocationModelViewMtx = 0;

// Controller pointer line uniform locations
static int g_LineAttribLocationProjMtx = 0, g_LineAttribLocationViewMtx = 0, g_LineAttribLocationLineColor = 0;

// ImGui GUI geometry VBO and EBO handles
static unsigned int g_VboHandle = 0, g_ElementsHandle = 0;

// The size in pixels of the virtual GUI canvas, initialized with an arbitrary default
// value of 800x600, but is user-configurable via ImGui_ImplOvr_SetVirtualCanvasSize(glm::ivec2 size).
static glm::ivec2 g_VirtualCanvasSize = { 1600, 600 };

// The amount of pixels per worldspace unit. This will affect how big the virtual canvas is rendered.
// For example: a PPU value of 1000 will make a 1500x1000 size virtual canvas be 1.5x1.0 worldspace units.
// Default value is 1000 but is user-configurable via ImGui_ImplOvr_SetPixelsPerUnit(float ppu).
static float g_PixelsPerUnit = 1000;

// An array containing the data to fill the GUI render quad vertex buffer with.
// Format is GL_FLOAT, each vertex has Vec3 position and Vec2 UV coords, there
// are 4 vertices total.
static GLfloat g_QuadVertexBuffer[(3 + 2) * 4] = 
{
//	(x,  y,  z)  (u, v)
	-1, -1,  0,   0, 0,
	-1,  1,  0,   0, 1,
	 1,  1,  0,   1, 1,
	 1, -1,  0,   1, 0
};

// An array containing the data to fill the GUI render quad index buffer with.
// Format is GL_UNSIGNED_INT, there are 6 indices here to make up the 2 triangles
// of the quad.
static GLuint g_QuadIndices[6] = { 1, 0, 2, 0, 3, 2 };

// The deadzone of the Oculus Touch thumbsticks. Initialized by default to 0.3f, but is
// user-configurable via ImGui_ImplOvr_SetThumbstickDeadZone(float deadzone).
static float g_ThumbstickDeadzone = 0.3f;

// A buffer to store values for pulsing the held controller when a UI element is hovered over.
// See: https://developer.oculus.com/documentation/pcsdk/latest/concepts/dg-input-touch-haptic/
static ovrHapticsBuffer g_HapticPulseBuffer;

// A pointer to the current frame index of the Oculus HMD. Should start at 0 and increment
// each time a frame is rendered. Value is initialised in ImGui_ImplOvr_Init().
// Required for Oculus API call to get tracking state.
static long long* g_VRFrameIndex;

// The current ovrSession of the Oculus API. Initialised in ImGui_ImplOvr_Init().
static ovrSession g_VRSession;

/**
 * @brief Maps an analog input with a lower and higher value to [0, 1]
 * 
 * v < VL maps to 0, v > VH maps to 1, values inbetween are linearly interpolated.
 * 
 * @param v The analog value
 * @param VL The lower value to map from
 * @param VH The higher value to map from
 * @return Analog input mapped to interval [0, 1]
 */
static float ImGui_ImplOvr_MapAnalogInput(float v, float VL, float VH)
{
	const float interpolated = (v - VL) / (VH - VL);
	return glm::clamp(interpolated, 0.f, 1.f);
}

/**
 * @brief Gets input state from Oculus API and uses it to update ImGui NavInputs for
 * navigation.
 */
static void ImGui_ImplOvr_UpdateOculusTouchButtons()
{
	ovrInputState inputState;
	ovr_GetInputState(g_VRSession, ovrControllerType_Touch, &inputState);

	ImGuiIO& io = ImGui::GetIO();

	io.NavInputs[ImGuiNavInput_DpadDown] = inputState.ThumbstickNoDeadzone[0].y < -g_ThumbstickDeadzone;
	io.NavInputs[ImGuiNavInput_DpadUp] = inputState.ThumbstickNoDeadzone[0].y > g_ThumbstickDeadzone;
	io.NavInputs[ImGuiNavInput_DpadLeft] = inputState.ThumbstickNoDeadzone[0].x < -g_ThumbstickDeadzone;
	io.NavInputs[ImGuiNavInput_DpadRight] = inputState.ThumbstickNoDeadzone[0].x > g_ThumbstickDeadzone;
	io.NavInputs[ImGuiNavInput_Activate] = inputState.IndexTriggerRaw[0] > 0.5f;

	io.NavInputs[ImGuiNavInput_Cancel] = inputState.HandTriggerRaw[0] > 0.5f;
	io.NavInputs[ImGuiNavInput_Input] = inputState.Buttons & ovrButton_LThumb; // RThumb
	io.NavInputs[ImGuiNavInput_Menu] = inputState.Buttons & ovrButton_X; // A

	io.NavInputs[ImGuiNavInput_FocusPrev] = 0; // prev window (w/ PadMenu)
	io.NavInputs[ImGuiNavInput_FocusNext] = inputState.Buttons & ovrButton_Y; // B
	io.NavInputs[ImGuiNavInput_TweakSlow] = 0; // slower tweaks
	io.NavInputs[ImGuiNavInput_TweakFast] = 0; // faster tweaks

	io.MouseDown[0] = inputState.IndexTriggerRaw[0] > 0.5f;

	io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
}

/**
 * @brief Update the ImGui mouse position using Touch controller as pointer
 * 
 * Uses Oculus controller as a pointer and computes ray intersection with
 * GUI quad virtual canvas, then calculates virtual canvas mouse position from
 * this intersection point and submits it to ImGui.
 * 
 * @param guiModelMatrix The model matrix of the GUI quad
 */
static void ImGui_ImplOvr_UpdateMousePos(glm::mat4 guiModelMatrix)
{
	g_MouseOverUI = false;

	// create vertices of GUI virtual canvas quad in local space
	glm::vec3 p0(-1.f, -1.f, 0.f);
	glm::vec3 p1(-1.f, 1.f, 0.f);
	glm::vec3 p2(1.f, 1.f, 0.f);
	glm::vec3 p3(1.f, -1.f, 0.f);

	// apply scale to quad to make pixel size square, as well as model matrix to
	// transform it into world space
	const glm::mat4 modelMat = guiModelMatrix * glm::scale(glm::mat4(1),
		glm::vec3(g_VirtualCanvasSize.x / g_PixelsPerUnit,
			g_VirtualCanvasSize.y / g_PixelsPerUnit, 1.0f));
	p0 = modelMat * glm::vec4(p0, 1);
	p1 = modelMat * glm::vec4(p1, 1);
	p2 = modelMat * glm::vec4(p2, 1);
	p3 = modelMat * glm::vec4(p3, 1);

	// get tracking data from Oculus API and convert to GLM values
	const double displayMidpointSeconds = ovr_GetPredictedDisplayTime(g_VRSession, *g_VRFrameIndex);
	const ovrTrackingState trackState = ovr_GetTrackingState(g_VRSession, displayMidpointSeconds, ovrTrue);
	const ovrPosef handPose = trackState.HandPoses[0].ThePose;
	const glm::vec3 handPosition = glm::vec3(handPose.Position.x, handPose.Position.y, handPose.Position.z);
	const glm::quat handOrientation = glm::quat(handPose.Orientation.w, handPose.Orientation.x, handPose.Orientation.y, handPose.Orientation.z);
	const glm::vec3 handForward = handOrientation * glm::vec3(0, 0, -1);

	// find the inverse of the model matrix to get world-to-local matrix
	const glm::mat4 toLocal = glm::inverse(modelMat);
	const glm::vec3 start = glm::vec4(handPosition, 1);

	g_LineStart = handPosition;

	// now raycast from Touch controller in forward direction, looking for an
	// intersection with the GUI virtual canvas quad
	glm::vec2 b;
	bool intersect;
	float dist;
	static glm::vec2 mousePosLastFrame;
	ImGuiIO& io = ImGui::GetIO();
	do
	{
		// check for intersection with first triangle of quad
		intersect = glm::intersectRayTriangle(start, handForward, p1, p0, p2, b, dist);
		if (intersect) break;

		// check for intersection with other triangle of quad
		intersect = glm::intersectRayTriangle(start, handForward, p0, p3, p2, b, dist);
		if (intersect) break;
	}
	while (false);
	
	if (intersect)
	{
		// we found an intersection, the 'mouse' is over the UI
		g_MouseOverUI = true;

		// compute intersection point in worldspace
		const glm::vec3 pt = start + handForward * dist;
		g_LineEnd = pt;

		// convert intersection point to local space
		const glm::vec3 localPt = toLocal * glm::vec4(pt, 1);

		// linearly interpolate between virtual canvas sizes based on UI quad local intersection position
		// to get mouse position on canvas
		const glm::vec2 mousePos = { 
			(g_VirtualCanvasSize.x / 2.f) * localPt.x + (g_VirtualCanvasSize.x / 2.f),  // xPos = (width / 2) * localX + (width / 2)
			(g_VirtualCanvasSize.y / 2.f) - (g_VirtualCanvasSize.y / 2.f) * localPt.y   // yPos = (-height / 2) * localY + (height / 2)
		};

		io.MousePos = ImVec2(mousePos.x, mousePos.y);

		// cache mouse position in case intersection stops next frame
		mousePosLastFrame = mousePos;
	}
	else
	{
		// if no intersection, set the mouse position to the last known position
		// otherwise mouse may 'jump' to an unexpected position
		io.MousePos = ImVec2(mousePosLastFrame.x, mousePosLastFrame.y);
	}
}

/**
 * @brief If a UI element is hovered, trigger a haptic pulse.
 * 
 * @see https://github.com/temcgraw/ImguiVR/blob/master/imgui_impl_vr.cpp#L715-L729
 */
static void PulseIfItemHovered()
{
	static bool itemHoveredLastFrame = false;

	const bool itemHoveredThisFrame = ImGui::IsAnyItemHovered();

	if (itemHoveredLastFrame != itemHoveredThisFrame)
	{
		ovr_SubmitControllerVibration(g_VRSession, ovrControllerType_LTouch, &g_HapticPulseBuffer);
	}
	itemHoveredLastFrame = itemHoveredThisFrame;
}

/**
 * @brief Check to see if a shader has been compiled successfully.
 * 
 * Prints error message to stderr if unsuccessful.
 * 
 * @param handle The handle of the shader to check
 * @param desc A short description of what the shader is to put in the error message
 * @return True if success, false otherwise
 */
static bool CheckShader(GLuint handle, const char* desc)
{
	GLint status = 0, log_length = 0;
	glGetShaderiv(handle, GL_COMPILE_STATUS, &status);
	glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &log_length);
	if (status == GL_FALSE)
		fprintf(stderr, "ERROR: ImGui_ImplOvr_CreateDeviceObjects: failed to compile %s!\n", desc);
	if (log_length > 0)
	{
		ImVector<char> buf;
		buf.resize((int)(log_length + 1));
		glGetShaderInfoLog(handle, log_length, NULL, (GLchar*)buf.begin());
		fprintf(stderr, "%s\n", buf.begin());
	}
	return status == GL_TRUE;
}

/**
 * @brief Check to see if a shader program has been linked successfully.
 * 
 * Prints error message to stderr if unsuccessful.
 * 
 * @param handle The handle of the program to check
 * @param desc A short description of what the program is to put in the error message
 * @return True if success, false otherwise
 */
static bool CheckProgram(GLuint handle, const char* desc)
{
	GLint status = 0, log_length = 0;
	glGetProgramiv(handle, GL_LINK_STATUS, &status);
	glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &log_length);
	if (status == GL_FALSE)
		fprintf(stderr, "ERROR: ImGui_ImplOvr_CreateDeviceObjects: failed to link %s!\n", desc);
	if (log_length > 0)
	{
		ImVector<char> buf;
		buf.resize((int)(log_length + 1));
		glGetProgramInfoLog(handle, log_length, NULL, (GLchar*)buf.begin());
		fprintf(stderr, "%s\n", buf.begin());
	}
	return status == GL_TRUE;
}

/**
 * @brief Initialise ImGui Oculus VR renderer. Must be called before any other ImGui_ImplOvr function.
 * 
 * @param session The ovrSession object from when you initialised the Oculus API.
 * @param frameIndex A pointer to where you're storing the current VR frame index.
 * @return True if successful initialisation, false otherwise.
 */
bool ImGui_ImplOvr_Init(ovrSession session, long long* const frameIndex)
{
	if (!frameIndex) return false;
	g_VRFrameIndex = frameIndex;
	g_VRSession = session;

	// create haptic pulse buffer
	g_HapticPulseBuffer.Samples = new unsigned char[7]{ 0, 255, 0, 255, 0, 255, 0 };
	g_HapticPulseBuffer.SamplesCount = 7;
	g_HapticPulseBuffer.SubmitMode = ovrHapticsBufferSubmit_Enqueue;
	
	ImGuiIO& io = ImGui::GetIO();

	// HACK: create device objects here otherwise assertion that font is already
	// built will fail in ImGui platform binding Init (i.e. ImGui_Impl_Glfw_Init())
	ImGui_ImplOvr_CreateDeviceObjects();

	return true;
}

/**
 * @brief Shutdown the ImGui Oculus VR renderer. Call this when your program exits, cleans up
 * OpenGL objects and deletes allocated memory.
 */
void ImGui_ImplOvr_Shutdown()
{
	ImGui_ImplOvr_DestroyDeviceObjects();
	delete[] static_cast<unsigned char const*>(g_HapticPulseBuffer.Samples);
}

/**
 * @brief Begin a new frame with this renderer. Call this before you begin drawing
 * ImGui elements, and before ImGui::NewFrame().
 * 
 * @param guiModelMatrix The model matrix of the quad to draw the GUI on in the scene. Used for
 * intersection calculations to determine mouse position.
 */
void ImGui_ImplOvr_NewFrame(glm::mat4 guiModelMatrix)
{
	ImGuiIO& io = ImGui::GetIO();

	io.DisplaySize = ImVec2(g_VirtualCanvasSize.x, g_VirtualCanvasSize.y);
	io.DisplayFramebufferScale = ImVec2(1, 1); // not used for anything

	if (!g_FontTexture)
	{
		ImGui_ImplOvr_CreateDeviceObjects();
	}

	// update mouse and gamepad
	ImGui_ImplOvr_UpdateMousePos(guiModelMatrix);
	ImGui_ImplOvr_UpdateOculusTouchButtons();
}

/**
 * @brief Call this every engine tick, will update internal state and do things
 * like pulse the Touch controller if a UI element is hovered over.
 */
void ImGui_ImplOvr_Update()
{
	PulseIfItemHovered();
}

/**
 * @brief Set the size of the virtual GUI canvas in pixels. Call this before ImGui_ImplOvr_Init() if you
 * want canvas size changes to have any effect.
 * 
 * @param size The size in pixels of the virtual canvas.
 */
void ImGui_ImplOvr_SetVirtualCanvasSize(glm::ivec2 size)
{
	g_VirtualCanvasSize = size;
}

/**
 * @brief Set the deadzone of the Oculus Touch thumbstick.
 * 
 * @param deadzone The deadzone.
 */
void ImGui_ImplOvr_SetThumbstickDeadzone(float deadzone)
{
	g_ThumbstickDeadzone = deadzone;
}

/**
 * @brief Set the color of the line drawn from the Touch controller (indicating 'mouse' position on UI).
 * 
 * @param color RGB vector color to set
 */
void ImGui_ImplOvr_SetControllerLineColor(glm::vec3 color)
{
	g_LineColor = color;
}

/**
 * @brief Set the pixels-per-unit (ppu) scale of the virtual canvas. Affects the rendered size of the
 * GUI virtual canvas quad. Denotes how many pixels occupy each world-space unit.
 * 
 * For example: a PPU of 1000 with a virtual canvas size of 1500x500px would make a quad size of
 * 1.5x0.5 units.
 * 
 * @param ppu The pixels-per-unit scale to set
 */
void ImGui_ImplOvr_SetPixelsPerUnit(float ppu)
{
	g_PixelsPerUnit = ppu;
}

/**
 * @brief Creates a texture for the currently in-use ImGui font.
 * 
 * @return True
 */
bool ImGui_ImplOvr_CreateFontsTexture()
{
	// Build texture atlas
	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);   // Load as RGBA 32-bits (75% of the memory is wasted, but default font is so small) because it is more likely to be compatible with user's existing shaders. If your ImTextureId represent a higher-level concept than just a GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU memory.

															  // Upload texture to graphics system
	GLint last_texture;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
	glGenTextures(1, &g_FontTexture);
	glBindTexture(GL_TEXTURE_2D, g_FontTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	// Store our identifier
	io.Fonts->TexID = (void *)(intptr_t)g_FontTexture;

	// Restore state
	glBindTexture(GL_TEXTURE_2D, last_texture);

	return true;
}

/**
 * @brief Destroys the created ImGui font texture.
 */
void ImGui_ImplOvr_DestroyFontsTexture()
{
	if (g_FontTexture)
	{
		ImGuiIO& io = ImGui::GetIO();
		glDeleteTextures(1, &g_FontTexture);
		io.Fonts->TexID = 0;
		g_FontTexture = 0;
	}
}

/**
 * @brief Creates all necessary OpenGL objects for rendering the GUI in VR.
 * 
 * @return True
 */
bool ImGui_ImplOvr_CreateDeviceObjects()
{
	// Backup GL state
	GLint last_texture, last_array_buffer, last_vertex_array;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);

	const GLchar* vertex_shader =
		"uniform mat4 ProjMtx;\n"
		"in vec2 Position;\n"
		"in vec2 UV;\n"
		"in vec4 Color;\n"
		"out vec2 Frag_UV;\n"
		"out vec4 Frag_Color;\n"
		"void main()\n"
		"{\n"
		"    Frag_UV = UV;\n"
		"    Frag_Color = Color;\n"
		"    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
		"}\n";

	const GLchar* fragment_shader =
		"uniform sampler2D Texture;\n"
		"in vec2 Frag_UV;\n"
		"in vec4 Frag_Color;\n"
		"out vec4 Out_Color;\n"
		"void main()\n"
		"{\n"
		"    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
		"}\n";

	const GLchar* quad_vert_shader =
		"uniform mat4 ModelViewMtx;\n"
		"uniform mat4 ProjMtx;\n"
		"in vec3 Position;\n"
		"in vec2 UV;\n"
		"out vec2 Frag_UV;\n"
		"void main()\n"
		"{\n"
		"    Frag_UV = UV;\n"
		"    gl_Position = ProjMtx * ModelViewMtx * vec4(Position.xyz, 1.0);\n"
		"}\n";

	const GLchar* quad_frag_shader =
		"uniform sampler2D Texture;\n"
		"in vec2 Frag_UV;\n"
		"out vec4 Out_Color;\n"
		"void main()\n"
		"{\n"
		"    Out_Color = texture(Texture, Frag_UV);\n"
		"}\n";

	const GLchar* line_vert_shader =
		"uniform mat4 ProjMtx;\n"
		"uniform mat4 ViewMtx;\n"
		"in vec3 Position;\n"
		"void main()\n"
		"{\n"
		"    gl_Position = ProjMtx * ViewMtx * vec4(Position.xyz, 1.0);\n"
		"}\n";

	const GLchar* line_frag_shader =
		"uniform vec3 LineColor;\n"
		"out vec4 Out_Color;\n"
		"void main()\n"
		"{\n"
		"    Out_Color = vec4(LineColor.rgb, 1.0);\n"
		"}\n";

	// Create shaders for GUI
	const GLchar* vertex_shader_with_version[2] = { g_GlslVersionString.c_str(), vertex_shader };
	g_VertHandle = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(g_VertHandle, 2, vertex_shader_with_version, nullptr);
	glCompileShader(g_VertHandle);
	CheckShader(g_VertHandle, "vertex shader");

	const GLchar* fragment_shader_with_version[2] = { g_GlslVersionString.c_str(), fragment_shader };
	g_FragHandle = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(g_FragHandle, 2, fragment_shader_with_version, nullptr);
	glCompileShader(g_FragHandle);
	CheckShader(g_FragHandle, "fragment shader");

	g_ShaderHandle = glCreateProgram();
	glAttachShader(g_ShaderHandle, g_VertHandle);
	glAttachShader(g_ShaderHandle, g_FragHandle);
	glLinkProgram(g_ShaderHandle);
	CheckProgram(g_ShaderHandle, "shader program");

	g_AttribLocationTex = glGetUniformLocation(g_ShaderHandle, "Texture");
	g_AttribLocationProjMtx = glGetUniformLocation(g_ShaderHandle, "ProjMtx");
	g_AttribLocationPosition = glGetAttribLocation(g_ShaderHandle, "Position");
	g_AttribLocationUV = glGetAttribLocation(g_ShaderHandle, "UV");
	g_AttribLocationColor = glGetAttribLocation(g_ShaderHandle, "Color");

	// create shaders for quad
	const GLchar* quad_vertex_shader_with_version[2] = { g_GlslVersionString.c_str(), quad_vert_shader };
	g_QuadVertHandle = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(g_QuadVertHandle, 2, quad_vertex_shader_with_version, nullptr);
	glCompileShader(g_QuadVertHandle);
	CheckShader(g_QuadVertHandle, "quad vertex shader");

	const GLchar* quad_fragment_shader_with_version[2] = { g_GlslVersionString.c_str(), quad_frag_shader };
	g_QuadFragHandle = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(g_QuadFragHandle, 2, quad_fragment_shader_with_version, nullptr);
	glCompileShader(g_QuadFragHandle);
	CheckShader(g_QuadFragHandle, "quad fragment shader");

	g_QuadShaderHandle = glCreateProgram();
	glAttachShader(g_QuadShaderHandle, g_QuadVertHandle);
	glAttachShader(g_QuadShaderHandle, g_QuadFragHandle);
	glLinkProgram(g_QuadShaderHandle);
	CheckProgram(g_QuadShaderHandle, "quad shader program");

	g_QuadAttribLocationTex = glGetUniformLocation(g_QuadShaderHandle, "Texture");
	g_QuadAttribLocationProjMtx = glGetUniformLocation(g_QuadShaderHandle, "ProjMtx");
	g_QuadAttribLocationModelViewMtx = glGetUniformLocation(g_QuadShaderHandle, "ModelViewMtx");

	// create shaders for line
	const GLchar* line_vertex_shader_with_version[2] = { g_GlslVersionString.c_str(), line_vert_shader };
	g_LineVertHandle = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(g_LineVertHandle, 2, line_vertex_shader_with_version, nullptr);
	glCompileShader(g_LineVertHandle);
	CheckShader(g_LineVertHandle, "line vertex shader");

	const GLchar* line_fragment_shader_with_version[2] = { g_GlslVersionString.c_str(), line_frag_shader };
	g_LineFragHandle = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(g_LineFragHandle, 2, line_fragment_shader_with_version, nullptr);
	glCompileShader(g_LineFragHandle);
	CheckShader(g_LineFragHandle, "line fragment shader");

	g_LineShaderHandle = glCreateProgram();
	glAttachShader(g_LineShaderHandle, g_LineVertHandle);
	glAttachShader(g_LineShaderHandle, g_LineFragHandle);
	glLinkProgram(g_LineShaderHandle);
	CheckProgram(g_LineShaderHandle, "line shader program");

	g_LineAttribLocationProjMtx = glGetUniformLocation(g_LineShaderHandle, "ProjMtx");
	g_LineAttribLocationViewMtx = glGetUniformLocation(g_LineShaderHandle, "ViewMtx");
	g_LineAttribLocationLineColor = glGetUniformLocation(g_LineShaderHandle, "LineColor");

	// create vao for quad
	glGenVertexArrays(1, &g_QuadVao);
	glBindVertexArray(g_QuadVao);
	glGenBuffers(1, &g_QuadVbo);
	glGenBuffers(1, &g_QuadEbo);
	glBindBuffer(GL_ARRAY_BUFFER, g_QuadVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * (3 + 2) * 4, g_QuadVertexBuffer, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_QuadEbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * 6, g_QuadIndices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * (3 + 2), nullptr);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * (3 + 2), reinterpret_cast<GLvoid*>(sizeof(GLfloat) * 3));
	glBindVertexArray(0);

	// Create buffers
	glGenBuffers(1, &g_VboHandle);
	glGenBuffers(1, &g_ElementsHandle);
	glGenBuffers(1, &g_LineVbo);

	ImGui_ImplOvr_CreateFontsTexture();

	// create GUI render texture
	glGenTextures(1, &g_GuiTexture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_GuiTexture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, g_VirtualCanvasSize.x, g_VirtualCanvasSize.y);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// create GUI fbo
	glGenFramebuffers(1, &g_GuiFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, g_GuiFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, g_GuiTexture, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Restore modified GL state
	glBindTexture(GL_TEXTURE_2D, last_texture);
	glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
	glBindVertexArray(last_vertex_array);

	return true;
}

/**
 * @brief Destroys OpenGL state created on Initialise.
 */
void ImGui_ImplOvr_DestroyDeviceObjects()
{
	if (g_VboHandle) glDeleteBuffers(1, &g_VboHandle);
	if (g_ElementsHandle) glDeleteBuffers(1, &g_ElementsHandle);
	g_VboHandle = g_ElementsHandle = 0;

	if (g_LineVbo) glDeleteBuffers(1, &g_LineVbo);
	g_LineVbo = 0;

	if (g_ShaderHandle && g_VertHandle) glDetachShader(g_ShaderHandle, g_VertHandle);
	if (g_VertHandle) glDeleteShader(g_VertHandle);
	g_VertHandle = 0;

	if (g_ShaderHandle && g_FragHandle) glDetachShader(g_ShaderHandle, g_FragHandle);
	if (g_FragHandle) glDeleteShader(g_FragHandle);
	g_FragHandle = 0;

	if (g_ShaderHandle) glDeleteProgram(g_ShaderHandle);
	g_ShaderHandle = 0;

	if (g_QuadShaderHandle && g_QuadVertHandle) glDetachShader(g_QuadShaderHandle, g_QuadVertHandle);
	if (g_QuadVertHandle) glDeleteShader(g_QuadVertHandle);
	g_QuadVertHandle = 0;

	if (g_QuadShaderHandle && g_QuadFragHandle) glDetachShader(g_QuadShaderHandle, g_QuadFragHandle);
	if (g_QuadFragHandle) glDeleteShader(g_QuadFragHandle);
	g_QuadVertHandle = 0;

	if (g_QuadShaderHandle) glDeleteProgram(g_QuadShaderHandle);
	g_QuadShaderHandle = 0;

	if (g_LineShaderHandle && g_LineVertHandle) glDetachShader(g_LineShaderHandle, g_LineVertHandle);
	if (g_LineVertHandle) glDeleteShader(g_LineVertHandle);
	g_LineVertHandle = 0;

	if (g_LineShaderHandle && g_LineFragHandle) glDetachShader(g_LineShaderHandle, g_LineFragHandle);
	if (g_LineFragHandle) glDeleteShader(g_LineFragHandle);
	g_LineFragHandle = 0;

	if (g_LineShaderHandle) glDeleteProgram(g_LineShaderHandle);
	g_LineShaderHandle = 0;

	if (g_GuiTexture) glDeleteTextures(1, &g_GuiTexture);
	g_GuiTexture = 0;

	if (g_GuiFBO) glDeleteFramebuffers(1, &g_GuiFBO);
	g_GuiFBO = 0;

	if (g_QuadVao) glDeleteVertexArrays(1, &g_QuadVao);
	g_QuadVao = 0;

	if (g_QuadVbo) glDeleteBuffers(1, &g_QuadVbo);
	g_QuadVbo = 0;

	if (g_QuadEbo) glDeleteBuffers(1, &g_QuadEbo);
	g_QuadEbo = 0;

	ImGui_ImplOvr_DestroyFontsTexture();
}

/**
 * @brief Renders ImGui draw data on the virtual canvas.
 * Call this after ImGui::Render() when you want your GUI to be rendered.
 * 
 * @param draw_data The ImGui draw data to render.
 */
void ImGui_ImplOvr_RenderDrawData(ImDrawData * draw_data)
{
	// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
	ImGuiIO& io = ImGui::GetIO();
	int fb_width = (int)(draw_data->DisplaySize.x * io.DisplayFramebufferScale.x);
	int fb_height = (int)(draw_data->DisplaySize.y * io.DisplayFramebufferScale.y);
	if (fb_width <= 0 || fb_height <= 0)
		return;
	draw_data->ScaleClipRects(io.DisplayFramebufferScale);

	// Backup GL state
	GLenum last_active_texture; glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint*)&last_active_texture);
	glActiveTexture(GL_TEXTURE0);
	GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
	GLint last_texture; glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
	GLint last_sampler; glGetIntegerv(GL_SAMPLER_BINDING, &last_sampler);
	GLint last_array_buffer; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
	GLint last_vertex_array; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
	GLint last_polygon_mode[2]; glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
	GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
	GLint last_scissor_box[4]; glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
	GLint last_framebuffer; glGetIntegerv(GL_FRAMEBUFFER_BINDING, &last_framebuffer);
	GLenum last_blend_src_rgb; glGetIntegerv(GL_BLEND_SRC_RGB, (GLint*)&last_blend_src_rgb);
	GLenum last_blend_dst_rgb; glGetIntegerv(GL_BLEND_DST_RGB, (GLint*)&last_blend_dst_rgb);
	GLenum last_blend_src_alpha; glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint*)&last_blend_src_alpha);
	GLenum last_blend_dst_alpha; glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint*)&last_blend_dst_alpha);
	GLenum last_blend_equation_rgb; glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint*)&last_blend_equation_rgb);
	GLenum last_blend_equation_alpha; glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint*)&last_blend_equation_alpha);
	GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
	GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
	GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
	GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);

	// Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, polygon fill
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_SCISSOR_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glBindFramebuffer(GL_FRAMEBUFFER, g_GuiFBO);

	glScissor(0, 0, draw_data->DisplaySize.x, draw_data->DisplaySize.y);

	// Setup viewport, orthographic projection matrix
	// Our visible imgui space lies from draw_data->DisplayPps (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayMin is typically (0,0) for single viewport apps.
	glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
	float L = draw_data->DisplayPos.x;
	float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
	float T = draw_data->DisplayPos.y;
	float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
	const float ortho_projection[4][4] =
	{
		{ 2.0f / (R - L),   0.0f,         0.0f,   0.0f },
		{ 0.0f,         2.0f / (T - B),   0.0f,   0.0f },
		{ 0.0f,         0.0f,        -1.0f,   0.0f },
		{ (R + L) / (L - R),  (T + B) / (B - T),  0.0f,   1.0f },
	};

	glUseProgram(g_ShaderHandle);
	glUniform1i(g_AttribLocationTex, 0);
	glUniformMatrix4fv(g_AttribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);
	if (glBindSampler) glBindSampler(0, 0); // We use combined texture/sampler state. Applications using GL 3.3 may set that otherwise.

											// Recreate the VAO every time 
											// (This is to easily allow multiple GL contexts. VAO are not shared among GL contexts, and we don't track creation/deletion of windows so we don't have an obvious key to use to cache them.)
	GLuint vao_handle = 0;
	glGenVertexArrays(1, &vao_handle);
	glBindVertexArray(vao_handle);
	glBindBuffer(GL_ARRAY_BUFFER, g_VboHandle);
	glEnableVertexAttribArray(g_AttribLocationPosition);
	glEnableVertexAttribArray(g_AttribLocationUV);
	glEnableVertexAttribArray(g_AttribLocationColor);
	glVertexAttribPointer(g_AttribLocationPosition, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, pos));
	glVertexAttribPointer(g_AttribLocationUV, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, uv));
	glVertexAttribPointer(g_AttribLocationColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, col));

	glClear(GL_COLOR_BUFFER_BIT);

	// Draw
	ImVec2 pos = draw_data->DisplayPos;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		const ImDrawIdx* idx_buffer_offset = 0;

		glBindBuffer(GL_ARRAY_BUFFER, g_VboHandle);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), (const GLvoid*)cmd_list->VtxBuffer.Data, GL_STREAM_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ElementsHandle);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), (const GLvoid*)cmd_list->IdxBuffer.Data, GL_STREAM_DRAW);

		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback)
			{
				// User callback (registered via ImDrawList::AddCallback)
				pcmd->UserCallback(cmd_list, pcmd);
			}
			else
			{
				ImVec4 clip_rect = ImVec4(pcmd->ClipRect.x - pos.x, pcmd->ClipRect.y - pos.y, pcmd->ClipRect.z - pos.x, pcmd->ClipRect.w - pos.y);
				if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f)
				{
					// Apply scissor/clipping rectangle
					glScissor((int)clip_rect.x, (int)(fb_height - clip_rect.w), (int)(clip_rect.z - clip_rect.x), (int)(clip_rect.w - clip_rect.y));

					// Bind texture, Draw
					glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
					glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer_offset);
				}
			}
			idx_buffer_offset += pcmd->ElemCount;
		}
	}
	glDeleteVertexArrays(1, &vao_handle);

	// Restore modified GL state
	glUseProgram(last_program);
	glBindTexture(GL_TEXTURE_2D, last_texture);
	if (glBindSampler) glBindSampler(0, last_sampler);
	glActiveTexture(last_active_texture);
	glBindVertexArray(last_vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
	glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
	glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha);
	if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
	if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
	if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, (GLenum)last_polygon_mode[0]);
	glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
	glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei)last_scissor_box[2], (GLsizei)last_scissor_box[3]);
	glBindFramebuffer(GL_FRAMEBUFFER, last_framebuffer);
}

/**
 * @brief Renders the GUI virtual canvas quad. Call this when you're rendering your VR scene
 * and make sure that it gets rendered as any other geometry would in VR (i.e. by both eyes).
 * 
 * @param proj The projection matrix
 * @param view The view matrix
 * @param model The model matrix of the GUI quad. Use this to move it around to wherever you want it.
 */
void ImGui_ImplOvr_RenderGUIQuad(glm::mat4 proj, glm::mat4 view, glm::mat4 model)
{
	// Backup GL state
	GLenum last_active_texture; glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint*)&last_active_texture);
	glActiveTexture(GL_TEXTURE0);
	GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
	GLint last_texture; glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
	GLint last_array_buffer; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
	GLint last_vertex_array; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
	GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
	GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
	GLint last_polygon_mode[2]; glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
	GLenum last_blend_src_rgb; glGetIntegerv(GL_BLEND_SRC_RGB, (GLint*)&last_blend_src_rgb);
	GLenum last_blend_dst_rgb; glGetIntegerv(GL_BLEND_DST_RGB, (GLint*)&last_blend_dst_rgb);
	GLenum last_blend_src_alpha; glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint*)&last_blend_src_alpha);
	GLenum last_blend_dst_alpha; glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint*)&last_blend_dst_alpha);
	GLenum last_blend_equation_rgb; glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint*)&last_blend_equation_rgb);
	GLenum last_blend_equation_alpha; glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint*)&last_blend_equation_alpha);

	// Setup render state: alpha-blending enabled, no face culling, polygon fill
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glm::mat4 modelView = view * model * 
		glm::scale(glm::mat4(1), 
			glm::vec3(g_VirtualCanvasSize.x / g_PixelsPerUnit, 
				g_VirtualCanvasSize.y / g_PixelsPerUnit, 1.0f));

	glUseProgram(g_QuadShaderHandle);
	glBindTexture(GL_TEXTURE_2D, g_GuiTexture);
	glUniform1i(g_QuadAttribLocationTex, 0);
	glUniformMatrix4fv(g_QuadAttribLocationProjMtx, 1, GL_FALSE, glm::value_ptr(proj));
	glUniformMatrix4fv(g_QuadAttribLocationModelViewMtx, 1, GL_FALSE, glm::value_ptr(modelView));
	glBindVertexArray(g_QuadVao);
	glDrawElements(GL_TRIANGLES, sizeof(g_QuadIndices) / sizeof(*g_QuadIndices), GL_UNSIGNED_INT, nullptr);

	// Restore modified GL state
	glUseProgram(last_program);
	glBindTexture(GL_TEXTURE_2D, last_texture);
	glActiveTexture(last_active_texture);
	glBindVertexArray(last_vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
	if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
	if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, (GLenum)last_polygon_mode[0]);
	glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
	glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha);
}

/**
 * @brief Render the line from the Touch controller along its forward axis. Used for
 * aiming the 'mouse' pointer to select elements on the virtual canvas.
 * Like ImGui_ImplOvr_RenderGUIQuad() this should be rendered when you render your
 * VR scene.
 * 
 * @note Will only be rendered if the controller is pointed at the virtual canvas.
 * 
 * @param proj The projection matrix
 * @param view The view matrix
 */
void ImGui_ImplOvr_RenderControllerLine(glm::mat4 proj, glm::mat4 view)
{
	if (!g_MouseOverUI) return;
	
	// backup GL state
	GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
	GLint last_array_buffer; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
	GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);

	GLfloat lineVerts[6] = { g_LineStart.x, g_LineStart.y, g_LineStart.z, g_LineEnd.x, g_LineEnd.y, g_LineEnd.z };

	glDisable(GL_DEPTH_TEST);

	glGenVertexArrays(1, &g_LineVao);
	glBindVertexArray(g_LineVao);
	glBindBuffer(GL_ARRAY_BUFFER, g_LineVbo);
	glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(GLfloat), lineVerts, GL_STREAM_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, nullptr);
	glUseProgram(g_LineShaderHandle);
	glUniformMatrix4fv(g_LineAttribLocationProjMtx, 1, GL_FALSE, glm::value_ptr(proj));
	glUniformMatrix4fv(g_LineAttribLocationViewMtx, 1, GL_FALSE, glm::value_ptr(view));
	glUniform3fv(g_LineAttribLocationLineColor, 1, glm::value_ptr(g_LineColor));

	glDrawArrays(GL_LINES, 0, 2);
	glDisableVertexAttribArray(0);

	glDeleteVertexArrays(1, &g_LineVao);
	g_LineVao = 0;

	// restore modified GL state
	glUseProgram(last_program);
	glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
	if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
}
