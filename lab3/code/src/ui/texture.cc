// Copyright 2026 R
#include "ui/texture.h"

#include <string>
#include <vector>

#include "common/file_util.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

namespace ticketing {
namespace ui {
namespace {

// stb_image_write 的回调：把编码好的 PNG 追加到字符串中。
void AppendToString(void* context, void* data, int size) {
  static_cast<std::string*>(context)->append(static_cast<const char*>(data),
                                             size);
}

}  // namespace

bool LoadTexture(ID3D11Device* device, const std::string& path,
                 ID3D11ShaderResourceView** out, int* width, int* height) {
  std::string bytes;
  if (!ReadWholeFile(path, &bytes)) return false;
  int w = 0, h = 0, channels = 0;
  unsigned char* pixels = stbi_load_from_memory(
      reinterpret_cast<const stbi_uc*>(bytes.data()),
      static_cast<int>(bytes.size()), &w, &h, &channels, 4);
  if (pixels == nullptr) return false;

  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = w;
  desc.Height = h;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  D3D11_SUBRESOURCE_DATA data = {};
  data.pSysMem = pixels;
  data.SysMemPitch = desc.Width * 4;
  ID3D11Texture2D* texture = nullptr;
  const HRESULT created = device->CreateTexture2D(&desc, &data, &texture);
  stbi_image_free(pixels);
  if (FAILED(created)) return false;

  D3D11_SHADER_RESOURCE_VIEW_DESC view = {};
  view.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  view.Texture2D.MipLevels = 1;
  const HRESULT viewed = device->CreateShaderResourceView(texture, &view, out);
  texture->Release();
  if (FAILED(viewed)) return false;
  *width = w;
  *height = h;
  return true;
}

bool SaveBackBuffer(ID3D11Device* device, ID3D11DeviceContext* context,
                    IDXGISwapChain* swap_chain, const std::string& path) {
  ID3D11Texture2D* back = nullptr;
  if (FAILED(swap_chain->GetBuffer(0, IID_PPV_ARGS(&back)))) return false;
  D3D11_TEXTURE2D_DESC desc;
  back->GetDesc(&desc);
  // 复制到 CPU 可读的暂存贴图再读取像素。
  desc.Usage = D3D11_USAGE_STAGING;
  desc.BindFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  desc.MiscFlags = 0;
  ID3D11Texture2D* staging = nullptr;
  if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging))) {
    back->Release();
    return false;
  }
  context->CopyResource(staging, back);
  back->Release();
  D3D11_MAPPED_SUBRESOURCE mapped;
  if (FAILED(context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
    staging->Release();
    return false;
  }
  const int width = static_cast<int>(desc.Width);
  const int height = static_cast<int>(desc.Height);
  std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
  for (int y = 0; y < height; ++y) {
    const unsigned char* row =
        static_cast<const unsigned char*>(mapped.pData) + y * mapped.RowPitch;
    for (int x = 0; x < width * 4; x += 4) {
      unsigned char* dst = &pixels[(static_cast<size_t>(y) * width) * 4 + x];
      dst[0] = row[x];
      dst[1] = row[x + 1];
      dst[2] = row[x + 2];
      dst[3] = 255;  // 窗口画面不需要透明度
    }
  }
  context->Unmap(staging, 0);
  staging->Release();
  std::string png;
  if (!stbi_write_png_to_func(AppendToString, &png, width, height, 4,
                              pixels.data(), width * 4)) {
    return false;
  }
  return WriteWholeFile(path, png);
}

}  // namespace ui
}  // namespace ticketing
