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

/** @brief Wrapper to manage COM pointers

    Detach would not be used at all (really only attach detach will be used)
 */
template <typename T>
struct ImGui_ImplD2D_ComPtr
{
    /** @brief Stored shared pointer */
    T* Pointer;

    /** @brief Is Com object attached */
    bool IsAttached;

    /** @brief Default constructor - just clean memory */
    ImGui_ImplD2D_ComPtr() {
        memset((void*)this, 0, sizeof(*this));
    }

    /** @brief Get raw pointer

        @returns
            This method returns value of @see Pointer
     */
    T* Get() const {
        return Pointer;
    }

    /** @brief Get pointer to raw pointer

        @returns
            This method returs pointer to field @see Pointer
     */
    T* const* GetAddressOf() const {
        return &Pointer;
    }

    T** GetAddressOf() {
        return &Pointer;
    }

    T* operator->() const {
        return Pointer;
    }

    void AddRef() {
        if (Pointer) {
            Pointer->AddRef();
            IsAttached = true;
        }
    }

    ULONG ReleaseRef() {
        ULONG refs = 0;
        T* tmp = Pointer;
        if (tmp != nullptr) {
            Pointer = nullptr;
            refs = tmp->Release();
        }
        return refs;
    }

    void Release() {
        ReleaseRef();
    }

    T* Detach() {
        T* tmp = Pointer;
        Pointer = nullptr;
        IsAttached = false;
        return tmp;
    }

    void Acquire(T* pointer) {
        if (Pointer != pointer) {
            ReleaseRef();
            Pointer = pointer;
            AddRef();
        }
    }
    void Attach(T* pointer) {
        if (Pointer != nullptr && Pointer != pointer) {
            Pointer->Release();
            Pointer = pointer;
            IsAttached = true;
        }
    }

    ~ImGui_ImplD2D_ComPtr() {
        Release();
    }
};

/** @brief Font store object */
struct ImGui_ImplD2D_Fonts {
    // store texture bitmap
    ImGui_ImplD2D_ComPtr<ID2D1Bitmap> FontBitmap;
    // store texture bitmap brush
    ImGui_ImplD2D_ComPtr<ID2D1BitmapBrush> FontBitmapBrush;
};

struct ImGui_ImplD2D_Images {
};

using ImGui_ImplD2D_Factory = ID2D1Factory;
struct ImGui_ImplD2D_Data
{
    ImGui_ImplD2D_ComPtr<ImGui_ImplD2D_Factory> Factory;
    ImGui_ImplD2D_ComPtr<ImGui_ImplD2D_RenderTarget> RenderTarget;
    ImGui_ImplD2D_ComPtr<ImGui_ImplD2D_WriteFactory> WriteFactory;
    ImGui_ImplD2D_ComPtr<IWICImagingFactory> ImagingFactory;
    ImGui_ImplD2D_Fonts* Fonts;
    ImGui_ImplD2D_ComPtr<ID2D1SolidColorBrush> SolidColorBrush;
    ImGui_ImplD2D_ComPtr<ID2D1StrokeStyle> StrokeStyle;
    D2D1_GRADIENT_STOP GradientStops[2];
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
    bd->Factory.IsAttached = false;
    rendererTarget->GetFactory(bd->Factory.GetAddressOf());
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
        renderTarget = bd->RenderTarget.Pointer;
    }
    IM_ASSERT(renderTarget != nullptr && "Render target must be initialized");

    HRESULT hr = S_OK;
    if (renderTarget != bd->RenderTarget.Pointer) {
        ImGui_ImplD2D_DestroyDeviceObjects();
        bd->RenderTarget.Release();
        bd->RenderTarget.Acquire(renderTarget);
        // those this increase reference count?
        bd->Factory.Release();
        bd->Factory.IsAttached = false;
        renderTarget->GetFactory(bd->Factory.GetAddressOf());
        if (FAILED(hr)) {
            return false;
        }
        bd->SolidColorBrush.Release();
        hr = renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), bd->SolidColorBrush.GetAddressOf());
        if (FAILED(hr)) {
            return false;
        }
    }
    return ImGui_ImplD2D_CreateFontsTexture();
}

