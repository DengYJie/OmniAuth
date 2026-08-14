#include "BehaviorRiskRepositoryImpl.h"
#include <QDebug>

BehaviorRiskRepositoryImpl::BehaviorRiskRepositoryImpl(
    std::unique_ptr<BehaviorRiskRepository> localSource,
    std::unique_ptr<BehaviorRiskRepository> remoteSource)
    : m_localSource(std::move(localSource)),
      m_remoteSource(std::move(remoteSource)) {}

bool BehaviorRiskRepositoryImpl::init() {
  bool ok = true;
  if (m_localSource) ok = m_localSource->init() && ok;
  if (m_remoteSource) ok = m_remoteSource->init() && ok;
  return ok;
}

void BehaviorRiskRepositoryImpl::evaluateAsync(const TrajectoryFeature& feature,
                                               BehaviorRiskRepository::Callback callback) {
  if (m_useRemote && m_remoteSource) {
    qDebug() << "[BehaviorRiskRepo] Dispatching to Remote Data Source.";
    m_remoteSource->evaluateAsync(feature, callback);
  } else if (m_localSource) {
    qDebug() << "[BehaviorRiskRepo] Dispatching to Local Data Source.";
    m_localSource->evaluateAsync(feature, callback);
  } else {
    callback(std::unexpected("No valid data source available in BehaviorRiskRepository."));
  }
}
