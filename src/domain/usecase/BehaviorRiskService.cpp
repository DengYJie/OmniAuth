#include "BehaviorRiskService.h"

BehaviorRiskService::BehaviorRiskService(std::shared_ptr<BehaviorRiskRepository> repository)
    : m_repository(std::move(repository)) {}

void BehaviorRiskService::verifyTrajectoryAsync(const std::vector<TrackPoint>& trajectory,
                                                BehaviorRiskRepository::Callback callback) {
  // 1. 业务逻辑：提取轨迹特征
  TrajectoryFeature feat = BehaviorTracker::extractFeatures(trajectory);

  // 2. 将特征抛给数据层（Repository）进行打分
  if (m_repository) {
    m_repository->evaluateAsync(feat, std::move(callback));
  } else {
    callback(std::unexpected("BehaviorRiskService: Repository not initialized"));
  }
}
