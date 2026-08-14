#pragma once

#include "domain/repository/BehaviorRiskRepository.h"

/**
 * @brief 远程网络风控数据源（桩代码）
 *
 * 将 10 维轨迹特征打包为 JSON 发送给远程服务器，服务器运用更大参数规模的模型进行综合评估。
 */
class RemoteRiskDataSource : public BehaviorRiskRepository {
 public:
  RemoteRiskDataSource() = default;
  ~RemoteRiskDataSource() override = default;

  bool init() override {
    // 这里可以初始化 QNetworkAccessManager 等
    return true; 
  }

  void evaluateAsync(const TrajectoryFeature& feature, Callback callback) override;
};
