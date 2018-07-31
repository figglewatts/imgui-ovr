#pragma once
#include <LibOVR/OVR_CAPI_GL.h>
#include <glm/glm.hpp>
#include "GL.h"
#include "Camera.h"
#include "TextureBuffer.h"
#include "VAO.h"
#include "Shader.h"

#define OVR_VALIDATE(x, msg) if (!(x)) { std::cerr << msg << std::endl; }

class VR
{
public:
	static ovrSession vrSession;
	static ovrGraphicsLuid hmdLuid;
	static ovrHmdDesc hmdDesc;
	static TextureBuffer *textureSwapchains[2];
	static DepthBuffer *textureDepthBuffers[2];
	static ovrSizei textureSizes[2];
	static ovrMirrorTexture mirrorTexture;
	static ovrSizei bufferSize;
	static GLuint mirrorFBO;
	static GLuint mirrorTextureHandle;
	static ovrEyeRenderDesc eyeRenderDescs[2];
	static ovrLayerEyeFov layer;
	static Camera* pCamera;
	static glm::mat4 currentProjection;
	static glm::mat4 currentView;
	static ovrSizei windowSize;
	static long long frameIndex;

	// VAO and shader used for drawing mirror texture quad on screen
	static VAO *pMirrorQuadVao;
	static Shader *pMirrorShader;

	static bool init(size_t window_width, size_t window_height);

	static void begin_frame();
	static void end_frame();
	static void begin_eye(int eye);
	static void end_eye(int eye);
	static void set_screen(size_t width, size_t height);
};
