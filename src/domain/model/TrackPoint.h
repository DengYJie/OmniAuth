#pragma once

#include <QtGlobal>

/**
 * @brief 滑块拖动轨迹采样点
 */
struct TrackPoint {
  int x;
  int y;
  qint64 timestamp;
};
