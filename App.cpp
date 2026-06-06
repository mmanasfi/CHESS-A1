#include "App.h"
#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <windowsx.h>
#include <Magic.h>
#include <bitset>
#include <Engine.h>
#include <chrono>


// fine cause we're only using one app at a time (obviously)

UINT App::g_ResizeWidth = 0;
UINT App::g_ResizeHeight = 0;

float border_padding = 0;
float square_width = 0, square_height = 0;

int selected_piece_index = -1;
MoveList last_cached_moves;
uint64_t current_move_mask = 0;

bool App::Init()
{
    // Make process DPI aware and obtain main monitor scale
    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    // Create application window
    wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"CHESS A1", nullptr };
    ::RegisterClassExW(&wc);
    hwnd = ::CreateWindowW(wc.lpszClassName, L"CHESS A1", WS_OVERLAPPEDWINDOW, 100, 100, (int)(1280 * main_scale), (int)(800 * main_scale), nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 0;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    // Setup Dear ImGui style: ADD MORE SHIT HERE LATER ON
    ImGui::StyleColorsDark();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts
    // - If fonts are not explicitly loaded, Dear ImGui will call AddFontDefault() to select an embedded font: either AddFontDefaultVector() or AddFontDefaultBitmap().
    //   This selection is based on (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) reaching a small threshold.
    // - You can load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - If a file cannot be loaded, AddFont functions will return a nullptr. Please handle those errors in your code (e.g. use an assertion, display an error and quit).
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType for higher quality font rendering.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //style.FontSizeBase = 20.0f;
    //io.Fonts->AddFontDefaultVector();
    //io.Fonts->AddFontDefaultBitmap();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);

    int dw = 0, dh = 0; //dummy width and height

    return
        LoadTextureFromFile("sprites/white-pawn.png", &textures[0], &dw, &dh) &&
        LoadTextureFromFile("sprites/white-rook.png", &textures[1], &dw, &dh) &&
        LoadTextureFromFile("sprites/white-knight.png", &textures[2], &dw, &dh) &&
        LoadTextureFromFile("sprites/white-bishop.png", &textures[3], &dw, &dh) &&
        LoadTextureFromFile("sprites/white-queen.png", &textures[4], &dw, &dh) &&
        LoadTextureFromFile("sprites/white-king.png", &textures[5], &dw, &dh) &&
        LoadTextureFromFile("sprites/black-pawn.png", &textures[6], &dw, &dh) &&
        LoadTextureFromFile("sprites/black-rook.png", &textures[7], &dw, &dh) &&
        LoadTextureFromFile("sprites/black-knight.png", &textures[8], &dw, &dh) &&
        LoadTextureFromFile("sprites/black-bishop.png", &textures[9], &dw, &dh) &&
        LoadTextureFromFile("sprites/black-queen.png", &textures[10], &dw, &dh) &&
        LoadTextureFromFile("sprites/black-king.png", &textures[11], &dw, &dh) &&
        LoadTextureFromFile("sprites/none.png", &textures[12], &dw, &dh)
   ;
}

