#pragma once

#include "domain/repository/BehaviorRiskRepository.h"
#include "data/local/BehaviorRiskEngine.h"
#include <memory>
#include <string>

/**
 * @brief 本地风控数据源
 * 
 * 作为数据源层 (Data Layer)，实现异步接口规范，并将底层的核心推理工作委托给
 * 基础设施层 (Infrastructure Layer) 的 BehaviorRiskEngine。
 */
class LocalRiskDataSource : public BehaviorRiskRepository {
 public:
  explicit LocalRiskDataSource(std::string modelPath = "models/behavior_mlp.onnx");
  ~LocalRiskDataSource() override = default;

  bool init() override;
  void evaluateAsync(const TrajectoryFeature& feature, Callback callback) override;

 private:
  std::unique_ptr<BehaviorRiskEngine> m_engine;
};
