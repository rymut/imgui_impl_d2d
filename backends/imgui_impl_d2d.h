// dear imgui: Renderer Backend for Direct2D
// This needs to be used along with a Platform Backend (e.g. Win32)

// Implemented features:
//  [x] Init: Initialize/shutdown context
//  [ ] Font: Custom font builder for Direct Write
//  [ ] Renderer: Render fonts using Direct Write
//  [x] Renderer: Render single color triangles
//  [x] Renderer: Render triangles with gradient
//  [ ] Renderer: Render triangles with texture

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#pragma once
#include "imgui.h"      // IMGUI_IMPL_API
#ifndef IMGUI_DISABLE

struct ID2D1RenderTarget;
struct IDWriteFactory;
struct IDWriteFactory5;

using ImGui_ImplD2D_RenderTarget = ID2D1RenderTarget;
using ImGui_ImplD2D_WriteFactory = IDWriteFactory5;

IMGUI_IMPL_API bool     ImGui_ImplD2D_Init(ImGui_ImplD2D_RenderTarget* renderTarget, IDWriteFactory *writeFactory);
IMGUI_IMPL_API void     ImGui_ImplD2D_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplD2D_NewFrame();
IMGUI_IMPL_API void     ImGui_ImplD2D_RenderDrawData(ImDrawData* draw_data);

/** @brief Fonut builder using Direct2D

    Used when font renderer is being used for rendering fonts using direct write5 interface
 */
IMGUI_IMPL_API bool		ImGui_ImplD2D_FontBuilder_Build(ImFontAtlas* atlas);

// Called by Init/NewFrame/Shutdown
IMGUI_IMPL_API bool     ImGui_ImplD2D_CreateFontsTexture();
/** @brief Destroy texture fonts assets
 */
IMGUI_IMPL_API void     ImGui_ImplD2D_DestroyFontsTexture();
IMGUI_IMPL_API void     ImGui_ImplD2D_DestroyDeviceObjects();
IMGUI_IMPL_API bool     ImGui_ImplD2D_CreateDeviceObjects(ImGui_ImplD2D_RenderTarget* renderTarget);

/** @brief Utility for loading textures
 */

#endif // #ifndef IMGUI_DISABLE
