#pragma once

#include "domain/repository/BehaviorRiskRepository.h"
#include "domain/model/BehaviorTracker.h"
#include <memory>
#include <vector>

/**
 * @brief 行为风控网域服务 (Domain Service)
 *
 * 处理验证码滑动轨迹的业务逻辑：特征提取 + 调度数据层仓库进行打分
 */
class BehaviorRiskService {
 public:
  explicit BehaviorRiskService(std::shared_ptr<BehaviorRiskRepository> repository);

  // 对外提供的业务接口：验证一条滑动轨迹，并异步返回结果
  void verifyTrajectoryAsync(const std::vector<TrackPoint>& trajectory,
                             BehaviorRiskRepository::Callback callback);

 private:
  std::shared_ptr<BehaviorRiskRepository> m_repository;
};
