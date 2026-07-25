#include "platform/filesystem.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#include <cstring>

namespace {

constexpr uint8_t kSckPin = 40;
constexpr uint8_t kMisoPin = 39;
constexpr uint8_t kMosiPin = 14;
constexpr uint8_t kChipSelectPin = 12;
constexpr uint32_t kSpiFrequency = 25000000;

bool initialized = false;
bool available = false;

}  // namespace

bool Filesystem::init() {
  if (initialized) {
    return available;
  }
  initialized = true;

  delay(100);
  SPI.begin(kSckPin, kMisoPin, kMosiPin, kChipSelectPin);

  for (uint8_t attempt = 0; attempt < 3; ++attempt) {
    if (SD.begin(kChipSelectPin, SPI, kSpiFrequency) &&
        SD.cardType() != CARD_NONE) {
      available = true;
      return true;
    }
    if (attempt < 2) {
      delay(100);
    }
  }

  available = false;
  return false;
}

bool Filesystem::isAvailable() {
  return available && SD.cardType() != CARD_NONE;
}

bool Filesystem::exists(const char* path) {
  return isAvailable() && path != nullptr && SD.exists(path);
}

bool Filesystem::createDirectory(const char* path) {
  if (!isAvailable() || path == nullptr) {
    return false;
  }
  return SD.exists(path) || SD.mkdir(path);
}

bool Filesystem::remove(const char* path) {
  return isAvailable() && path != nullptr && SD.remove(path);
}

std::size_t Filesystem::fileSize(const char* path) {
  if (!isAvailable() || path == nullptr) {
    return 0;
  }
  File file = SD.open(path, FILE_READ);
  if (!file || file.isDirectory()) {
    return 0;
  }
  const std::size_t size = file.size();
  file.close();
  return size;
}

bool Filesystem::readFile(
    const char* path,
    uint8_t* output,
  std::size_t outputCapacity,
  std::size_t& bytesRead) {
  bytesRead = 0;
  if (!isAvailable() || path == nullptr) {
    return false;
  }

  File file = SD.open(path, FILE_READ);
  const std::size_t expected = file ? file.size() : 0;
  if (!file || file.isDirectory() || expected > outputCapacity ||
      (expected > 0 && output == nullptr)) {
    if (file) {
      file.close();
    }
    return false;
  }

  if (expected > 0) {
    bytesRead = file.read(output, expected);
  }
  file.close();
  return bytesRead == expected;
}

bool Filesystem::writeFile(
    const char* path,
    const uint8_t* data,
    std::size_t size) {
  if (!isAvailable() || path == nullptr || (size > 0 && data == nullptr)) {
    return false;
  }

  if (SD.exists(path) && !SD.remove(path)) {
    return false;
  }

  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    return false;
  }
  const std::size_t bytesWritten = size > 0 ? file.write(data, size) : 0;
  file.close();
  return bytesWritten == size;
}

bool Filesystem::listDirectory(
    const char* path,
    ListCallback callback,
    void* context) {
  if (!isAvailable() || path == nullptr || callback == nullptr) {
    return false;
  }

  File directory = SD.open(path);
  if (!directory || !directory.isDirectory()) {
    return false;
  }

  File entry = directory.openNextFile();
  while (entry) {
    FileInfo info;
    const char* name = entry.name();
    if (name != nullptr) {
      std::strncpy(info.name, name, sizeof(info.name) - 1);
      info.name[sizeof(info.name) - 1] = '\0';
    }
    info.size = entry.size();
    info.isDirectory = entry.isDirectory();

    const bool shouldContinue = callback(info, context);
    entry.close();
    if (!shouldContinue) {
      break;
    }
    entry = directory.openNextFile();
  }

  directory.close();
  return true;
}
