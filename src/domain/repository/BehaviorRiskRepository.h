#pragma once

#include <expected>
#include <functional>
#include <string>

struct TrajectoryFeature; // 避免完整引入 BehaviorTracker.h

/**
 * @brief 领域仓库接口：行为风控评估契约
 */
class BehaviorRiskRepository {
 public:
  virtual ~BehaviorRiskRepository() = default;

  // 异步回调类型：成功返回 float (置信度)，失败返回 string (错误原因)
  using Callback = std::function<void(std::expected<float, std::string>)>;

  /**
   * @brief 初始化数据源（预加载模型或建立连接等）
   */
  virtual bool init() = 0;

  /**
   * @brief 异步评估轨迹特征
   * @param feature 10 维轨迹特征
   * @param callback 评估结果回调
   */
  virtual void evaluateAsync(const TrajectoryFeature& feature, Callback callback) = 0;
};
