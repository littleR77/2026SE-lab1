// Copyright 2026 R
// 程序入口：创建 Win32 窗口和 DirectX 11 设备，运行 Dear ImGui 主循环。
//
// 用法：
//   ticketing_app.exe                      正常运行，数据保存在 exe 旁的
//                                          ticketing_data.txt
//   ticketing_app.exe --data <文件>        使用指定的数据文件
//   ticketing_app.exe --shot <账号> <页面> <活动编号> <输出.png> [推进分钟]
//                                          不显示窗口，渲染指定页面并保存截图
#include <windows.h>
#include <d3d11.h>

#include <cstdlib>
#include <string>

#include "common/file_util.h"
#include "common/time_util.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/imgui.h"
#include "service/system.h"
#include "ui/app.h"
#include "ui/texture.h"
#include "ui/theme.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd,
                                                             UINT msg,
                                                             WPARAM wparam,
                                                             LPARAM lparam);

namespace ticketing {
namespace {

// 演示数据中的“今天”：2026-11-01 10:00。
const Time kDemoNow = MakeTime(2026, 11, 1, 10, 0);

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swap_chain = nullptr;
ID3D11RenderTargetView* g_target = nullptr;
UINT g_resize_width = 0;
UINT g_resize_height = 0;

void CreateRenderTarget() {
  ID3D11Texture2D* back = nullptr;
  g_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back));
  g_device->CreateRenderTargetView(back, nullptr, &g_target);
  back->Release();
}

void CleanupRenderTarget() {
  if (g_target != nullptr) {
    g_target->Release();
    g_target = nullptr;
  }
}

bool CreateDevice(HWND hwnd) {
  DXGI_SWAP_CHAIN_DESC desc = {};
  desc.BufferCount = 2;
  desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.BufferDesc.RefreshRate.Numerator = 60;
  desc.BufferDesc.RefreshRate.Denominator = 1;
  desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.OutputWindow = hwnd;
  desc.SampleDesc.Count = 1;
  desc.Windowed = TRUE;
  desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0,
                                      D3D_FEATURE_LEVEL_10_0};
  D3D_FEATURE_LEVEL level;
  HRESULT result = D3D11CreateDeviceAndSwapChain(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
      D3D11_SDK_VERSION, &desc, &g_swap_chain, &g_device, &level, &g_context);
  if (result == DXGI_ERROR_UNSUPPORTED) {
    // 没有可用显卡时使用 WARP 软件渲染。
    result = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2,
        D3D11_SDK_VERSION, &desc, &g_swap_chain, &g_device, &level,
        &g_context);
  }
  if (result != S_OK) return false;
  CreateRenderTarget();
  return true;
}

void CleanupDevice() {
  CleanupRenderTarget();
  if (g_swap_chain != nullptr) g_swap_chain->Release();
  if (g_context != nullptr) g_context->Release();
  if (g_device != nullptr) g_device->Release();
  g_swap_chain = nullptr;
  g_context = nullptr;
  g_device = nullptr;
}

