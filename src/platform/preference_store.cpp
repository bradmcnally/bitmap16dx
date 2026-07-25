#include "platform/preference_store.h"

#include <Preferences.h>

namespace {

constexpr const char* kNamespace = "bitmap16dx";

}  // namespace

bool PreferenceStore::readBool(const char* key, bool fallback) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) {
    return fallback;
  }
  const bool value = preferences.getBool(key, fallback);
  preferences.end();
  return value;
}

uint8_t PreferenceStore::readUInt8(const char* key, uint8_t fallback) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) {
    return fallback;
  }
  const uint8_t value = preferences.getUChar(key, fallback);
  preferences.end();
  return value;
}

uint32_t PreferenceStore::readUInt32(const char* key, uint32_t fallback) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) {
    return fallback;
  }
  const uint32_t value = preferences.getULong(key, fallback);
  preferences.end();
  return value;
}

std::size_t PreferenceStore::readBytes(
    const char* key,
    void* output,
    std::size_t outputSize) {
  if (output == nullptr || outputSize == 0) {
    return 0;
  }

  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) {
    return 0;
  }
  const std::size_t bytesRead =
      preferences.getBytes(key, output, outputSize);
  preferences.end();
  return bytesRead;
}

bool PreferenceStore::writeBool(const char* key, bool value) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    return false;
  }
  const bool success = preferences.putBool(key, value) == sizeof(uint8_t);
  preferences.end();
  return success;
}

bool PreferenceStore::writeUInt8(const char* key, uint8_t value) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    return false;
  }
  const bool success = preferences.putUChar(key, value) == sizeof(value);
  preferences.end();
  return success;
}

bool PreferenceStore::writeUInt32(const char* key, uint32_t value) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    return false;
  }
  const bool success = preferences.putULong(key, value) == sizeof(value);
  preferences.end();
  return success;
}

bool PreferenceStore::writeBytes(
    const char* key,
    const void* data,
    std::size_t size) {
  if (data == nullptr || size == 0) {
    return false;
  }

  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    return false;
  }
  const bool success = preferences.putBytes(key, data, size) == size;
  preferences.end();
  return success;
}

bool PreferenceStore::remove(const char* key) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    return false;
  }
  const bool success = preferences.remove(key);
  preferences.end();
  return success;
}
