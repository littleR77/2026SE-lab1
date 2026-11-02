// Copyright 2026 R
#include "common/file_util.h"

#include <windows.h>

#include <cstdio>
#include <string>

namespace ticketing {
namespace {

std::wstring Widen(const std::string& text) {
  if (text.empty()) return std::wstring();
  const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                       static_cast<int>(text.size()),
                                       nullptr, 0);
  std::wstring wide(size, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      &wide[0], size);
  return wide;
}

std::string Narrow(const std::wstring& wide) {
  if (wide.empty()) return std::string();
  const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                       static_cast<int>(wide.size()),
                                       nullptr, 0, nullptr, nullptr);
  std::string text(size, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                      &text[0], size, nullptr, nullptr);
  return text;
}

}  // namespace

bool ReadWholeFile(const std::string& path, std::string* content) {
  FILE* file = _wfopen(Widen(path).c_str(), L"rb");
  if (file == nullptr) return false;
  std::string result;
  char buffer[8192];
  size_t count = 0;
  while ((count = fread(buffer, 1, sizeof(buffer), file)) > 0) {
    result.append(buffer, count);
  }
  const bool ok = ferror(file) == 0;
  fclose(file);
  if (ok) *content = result;
  return ok;
}

bool WriteWholeFile(const std::string& path, const std::string& content) {
  const std::wstring target = Widen(path);
  const std::wstring temp = target + L".tmp";
  FILE* file = _wfopen(temp.c_str(), L"wb");
  if (file == nullptr) return false;
  const bool written =
      fwrite(content.data(), 1, content.size(), file) == content.size();
  const bool closed = fclose(file) == 0;
  if (!written || !closed) {
    _wremove(temp.c_str());
    return false;
  }
  return MoveFileExW(temp.c_str(), target.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

bool FileExists(const std::string& path) {
  const DWORD attributes = GetFileAttributesW(Widen(path).c_str());
  return attributes != INVALID_FILE_ATTRIBUTES &&
         (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool RemoveFile(const std::string& path) {
  return DeleteFileW(Widen(path).c_str()) != 0;
}

std::string ExecutableDir() {
  wchar_t buffer[MAX_PATH];
  const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
  std::wstring path(buffer, length);
  const size_t slash = path.find_last_of(L"\\/");
  if (slash != std::wstring::npos) path.resize(slash);
  return Narrow(path);
}

}  // namespace ticketing
