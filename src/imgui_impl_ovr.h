#pragma once

#include <imgui.h>
#include <glm/glm.hpp>

struct GLFWwindow;

IMGUI_IMPL_API bool ImGui_ImplOvr_Init(GLFWwindow* window);
IMGUI_IMPL_API void ImGui_ImplOvr_Shutdown();
IMGUI_IMPL_API void ImGui_ImplOvr_NewFrame();

IMGUI_IMPL_API void ImGui_ImplOvr_KeyFunc(int key, int action, int mods);
IMGUI_IMPL_API void ImGui_ImplOvr_SetVirtualCanvasSize(glm::ivec2 size);

IMGUI_IMPL_API bool ImGui_ImplOvr_CreateFontsTexture();
IMGUI_IMPL_API void ImGui_ImplOvr_DestroyFontsTexture();
IMGUI_IMPL_API bool ImGui_ImplOvr_CreateDeviceObjects();
IMGUI_IMPL_API void ImGui_ImplOvr_DestroyDeviceObjects();
IMGUI_IMPL_API void ImGui_ImplOvr_RenderDrawData(ImDrawData* draw_data);
IMGUI_IMPL_API void ImGui_ImplOvr_RenderGUIQuad(glm::mat4 proj, glm::mat4 view, glm::mat4 model);