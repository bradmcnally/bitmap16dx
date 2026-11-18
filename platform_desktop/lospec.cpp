#include "lospec.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>

#include <curl/curl.h>
#include "picojson.h"
#include "core/palette.h"
#include "lospec_config.h"

namespace bitmap16 {
namespace Lospec {
namespace {

constexpr std::size_t kMaximumResponseBytes = 2 * 1024 * 1024;

bool validSlug(const std::string& slug) {
  return !slug.empty() && slug.size() <= 128 &&
      std::all_of(slug.begin(), slug.end(), [](unsigned char ch) {
        return (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') || ch == '-';
      });
}

bool validKey(const std::string& key) {
  return !key.empty() && key.size() <= 512 &&
      std::all_of(key.begin(), key.end(), [](unsigned char ch) {
        return ch > 32 && ch < 127;
      });
}

std::string stringField(const picojson::value& value, const char* key) {
  const auto& field = value.get(key);
  return field.is<std::string>() ? field.get<std::string>() : "";
}

std::string displayName(std::string text) {
  // The bitmap font covers ASCII; bound labels and remove control characters.
  for (char& ch : text) {
    if (static_cast<unsigned char>(ch) < 32 ||
        static_cast<unsigned char>(ch) > 126) ch = '?';
  }
  return text.substr(0, 80);
}

bool parsePalette(const picojson::value& value, Palette& palette) {
  if (!value.is<picojson::object>()) return false;
  Palette result;
  result.slug = stringField(value, "slug");
  result.title = displayName(stringField(value, "title"));
  const auto& likes = value.get("likes");
  const auto& colors = value.get("colors");
  if (!validSlug(result.slug) || result.title.empty() ||
      !likes.is<double>() || !std::isfinite(likes.get<double>()) ||
      likes.get<double>() < 0 ||
      likes.get<double>() > std::numeric_limits<int>::max() ||
      std::floor(likes.get<double>()) != likes.get<double>() ||
      !colors.is<picojson::array>()) return false;
  result.likes = static_cast<int>(likes.get<double>());
  const auto& array = colors.get<picojson::array>();
  if (array.size() != 4 && array.size() != 8 && array.size() != 16) {
    return false;
  }
  std::string hexText;
  for (std::size_t index = 0; index < array.size(); ++index) {
    if (!array[index].is<std::string>()) return false;
    const auto& hex = array[index].get<std::string>();
    if (hex.size() != 6 ||
        !std::all_of(hex.begin(), hex.end(), [](unsigned char ch) {
          return std::isxdigit(ch) != 0;
        })) return false;
    result.hex[index] = hex;
    hexText += hex + '\n';
  }
  bitmap16::Palette::Parsed parsed;
  if (!bitmap16::Palette::parseLospecHex(
          hexText.data(), hexText.size(), parsed)) return false;
  result.size = parsed.size;
  std::copy(std::begin(parsed.colors), std::end(parsed.colors),
            result.colors.begin());
  const auto& user = value.get("user");
  if (user.is<picojson::object>()) {
    result.author = displayName(stringField(user, "name"));
  }
  palette = std::move(result);
  return true;
}

std::string readFile(const std::filesystem::path& path, std::size_t limit) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return {};
  std::string text;
  char buffer[4096];
  while (input.read(buffer, sizeof(buffer)) || input.gcount() > 0) {
    text.append(buffer, static_cast<std::size_t>(input.gcount()));
    if (text.size() > limit) return {};
  }
  return input.bad() ? std::string{} : text;
}

bool writeFile(
    const std::filesystem::path& path, const std::string& text,
    bool privateFile = false) {
  const auto temporary = std::filesystem::path(path.string() + ".tmp");
  std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  std::error_code error;
  if (privateFile) {
    std::filesystem::permissions(
        temporary,
        std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace, error);
  }
  if (!error) output.write(text.data(), static_cast<std::streamsize>(text.size()));
  output.close();
  if (!output || error) {
    std::filesystem::remove(temporary, error);
    return false;
  }
  std::filesystem::rename(temporary, path, error);
  if (!error) return true;
  std::filesystem::remove(temporary, error);
  return false;
}

std::filesystem::path palettePath(
    const std::filesystem::path& root, const std::string& slug) {
  return root / "palettes" / ("lospec-" + slug + ".hex");
}

std::size_t receiveBody(char* data, std::size_t size, std::size_t count, void* out) {
  auto& body = *static_cast<std::string*>(out);
  const std::size_t bytes = size * count;
  if (bytes > kMaximumResponseBytes - body.size()) return 0;
  body.append(data, bytes);
  return bytes;
}

int transferProgress(void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
  return static_cast<const std::atomic_bool*>(context)->load() ? 1 : 0;
}

}  // namespace

bool parsePage(const std::string& json, Page& page) {
  picojson::value root;
  if (json.size() > kMaximumResponseBytes ||
      !picojson::parse(root, json).empty() ||
      !root.is<picojson::object>() ||
      !root.get("data").is<picojson::array>() ||
      !root.get("meta").is<picojson::object>() ||
      !root.get("meta").get("hasMore").is<bool>()) return false;
  Page result;
  result.hasMore = root.get("meta").get("hasMore").get<bool>();
  for (const auto& value : root.get("data").get<picojson::array>()) {
    Palette palette;
    if (parsePalette(value, palette)) result.palettes.push_back(std::move(palette));
  }
  page = std::move(result);
  return true;
}

std::vector<Palette> matching(
    const std::vector<Palette>& palettes, int minimumLikes,
    const std::vector<std::string>& saved) {
  std::vector<Palette> result;
  for (const auto& palette : palettes) {
    if (palette.likes < minimumLikes ||
        std::find(saved.begin(), saved.end(), palette.slug) != saved.end()) continue;
    result.push_back(palette);
    if (result.size() == kMaximumMatches) break;
  }
  return result;
}

Result refresh(
    int minimumLikes, const std::vector<std::string>& saved,
    const std::atomic_bool& cancel, const FetchPage& fetch) {
  Result result;
  for (int pageIndex = 0; pageIndex < kMaximumPages; ++pageIndex) {
    if (cancel.load()) { result.error = "Refresh cancelled"; return result; }
    const Response response = fetch(pageIndex * kPageSize);
    ++result.calls;
    Page page;
    if (!response.error.empty() || !parsePage(response.body, page)) {
      result.error = response.error.empty() ? "Invalid Lospec response" : response.error;
      return result;
    }
    for (auto& palette : page.palettes) {
      const auto duplicate = std::find_if(
          result.palettes.begin(), result.palettes.end(),
          [&](const Palette& entry) { return entry.slug == palette.slug; });
      if (duplicate == result.palettes.end()) {
        result.palettes.push_back(std::move(palette));
      }
    }
    if (!page.hasMore || matching(result.palettes, minimumLikes, saved).size() >=
            kMaximumMatches) break;
  }
  return result;
}

static Response download(
    const std::string& url, const std::string& apiKey,
    const std::atomic_bool& cancel) {
  Response result;
  static const bool initialized = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
  if (!initialized) { result.error = "Network setup failed"; return result; }
  CURL* curl = curl_easy_init();
  if (curl == nullptr) { result.error = "Network setup failed"; return result; }
  const std::string authorization = "Authorization: Bearer " + apiKey;
  curl_slist* headers = apiKey.empty() ? nullptr :
      curl_slist_append(nullptr, authorization.c_str());
  if (!apiKey.empty() && headers == nullptr) {
    curl_easy_cleanup(curl);
    result.error = "Network setup failed";
    return result;
  }
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "BitMap16DX/0.8 Lospec palettes");
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receiveBody);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, transferProgress);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cancel);
  const CURLcode code = curl_easy_perform(curl);
  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  if (cancel.load()) result.error = "Refresh cancelled";
  else if (code != CURLE_OK) result.error = "Connection failed; cached kept";
  else if (status == 401 || status == 403) result.error = "Lospec access denied";
  else if (status == 429) result.error = "Lospec limit reached; try later";
  else if (status != 200) result.error = "Lospec unavailable; cached kept";
  return result;
}

