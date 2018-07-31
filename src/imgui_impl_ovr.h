#pragma once

#include <glm/glm.hpp>

struct ImDrawData;

bool ImGui_ImplOvr_Init();
void ImGui_ImplOvr_Shutdown();
void ImGui_ImplOvr_NewFrame(glm::mat4 guiModelMatrix);

void ImGui_ImplOvr_SetVirtualCanvasSize(glm::ivec2 size);
void ImGui_ImplOvr_SetThumbstickDeadzone(float deadzone);
void ImGui_ImplOvr_SetMaxRaycastDistance(float distance);

bool ImGui_ImplOvr_CreateFontsTexture();
void ImGui_ImplOvr_DestroyFontsTexture();
bool ImGui_ImplOvr_CreateDeviceObjects();
void ImGui_ImplOvr_DestroyDeviceObjects();
void ImGui_ImplOvr_RenderDrawData(ImDrawData* draw_data);
void ImGui_ImplOvr_RenderGUIQuad(glm::mat4 proj, glm::mat4 view, glm::mat4 model);
void ImGui_ImplOvr_RenderControllerLine(glm::mat4 proj, glm::mat4 view);