void    ImGui_ImplD2D_DestroyDeviceObjects()
{
    ImGui_ImplD2D_Data* backendData = ImGui_ImplD2D_GetBackendData();
    backendData->SolidColorBrush.Release();
    backendData->StrokeStyle.Release();

    ImGui_ImplD2D_DestroyFontsTexture();
}

bool    ImGui_ImplD2D_CreateFontsTexture() {
    ImGui_ImplD2D_Data* backendData = ImGui_ImplD2D_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();
    return true;
}

void    ImGui_ImplD2D_DestroyFontsTexture()
{
    ImGui_ImplD2D_Data* backendData = ImGui_ImplD2D_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();

    if (backendData->Fonts) {
        ///  io.Fonts->SetTexID(0);

    }
}

void     ImGui_ImplD2D_Shutdown() {
    ImGui_ImplD2D_Data* backendData = ImGui_ImplD2D_GetBackendData();
    IM_ASSERT(backendData != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplD2D_DestroyDeviceObjects();

    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;
    IM_DELETE(backendData);
}

void     ImGui_ImplD2D_NewFrame() {
    ImGui_ImplD2D_Data* bd = ImGui_ImplD2D_GetBackendData();
    IM_ASSERT(bd != nullptr && "Did you call ImGui_ImplD2D_Init()?");
    // auto detect new fonts if required

    ImGui_ImplD2D_CreateFontsTexture();
}
static float ImGui_Impl2D2_ColorMap[256] = {
    0.0f / 255.0f,
    1.0f / 255.0f,
    2.0f / 255.0f,
    3.0f / 255.0f,
    4.0f / 255.0f,
    5.0f / 255.0f,
    6.0f / 255.0f,
    7.0f / 255.0f,
    8.0f / 255.0f,
    9.0f / 255.0f,
    10.0f / 255.0f,
    11.0f / 255.0f,
    12.0f / 255.0f,
    13.0f / 255.0f,
    14.0f / 255.0f,
    15.0f / 255.0f,
    16.0f / 255.0f,
    17.0f / 255.0f,
    18.0f / 255.0f,
    19.0f / 255.0f,
    20.0f / 255.0f,
    21.0f / 255.0f,
    22.0f / 255.0f,
    23.0f / 255.0f,
    24.0f / 255.0f,
    25.0f / 255.0f,
    26.0f / 255.0f,
    27.0f / 255.0f,
    28.0f / 255.0f,
    29.0f / 255.0f,
    30.0f / 255.0f,
    31.0f / 255.0f,
    32.0f / 255.0f,
    33.0f / 255.0f,
    34.0f / 255.0f,
    35.0f / 255.0f,
    36.0f / 255.0f,
    37.0f / 255.0f,
    38.0f / 255.0f,
    39.0f / 255.0f,
    40.0f / 255.0f,
    41.0f / 255.0f,
    42.0f / 255.0f,
    43.0f / 255.0f,
    44.0f / 255.0f,
    45.0f / 255.0f,
    46.0f / 255.0f,
    47.0f / 255.0f,
    48.0f / 255.0f,
    49.0f / 255.0f,
    50.0f / 255.0f,
    51.0f / 255.0f,
    52.0f / 255.0f,
    53.0f / 255.0f,
    54.0f / 255.0f,
    55.0f / 255.0f,
    56.0f / 255.0f,
    57.0f / 255.0f,
    58.0f / 255.0f,
    59.0f / 255.0f,
    60.0f / 255.0f,
    61.0f / 255.0f,
    62.0f / 255.0f,
    63.0f / 255.0f,
    64.0f / 255.0f,
    65.0f / 255.0f,
    66.0f / 255.0f,
    67.0f / 255.0f,
    68.0f / 255.0f,
    69.0f / 255.0f,
    70.0f / 255.0f,
    71.0f / 255.0f,
    72.0f / 255.0f,
    73.0f / 255.0f,
    74.0f / 255.0f,
    75.0f / 255.0f,
    76.0f / 255.0f,
    77.0f / 255.0f,
    78.0f / 255.0f,
    79.0f / 255.0f,
    80.0f / 255.0f,
    81.0f / 255.0f,
    82.0f / 255.0f,
    83.0f / 255.0f,
    84.0f / 255.0f,
    85.0f / 255.0f,
    86.0f / 255.0f,
    87.0f / 255.0f,
    88.0f / 255.0f,
    89.0f / 255.0f,
    90.0f / 255.0f,
    91.0f / 255.0f,
    92.0f / 255.0f,
    93.0f / 255.0f,
    94.0f / 255.0f,
    95.0f / 255.0f,
    96.0f / 255.0f,
    97.0f / 255.0f,
    98.0f / 255.0f,
    99.0f / 255.0f,
    100.0f / 255.0f,
    101.0f / 255.0f,
    102.0f / 255.0f,
    103.0f / 255.0f,
    104.0f / 255.0f,
    105.0f / 255.0f,
    106.0f / 255.0f,
    107.0f / 255.0f,
    108.0f / 255.0f,
    109.0f / 255.0f,
    110.0f / 255.0f,
    111.0f / 255.0f,
    112.0f / 255.0f,
    113.0f / 255.0f,
    114.0f / 255.0f,
    115.0f / 255.0f,
    116.0f / 255.0f,
    117.0f / 255.0f,
    118.0f / 255.0f,
    119.0f / 255.0f,
    120.0f / 255.0f,
    121.0f / 255.0f,
    122.0f / 255.0f,
    123.0f / 255.0f,
    124.0f / 255.0f,
    125.0f / 255.0f,
    126.0f / 255.0f,
    127.0f / 255.0f,
    128.0f / 255.0f,
    129.0f / 255.0f,
    130.0f / 255.0f,
    131.0f / 255.0f,
    132.0f / 255.0f,
    133.0f / 255.0f,
    134.0f / 255.0f,
    135.0f / 255.0f,
    136.0f / 255.0f,
    137.0f / 255.0f,
    138.0f / 255.0f,
    139.0f / 255.0f,
    140.0f / 255.0f,
    141.0f / 255.0f,
    142.0f / 255.0f,
    143.0f / 255.0f,
    144.0f / 255.0f,
    145.0f / 255.0f,
    146.0f / 255.0f,
    147.0f / 255.0f,
    148.0f / 255.0f,
    149.0f / 255.0f,
    150.0f / 255.0f,
    151.0f / 255.0f,
    152.0f / 255.0f,
    153.0f / 255.0f,
    154.0f / 255.0f,
    155.0f / 255.0f,
    156.0f / 255.0f,
    157.0f / 255.0f,
    158.0f / 255.0f,
    159.0f / 255.0f,
    160.0f / 255.0f,
    161.0f / 255.0f,
    162.0f / 255.0f,
    163.0f / 255.0f,
    164.0f / 255.0f,
    165.0f / 255.0f,
    166.0f / 255.0f,
    167.0f / 255.0f,
    168.0f / 255.0f,
    169.0f / 255.0f,
    170.0f / 255.0f,
    171.0f / 255.0f,
    172.0f / 255.0f,
    173.0f / 255.0f,
    174.0f / 255.0f,
    175.0f / 255.0f,
    176.0f / 255.0f,
    177.0f / 255.0f,
    178.0f / 255.0f,
    179.0f / 255.0f,
    180.0f / 255.0f,
    181.0f / 255.0f,
    182.0f / 255.0f,
    183.0f / 255.0f,
    184.0f / 255.0f,
    185.0f / 255.0f,
    186.0f / 255.0f,
    187.0f / 255.0f,
    188.0f / 255.0f,
    189.0f / 255.0f,
    190.0f / 255.0f,
    191.0f / 255.0f,
    192.0f / 255.0f,
    193.0f / 255.0f,
    194.0f / 255.0f,
    195.0f / 255.0f,
    196.0f / 255.0f,
    197.0f / 255.0f,
    198.0f / 255.0f,
    199.0f / 255.0f,
    200.0f / 255.0f,
    201.0f / 255.0f,
    202.0f / 255.0f,
    203.0f / 255.0f,
    204.0f / 255.0f,
    205.0f / 255.0f,
    206.0f / 255.0f,
    207.0f / 255.0f,
    208.0f / 255.0f,
    209.0f / 255.0f,
    210.0f / 255.0f,
    211.0f / 255.0f,
    212.0f / 255.0f,
    213.0f / 255.0f,
    214.0f / 255.0f,
    215.0f / 255.0f,
    216.0f / 255.0f,
    217.0f / 255.0f,
    218.0f / 255.0f,
    219.0f / 255.0f,
    220.0f / 255.0f,
    221.0f / 255.0f,
    222.0f / 255.0f,
    223.0f / 255.0f,
    224.0f / 255.0f,
    225.0f / 255.0f,
    226.0f / 255.0f,
    227.0f / 255.0f,
    228.0f / 255.0f,
    229.0f / 255.0f,
    230.0f / 255.0f,
    231.0f / 255.0f,
    232.0f / 255.0f,
    233.0f / 255.0f,
    234.0f / 255.0f,
    235.0f / 255.0f,
    236.0f / 255.0f,
    237.0f / 255.0f,
    238.0f / 255.0f,
    239.0f / 255.0f,
    240.0f / 255.0f,
    241.0f / 255.0f,
    242.0f / 255.0f,
    243.0f / 255.0f,
    244.0f / 255.0f,
    245.0f / 255.0f,
    246.0f / 255.0f,
    247.0f / 255.0f,
    248.0f / 255.0f,
    249.0f / 255.0f,
    250.0f / 255.0f,
    251.0f / 255.0f,
    252.0f / 255.0f,
    253.0f / 255.0f,
    254.0f / 255.0f,
    255.0f / 255.0f
};

inline static D2D1_COLOR_F ImGui_ImplD2D_Color(ImU32 color) {
    constexpr float norm = 1.0f / 255.0f;
    const float a = ImGui_Impl2D2_ColorMap[color >> 24 & 0xFFu];
    const float b = ImGui_Impl2D2_ColorMap[(color >> 16) & 0xFFu];
    const float g = ImGui_Impl2D2_ColorMap[(color >> 8) & 0xFFu];
    const float r = ImGui_Impl2D2_ColorMap[(color >> 0) & 0xFFu];
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
/** @brief Return of element is a glayh

    @params RenderTarget The target of rendering
    @param bd[in]
    @param io[in]
    @param pcmd[in]

    @returns
        This function returns number of indicates to skip for it to be rendered
*/
static int ImGui_ImplD2D_IsGlyph(ID2D1RenderTarget* RendererTarget,
    ImGui_ImplD2D_Data* bd, const ImGuiIO& io,
    const ImDrawCmd* pcmd,
    const ImDrawVert* vert,
    const ImDrawIdx* idx,
    const int offset = 0) {
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
    // get glyphs metadata
    const auto& fontData = io.Fonts->Fonts.Data[font];
    auto fontGlyphs = io.Fonts->Fonts.Data[font]->Glyphs;
    auto fontScale = io.FontGlobalScale * fontData->Scale;
    auto fontSize = io.Fonts->Fonts.Data[font]->FontSize * fontScale;
    const auto top = (io.Fonts->Fonts.Data[font]->FontSize - fontData->Ascent) * fontScale;
    // Each letter is rendered as two polygons (4 vecticles/6 indicates)
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
        static HRESULT hresult = S_OK;
        static IDWriteTextFormat* textFormat = NULL;
        static IDWriteInMemoryFontFileLoader* fontLoader = NULL;
        static IDWriteFontFile* fontFile = NULL;
        static IDWriteFontFaceReference* fontFace = NULL;
        static IDWriteFontSetBuilder* fontBuilder = NULL;
        static IDWriteFontSet* fontSet = NULL;
        static IDWriteFontCollection1* fontCollection = NULL;
        hresult = bd->WriteFactory->CreateInMemoryFontFileLoader(&fontLoader);
        if (fontLoader == NULL) {
            auto configData = io.Fonts->Fonts.Data[0]->ConfigData;
            fontLoader->CreateInMemoryFontFileReference(bd->WriteFactory.Get(), configData->FontData, configData->FontDataSize, NULL, &fontFile);
            if (fontFile != NULL) {
                hresult = bd->WriteFactory->CreateFontFaceReference(fontFile, 0, DWRITE_FONT_SIMULATIONS_NONE, &fontFace);
            }
            if (fontFace == NULL) {
                // cannot continue

            }
            hresult = bd->WriteFactory->CreateFontSetBuilder(&fontBuilder);
            if (fontBuilder == NULL) {
                // cannot continue
            }

            DWRITE_FONT_PROPERTY props[] =
            {
                // We're only using names to reference fonts programmatically, so won't worry about localized names.
                { DWRITE_FONT_PROPERTY_ID_FAMILY_NAME, L"Arial", L"en-US"},
                { DWRITE_FONT_PROPERTY_ID_FULL_NAME, L"Arial", L"en-US"},
                { DWRITE_FONT_PROPERTY_ID_WEIGHT, L"400", nullptr}
            };
            hresult = fontBuilder->AddFontFaceReference(fontFace, props, ARRAYSIZE(props));
            fontBuilder->CreateFontSet(&fontSet);
            hresult = bd->WriteFactory->CreateFontCollectionFromFontSet(fontSet, &fontCollection);
        }
        if (textFormat == NULL) {

            hresult = bd->WriteFactory->CreateTextFormat(L"Arial", fontCollection, DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                fontSize,
                L"en-US",
                &textFormat);
        }
        if (SUCCEEDED(hresult)) {
            const D2D1_SIZE_U renderTargetSize = RendererTarget->GetPixelSize();
            bd->SolidColorBrush.Pointer->SetColor(ImGui_ImplD2D_Color(v0->col));
            bd->RenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

            for (size_t c = 0; c < codepointRun.size() - 1; c++) {
                ImVec2 pos = codepointPos[c];
                D2D1_RECT_F rect = D2D1::RectF(pos.x, pos.y, renderTargetSize.width, renderTargetSize.height);
#if defined(UNICODE)
                bd->RenderTarget->DrawText(codepointRun.data() + c, 1, textFormat, rect, bd->SolidColorBrush.Get());
#else
                bd->RenderTarget->DrawTextA(codepointRun.data() + c, 1, textFormat, &rect, bd->SolidColorBrush.Get());
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
    ImGui_ImplD2D_Data* backendData = ImGui_ImplD2D_GetBackendData();

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = ImVec2{ 0, 0 };         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = ImVec2{ 1, 1 };
    const float fb_width = static_cast<float>(backendData->RenderTarget->GetPixelSize().width);
    const float fb_height = static_cast<float>(backendData->RenderTarget->GetPixelSize().height);

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
                backendData->RenderTarget->PushAxisAlignedClip(clip, D2D1_ANTIALIAS_MODE_ALIASED);

                unsigned int indCount = pcmd->ElemCount;
                if (indCount == 0)
                {
                    continue;
                }
                const ImDrawVert* vert = vtx_buffer + pcmd->VtxOffset;
                const ImDrawIdx* idx = idx_buffer + pcmd->IdxOffset;
                const ImTextureID texture = pcmd->GetTexID();
                const bool isFontOrLine = texture == io.Fonts->TexID;
                int idxOffset = 0;
                pathGeometry.Release();
                geometrySink.Release();
                while (idxOffset < indCount) {
                    int skip = ImGui_ImplD2D_IsGlyph(backendData->RenderTarget.Get(), backendData, io, pcmd, vert, idx, idxOffset);
                    if (skip > 0) {
                        // print stream

                        idxOffset += skip - 3;
                        continue;
                    }

                    int polygonIndicates = 0;
                    int polygonColorsCount = 1;
                    ImDrawIdx prevIdx[3] = { idx[idxOffset + 0], idx[idxOffset + 1], idx[idxOffset + 2] };
                    ImU32 polygonColors[6] = { (vert + prevIdx[0])->col, 0x0, 0x0, 0x0, 0x0, 0x0 };
                    for (int i = idxOffset; i < indCount; i += 3) {
                        ImDrawIdx currIdx[3] = { idx[i], idx[i + 1], idx[i + 2] };
                        const bool commonIndicateTest =
                            prevIdx[0] == currIdx[0] || prevIdx[0] == currIdx[1] || prevIdx[0] == currIdx[2] ||
                            prevIdx[1] == currIdx[0] || prevIdx[1] == currIdx[1] || prevIdx[1] == currIdx[2] ||
                            prevIdx[2] == currIdx[0] || prevIdx[2] == currIdx[1] || prevIdx[2] == currIdx[2];
                        if (commonIndicateTest == false) {
                            break;
                        }
                        const ImDrawVert* verts[3] = { vert + currIdx[0], vert + currIdx[1], vert + currIdx[2] };
                        int currColCount = 0;
                        const ImU32 currCol[3] = { verts[0]->col, verts[1]->col, verts[2]->col };
                        int nextPolygonColorsCount = polygonColorsCount;
                        for (int c = 0; c < 3; c++) {
                            bool nextColor = true;
                            for (int p = 0; p < nextPolygonColorsCount; p++) {
                                if (currCol[c] == polygonColors[p]) {
                                    nextColor = false;
                                    break;
                                }
                            }
                            if (nextColor) {
                                polygonColors[nextPolygonColorsCount] = currCol[c];
                                nextPolygonColorsCount++;
                            }
                        }

                        // only triangles & quads can be renderer with more than one color
                        if (polygonIndicates > 6 && nextPolygonColorsCount > 1) {
                            break;
                        }
                        const bool isOnePoly = prevIdx[0] == currIdx[0] && prevIdx[2] == currIdx[1];
                        polygonColorsCount = nextPolygonColorsCount;
                        memcpy(&prevIdx, &currIdx, sizeof(currIdx));
                        polygonIndicates += 3;
                        if (polygonIndicates >= 3 && polygonColorsCount > 2) {
                            break;
                        }
                        if (polygonIndicates == 6 && polygonColorsCount == 2) {
                            break;
                        }

                    }
                    const int idxStart = idxOffset;
                    idxOffset += polygonIndicates;
                    // drawing
                    hr = backendData->Factory->CreatePathGeometry(&pathGeometry.Pointer);
                    if (FAILED(hr))
                    {
                        continue;
                    }
                    hr = pathGeometry.Pointer->Open(&geometrySink.Pointer);
                    if (FAILED(hr))
                    {
                        continue;
                    }
                    geometrySink.Pointer->SetFillMode(D2D1_FILL_MODE_ALTERNATE);
                    geometrySink.Pointer->SetSegmentFlags(D2D1_PATH_SEGMENT_FORCE_ROUND_LINE_JOIN);

                    D2D1_POINT_2F point;
                    for (int i = idxStart; i < idxOffset; i += 3) {
                        int o = idx[i];
                        point.x = (vert + o)->pos.x;
                        point.y = (vert + o)->pos.y;
                        geometrySink.Pointer->BeginFigure(point, D2D1_FIGURE_BEGIN_FILLED);

                        geometrySink.Pointer->AddLine(point);
                        o = idx[i + 1];
                        point.x = (vert + o)->pos.x;
                        point.y = (vert + o)->pos.y;
                        geometrySink.Pointer->AddLine(point);
                        o = idx[i + 2];
                        point.x = (vert + o)->pos.x;
                        point.y = (vert + o)->pos.y;
                        geometrySink.Pointer->AddLine(point);
                        geometrySink.Pointer->EndFigure(D2D1_FIGURE_END_CLOSED);
                    }
                    hr = geometrySink.Pointer->Close();
                    geometrySink.Release();
                    if (FAILED(hr))
                    {
                        continue;
                    }
                    const ImDrawVert* verts[4] = {
                        vert + idx[idxStart],
                        vert + idx[idxStart + 1],
                        vert + idx[idxStart + 2],
                        vert + idx[idxOffset - 1],
                    };
                    backendData->RenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

                    if (polygonColorsCount == 1) {
                        backendData->SolidColorBrush.Pointer->SetColor(ImGui_ImplD2D_Color(polygonColors[0]));
                        backendData->RenderTarget->FillGeometry(pathGeometry.Pointer, backendData->SolidColorBrush.Pointer);
                    }
                    else if (polygonColorsCount == 2)
                    {
                        const ImDrawVert* verts[4] = {
                            vert + idx[idxStart],
                            vert + idx[idxStart + 1],
                            vert + idx[idxStart + 2],
                            vert + idx[idxOffset - 1],
                        };
                        bool success = true;
                        if (verts[0]->col == verts[3]->col) {
                            success = ImGui_ImplD2D_CreateBrush(linGradBrush, backendData->GradientStops, stopsCol, linGradProps, backendData->RenderTarget.Get(),
                                verts[0]->pos, verts[1]->pos, verts[0]->col, verts[1]->col);
                        }
                        else {
                            success = ImGui_ImplD2D_CreateBrush(linGradBrush, backendData->GradientStops, stopsCol, linGradProps, backendData->RenderTarget.Get(),
                                verts[1]->pos, verts[2]->pos, verts[1]->col, verts[2]->col);
                        }
                        if (success) {
                            backendData->RenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
                            backendData->RenderTarget->FillGeometry(pathGeometry.Pointer, linGradBrush.Pointer);
                            linGradBrush.Release();
                            stopsCol.Release();
                        }
                    }
                    else if (polygonColorsCount == 3) {
                        ImVec2 middle;
                        middle.x = 0.25 * (verts[0]->pos.x + verts[1]->pos.x + verts[2]->pos.x + verts[3]->pos.x);
                        middle.y = 0.25 * (verts[0]->pos.y + verts[1]->pos.y + verts[2]->pos.y + verts[3]->pos.y);
                        backendData->RenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
                        if (ImGui_ImplD2D_CreateBrush(radGradBrush, backendData->GradientStops, stopsCol, radGradProps, backendData->RenderTarget.Get(),
                            verts[0]->pos, verts[2]->pos, verts[0]->col, verts[0]->col & 0x00FFFFFFu)) {
                            backendData->RenderTarget->FillGeometry(pathGeometry.Pointer, radGradBrush.Pointer);
                        }
                        if (ImGui_ImplD2D_CreateBrush(radGradBrush, backendData->GradientStops, stopsCol, radGradProps, backendData->RenderTarget.Get(),
                            verts[2]->pos, verts[0]->pos, verts[2]->col, verts[2]->col & 0x00FFFFFFu)) {
                            backendData->RenderTarget->FillGeometry(pathGeometry.Pointer, radGradBrush.Pointer);
                        }
                        if (ImGui_ImplD2D_CreateBrush(radGradBrush, backendData->GradientStops, stopsCol, radGradProps, backendData->RenderTarget.Get(),
                            verts[1]->pos, verts[3]->pos, verts[1]->col, verts[1]->col & 0x00FFFFFFu)) {
                            backendData->RenderTarget->FillGeometry(pathGeometry.Pointer, radGradBrush.Pointer);
                        }
                        if (polygonIndicates > 3 && ImGui_ImplD2D_CreateBrush(radGradBrush, backendData->GradientStops, stopsCol, radGradProps, backendData->RenderTarget.Get(),
                            verts[3]->pos, verts[1]->pos, verts[3]->col, verts[3]->col & 0x00FFFFFFu)) {
                            backendData->RenderTarget->FillGeometry(pathGeometry.Pointer, radGradBrush.Pointer);
                        }
                        radGradBrush.Release();
                        stopsCol.Release();
                        // only triangle rendering
                    }
                    pathGeometry.Release();
                }
                backendData->RenderTarget->PopAxisAlignedClip();
            }
        }
    }

}

#endif
#if 0
ID2D1Bitmap* ImGui_Impl2D2_CreateTexture(ID2D1RenderTarget* renderTarget, IWICImagingFactory* WICFactory, IWICBitmapSource* source) {
    ComPtr<IWICFormatConverter> pConverter = nullptr;
    HRESULT hr = S_OK;
    if (SUCCEEDED(hr))
    {
        // Convert the image format to 32bppPBGRA
        // (DXGI_FORMAT_B8G8R8A8_UNORM + D2D1_ALPHA_MODE_PREMULTIPLIED).
        hr = WICFactory->CreateFormatConverter(pConverter.GetAddressOf());
    }
    // TODO: it seems that is better option WICConvertBitmapSource
    if (SUCCEEDED(hr))
    {
        hr = pConverter->Initialize(
            source,
            GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone,
            NULL,
            0.f,
            WICBitmapPaletteTypeMedianCut
        );
    }
    ID2D1Bitmap* texture = nullptr;
    if (SUCCEEDED(hr))
    {
        //create a Direct2D bitmap from the WIC bitmap.
        hr = renderTarget->CreateBitmapFromWicBitmap(
            pConverter.Get(),
            NULL,
            &texture
        );
    }
    return texture;
}
static ID2D1Bitmap* ImGui_ImplD2D_CreateTexture(ID2D1RenderTarget* renderTarget, IWICImagingFactory* WICFactory, IWICBitmapDecoder* decoder);

static ID2D1Bitmap* ImGui_ImplD2D_CreateTexture(ID2D1RenderTarget* renderTarget, IWICImagingFactory* WICFactory, IWICBitmapDecoder* decoder) {
    ImGui_ImplD2D_ComPtr<IWICBitmapFrameDecode> pSource;
    HRESULT hr = decoder->GetFrame(0, &pSource.Pointer);
    if (FAILED(hr)) {
        return nullptr;
    }
    return ImGui_Impl2D2_CreateTexture(renderTarget, WICFactory, pSource.Pointer);
}
ID2D1Bitmap* ImGui_ImplD2D_LoadTextureRgb32(ID2D1RenderTarget* renderTarget, IWICImagingFactory* WICFactory, const void* image, int width, int height, int stride, size_t size)
{
    using Microsoft::WRL::ComPtr;
    ComPtr<IWICBitmap> raw = nullptr;
    HRESULT hr = S_OK;
    hr = WICFactory->CreateBitmapFromMemory(width, height, GUID_WICPixelFormat32bppRGBA, stride, size, (BYTE*)image, &raw);
    return ImGui_Impl2D2_CreateTexture(renderTarget, WICFactory, raw.Get());
}
ID2D1Bitmap* ImGui_ImplD2D_LoadTexture(ID2D1RenderTarget* renderTarget, IWICImagingFactory* WICFactory, const void* image, size_t size) {
    using Microsoft::WRL::ComPtr;

    ComPtr<IWICBitmapDecoder> pDecoder = nullptr;
    ComPtr<IWICStream> pStream = nullptr;
    HRESULT hr = S_OK;
    if (SUCCEEDED(hr))
    {
        // Create a WIC stream to map onto the memory.
        hr = WICFactory->CreateStream(pStream.GetAddressOf());
    }
    if (SUCCEEDED(hr))
    {
        // Initialize the stream with the memory pointer and size.
        hr = pStream->InitializeFromMemory(
            (WICInProcPointer)image,
            size
        );
    }
    if (SUCCEEDED(hr))
    {
        // Create a decoder for the stream.
        hr = WICFactory->CreateDecoderFromStream(
            pStream.Get(),
            NULL,
            WICDecodeMetadataCacheOnLoad,
            &pDecoder
        );
            }
    return ImGui_ImplD2D_CreateTexture(renderTarget, WICFactory, pDecoder.Get());
        }

#endif // 0

