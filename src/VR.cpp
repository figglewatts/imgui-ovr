#include "VR.h"
#include <algorithm>
#include <iostream>
#include <LibOVR/Extras/OVR_Math.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

ovrSession VR::vrSession;
ovrGraphicsLuid VR::hmdLuid;
ovrHmdDesc VR::hmdDesc;
TextureBuffer *VR::textureSwapchains[2] = {};
DepthBuffer *VR::textureDepthBuffers[2] = {};
ovrSizei VR::textureSizes[2] = {};
ovrMirrorTexture VR::mirrorTexture;
ovrSizei VR::bufferSize;
GLuint VR::mirrorFBO;
GLuint VR::mirrorTextureHandle;
ovrEyeRenderDesc VR::eyeRenderDescs[2] = {};
ovrLayerEyeFov VR::layer;
Camera *VR::pCamera;
glm::mat4 VR::currentProjection;
glm::mat4 VR::currentView;
ovrSizei VR::windowSize;

VAO *VR::pMirrorQuadVao;
Shader *VR::pMirrorShader;

void VR::begin_frame()
{
	ovrPosef ViewOffset[2] =
	{
		eyeRenderDescs[0].HmdToEyePose,
		eyeRenderDescs[1].HmdToEyePose
	};

	layer.Header.Type = ovrLayerType_EyeFov;
	layer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;
	layer.ColorTexture[0] = textureSwapchains[0]->textureChain;
	layer.ColorTexture[1] = textureSwapchains[1]->textureChain;
	layer.Fov[0] = eyeRenderDescs[0].Fov;
	layer.Fov[1] = eyeRenderDescs[1].Fov;
	layer.Viewport[0] = OVR::Recti(textureSwapchains[0]->GetSize());
	layer.Viewport[1] = OVR::Recti(textureSwapchains[1]->GetSize());

	// Get both eye poses simultaneously, with IPD offset already included.
	const double displayMidpointSeconds = ovr_GetPredictedDisplayTime(vrSession, 0);
	const ovrTrackingState hmdState = ovr_GetTrackingState(vrSession, displayMidpointSeconds, ovrTrue);
	ovr_CalcEyePoses(hmdState.HeadPose.ThePose, ViewOffset, layer.RenderPose);

	ovrResult result = ovr_WaitToBeginFrame(vrSession, 0);
	result = ovr_BeginFrame(vrSession, 0);
}

void VR::end_frame()
{
	ovrLayerHeader *layers = &layer.Header;
	ovrResult result = ovr_EndFrame(vrSession, 0, nullptr, &layers, 1);
	OVR_VALIDATE(OVR_SUCCESS(result), "Failed to submit frame to HMD");

	ovrSessionStatus sessionStatus;
	ovr_GetSessionStatus(vrSession, &sessionStatus);
	if (sessionStatus.ShouldQuit)
		exit(-1);
	if (sessionStatus.ShouldRecenter)
		ovr_RecenterTrackingOrigin(vrSession);

	// bind framebuffers to draw mirror texture
	glViewport(0, 0, windowSize.w, windowSize.h);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, mirrorFBO);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

	// bind mirror texture to TEXTURE0
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mirrorTextureHandle);

	// disable depth testing so we can render a full screen quad
	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);

	// render the quad
	pMirrorShader->bind();
	pMirrorShader->setUniform("ScreenSize", glm::ivec2(windowSize.w, windowSize.h));
	pMirrorShader->setUniform("MirrorBuffer", 0);
	pMirrorQuadVao->render();
	pMirrorShader->unbind();

	// reset OpenGL state
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
}

void VR::begin_eye(int eye)
{
	textureSwapchains[eye]->SetAndClearRenderSurface(textureDepthBuffers[eye]);
	
	ovrVector3f ovrEyePos = layer.RenderPose[eye].Position;
	glm::vec3 eyePos = glm::vec3(ovrEyePos.x, ovrEyePos.y, ovrEyePos.z);
	ovrQuatf ovrEyeRot = layer.RenderPose[eye].Orientation;
	glm::quat eyeRot = glm::quat(ovrEyeRot.w, ovrEyeRot.x, ovrEyeRot.y, ovrEyeRot.z);

	glm::vec3 pos = pCamera->pos + pCamera->rot * eyePos;
	glm::quat orient = eyeRot * pCamera->rot;

	glm::vec3 up = orient * glm::vec4(0, 1, 0, 0);
	glm::vec3 forward = orient * glm::vec4(0, 0, -1, 0);
	glm::mat4 view = glm::lookAt(pos, pos + forward, up);
	currentView = view;

	ovrMatrix4f ovrProj = ovrMatrix4f_Projection(layer.Fov[eye], 0.1f, 100.f, 0);
	glm::mat4 proj = glm::make_mat4((float*)&ovrProj.M);
	currentProjection = glm::transpose(proj);
}

