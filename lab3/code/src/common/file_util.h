// Copyright 2026 R
// 文件读写工具。路径一律使用 UTF-8，内部转换为 Windows 宽字符路径，
// 所以放在中文目录下也能正常读写。
#ifndef COMMON_FILE_UTIL_H_
#define COMMON_FILE_UTIL_H_

#include <string>

namespace ticketing {

// 读取整个文件。文件不存在或读取失败时返回 false。
bool ReadWholeFile(const std::string& path, std::string* content);
// 先写入 path.tmp，再替换 path，避免程序中途退出留下半个文件。
bool WriteWholeFile(const std::string& path, const std::string& content);
bool FileExists(const std::string& path);
bool RemoveFile(const std::string& path);
// 当前可执行文件所在目录（UTF-8，末尾不带分隔符）。
std::string ExecutableDir();

}  // namespace ticketing

#endif  // COMMON_FILE_UTIL_H_
