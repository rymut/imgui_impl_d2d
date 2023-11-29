// dear imgui: Renderer Backend for SDL_Renderer for Direct2D

// You can copy and use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// CHANGELOG
//  2023-10-20: Initial version.
//  2023-11-29: Rendering triangles

#include "imgui.h"
#ifndef IMGUI_DISABLE
#include "imgui_impl_d2d.h"
#include <stdint.h>     // intptr_t

#include <stdio.h>
#include <wincodec.h>

#include <cmath>
#include <dwrite_3.h>
#include <d2d1.h>
#include <d2d1_3.h>

template <typename T>
struct ImGui_ImplD2D_ComPtr
{
    T* Pointer;
    ImGui_ImplD2D_ComPtr() {
        memset((void*)this, 0, sizeof(*this));
    }
    void Release() {
        if (Pointer) {
            Pointer->Release();
            Pointer = nullptr;
        }
    }
    ~ImGui_ImplD2D_ComPtr() {
        Release();
    }
};

struct ImGui_ImplD2D_Fonts {

};

struct ImGui_ImplD2D_Data
{
    ID2D1Factory* Factory;
    ID2D1RenderTarget* RenderTarget;
    ImGui_ImplD2D_Fonts* Fonts;
    ImGui_ImplD2D_ComPtr<ID2D1SolidColorBrush> SolidColorBrush;
    ImGui_ImplD2D_ComPtr<ID2D1StrokeStyle> StrokeStyle;
    D2D1_GRADIENT_STOP GradientStops[2];
    IDWriteFactory5* WriteFactory;
    ImGui_ImplD2D_Data() { memset((void*)this, 0, sizeof(*this)); }
};

inline static ImGui_ImplD2D_Data* ImGui_ImplD2D_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplD2D_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

bool     ImGui_ImplD2D_Init(ID2D1RenderTarget* rendererTarget, IDWriteFactory* writeFactory) {
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");
    IM_ASSERT(rendererTarget != nullptr && "Direct2D renderer target not initialized!");
    IM_ASSERT(writeFactory != nullptr && "DirectWrite factory not initialized!");

    // Setup backend capabilities flags
    ImGui_ImplD2D_Data* bd = IM_NEW(ImGui_ImplD2D_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_d2d";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

    bd->GradientStops[0U].position = 0.f;
    bd->GradientStops[1U].position = 1.f;
    HRESULT hr = S_OK;
    bool success = SUCCEEDED(hr);
    rendererTarget->GetFactory(&bd->Factory);
    if (success) {
        success = ImGui_ImplD2D_CreateDeviceObjects(rendererTarget);
    }
    if (success) {
        success = io.Fonts->Build();
    }
    if (success) {

        if (io.Fonts->TexID == 0) {
            io.Fonts->SetTexID((ImTextureID)(intptr_t)1);
        }
    }
    if (success) {
        D2D1_STROKE_STYLE_PROPERTIES props = D2D1::StrokeStyleProperties();
        props.miterLimit = 0;
        props.lineJoin = D2D1_LINE_JOIN_ROUND;
        hr = bd->Factory->CreateStrokeStyle(props, NULL, 0U, &bd->StrokeStyle.Pointer);
        success = SUCCEEDED(hr);
    }
    if (success) {
        hr = writeFactory->QueryInterface(__uuidof(IDWriteFactory5), (void**)&bd->WriteFactory);
        success = SUCCEEDED(hr);
    }
    if (success) {
        return true;
    }
    return false;
}

bool ImGui_ImplD2D_CreateDeviceObjects(ID2D1RenderTarget* renderTarget) {
    ImGui_ImplD2D_Data* bd = ImGui_ImplD2D_GetBackendData();
    if (renderTarget == nullptr) {
        renderTarget = bd->RenderTarget;
    }
    IM_ASSERT(renderTarget != nullptr && "Render target must be initialized");

    HRESULT hr = S_OK;
    if (renderTarget != bd->RenderTarget) {
        ImGui_ImplD2D_DestroyDeviceObjects();
        bd->RenderTarget = renderTarget;
        // those this increase reference count?
        renderTarget->GetFactory(&bd->Factory);
        if (FAILED(hr)) {
            return false;
        }
        hr = renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &bd->SolidColorBrush.Pointer);
        if (FAILED(hr)) {
            return false;
        }
    }
    return ImGui_ImplD2D_CreateFontsTexture();
}

