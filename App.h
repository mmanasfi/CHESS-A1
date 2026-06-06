#pragma once
#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx11.h>
#include <imgui/imgui_impl_win32.h>
#include <Windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <iostream>
#include <Board.h>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

class App
{
public:
	bool Init();
	void Update(Board& board);
	void Shutdown();
	bool isDone();
private:
	HWND hwnd = nullptr;
	WNDCLASSEXW wc = {};
	ID3D11Device* g_pd3dDevice = nullptr;
	ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
	IDXGISwapChain* g_pSwapChain = nullptr;
	bool g_SwapChainOccluded = false;
	ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
	bool done = false;
	ID3D11ShaderResourceView* textures[13];
	ImVec4 background_color = ImVec4(0.1f, 0.1f, 0.1f, 1.00f);
	static UINT g_ResizeWidth , g_ResizeHeight;
	bool CreateDeviceD3D(HWND hWnd);
	void CleanupDeviceD3D();
	void CreateRenderTarget();
	void CleanupRenderTarget();
	bool LoadTextureFromMemory(const void* data, size_t data_size, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height);
	bool LoadTextureFromFile(const char* file_name, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height);
	static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	
};