void App::Update(Board& board)
{
    // Poll and handle messages (inputs, window resize, etc.)
            // See the WndProc() function below for our to dispatch events to the Win32 backend.
    MSG msg;
    while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
        if(msg.message == WM_LBUTTONDOWN)
        {
            int xPos = GET_X_LPARAM(msg.lParam);
            int yPos = GET_Y_LPARAM(msg.lParam);
            int square_index_clicked = ((int)((yPos - border_padding) / (square_height))) * 8 + ((int)((xPos - border_padding)) / (square_width));
            if (selected_piece_index == -1)
            {           
                if (board.get_piece(square_index_clicked)!=12 && !(board.white_to_move && (board.side_total_occ[1] & (1ULL << square_index_clicked))) && !(!board.white_to_move && (board.side_total_occ[0] & (1ULL << square_index_clicked))))
                {
                    selected_piece_index = square_index_clicked;
                    board.generate_moves(last_cached_moves);
                    current_move_mask = 0;
                    for (int i = 0; i < 256; i++)
                    {
                        if (last_cached_moves.moves[i] != 0 && (last_cached_moves.moves[i] & 0x003f) == selected_piece_index)
                            current_move_mask |= (1ULL << ((last_cached_moves.moves[i] & 0x0fc0) >> 6));
                    }
                }
            }
            else
            {
                if (current_move_mask & (1ULL << square_index_clicked))
                {
                    for (int i = 0; i < last_cached_moves.count; i++)
                    {
                        uint16_t curMove = last_cached_moves.moves[i];
                        if ((curMove & 0x003f) == selected_piece_index && (((curMove & 0x0fc0) >> 6)) == square_index_clicked)
                        {
                            board.make_move(curMove);
                            break;
                        }
                    }

                    current_move_mask = 0;
                    selected_piece_index = -1;
                }
                else if(board.get_piece(square_index_clicked) != 12 && !(board.white_to_move && (board.side_total_occ[1] & (1ULL << square_index_clicked))) && !(!board.white_to_move && (board.side_total_occ[0] & (1ULL << square_index_clicked))))
                {
                    selected_piece_index = square_index_clicked;
                    board.generate_moves(last_cached_moves);
                    current_move_mask = 0;
                    for (int i = 0; i < 256; i++)
                    {
                        if (last_cached_moves.moves[i] != 0 && (last_cached_moves.moves[i] & 0x003f) == selected_piece_index)
                            current_move_mask |= (1ULL << ((last_cached_moves.moves[i] & 0x0fc0) >> 6));
                    }
                }
                else
                {
                    current_move_mask = 0;
                    selected_piece_index = -1;
                }

            }
        }
        if (msg.message == WM_RBUTTONDOWN)
        {
            static Engine e;

            auto start = std::chrono::high_resolution_clock::now();

            uint16_t best_next_move = e.minmax_best_move(board, 6);

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            if (best_next_move == UINT16_MAX)
            {
                std::cout << "Whoops! someone is checkmated!" << "\n";
            }
            else
            {
                std::cout << "Move: " << (best_next_move & 0x003f) << " To: " << ((best_next_move & 0x0fc0) >> 6) <<" "<<e.eval(board) <<" "<<duration.count()<<"ms" << "\n";
                board.make_move(best_next_move);
            }
            
        }
        if (msg.message == WM_QUIT)
            done = true;
    }
    // Handle window being minimized or screen locked
    if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
    {
        ::Sleep(10);
        return;
    }
    g_SwapChainOccluded = false;

    // Handle window resize (we don't resize directly in the WM_SIZE handler)
    if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
    {
        CleanupRenderTarget();
        g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
        g_ResizeWidth = g_ResizeHeight = 0;
        CreateRenderTarget();
    }

    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // all goes here --

    static UINT32 border_color = IM_COL32(12,12,12, 255);
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 viewportSize = viewport->Size;
    static float board_width_total_ocp = 0.7;
    border_padding = viewportSize.y * 0.025;
    static float border_rouding = 115.f;
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    
    //drawing border
    drawList->AddRectFilled(ImVec2(0, 0), ImVec2(border_padding, viewportSize.y), border_color, border_rouding, 0);// left
    drawList->AddRectFilled(ImVec2(0, 0), ImVec2(board_width_total_ocp* viewportSize.x, border_padding), border_color, border_rouding, 0); // top
    drawList->AddRectFilled(ImVec2(0, viewportSize.y - border_padding), ImVec2(board_width_total_ocp * viewportSize.x, viewportSize.y), border_color, border_rouding, 0); // bottom
    drawList->AddRectFilled(ImVec2(board_width_total_ocp * viewportSize.x - border_padding, 0), ImVec2(board_width_total_ocp * viewportSize.x, viewportSize.y), border_color, border_rouding, 0); // right
    // drawing board + pieces (later)
    static UINT32 square1_color = IM_COL32(66, 66, 66, 255);
    static UINT32 square2_color = IM_COL32(28, 28, 28, 255);

    square_width = (board_width_total_ocp * viewportSize.x - border_padding * 2) / 8.f;
    square_height = (viewportSize.y - border_padding * 2) / 8.f;
    UINT32 square_draw_color;

    
    for (int row = 0; row < 8; row++)
    {
        if (row % 2 == 0)
            square_draw_color = square1_color;
        else
            square_draw_color = square2_color;
        for (int col = 0; col < 8; col++)
        {
            int square_index = row * 8 + col;
            if (selected_piece_index!=-1 && (current_move_mask & (1ULL << square_index)))
            {
                drawList->AddRectFilled(ImVec2(border_padding + square_width * col, border_padding + square_height * row), ImVec2(border_padding + square_width * col + square_width, border_padding + square_height * row + square_height), IM_COL32(255, 12, 12, 50), 0, 0);
            }
            else
            {
                if (square_index == selected_piece_index)
                    drawList->AddRectFilled(ImVec2(border_padding + square_width * col, border_padding + square_height * row), ImVec2(border_padding + square_width * col + square_width, border_padding + square_height * row + square_height), IM_COL32(40, 12, 12, 100), 0, 0);
                else
                    drawList->AddRectFilled(ImVec2(border_padding + square_width * col, border_padding + square_height * row), ImVec2(border_padding + square_width * col + square_width, border_padding + square_height * row + square_height), square_draw_color, 0, 0);
            }  
            drawList->AddImage((ImTextureID)textures[board.get_piece(row*8+col)], ImVec2(border_padding + square_width * col, border_padding + square_height * row), ImVec2(border_padding + square_width * col + square_width, border_padding + square_height * row + square_height));
            drawList->AddText(NULL, 21.0f, ImVec2(border_padding + square_width * col + square_width - 25, border_padding + square_height * row + square_height - 20), IM_COL32(255, 0, 0, 255), std::to_string(row * 8 + col).c_str());            
            if (square_draw_color == square2_color)
                square_draw_color = square1_color;
            else
                square_draw_color = square2_color;
        }
    }
    // Rendering
    ImGui::Render();
    const float clear_color_with_alpha[4] = { background_color.x * background_color.w, background_color.y * background_color.w, background_color.z * background_color.w, background_color.w };
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // Present
    HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
    g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);

}

