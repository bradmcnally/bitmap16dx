#include "imu.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

#include "core/editor.h"
#include "core/settings_view.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

struct Fixture {
  std::filesystem::path root;
  Fixture() {
    std::string pattern = (std::filesystem::temp_directory_path() /
                           "bitmap16dx-imu-XXXXXX").string();
    char* created = mkdtemp(pattern.data());
    require(created != nullptr, "create isolated IIO fixture");
    root = created;
  }
  ~Fixture() { std::filesystem::remove_all(root); }
  std::filesystem::path device(const char* index, const char* name) {
    const auto path = root / index;
    std::filesystem::create_directory(path);
    write(path / "name", name);
    return path;
  }
  static void write(const std::filesystem::path& path, const std::string& value) {
    require(static_cast<bool>(std::ofstream(path) << value << '\n'), "write fixture");
  }
  static void acceleration(const std::filesystem::path& path,
                           const char* x, const char* y, const char* z) {
    write(path / "in_accel_x_raw", x);
    write(path / "in_accel_y_raw", y);
    write(path / "in_accel_z_raw", z);
  }
};

void testMissingSensor() {
  Fixture fixture;
  fixture.device("iio:device0", "m5ioe1");
  fixture.device("iio:device1", "bmm150");
  bitmap16::Zero::Imu imu;
  require(!imu.open(fixture.root), "Lite must not detect a nonexistent BMI270");
  require(!imu.isAvailable() && !imu.readAcceleration().available,
          "missing sensor must stay unavailable");
  require(imu.waitTimeout(1000, true) == -1 && !imu.pollShake(1000, true),
          "missing sensor must not poll or undo");
  require(!imu.error().empty(), "unavailable sensor needs a diagnostic");
  // These are the same capability arguments used by the Zero settings UI.
  const int withoutImu = bitmap16::SettingsView::itemCount(false, false, imu.isAvailable());
  const int withImu = bitmap16::SettingsView::itemCount(false, false, true);
  require(withoutImu + 1 == withImu, "Lite settings must omit the shake toggle");
  bitmap16::Settings settings;
  settings.shakeUndoEnabled = true;
  for (int cursor = 0; cursor < withoutImu; ++cursor) {
    bitmap16::SettingsView::State state;
    state.cursor = cursor;
    const auto action = bitmap16::SettingsView::activate(
        state, settings, false, false, imu.isAvailable());
    require(action != bitmap16::SettingsView::Action::ShakeUndoChanged,
            "Lite navigation must skip the hidden shake setting");
  }
  require(settings.shakeUndoEnabled, "Lite must retain a previously saved shake preference");
  require(!imu.open(fixture.root / "missing"), "missing IIO subsystem must not throw");
}

