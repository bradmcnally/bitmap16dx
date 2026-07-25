#pragma once

#include <cstddef>
#include <cstdint>

namespace PreferenceStore {

bool readBool(const char* key, bool fallback);
uint8_t readUInt8(const char* key, uint8_t fallback);
uint32_t readUInt32(const char* key, uint32_t fallback);
std::size_t readBytes(
    const char* key,
    void* output,
    std::size_t outputSize);

bool writeBool(const char* key, bool value);
bool writeUInt8(const char* key, uint8_t value);
bool writeUInt32(const char* key, uint32_t value);
bool writeBytes(const char* key, const void* data, std::size_t size);
bool remove(const char* key);

}  // namespace PreferenceStore
