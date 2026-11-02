// Copyright 2026 R
// DirectX 11 贴图工具：读取 PNG 作为贴图，以及把当前画面保存为 PNG。
#ifndef UI_TEXTURE_H_
#define UI_TEXTURE_H_

#include <d3d11.h>

#include <string>

namespace ticketing {
namespace ui {

// 读取 PNG 并创建着色器资源视图。成功时 *out 需要调用者 Release。
bool LoadTexture(ID3D11Device* device, const std::string& path,
                 ID3D11ShaderResourceView** out, int* width, int* height);

// 把交换链的后台缓冲保存为 PNG（用于生成运行截图）。
bool SaveBackBuffer(ID3D11Device* device, ID3D11DeviceContext* context,
                    IDXGISwapChain* swap_chain, const std::string& path);

}  // namespace ui
}  // namespace ticketing

#endif  // UI_TEXTURE_H_
