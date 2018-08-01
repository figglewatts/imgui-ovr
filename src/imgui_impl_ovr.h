// ImGui Renderer for: Oculus Rift CV1 with OpenGL3+
// This needs to be used along with a Platform Binding (e.g. GLFW, SDL, Win32, custom..)
// (Note: Glad is used as the OpenGL platform binding, but can be replaced with whatever works best for you)

// About OpenGL function loaders:
// Modern OpenGL requires individual functions to be loaded manually. Helper libraries are often used for this purpose.
// In this file Glad is used (https://github.com/Dav1dde/glad)
// You may use another any other loader/header of your choice, such as glew, glext, gl3w, glLoadGen, etc.

#pragma once

#include <glm/glm.hpp>
#include <LibOVR/OVR_CAPI.h>

struct ImDrawData;

// functions called by user to use renderer
bool ImGui_ImplOvr_Init(ovrSession session, long long* const frameIndex);
void ImGui_ImplOvr_Shutdown();
void ImGui_ImplOvr_NewFrame(glm::mat4 guiModelMatrix);
void ImGui_ImplOvr_Update();
void ImGui_ImplOvr_RenderDrawData(ImDrawData* draw_data);
void ImGui_ImplOvr_RenderGUIQuad(glm::mat4 proj, glm::mat4 view, glm::mat4 model);
void ImGui_ImplOvr_RenderControllerLine(glm::mat4 proj, glm::mat4 view);

// mutation functions to modify various globals
void ImGui_ImplOvr_SetVirtualCanvasSize(glm::ivec2 size);
void ImGui_ImplOvr_SetThumbstickDeadzone(float deadzone);
void ImGui_ImplOvr_SetControllerLineColor(glm::vec3 color);
void ImGui_ImplOvr_SetPixelsPerUnit(float ppu);

// called internally
bool ImGui_ImplOvr_CreateFontsTexture();
void ImGui_ImplOvr_DestroyFontsTexture();
bool ImGui_ImplOvr_CreateDeviceObjects();
void ImGui_ImplOvr_DestroyDeviceObjects();