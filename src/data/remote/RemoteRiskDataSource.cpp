#include "RemoteRiskDataSource.h"
#include <QDebug>
#include <QTimer>
#include <QCoreApplication>

void RemoteRiskDataSource::evaluateAsync(const TrajectoryFeature& feature, Callback callback) {
  // TODO: 这里应接入 QNetworkAccessManager，组装 JSON POST 请求发给云端风控 API。
  // 假装网络请求耗时 200ms 后返回成功。
  qDebug() << "[RemoteRiskDataSource] Sending feature to remote risk API... (Mock)";
  
  QTimer::singleShot(200, [callback]() {
      // 假设云端 API 返回 0.95
      callback(0.95f);
  });
}
