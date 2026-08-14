#pragma once

#include <array>
#include <cmath>
#include <numeric>
#include <ranges>
#include <string>
#include <vector>

#include <QString>

#include "domain/model/TrackPoint.h"

/**
 * @brief 滑块拖动轨迹的 10 维行为特征向量
 *
 * 由 BehaviorTracker::extractFeatures() 从原始 (x, y, timestamp) 时序序列中提取，
 * 用于后续 MLP 模型判定真人/机器人。
 */
struct TrajectoryFeature {
  float totalDuration;      // 1. 总体拖动耗时 (ms)
  float maxSpeedX;          // 2. X轴最大速度 (px/ms)
  float avgSpeedX;          // 3. X轴平均速度
  float varDisplacementY;   // 4. Y轴位移方差（真人手抖 vs 机器直线）
  float reversalCount;      // 5. 轨迹折返次数（X轴速度 < 0 的次数）
  float pauseCount;         // 6. 停顿次数（速度 ≈ 0 的点数）
  float maxAccelX;          // 7. X轴最大加速度
  float speedEntropy;       // 8. 速度变化熵值（分布均匀性）
  float straightnessRatio;  // 9. 直线度比（X净位移 / 实际路径总长度）
  float endSlowdownRatio;   // 10. 末段减速比（最后20%速度 / 整体平均速度）

  [[nodiscard]] std::array<float, 10> toArray() const {
    return {totalDuration, maxSpeedX,    avgSpeedX,        varDisplacementY,
            reversalCount, pauseCount,   maxAccelX,        speedEntropy,
            straightnessRatio,           endSlowdownRatio};
  }
};

/**
 * @brief AI 行为风控特征提取器
 *
 * 利用 C++23 Ranges (std::views::slide, std::views::transform)
 * 以声明式管道从滑块拖动轨迹提取 10 维行为特征，零中间容器。
 */
class BehaviorTracker {
 public:
  /**
   * @brief 从原始轨迹提取 10 维行为特征
   * @param trajectory 滑块拖动过程中采集的 (x, y, timestamp) 序列
   * @return 10 维特征向量
   */
  static TrajectoryFeature extractFeatures(
      const std::vector<TrackPoint>& trajectory);

#ifdef COLLECT_BEHAVIOR_DATA
  /**
   * @brief 将特征向量转换为 CSV 行（用于训练数据采集）
   * @param feat 特征向量
   * @param label 标签 (1=真人, 0=机器人)
   * @return CSV 格式行字符串
   */
  static QString featureToCsvLine(const TrajectoryFeature& feat, int label);

  /**
   * @brief CSV 表头
   */
  static QString csvHeader();

  /**
   * @brief 将特征追加写入 CSV 文件
   * @param filePath CSV 文件路径
   * @param feat 特征向量
   * @param label 标签
   * @return 写入是否成功
   */
  static bool appendToCsv(const QString& filePath,
                          const TrajectoryFeature& feat, int label);
#endif
};