void    ImGui_ImplD2D_DestroyDeviceObjects()
{
    ImGui_ImplD2D_Data* bd = ImGui_ImplD2D_GetBackendData();
    bd->SolidColorBrush.Release();
    bd->StrokeStyle.Release();

    ImGui_ImplD2D_DestroyFontsTexture();
}

bool    ImGui_ImplD2D_CreateFontsTexture() {
    ImGui_ImplD2D_Data* bd = ImGui_ImplD2D_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();
    return true;
}

void    ImGui_ImplD2D_DestroyFontsTexture()
{
    ImGui_ImplD2D_Data* bd = ImGui_ImplD2D_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();

    if (bd->Fonts) {
        ///  io.Fonts->SetTexID(0);

    }
}

void     ImGui_ImplD2D_Shutdown() {
    ImGui_ImplD2D_Data* bd = ImGui_ImplD2D_GetBackendData();
    IM_ASSERT(bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplD2D_DestroyDeviceObjects();

    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;
    IM_DELETE(bd);
}

void     ImGui_ImplD2D_NewFrame() {
    ImGui_ImplD2D_Data* bd = ImGui_ImplD2D_GetBackendData();
    IM_ASSERT(bd != nullptr && "Did you call ImGui_ImplD2D_Init()?");
    // auto detect new fonts if required
    ImGui_ImplD2D_CreateFontsTexture();
}

inline static D2D1_COLOR_F ImGui_ImplD2D_Color(ImU32 color) {
    constexpr float norm = 1.0f / 255.0f;
    const float a = (color >> 24 & 0xFFu) * norm;
    const float b = ((color >> 16) & 0xFFu) * norm;
    const float g = ((color >> 8) & 0xFFu) * norm;
    const float r = ((color >> 0) & 0xFFu) * norm;
    return D2D1_COLOR_F{ r, g, b, a };
}

inline static D2D1_POINT_2F ImGui_ImplD2D_Point(const ImVec2& point) {
    return D2D1_POINT_2F{ point.x, point.y };
}

static bool ImGui_ImplD2D_CreateBrush(
    ImGui_ImplD2D_ComPtr<ID2D1RadialGradientBrush>& brush,
    D2D1_GRADIENT_STOP(&stops)[2U],
    ImGui_ImplD2D_ComPtr<ID2D1GradientStopCollection>& stopCollection,
    D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES& props,
    ID2D1RenderTarget* renderTarget,
    ImVec2 aPos, ImVec2 bPos, ImU32 aCol, ImU32 bCol) {
    props.center = ImGui_ImplD2D_Point(aPos);
    props.gradientOriginOffset = D2D1_POINT_2F{ 0, 0 };
    props.radiusX = abs(aPos.x - bPos.x);
    props.radiusY = abs(aPos.y - bPos.y);
    stops[0U].color = ImGui_ImplD2D_Color(aCol);
    stops[1U].color = ImGui_ImplD2D_Color(bCol);
    stopCollection.Release();
    brush.Release();
    HRESULT hr = renderTarget->CreateGradientStopCollection(stops, 2U, &stopCollection.Pointer);
    if (SUCCEEDED(hr))
    {
        hr = renderTarget->CreateRadialGradientBrush(props, stopCollection.Pointer, &brush.Pointer);
    }
    return SUCCEEDED(hr);
}

static bool ImGui_ImplD2D_CreateBrush(
    ImGui_ImplD2D_ComPtr<ID2D1LinearGradientBrush>& brush,
    D2D1_GRADIENT_STOP(&stops)[2U],
    ImGui_ImplD2D_ComPtr<ID2D1GradientStopCollection>& stopCollection,
    D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES& props,
    ID2D1RenderTarget* renderTarget,
    ImVec2 aPos, ImVec2 bPos, ImU32 aCol, ImU32 bCol
) {
    props.startPoint = ImGui_ImplD2D_Point(aPos);
    props.endPoint = ImGui_ImplD2D_Point(bPos);
    stops[0U].color = ImGui_ImplD2D_Color(aCol);
    stops[1U].color = ImGui_ImplD2D_Color(bCol);
    stopCollection.Release();
    brush.Release();
    HRESULT hr = renderTarget->CreateGradientStopCollection(stops, 2U, &stopCollection.Pointer);
    if (SUCCEEDED(hr)) {
        hr = renderTarget->CreateLinearGradientBrush(props, stopCollection.Pointer, &brush.Pointer);
    }
    return SUCCEEDED(hr);
}

#if 1
#include <vector>
static int ImGui_ImplD2D_IsGlyph(ID2D1RenderTarget* RendererTarget, ImGui_ImplD2D_Data* bd, const ImGuiIO& io, const ImDrawCmd* pcmd, const ImDrawVert* vert, const ImDrawIdx* idx, const int offset = 0) {
    ImTextureID texture = pcmd->GetTexID();
    if (texture != io.Fonts->TexID) {
        return 0;
    }
    const ImU32 noFont = io.Fonts->Fonts.Size;
    ImU32 font = noFont; // out of range
    const ImDrawVert* v0 = vert + idx[offset];
    if (offset < pcmd->ElemCount) {
        for (int f = 0; f < io.Fonts->Fonts.Size && font == noFont; ++f) {
            auto glyphs = io.Fonts->Fonts.Data[f]->Glyphs;
            const auto uv = v0->uv;
            for (int c = 0; c < glyphs.size(); c++) {
                if (uv.x == glyphs[c].U0 && uv.y == glyphs[c].V0 ||
                    uv.x == glyphs[c].U1 && uv.y == glyphs[c].V1) {
                    font = f;
                    break;
                }
            }
        }
    }
    // not a glpyh
    if (font == noFont) {
        return 0;
    }
    // contener
    ImU32 item = 0;
    int glyphCount = 0;
    std::vector<int> glyphRun;
    std::vector<WCHAR> codepointRun;
    std::vector<ImVec2> codepointPos;
    std::vector<char> codeRun;

    codepointRun.reserve(255);
    glyphRun.reserve(255);
    codepointPos.reserve(255);
    // get glyphs
    const auto& fontData = io.Fonts->Fonts.Data[font];
    auto fontGlyphs = io.Fonts->Fonts.Data[font]->Glyphs;
    auto fontScale = io.FontGlobalScale * fontData->Scale;
    auto fontSize = io.Fonts->Fonts.Data[font]->FontSize * fontScale;
    const auto top = (io.Fonts->Fonts.Data[font]->FontSize - fontData->Ascent) * fontScale;
    constexpr int countPerLetter = 6;
    for (int i = offset, c = 0; i < pcmd->ElemCount && c < fontGlyphs.size(); i += countPerLetter) {
        const ImDrawVert* v = vert + idx[i];
        const auto uv = v->uv;
        for (c = 0; c < fontGlyphs.size(); c++) {
            const auto& glyph = fontGlyphs[c];
            if (uv.x == glyph.U0 && uv.y == glyph.V0 ||
                uv.x == glyph.U1 && uv.y == glyph.V1) {
                glyphRun.push_back(c);
                ImVec2 pos = { v->pos.x - glyph.X0 * fontScale, v->pos.y - glyph.Y0 * fontScale + top };
                codepointPos.push_back(pos);

                codepointRun.push_back(fontGlyphs[c].Codepoint);
                codeRun.push_back(fontGlyphs[c].Codepoint);

                break;
            }
        }
    }
    if (glyphRun.size()) {
        codepointRun.push_back(0);
        codeRun.push_back(0);
        //std::wcerr << codepointRun.data() << std::endl << std::flush;
        static HRESULT hr = S_OK;
        static IDWriteTextFormat* textFormat = NULL;
        static IDWriteInMemoryFontFileLoader* fontLoader = NULL;
        static IDWriteFontFile* fontFile = NULL;
        static IDWriteFontFaceReference* fontFace = NULL;
        static IDWriteFontSetBuilder* fontBuilder = NULL;
        static IDWriteFontSet* fontSet = NULL;
        static IDWriteFontCollection1* fontCollection = NULL;
        if (fontLoader == NULL) {
            hr = bd->WriteFactory->CreateInMemoryFontFileLoader(&fontLoader);
            hr = bd->WriteFactory->RegisterFontFileLoader(fontLoader);
            auto configData = io.Fonts->Fonts.Data[0]->ConfigData;
            fontLoader->CreateInMemoryFontFileReference(bd->WriteFactory, configData->FontData, configData->FontDataSize, NULL, &fontFile);
            hr = bd->WriteFactory->CreateFontFaceReference(fontFile, 0, DWRITE_FONT_SIMULATIONS_NONE, &fontFace);
            hr = bd->WriteFactory->CreateFontSetBuilder(&fontBuilder);

            DWRITE_FONT_PROPERTY props[] =
            {
                // We're only using names to reference fonts programmatically, so won't worry about localized names.
                { DWRITE_FONT_PROPERTY_ID_FAMILY_NAME, L"Arial", L"en-US"},
                { DWRITE_FONT_PROPERTY_ID_FULL_NAME, L"Arial", L"en-US"},
                { DWRITE_FONT_PROPERTY_ID_WEIGHT, L"400", nullptr}
            };
            hr = fontBuilder->AddFontFaceReference(fontFace, props, ARRAYSIZE(props));
            fontBuilder->CreateFontSet(&fontSet);
            hr = bd->WriteFactory->CreateFontCollectionFromFontSet(fontSet, &fontCollection);
        }
        if (textFormat == NULL) {

            hr = bd->WriteFactory->CreateTextFormat(L"Arial", fontCollection, DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                fontSize,
                L"en-US",
                &textFormat);
        }
        if (SUCCEEDED(hr)) {
            const D2D1_SIZE_U renderTargetSize = RendererTarget->GetPixelSize();
            bd->SolidColorBrush.Pointer->SetColor(ImGui_ImplD2D_Color(v0->col));
            bd->RenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
            for (size_t c = 0; c < codepointRun.size() - 1; c++) {
                ImVec2 pos = codepointPos[c];
#if UNICODE
                bd->RenderTarget->DrawText(codepointRun.data() + c, 1, textFormat,
                    D2D1::RectF(pos.x, pos.y, renderTargetSize.width, renderTargetSize.height),
                    bd->SolidColorBrush);
#else
#endif 
            }
        }
        if (textFormat != NULL) {
            textFormat->Release();
            textFormat = NULL;
        }
    }
    return glyphRun.size() * countPerLetter;
}

#endif // 1



inline static ImVec2 from(ImVec2 a, ImVec2 b, ImVec2 c, float u, float v, float w) {
    return ImVec2(u * a.x + v * b.x + w * c.x, u * a.y + v * b.y + w * c.y);

}
static inline ImVec2 ret(ImVec2 a0, ImVec2 a1, ImVec2 b) {
    // first convert line to normalized unit vector
    auto dx = a1.x - a0.x;
    auto dy = a1.y - a0.y;
    auto mag = sqrt(dx * dx + dy * dy);
    dx /= mag;
    dy /= mag;

    // translate the point and get the dot product
    double lambda = (dx * (b.x - a0.x)) + (dy * (b.y - a0.y));
    ImVec2 res;
    res.x = (dx * lambda) + a0.x;
    res.y = (dy * lambda) + a0.y;
    return res;
}

static inline bool isWhite(const ImVec2& uv, const ImVec2& white) {
    return uv.x == white.x && uv.y == white.y;
}

static bool isLine(const ImVec2& uv, ImVec4* texUvLines, int count = IM_DRAWLIST_TEX_LINES_WIDTH_MAX) {
    for (auto it = 0; it < count; it++) {
        const auto& line = texUvLines[it];
        if (uv.x == line.x && uv.y == line.y) {
            return true;
        }
        if (uv.x == line.z && uv.y == line.w) {
            return true;
        }
    }
    return false;
}



void     ImGui_ImplD2D_RenderDrawData(ImDrawData* draw_data) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplD2D_Data* bd = ImGui_ImplD2D_GetBackendData();

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = ImVec2{ 0, 0 };         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = ImVec2{ 1, 1 };
    const float fb_width = static_cast<float>(bd->RenderTarget->GetPixelSize().width);
    const float fb_height = static_cast<float>(bd->RenderTarget->GetPixelSize().height);

    D2D1_POINT_2F points2f[3];
    memset(&points2f, 0, sizeof(points2f));
    ImGui_ImplD2D_ComPtr<ID2D1BitmapBrush> bitmapBrush;
    ImGui_ImplD2D_ComPtr<ID2D1PathGeometry> pathGeometry;
    ImGui_ImplD2D_ComPtr<ID2D1GeometrySink> geometrySink;
    ImGui_ImplD2D_ComPtr<ID2D1GradientStopCollection> stopsCol;
    D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES radGradProps;
    memset(&radGradProps, 0, sizeof(radGradProps));
    ImGui_ImplD2D_ComPtr<ID2D1RadialGradientBrush> radGradBrush;
    D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES linGradProps;
    memset(&linGradProps, 0, sizeof(linGradProps));
    ImGui_ImplD2D_ComPtr<ID2D1LinearGradientBrush> linGradBrush;

    HRESULT hr = S_OK;

    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const ImDrawVert* vtx_buffer = cmd_list->VtxBuffer.Data;
        const ImDrawIdx* idx_buffer = cmd_list->IdxBuffer.Data;
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ; //ImGui_ImplSDLRenderer2_SetupRenderState();
                else
                    pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);
                if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
                if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
                if (clip_max.x > (float)fb_width) { clip_max.x = (float)fb_width; }
                if (clip_max.y > (float)fb_height) { clip_max.y = (float)fb_height; }
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                D2D1_RECT_F clip = D2D1::RectF(clip_min.x, clip_min.y, clip_max.x, clip_max.y);
                bd->RenderTarget->PushAxisAlignedClip(clip, D2D1_ANTIALIAS_MODE_ALIASED);


                int vertCount = cmd_list->VtxBuffer.Size - pcmd->VtxOffset;
                int indCount = pcmd->ElemCount;
                const ImDrawVert* vert = vtx_buffer + pcmd->VtxOffset;
                const ImDrawIdx* idx = idx_buffer + pcmd->IdxOffset;

                {
                    pathGeometry.Release();
                    hr = bd->Factory->CreatePathGeometry(&pathGeometry.Pointer);
                    if (FAILED(hr))
                    {
                        continue;
                    }
                    geometrySink.Release();
                    hr = pathGeometry.Pointer->Open(&geometrySink.Pointer);
                    if (FAILED(hr))
                    {
                        continue;
                    }

                    ImTextureID texture = pcmd->GetTexID();
                    const bool isFont = texture = io.Fonts->TexID;
                    const ImDrawVert* xyFirst = vert + idx[0];
                    ImU32 color = xyFirst->col;
                    ImDrawIdx index = idx[2];
                    int figureIndCount = 0;
                    int figureIndStart = 0;
                    ImU32 green = ImGui::GetColorU32(IM_COL32(0, 255, 0, 255));
                    for (int i = 0; i < indCount; i += 3) {
                        const ImDrawVert* xy0 = vert + idx[i];
                        const ImDrawVert* xy1 = vert + idx[i + 1];
                        const ImDrawVert* xy2 = vert + idx[i + 2];

                        points2f[0].x = (float)(xy0->pos.x);
                        points2f[0].y = (float)(xy0->pos.y);
                        points2f[1].x = (float)(xy1->pos.x);
                        points2f[1].y = (float)(xy1->pos.y);
                        points2f[2].x = (float)(xy2->pos.x);
                        points2f[2].y = (float)(xy2->pos.y);
                        int ioffset = i > 0 ? i - 3 : 0;
                        bool test0 = idx[ioffset] == idx[i] || idx[ioffset] == idx[i + 1] || idx[ioffset] == idx[i + 2];
                        bool test1 = idx[ioffset + 1] == idx[i] || idx[ioffset + 1] == idx[i + 1] || idx[ioffset + 1] == idx[i + 2];
                        bool test2 = idx[ioffset + 2] == idx[i] || idx[ioffset + 2] == idx[i + 1] || idx[ioffset + 2] == idx[i + 2];
                        bool test = test0 || test1 || test2;
                        if (!((color == xy0->col || color == xy1->col || color == xy2->col) && test)) {
                            if (geometrySink.Pointer) {
                                hr = geometrySink.Pointer->Close();
                                geometrySink.Release();
                                if (FAILED(hr))
                                {
                                    continue;
                                }
                            }
                            if (!isFont || (isWhite(xyFirst->uv, io.Fonts->TexUvWhitePixel) || isLine(xyFirst->uv, io.Fonts->TexUvLines))) {

                                bd->RenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                                const ImDrawVert* verts[4] = { nullptr, nullptr, nullptr, nullptr };
                                if (figureIndCount <= 6) {
                                    int ioffset = figureIndStart;
                                    verts[0] = vert + idx[ioffset];
                                    verts[1] = vert + idx[ioffset + 1];
                                    verts[2] = vert + idx[ioffset + 2];
                                    verts[3] = vert + idx[ioffset + figureIndCount - 1];
                                    if (verts[0]->col == verts[1]->col && verts[2]->col == verts[3]->col && verts[1]->col == verts[2]->col) {
                                        bd->SolidColorBrush.Pointer->SetColor(ImGui_ImplD2D_Color(color));
                                        bd->RenderTarget->FillGeometry(pathGeometry.Pointer, bd->SolidColorBrush.Pointer);
                                    }
                                    else if (figureIndCount == 6 && (verts[0]->col == verts[3]->col || verts[0]->col == verts[1]->col)) {
                                        bool success = true;
                                        if (verts[0]->col == verts[3]->col) {
                                            success = ImGui_ImplD2D_CreateBrush(linGradBrush, bd->GradientStops, stopsCol, linGradProps, bd->RenderTarget,
                                                verts[0]->pos, verts[1]->pos, verts[0]->col, verts[1]->col);
                                        }
                                        else {
                                            success = ImGui_ImplD2D_CreateBrush(linGradBrush, bd->GradientStops, stopsCol, linGradProps, bd->RenderTarget,
                                                verts[1]->pos, verts[2]->pos, verts[1]->col, verts[2]->col);
                                        }

                                        if (success) {
                                            bd->RenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
                                            bd->RenderTarget->FillGeometry(pathGeometry.Pointer, linGradBrush.Pointer);
                                            bd->RenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                                            linGradBrush.Release();
                                            stopsCol.Release();
                                        }
                                    }
                                    else {
                                        ImVec2 middle;
                                        middle.x = 0.25 * (verts[0]->pos.x + verts[1]->pos.x + verts[2]->pos.x + verts[3]->pos.x);
                                        middle.y = 0.25 * (verts[0]->pos.y + verts[1]->pos.y + verts[2]->pos.y + verts[3]->pos.y);

                                        if (ImGui_ImplD2D_CreateBrush(radGradBrush, bd->GradientStops, stopsCol, radGradProps, bd->RenderTarget,
                                            verts[0]->pos, verts[2]->pos, verts[0]->col, verts[0]->col & 0x00FFFFFFu)) {
                                            bd->RenderTarget->FillGeometry(pathGeometry.Pointer, radGradBrush.Pointer);
                                        }
                                        if (ImGui_ImplD2D_CreateBrush(radGradBrush, bd->GradientStops, stopsCol, radGradProps, bd->RenderTarget,
                                            verts[2]->pos, verts[0]->pos, verts[2]->col, verts[2]->col & 0x00FFFFFFu)) {
                                            bd->RenderTarget->FillGeometry(pathGeometry.Pointer, radGradBrush.Pointer);
                                        }
                                        if (ImGui_ImplD2D_CreateBrush(radGradBrush, bd->GradientStops, stopsCol, radGradProps, bd->RenderTarget,
                                            verts[1]->pos, verts[3]->pos, verts[1]->col, verts[1]->col & 0x00FFFFFFu)) {
                                            bd->RenderTarget->FillGeometry(pathGeometry.Pointer, radGradBrush.Pointer);
                                        }
                                        if (figureIndCount > 3 && ImGui_ImplD2D_CreateBrush(radGradBrush, bd->GradientStops, stopsCol, radGradProps, bd->RenderTarget,
                                            verts[3]->pos, verts[1]->pos, verts[3]->col, verts[3]->col & 0x00FFFFFFu)) {
                                            bd->RenderTarget->FillGeometry(pathGeometry.Pointer, radGradBrush.Pointer);
                                        }
                                        radGradBrush.Release();
                                        stopsCol.Release();
                                    }
                                }
                                else {
                                    bd->SolidColorBrush.Pointer->SetColor(ImGui_ImplD2D_Color(color));
                                    bd->RenderTarget->FillGeometry(pathGeometry.Pointer, bd->SolidColorBrush.Pointer);
                                }
                            }
                            else
                            {
#if 0
                                IWICImagingFactory* WicFactory = nullptr;
                                hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&WicFactory));
                                unsigned char* pixels = nullptr;
                                int width = 0, height = 0, bpp = 0;
                                io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bpp);

                                ID2D1BitmapBrush* bitmapBrush = nullptr;
                                ID2D1Bitmap* b = ImGui_ImplD2D_LoadTextureRgb32(RendererTarget, WicFactory, pixels, width, height, width * bpp, width * bpp * height);
                                RendererTarget->CreateBitmapBrush(b, &bitmapBrush);
                                ImVec2 pos00 = xyFirst->pos;
                                ImVec2 pos11 = (xyFirst + 2)->pos;
                                ImVec2 uv00 = xyFirst->uv;
                                ImVec2 uv11 = (xyFirst + 2)->uv;
                                ImVec2 rectCropFactor = { uv11.x - uv00.x, uv11.y - uv00.y };
                                ImVec2 rectSize = { pos11.x - pos00.x, pos11.y - pos00.y };
                                ImVec2 scaleFactor = { rectSize.x / (rectCropFactor.x * width), rectSize.y / (rectCropFactor.y * height) };

                                D2D1::Matrix3x2F transUv = D2D1::Matrix3x2F::Translation(-uv00.x * width, -uv00.y * height);
                                D2D1::Matrix3x2F trans = D2D1::Matrix3x2F::Translation(pos00.x, pos00.y);
                                D2D1::Matrix3x2F scale = D2D1::Matrix3x2F::Scale(scaleFactor.x, scaleFactor.y);

                                bitmapBrush->SetTransform(transUv * scale * trans);

                                RendererTarget->FillGeometry(pathGeometry, bitmapBrush);

                                bitmapBrush->Release();
                                b->Release();
                                WicFactory->Release();
                                WicFactory = nullptr;
#else
                                bd->SolidColorBrush.Pointer->SetColor(D2D1::ColorF(D2D1::ColorF::White));
                                bd->RenderTarget->FillGeometry(pathGeometry.Pointer, bd->SolidColorBrush.Pointer);
#endif // 0
                            }
                            xyFirst = xy0;
                            figureIndCount = 0;
                            figureIndStart = i;
                            color = xy0->col;
                            pathGeometry.Release();
                            hr = bd->Factory->CreatePathGeometry(&pathGeometry.Pointer);
                            if (FAILED(hr))
                            {
                                continue;
                            }
                            hr = pathGeometry.Pointer->Open(&geometrySink.Pointer);
                            if (FAILED(hr))
                            {
                                continue;
                            }
                        }
                        int skip = ImGui_ImplD2D_IsGlyph(bd->RenderTarget, bd, io, pcmd, vert, idx, i);
                        if (skip > 0) {
                            // print stream

                            i += skip - 3;
                            continue;
                        }

                        geometrySink.Pointer->SetFillMode(D2D1_FILL_MODE_ALTERNATE);
                        geometrySink.Pointer->SetSegmentFlags(D2D1_PATH_SEGMENT_FORCE_ROUND_LINE_JOIN);
                        geometrySink.Pointer->BeginFigure(points2f[0], D2D1_FIGURE_BEGIN_FILLED);
                        geometrySink.Pointer->AddLines(&points2f[0], 3);
                        geometrySink.Pointer->EndFigure(D2D1_FIGURE_END_CLOSED);
                        figureIndCount += 3;

                        ImTextureID texture = pcmd->GetTexID();
                    }
                    hr = geometrySink.Pointer->Close();
                    if (FAILED(hr))
                    {
                        continue;
                    }
                    geometrySink.Release();
                    bd->SolidColorBrush.Pointer->SetColor(ImGui_ImplD2D_Color(color));
                    bd->RenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
                    bd->RenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
