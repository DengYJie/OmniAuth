#include "BehaviorTracker.h"

#include <algorithm>
#include <cmath>
#include <ranges>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTextStream>

TrajectoryFeature BehaviorTracker::extractFeatures(
    const std::vector<TrackPoint>& trajectory) {
  TrajectoryFeature feat{};

  if (trajectory.size() < 3) {
    // 数据点不足，返回零特征（将被模型判为异常）
    return feat;
  }

  const size_t n = trajectory.size();

  // 1. 总体拖动耗时 (ms)
  feat.totalDuration = static_cast<float>(trajectory.back().timestamp -
                                          trajectory.front().timestamp);

  // 速度序列
  // 使用 slide(2) 对轨迹相邻点做滑动窗口，声明式计算 X 速度
  auto velocitiesX_view =
      trajectory | std::views::slide(2) |
      std::views::transform([](auto window) -> double {
        auto it = window.begin();
        const auto& p0 = *it;
        const auto& p1 = *std::next(it);
        double dt = static_cast<double>(p1.timestamp - p0.timestamp);
        return dt > 0.0 ? static_cast<double>(p1.x - p0.x) / dt : 0.0;
      });

  // 物化速度序列（后续需要多次遍历：统计、熵、末段分析）
  std::vector<double> velocitiesX(velocitiesX_view.begin(),
                                  velocitiesX_view.end());

  if (velocitiesX.empty()) return feat;

  // 2. X轴最大速度
  auto absVelocities =
      velocitiesX |
      std::views::transform([](double v) { return std::abs(v); });
  feat.maxSpeedX =
      static_cast<float>(*std::ranges::max_element(absVelocities));

  // 3. X轴平均速度
  double sumVx = std::accumulate(velocitiesX.begin(), velocitiesX.end(), 0.0);
  feat.avgSpeedX = static_cast<float>(sumVx / velocitiesX.size());

  // 4. Y轴位移方差
  // 使用 std::views::transform 提取 y 分量
  auto yValues =
      trajectory | std::views::transform([](const TrackPoint& p) {
        return static_cast<double>(p.y);
      });
  double yMean = 0.0;
  for (auto y : yValues) yMean += y;
  yMean /= static_cast<double>(n);

  double yVariance = 0.0;
  for (auto y : yValues) {
    double diff = y - yMean;
    yVariance += diff * diff;
  }
  yVariance /= static_cast<double>(n);
  feat.varDisplacementY = static_cast<float>(yVariance);

  // 5. 轨迹折返次数
  // X轴速度 < 0 的次数（真人偶尔回拽修正，机器人单调递增）
  feat.reversalCount = static_cast<float>(
      std::ranges::count_if(velocitiesX, [](double v) { return v < 0.0; }));

  // 6. 停顿次数
  // 速度接近 0 的点数 (|v| < 0.005 px/ms)
  constexpr double kPauseThreshold = 0.005;
  feat.pauseCount = static_cast<float>(std::ranges::count_if(
      velocitiesX,
      [](double v) { return std::abs(v) < kPauseThreshold; }));

  // 7. X轴最大加速度
  // 对速度序列再做一次 slide(2) 计算加速度
  if (velocitiesX.size() >= 2) {
    auto accelerationsX_view =
        velocitiesX | std::views::slide(2) |
        std::views::transform([&](auto window) -> double {
          auto it = window.begin();
          double v0 = *it;
          double v1 = *std::next(it);
          // 使用轨迹中对应时间段的 dt
          return std::abs(v1 - v0);
        });

    std::vector<double> accels(accelerationsX_view.begin(),
                               accelerationsX_view.end());
    if (!accels.empty()) {
      feat.maxAccelX =
          static_cast<float>(*std::ranges::max_element(accels));
    }
  }

  // 8. 速度变化熵值
  // 将速度分桶后计算 Shannon 熵（衡量分布均匀性）
  {
    constexpr int kBuckets = 10;
    std::array<int, kBuckets> hist{};
    double vMin = *std::ranges::min_element(velocitiesX);
    double vMax = *std::ranges::max_element(velocitiesX);
    double range = vMax - vMin;

    if (range > 1e-9) {
      for (double v : velocitiesX) {
        int idx = static_cast<int>((v - vMin) / range * (kBuckets - 1));
        idx = std::clamp(idx, 0, kBuckets - 1);
        hist[idx]++;
      }

      double entropy = 0.0;
      double total = static_cast<double>(velocitiesX.size());
      for (int count : hist) {
        if (count > 0) {
          double p = static_cast<double>(count) / total;
          entropy -= p * std::log2(p);
        }
      }
      feat.speedEntropy = static_cast<float>(entropy);
    }
  }

  // 9. 直线度比
  // X净位移 / 实际路径总长度 (值 ≈ 1 = 完美直线，机器人特征)
  {
    double netDisplacement =
        std::abs(static_cast<double>(trajectory.back().x - trajectory.front().x));

    // 实际路径总长度
    auto segmentLengths_view =
        trajectory | std::views::slide(2) |
        std::views::transform([](auto window) -> double {
          auto it = window.begin();
          const auto& p0 = *it;
          const auto& p1 = *std::next(it);
          double dx = p1.x - p0.x;
          double dy = p1.y - p0.y;
          return std::sqrt(dx * dx + dy * dy);
        });

    double totalPath = 0.0;
    for (auto len : segmentLengths_view) totalPath += len;

    feat.straightnessRatio =
        (totalPath > 1e-6)
            ? static_cast<float>(netDisplacement / totalPath)
            : 1.0f;
  }

  // 10. 末段减速比
  // 最后20%速度的平均值 / 整体平均速度
  {
    size_t tailStart = velocitiesX.size() * 4 / 5;
    if (tailStart < velocitiesX.size() && std::abs(feat.avgSpeedX) > 1e-6f) {
      double tailSum = 0.0;
      size_t tailCount = 0;
      for (size_t i = tailStart; i < velocitiesX.size(); ++i) {
        tailSum += velocitiesX[i];
        ++tailCount;
      }
      double tailAvg = tailSum / static_cast<double>(tailCount);
      feat.endSlowdownRatio =
          static_cast<float>(tailAvg / static_cast<double>(feat.avgSpeedX));
    } else {
      feat.endSlowdownRatio = 1.0f;
    }
  }

  return feat;
}

