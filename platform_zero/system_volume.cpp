#include "system_volume.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;

namespace bitmap16::Zero {

int adjustSystemVolume(bool increase) {
  FILE* pipe = popen("LC_ALL=C wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null", "r");
  if (pipe == nullptr) return -1;
  char line[128] = {};
  const bool read = fgets(line, sizeof(line), pipe) != nullptr;
  const int readStatus = pclose(pipe);
  float current = 0.0f;
  if (!read || readStatus != 0 || std::sscanf(line, "Volume: %f", &current) != 1 ||
      !std::isfinite(current) || current < 0.0f || current > 1.0f) return -1;

  const int percent = static_cast<int>(current * 100.0f + 0.5f);
  const int next = increase ? (percent < 95 ? percent + 5 : 100)
                            : (percent > 5 ? percent - 5 : 0);
  // Allow a full-width integer, '%' and the terminator even when GCC cannot
  // infer the 0–100 range from the floating-point validation above.
  char volume[16];
  std::snprintf(volume, sizeof(volume), "%d%%", next);
  char* const arguments[] = {
      const_cast<char*>("wpctl"), const_cast<char*>("set-volume"),
      const_cast<char*>("-l"), const_cast<char*>("1.0"),
      const_cast<char*>("@DEFAULT_AUDIO_SINK@"), volume, nullptr};
  pid_t child = 0;
  if (posix_spawnp(&child, "wpctl", nullptr, nullptr, arguments, environ) != 0) return -1;
  int status = 0;
  pid_t result;
  do { result = waitpid(child, &status, 0); } while (result < 0 && errno == EINTR);
  if (result != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0) return -1;
  return next;
}

}  // namespace bitmap16::Zero
