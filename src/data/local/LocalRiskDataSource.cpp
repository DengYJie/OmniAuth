#include "LocalRiskDataSource.h"
#include <thread>

LocalRiskDataSource::LocalRiskDataSource(std::string modelPath) 
    : m_engine(std::make_unique<BehaviorRiskEngine>(std::move(modelPath))) {}

bool LocalRiskDataSource::init() {
  return m_engine->init();
}

void LocalRiskDataSource::evaluateAsync(const TrajectoryFeature& feature, Callback callback) {
  // 采用后台线程异步调用同步引擎，或者直接在当前线程调用（因为 MLP 推理极快）
  std::thread([this, feature, callback]() {
    auto result = m_engine->evaluate(feature);
    callback(result);
  }).detach();
}
