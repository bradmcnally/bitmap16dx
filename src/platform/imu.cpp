#include "platform/imu.h"

#include <M5Cardputer.h>

void IMU::init() {
  M5.Imu.begin();
}

bool IMU::isAvailable() {
  return M5.Imu.isEnabled();
}

bitmap16::AccelerationSample IMU::readAcceleration() {
  if (!isAvailable()) {
    return {};
  }

  M5.Imu.update();
  const auto data = M5.Imu.getImuData();
  return {
      data.accel.x,
      data.accel.y,
      data.accel.z,
      true,
  };
}