Response downloadPage(
    int offset, const std::string& apiKey, const std::atomic_bool& cancel) {
  if (!validKey(apiKey)) return {"", "Lospec key missing in build"};
  return download("https://api.lospec.com/api/v1/palettes?"
      "sort=-publishedAt&minColors=4&maxColors=16&limit=100&format=expanded&offset=" +
      std::to_string(offset), apiKey, cancel);
}

bool usesSharedFeed() { return kSharedFeedUrl[0] != '\0'; }

Result downloadLatest(
    int minimumLikes, const std::vector<std::string>& saved,
    const std::string& apiKey, const std::atomic_bool& cancel) {
  if (usesSharedFeed()) {
    const auto response = download(kSharedFeedUrl, "", cancel);
    Result result;
    result.calls = 1;
    Page page;
    if (!response.error.empty() || !parsePage(response.body, page)) {
      result.error = response.error.empty() ? "Invalid palette feed" : response.error;
    } else {
      result.palettes = std::move(page.palettes);
    }
    return result;
  }
  return refresh(minimumLikes, saved, cancel, [&](int offset) {
    return downloadPage(offset, apiKey, cancel);
  });
}

std::string readApiKey(const std::filesystem::path& root) {
  const char* environment = std::getenv("BITMAP16_LOSPEC_API_KEY");
  std::string key = environment == nullptr ? "" : environment;
  if (validKey(key)) return key;
  key = readFile(root / "lospec-key.txt", 514);
  while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) key.pop_back();
  if (validKey(key)) return key;
  return validKey(kBundledApiKey) ? kBundledApiKey : "";
}