void VR::end_eye(int eye)
{
	textureSwapchains[eye]->UnsetRenderSurface();
	textureSwapchains[eye]->Commit();
}

void VR::set_screen(size_t width, size_t height)
{
	windowSize.w = width;
	windowSize.h = height;
}

bool VR::init(size_t window_width, size_t window_height)
{
	// the quad that the mirror texture is rendered onto
	pMirrorQuadVao = new VAO(
		{
			Vertex({ -1, -1, 0 },{ 0, 0, 0 },{ 0, 0 },{ 1, 1, 1, 1 }),
			Vertex({ -1, 1, 0 },{ 0, 0, 0 },{ 0, 1 },{ 1, 1, 1, 1 }),
			Vertex({ 1, 1, 0 },{ 0, 0, 0 },{ 1, 1 },{ 1, 1, 1, 1 }),
			Vertex({ 1, -1, 0 },{ 0, 0, 0 },{ 1, 0 },{ 1, 1, 1, 1 })
		},
		{
			1, 0, 2, 0, 3, 2
		}
	);
	pMirrorShader = new Shader("vrMirror", "shaders/vrMirror");

	
	ovrResult result;

	result = ovr_Initialize(nullptr);
	OVR_VALIDATE(OVR_SUCCESS(result), "Failed to init HMD");
	if (result)
	{
		return false;
	}

	result = ovr_Create(&vrSession, &hmdLuid);
	OVR_VALIDATE(OVR_SUCCESS(result), "Failed to init HMD");
	if (result)
	{
		return false;
	}

	hmdDesc = ovr_GetHmdDesc(vrSession);

	// create eye Render buffers
	for (int eye = 0; eye < 2; ++eye)
	{
		const ovrSizei idealTextureSize = ovr_GetFovTextureSize(vrSession, ovrEyeType(eye), hmdDesc.DefaultEyeFov[eye], 1.f);
		textureSwapchains[eye] = new TextureBuffer(vrSession, true, true, idealTextureSize, 1, nullptr, 1);
		textureDepthBuffers[eye] = new DepthBuffer(textureSwapchains[eye]->GetSize(), 0);

		if (!textureSwapchains[eye]->textureChain)
		{
			OVR_VALIDATE(false, "Failed to create eye Render buffers");
			return false;
		}
	}

	textureSizes[0] = textureSwapchains[0]->GetSize();
	textureSizes[1] = textureSwapchains[1]->GetSize();

	bufferSize.w = textureSwapchains[0]->GetSize().w + textureSwapchains[1]->GetSize().w;
	bufferSize.h = std::max(textureSwapchains[0]->GetSize().h, textureSwapchains[1]->GetSize().h);

	// create mirror texture
	ovrMirrorTextureDesc mirrorDesc;
	memset(&mirrorDesc, 0, sizeof(mirrorDesc));
	mirrorDesc.Width = bufferSize.w;
	mirrorDesc.Height = bufferSize.h;
	mirrorDesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
	mirrorDesc.MirrorOptions = ovrMirrorOption_PostDistortion;
	result = ovr_CreateMirrorTextureWithOptionsGL(vrSession, &mirrorDesc, &mirrorTexture);
	OVR_VALIDATE(OVR_SUCCESS(result), "Failed to create mirror texture");
	if (result)
	{
		return false;
	}

	windowSize.w = window_width;
	windowSize.h = window_height;

	ovr_GetMirrorTextureBufferGL(vrSession, mirrorTexture, &mirrorTextureHandle);

	// configure read buffer for mirror
	glGenFramebuffers(1, &mirrorFBO);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, mirrorFBO);
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mirrorTextureHandle, 0);
	glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	eyeRenderDescs[0] = ovr_GetRenderDesc(vrSession, ovrEye_Left, hmdDesc.DefaultEyeFov[0]);
	eyeRenderDescs[1] = ovr_GetRenderDesc(vrSession, ovrEye_Right, hmdDesc.DefaultEyeFov[1]);

	// turn off vsync and let the compositor "do its magic"
	glfwSwapInterval(0);

	ovr_SetTrackingOriginType(vrSession, ovrTrackingOrigin_EyeLevel);

	ovrTrackingState ts = ovr_GetTrackingState(vrSession, ovr_GetTimeInSeconds(), ovrTrue);

	return true;
}