void App::Shutdown()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
}

bool App::isDone()
{
    return done;
}

// Helper functions

bool App::CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    // This is a basic setup. Optimally could use e.g. DXGI_SWAP_EFFECT_FLIP_DISCARD and handle fullscreen mode differently. See #8979 for suggestions.
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void App::CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void App::CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void App::CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI App::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;

    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;

    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool App::LoadTextureFromMemory(const void* data, size_t data_size, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height)
{
    // Load from disk into a raw RGBA buffer
    int image_width = 0;
    int image_height = 0;
    unsigned char* image_data = stbi_load_from_memory((const unsigned char*)data, (int)data_size, &image_width, &image_height, NULL, 4);
    if (image_data == NULL)
        return false;

    // Create texture
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = image_width;
    desc.Height = image_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    ID3D11Texture2D* pTexture = NULL;
    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = image_data;
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;
    g_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);

    // Create texture view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    g_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, out_srv);
    pTexture->Release();

    *out_width = image_width;
    *out_height = image_height;
    stbi_image_free(image_data);

    return true;
}

bool App::LoadTextureFromFile(const char* file_name, ID3D11ShaderResourceView * *out_srv, int* out_width, int* out_height)
{
    FILE* f;
    fopen_s(&f, file_name, "rb");
    if (f == NULL)
        return false;
    fseek(f, 0, SEEK_END);
    size_t file_size = (size_t)ftell(f);
    if (file_size == -1)
        return false;
    fseek(f, 0, SEEK_SET);
    void* file_data = IM_ALLOC(file_size);
    fread(file_data, 1, file_size, f);
    fclose(f);
    bool ret = LoadTextureFromMemory(file_data, file_size, out_srv, out_width, out_height);
    IM_FREE(file_data);
    return ret;
}