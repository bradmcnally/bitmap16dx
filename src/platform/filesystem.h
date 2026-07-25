#pragma once

#include <cstddef>
#include <cstdint>

namespace Filesystem {

struct FileInfo {
  char name[128] = {};
  std::size_t size = 0;
  bool isDirectory = false;
};

using ListCallback = bool (*)(const FileInfo& info, void* context);

bool init();
bool isAvailable();
bool exists(const char* path);
bool createDirectory(const char* path);
bool remove(const char* path);
std::size_t fileSize(const char* path);
bool readFile(
    const char* path,
    uint8_t* output,
    std::size_t outputCapacity,
    std::size_t& bytesRead);
bool writeFile(const char* path, const uint8_t* data, std::size_t size);
bool listDirectory(
    const char* path,
    ListCallback callback,
    void* context);

}  // namespace Filesystem
