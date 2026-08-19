#pragma once

#include "ui/common/BaseViewModel.h"
#include "ui/widget/FaceScannerWidget.h"
#include "domain/model/FaceTypes.h"
#include <QImage>
#include <memory>

class FaceLoginUseCase;

struct FaceScannerState {
  bool isScanning = false;
  bool scanSuccess = false;
  FaceScannerWidget::ScanState scanState = FaceScannerWidget::ScanState::Connecting;
  QString message;
  QString authenticatedUser;

  bool operator==(const FaceScannerState&) const = default;
};

class FaceScannerViewModel : public BaseViewModel<FaceScannerViewModel, FaceScannerState> {
  Q_OBJECT

 public:
  explicit FaceScannerViewModel(QObject* parent = nullptr);
  ~FaceScannerViewModel() override;

  // Intents
  void startScan(int cameraIndex = 0);
  void stopScan();
  void reset();

 signals:
  void stateChanged(const FaceScannerState& state);
  void faceScanSuccess(int uid, const QString& username);
  void faceScanFailed(const QString& message);
  void frameReceived(const QImage& frame);

 protected:
  void emitStateChanged() override;

 private slots:
  void handleFrameReceived(const QImage& frame);
  void handleAuthResult(AuthResult result, int uid, const QString& username, const QString& message);

 private:
  std::shared_ptr<FaceLoginUseCase> m_faceLoginUseCase;
};