#ifdef COLLECT_BEHAVIOR_DATA

QString BehaviorTracker::csvHeader() {
  return QStringLiteral(
      "totalDuration,maxSpeedX,avgSpeedX,varDisplacementY,reversalCount,"
      "pauseCount,maxAccelX,speedEntropy,straightnessRatio,"
      "endSlowdownRatio,label");
}

QString BehaviorTracker::featureToCsvLine(const TrajectoryFeature& feat,
                                          int label) {
  auto arr = feat.toArray();
  QStringList parts;
  for (float v : arr) {
    parts.append(QString::number(static_cast<double>(v), 'f', 4));
  }
  parts.append(QString::number(label));
  return parts.join(',');
}

bool BehaviorTracker::appendToCsv(const QString& filePath,
                                  const TrajectoryFeature& feat, int label) {
  QFile file(filePath);

  // 如果文件不存在，先创建目录并写入表头
  bool needHeader = !file.exists() || file.size() == 0;

  QDir().mkpath(QFileInfo(filePath).absolutePath());

  if (!file.open(QIODevice::Append | QIODevice::Text)) {
    qWarning() << "BehaviorTracker: Failed to open CSV file:" << filePath;
    return false;
  }

  QTextStream stream(&file);
  if (needHeader) {
    stream << csvHeader() << "\n";
  }
  stream << featureToCsvLine(feat, label) << "\n";
  file.close();

  qDebug() << "BehaviorTracker: Appended feature to" << filePath;
  return true;
}

#endif  // COLLECT_BEHAVIOR_DATA