LRESULT WINAPI WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) return true;
  switch (msg) {
    case WM_SIZE:
      if (wparam == SIZE_MINIMIZED) return 0;
      g_resize_width = LOWORD(lparam);
      g_resize_height = HIWORD(lparam);
      return 0;
    case WM_SYSCOMMAND:
      if ((wparam & 0xfff0) == SC_KEYMENU) return 0;  // 屏蔽 Alt 菜单
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

ui::Screen ScreenByName(const std::string& name) {
  if (name == "plaza") return ui::Screen::kPlaza;
  if (name == "detail") return ui::Screen::kDetail;
  if (name == "tickets") return ui::Screen::kTickets;
  if (name == "messages") return ui::Screen::kMessages;
  if (name == "checkin") return ui::Screen::kCheckin;
  if (name == "activities") return ui::Screen::kMyActivities;
  if (name == "edit") return ui::Screen::kEdit;
  if (name == "manage") return ui::Screen::kManage;
  if (name == "review") return ui::Screen::kReview;
  if (name == "platform") return ui::Screen::kPlatform;
  return ui::Screen::kLogin;
}

// 截图模式的参数。
struct ShotOptions {
  bool enabled = false;
  std::string account;
  std::string screen;
  Id activity_id = 0;
  std::string output;
  Time advance = 0;
};

int Run() {
  std::string data_path = ExecutableDir() + "\\ticketing_data.txt";
  ShotOptions shot;
  for (int i = 1; i < __argc; ++i) {
    const std::string arg = __argv[i];
    if (arg == "--data" && i + 1 < __argc) {
      data_path = __argv[++i];
    } else if (arg == "--shot" && i + 4 < __argc) {
      shot.enabled = true;
      shot.account = __argv[++i];
      if (shot.account == "guest") shot.account.clear();
      shot.screen = __argv[++i];
      shot.activity_id = std::atoll(__argv[++i]);
      shot.output = __argv[++i];
      if (i + 1 < __argc && __argv[i + 1][0] != '-') {
        shot.advance = std::atoll(__argv[++i]);
      }
    }
  }

  System system(kDemoNow);
  const Status opened = system.Open(data_path);
  if (!opened.ok()) {
    MessageBoxA(nullptr, opened.message().c_str(), "无法打开数据文件",
                MB_ICONERROR);
    return 1;
  }
  if (shot.advance > 0) system.AdvanceClock(shot.advance);

  ImGui_ImplWin32_EnableDpiAwareness();
  const float scale =
      shot.enabled ? 1.0f
                   : ImGui_ImplWin32_GetDpiScaleForMonitor(
                         MonitorFromPoint(POINT{0, 0},
                                          MONITOR_DEFAULTTOPRIMARY));
  WNDCLASSEXW wc = {sizeof(wc), CS_CLASSDC, WindowProc, 0, 0,
                    GetModuleHandleW(nullptr), nullptr, nullptr, nullptr,
                    nullptr, L"ActivityTicketing", nullptr};
  RegisterClassExW(&wc);
  RECT rect = {0, 0, static_cast<LONG>(1280 * scale),
               static_cast<LONG>(800 * scale)};
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
  HWND hwnd = CreateWindowW(wc.lpszClassName, L"活动现场 · 活动票务与报名系统",
                            WS_OVERLAPPEDWINDOW, 80, 60,
                            rect.right - rect.left, rect.bottom - rect.top,
                            nullptr, nullptr, wc.hInstance, nullptr);
  if (!CreateDevice(hwnd)) {
    CleanupDevice();
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 1;
  }
  if (!shot.enabled) {
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;  // 不生成 imgui.ini
  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX11_Init(g_device, g_context);
  ui::LoadFonts();
  ui::ApplyTheme(scale);

  ID3D11ShaderResourceView* mascot = nullptr;
  int mascot_width = 0, mascot_height = 0;
  if (ui::LoadTexture(g_device, ExecutableDir() + "\\mascot.png", &mascot,
                      &mascot_width, &mascot_height)) {
    ui::SetMascotTexture(reinterpret_cast<ImTextureID>(mascot), mascot_width,
                         mascot_height);
  }

  ui::App app(&system);
  if (shot.enabled &&
      !app.DebugOpen(shot.account, ScreenByName(shot.screen),
                     shot.activity_id)) {
    return 2;
  }

  bool done = false;
  int frame = 0;
  int exit_code = 0;
  while (!done) {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
      if (msg.message == WM_QUIT) done = true;
    }
    if (done) break;
    if (g_resize_width != 0 && g_resize_height != 0) {
      CleanupRenderTarget();
      g_swap_chain->ResizeBuffers(0, g_resize_width, g_resize_height,
                                  DXGI_FORMAT_UNKNOWN, 0);
      g_resize_width = g_resize_height = 0;
      CreateRenderTarget();
    }
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    app.Frame(io.DisplaySize);
    ImGui::Render();
    const float clear[4] = {0.36f, 0.62f, 0.94f, 1.0f};
    g_context->OMSetRenderTargets(1, &g_target, nullptr);
    g_context->ClearRenderTargetView(g_target, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    // 截图模式：等几帧让自动布局稳定后保存画面并退出。
    if (shot.enabled && ++frame == 6) {
      exit_code = ui::SaveBackBuffer(g_device, g_context, g_swap_chain,
                                     shot.output)
                      ? 0
                      : 3;
      done = true;
    }
    g_swap_chain->Present(1, 0);
  }

  if (!shot.enabled) system.Persist();
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
  if (mascot != nullptr) mascot->Release();
  CleanupDevice();
  DestroyWindow(hwnd);
  UnregisterClassW(wc.lpszClassName, wc.hInstance);
  return exit_code;
}

}  // namespace
}  // namespace ticketing

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  return ticketing::Run();
}