bool writeApiKey(const std::filesystem::path& root, const std::string& key) {
  return validKey(key) && writeFile(root / "lospec-key.txt", key + '\n', true);
}

int readMinimumLikes(const std::filesystem::path& root) {
  std::ifstream input(root / "lospec-likes.txt");
  int likes = 10;
  return input >> likes && likes >= 0 && likes <= 1000 ? likes : 10;
}

bool writeMinimumLikes(const std::filesystem::path& root, int likes) {
  return likes >= 0 && likes <= 1000 &&
      writeFile(root / "lospec-likes.txt", std::to_string(likes) + '\n');
}

std::vector<Palette> loadCache(const std::filesystem::path& root) {
  Page page;
  parsePage(readFile(root / "lospec-cache.json", kMaximumResponseBytes), page);
  return page.palettes;
}

bool saveCache(
    const std::filesystem::path& root, const std::vector<Palette>& palettes) {
  picojson::array data;
  for (const auto& palette : palettes) {
    picojson::object object;
    object["slug"] = picojson::value(palette.slug);
    object["title"] = picojson::value(palette.title);
    object["likes"] = picojson::value(static_cast<double>(palette.likes));
    picojson::object user;
    user["name"] = picojson::value(palette.author);
    object["user"] = picojson::value(user);
    picojson::array colors;
    for (int i = 0; i < palette.size; ++i) colors.emplace_back(palette.hex[i]);
    object["colors"] = picojson::value(colors);
    data.emplace_back(object);
  }
  picojson::object document;
  document["data"] = picojson::value(data);
  picojson::object meta;
  meta["hasMore"] = picojson::value(false);
  document["meta"] = picojson::value(meta);
  return writeFile(root / "lospec-cache.json", picojson::value(document).serialize());
}

bool isSaved(const std::filesystem::path& root, const Palette& palette) {
  std::error_code error;
  return validSlug(palette.slug) &&
      std::filesystem::is_regular_file(palettePath(root, palette.slug), error);
}

std::vector<std::string> savedSlugs(const std::filesystem::path& root) {
  std::vector<std::string> result;
  std::error_code error;
  for (const auto& entry : std::filesystem::directory_iterator(root / "palettes", error)) {
    if (error) break;
    const auto stem = entry.path().stem().string();
    if (entry.path().extension() == ".hex" && stem.compare(0, 7, "lospec-") == 0 &&
        entry.is_regular_file(error)) {
      result.push_back(stem.substr(7));
    }
  }
  return result;
}

bool savePalette(const std::filesystem::path& root, const Palette& palette) {
  if (!validSlug(palette.slug) ||
      (palette.size != 4 && palette.size != 8 && palette.size != 16)) return false;
  if (isSaved(root, palette)) return true;
  std::string text = "// Lospec palette\n// Name: " + displayName(palette.title) +
      "\n// Author: " + displayName(palette.author) +
      "\n// Likes: " + std::to_string(palette.likes) + '\n';
  for (int i = 0; i < palette.size; ++i) text += palette.hex[i] + '\n';
  return writeFile(palettePath(root, palette.slug), text);
}

}  // namespace Lospec
}  // namespace bitmap16