void testReadAndShakeUndo() {
  Fixture fixture;
  fixture.device("iio:device0", "bmm150");
  // IIO device numbers vary, and the sysfs bus directory contains symlinks.
  const auto hardware = fixture.device("hardware", "bmi27");
  std::filesystem::create_directory_symlink(hardware, fixture.root / "iio:device7");
  Fixture::write(hardware / "in_accel_scale", "0.002394");
  Fixture::acceleration(hardware, "0", "0", "4096");
  bitmap16::Zero::Imu imu;
  require(imu.open(fixture.root), "discover M5Stack bmi27 through a sysfs symlink");
  const auto gravity = imu.readAcceleration();
  require(gravity.available && gravity.x == 0 && gravity.y == 0 &&
              std::fabs(gravity.z - 1.0f) < 0.001f,
          "convert Linux m/s² into g before shake detection");
  require(imu.waitTimeout(1000, true) == 0, "first sample must not wait for a key");
  require(!imu.pollShake(1000, true), "stationary gravity must not undo");
  require(imu.waitTimeout(1000, true) == 10 && imu.waitTimeout(1007, true) == 3,
          "idle input wait must wake for the next acceleration sample");

  bitmap16::Editor editor;
  bitmap16::Sketch blank;
  editor.reset(blank);
  require(editor.draw(), "prepare first undo step");
  editor.setCursor(1, 0);
  require(editor.draw(), "prepare second undo step");
  Fixture::acceleration(hardware, "-28672", "0", "0");
  require(!imu.pollShake(1009, true), "busy keyboard loop must respect sampling cadence");
  require(imu.pollShake(1010, true) && editor.undo(), "physical shake must undo the last edit");
  require(editor.sketch().pixels[0][0] == 1 && editor.sketch().pixels[0][1] == 0,
          "shake must undo exactly one history step");
  require(!imu.pollShake(1509, true), "shake cooldown must suppress repeated undo");
  require(!imu.pollShake(1510, true), "sample timing must still hold during cooldown");
  require(imu.pollShake(1519, true) && editor.undo(), "shake must work after the ADV cooldown");
  require(editor.sketch().isEmpty && editor.redo(), "shake undo must retain normal redo history");
  require(!imu.pollShake(3000, false) && imu.waitTimeout(3000, false) == -1,
          "disabled setting and non-Canvas views must not sample or wake");
  require(imu.pollShake(3001, true), "reenabling must resume sampling immediately");

  Fixture::write(hardware / "in_accel_y_raw", "broken");
  require(!imu.readAcceleration().available && !imu.pollShake(4000, true),
          "failed axis read must not reuse a previous shake");
  Fixture::acceleration(hardware, "0", "0", "4096");
  require(imu.readAcceleration().available && !imu.pollShake(4010, true),
          "transient read failure must recover without a false shake");
  Fixture::write(hardware / "in_accel_scale", "0.004788");
  Fixture::acceleration(hardware, "0", "14336", "0");
  require(imu.pollShake(4500, true), "reread scale if another application changes the range");

  require(imu.open(fixture.root), "reinitialize sensor");
  require(imu.pollShake(0xfffffffau, true), "shake before clock rollover");
  require(imu.waitTimeout(3, true) == 1 && !imu.pollShake(3, true),
          "sample deadline must survive clock rollover");
  require(!imu.pollShake(4, true), "cooldown must survive clock rollover");
  require(imu.pollShake(494, true), "cooldown must expire across clock rollover");
}

void testRangeAndInvalidReadings() {
  Fixture fixture;
  const auto device = fixture.device("iio:device2", "bmi270");
  Fixture::acceleration(device, "0", "0", "4096");
  Fixture::write(device / "in_accel_scale", "0.000598");
  bitmap16::Zero::Imu imu;
  require(!imu.open(fixture.root), "±2 g alone cannot implement the ADV's >6 g threshold");
  Fixture::write(device / "in_accel_scale_available", "0.004788 0.000598 0.002394 0.001197");
  require(imu.open(fixture.root), "select an advertised ±8 g range when writable");
  require(std::fabs(imu.readAcceleration().z - 1.0f) < 0.001f, "prefer ±8 g over ±16 g");
  for (const char* invalid : {"0", "-1", "nan", "inf", "garbage", "0.002394 garbage"}) {
    Fixture::write(device / "in_accel_scale", invalid);
    require(!imu.readAcceleration().available, "invalid scale must never create a shake");
  }
  Fixture::write(device / "in_accel_scale", "0.002394");
  for (const char* invalid : {"32768", "-32769", "1.5", "99 garbage", ""}) {
    Fixture::write(device / "in_accel_z_raw", invalid);
    require(!imu.readAcceleration().available, "invalid raw sample must never create a shake");
  }
  std::filesystem::remove(device / "in_accel_z_raw");
  require(!imu.open(fixture.root), "incomplete accelerometer must not expose a settings toggle");
  require(!imu.isAvailable(), "failed reopening must clear the old device");
}

}  // namespace

int main() {
  testMissingSensor();
  testReadAndShakeUndo();
  testRangeAndInvalidReadings();
  std::cout << "Zero IMU: discovery, Lite settings, units, shake undo, timing, and failures passed\n";
}
