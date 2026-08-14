#pragma once

#include "domain/repository/BehaviorRiskRepository.h"
#include <memory>
#include <string>

/**
 * @brief 行为风控仓库层
 *
 * 封装并管理多个风控数据源（本地 ONNX 模型或远程风控服务）。
 * 网域层（Service）只与此仓库交互，不感知底层的具体数据源。
 */
class BehaviorRiskRepositoryImpl : public BehaviorRiskRepository {
 public:
  BehaviorRiskRepositoryImpl(std::unique_ptr<BehaviorRiskRepository> localSource,
                         std::unique_ptr<BehaviorRiskRepository> remoteSource);

  bool init() override;

  // 暴露给网域层的统一异步风控评估接口
  void evaluateAsync(const TrajectoryFeature& feature, BehaviorRiskRepository::Callback callback) override;

  // （可选）可以增加一个 setter 来动态切换使用网络还是本地
  void setUseRemote(bool useRemote) { m_useRemote = useRemote; }

 private:
  std::unique_ptr<BehaviorRiskRepository> m_localSource;
  std::unique_ptr<BehaviorRiskRepository> m_remoteSource;
  bool m_useRemote = false; // 默认使用本地
};