#if 0 
                    if (!isFont || (isWhite(xyFirst->uv, io.Fonts->TexUvWhitePixel) || isLine(xyFirst->uv, io.Fonts->TexUvLines))) {
                        const ImDrawVert* verts[4] = { nullptr, nullptr, nullptr, nullptr };
                        if (figureIndCount <= 6) {
                            int ioffset = figureIndStart;
                            verts[0] = vert + idx[ioffset];
                            verts[1] = vert + idx[ioffset + 1];
                            verts[2] = vert + idx[ioffset + 2];
                            verts[3] = vert + idx[ioffset + figureIndCount - 1];
                            if (verts[0]->col == verts[1]->col && verts[2]->col == verts[3]->col && verts[1]->col == verts[2]->col) {
                                bd->SolidColorBrush->SetColor(toColor(color));
                                RendererTarget->FillGeometry(pathGeometry, bd->SolidColorBrush);
                            }
                            else if (verts[0]->col == verts[3]->col || verts[1]->col == verts[2]->col) {
                                ID2D1LinearGradientBrush* gradientBrush = nullptr;
                                if (verts[0]->col == verts[3]->col) {
                                    auto props = D2D1::LinearGradientBrushProperties(D2D1::Point2F(verts[0]->pos.x, verts[0]->pos.y),
                                        D2D1::Point2F(verts[1]->pos.x, verts[1]->pos.y));
                                    bd->GradientStops[0].position = 0;
                                    bd->GradientStops[1].position = 1;
                                    bd->GradientStops[0].color = toColor(verts[0]->col);
                                    bd->GradientStops[1].color = toColor(verts[1]->col);
                                    bd->GradientStopCollection->Release();
                                    RendererTarget->CreateGradientStopCollection(bd->GradientStops, 2U, &bd->GradientStopCollection);
                                    RendererTarget->CreateLinearGradientBrush(props, bd->GradientStopCollection, &gradientBrush);

                                }
                                else {
                                    auto props = D2D1::LinearGradientBrushProperties(D2D1::Point2F(verts[1]->pos.x, verts[1]->pos.y),
                                        D2D1::Point2F(verts[0]->pos.x, verts[0]->pos.y));
                                    bd->GradientStops[0].position = 0;
                                    bd->GradientStops[1].position = 1;
                                    bd->GradientStops[0].color = toColor(verts[1]->col);
                                    bd->GradientStops[1].color = toColor(verts[0]->col);
                                    bd->GradientStopCollection->Release();
                                    RendererTarget->CreateGradientStopCollection(bd->GradientStops, 2U, &bd->GradientStopCollection);
                                    RendererTarget->CreateLinearGradientBrush(props, bd->GradientStopCollection, &gradientBrush);

                                }
                                bd->RenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
                                bd->RenderTarget->FillGeometry(pathGeometry.Pointer, gradientBrush);
                                RendererTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

                                if (gradientBrush) {
                                    gradientBrush->Release();
                                    gradientBrush = nullptr;
                                }
                            }
                        }
                        else {
                            bd->SolidColorBrush->SetColor(toColor(color));
                            RendererTarget->FillGeometry(pathGeometry, bd->SolidColorBrush);
                        }
                    }
                    else
                    {
#if 0 
                        IWICImagingFactory* WicFactory = nullptr;
                        hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&WicFactory));
                        unsigned char* pixels = nullptr;
                        int width = 0, height = 0, bpp = 0;
                        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bpp);

                        ID2D1BitmapBrush* bitmapBrush = nullptr;
                        ID2D1Bitmap* b = ImGui_ImplD2D_LoadTextureRgb32(RendererTarget, WicFactory, pixels, width, height, width * bpp, width * bpp * height);
                        RendererTarget->CreateBitmapBrush(b, &bitmapBrush);
                        ImVec2 pos00 = xyFirst->pos;
                        ImVec2 pos11 = (xyFirst + 2)->pos;
                        ImVec2 uv00 = xyFirst->uv;
                        ImVec2 uv11 = (xyFirst + 2)->uv;
                        ImVec2 rectCropFactor = { uv11.x - uv00.x, uv11.y - uv00.y };
                        ImVec2 rectSize = { pos11.x - pos00.x, pos11.y - pos00.y };
                        ImVec2 scaleFactor = { rectSize.x / (rectCropFactor.x * width), rectSize.y / (rectCropFactor.y * height) };

                        D2D1::Matrix3x2F transUv = D2D1::Matrix3x2F::Translation(-uv00.x * width, -uv00.y * height);
                        D2D1::Matrix3x2F trans = D2D1::Matrix3x2F::Translation(pos00.x, pos00.y);
                        D2D1::Matrix3x2F scale = D2D1::Matrix3x2F::Scale(scaleFactor.x, scaleFactor.y);

                        bitmapBrush->SetTransform(transUv * scale * trans);

                        bd->RenderTarget->FillGeometry(pathGeometry.Pointer, bitmapBrush);

                        bitmapBrush->Release();
                        b->Release();
                        WicFactory->Release();
                        WicFactory = nullptr;
#endif // 0
                    }
#endif // 0
                    pathGeometry.Release();
                }
                bd->RenderTarget->PopAxisAlignedClip();
            }
        }
    }

}

#endif

