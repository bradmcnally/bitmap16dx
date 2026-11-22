#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace bitmap16::Zero {
// Opening a USB HID interface can block even with O_NONBLOCK. Remember rejected
// interfaces until their device node changes or disappears, rather than opening
// them again during every hot-plug scan.
class KeyboardDiscovery {
 public:
  void beginScan() { for (auto& entry : rejected_) entry.seen = false; }
  bool shouldProbe(const std::string& path, uint64_t device, uint64_t inode) {
    for (auto& entry : rejected_) {
      if (entry.path != path) continue;
      entry.seen = true;
      return entry.device != device || entry.inode != inode;
    }
    return true;
  }
  void reject(const std::string& path, uint64_t device, uint64_t inode) {
    for (auto& entry : rejected_) {
      if (entry.path == path) {
        entry = {path, device, inode, true};
        return;
      }
    }
    rejected_.push_back({path, device, inode, true});
  }
  void endScan() {
    rejected_.erase(std::remove_if(rejected_.begin(), rejected_.end(),
        [](const Entry& entry) { return !entry.seen; }), rejected_.end());
  }
 private:
  struct Entry { std::string path; uint64_t device; uint64_t inode; bool seen; };
  std::vector<Entry> rejected_;
};
}  // namespace bitmap16::Zero
