#pragma once

#include <array>
#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace bitmap16 {
namespace Lospec {

constexpr int kPageSize = 100;
constexpr int kMaximumPages = 3;
constexpr int kMaximumMatches = 20;

struct Palette {
  std::string slug;
  std::string title;
  std::string author;
  std::array<std::string, 16> hex = {};
  std::array<uint16_t, 16> colors = {};
  uint8_t size = 0;
  int likes = 0;
};

struct Page {
  std::vector<Palette> palettes;
  bool hasMore = false;
};

struct Response {
  std::string body;
  std::string error;
};

struct Result {
  std::vector<Palette> palettes;
  std::string error;
  int calls = 0;
};

// The transport seam keeps pagination, validation and filtering testable offline.
using FetchPage = std::function<Response(int offset)>;
bool parsePage(const std::string& json, Page& page);
std::vector<Palette> matching(
    const std::vector<Palette>& palettes,
    int minimumLikes,
    const std::vector<std::string>& savedSlugs);
Result refresh(
    int minimumLikes,
    const std::vector<std::string>& savedSlugs,
    const std::atomic_bool& cancel,
    const FetchPage& fetch);
Response downloadPage(
    int offset, const std::string& apiKey, const std::atomic_bool& cancel);
bool usesSharedFeed();
Result downloadLatest(
    int minimumLikes, const std::vector<std::string>& savedSlugs,
    const std::string& apiKey, const std::atomic_bool& cancel);

std::string readApiKey(const std::filesystem::path& root);
bool writeApiKey(const std::filesystem::path& root, const std::string& key);
int readMinimumLikes(const std::filesystem::path& root);
bool writeMinimumLikes(const std::filesystem::path& root, int likes);
std::vector<Palette> loadCache(const std::filesystem::path& root);
bool saveCache(
    const std::filesystem::path& root, const std::vector<Palette>& palettes);
bool isSaved(const std::filesystem::path& root, const Palette& palette);
std::vector<std::string> savedSlugs(const std::filesystem::path& root);
bool savePalette(const std::filesystem::path& root, const Palette& palette);

}  // namespace Lospec
}  // namespace bitmap16
