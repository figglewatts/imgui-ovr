# imgui-ovr
Experimenting with ImGui and Oculus SDK

## Dependencies
- [GLAD](http://glad.dav1d.de/#profile=compatibility&specification=gl&api=gl%3D4.3&api=gles1%3Dnone&api=gles2%3Dnone&api=glsc2%3Dnone&language=c&loader=on)
	- Download as ZIP and extract in repo root
- [GLFW](http://www.glfw.org/)
	- Download Win32 binaries and extract `lib-vc2015/glfw3.dll` and `lib-vc2015/glfw3dll.lib` to `./deps`
- [GLM](https://glm.g-truc.net/0.9.9/index.html)
	- Download and extract to `./include`
- [dear imgui](https://github.com/ocornut/imgui/releases/tag/v1.62)
	- Download and extract `imconfig.h`, `imgui.cpp`, `imgui.h`, `imgui_demo.cpp`, `imgui_draw.cpp`, `imgui_internal.h`, `stb_rect_pack.h`, `stb_textedit.h`, and `stb_truetype.h` to `./deps`

## Quick-start
1. Clone the repo
2. Install dependencies as above
3. Build and run
