#pragma once

#include "ui/common/BaseViewModel.h"
#include "ui/widget/FaceScannerWidget.h"
#include <QImage>
#include <memory>

class FaceEnrollUseCase;

struct FaceEnrollState {
  bool isEnrolling = false;
  bool enrollSuccess = false;
  FaceScannerWidget::ScanState scanState = FaceScannerWidget::ScanState::Connecting;
  QString message;

  bool operator==(const FaceEnrollState&) const = default;
};

class FaceEnrollViewModel : public BaseViewModel<FaceEnrollViewModel, FaceEnrollState> {
  Q_OBJECT

 public:
  explicit FaceEnrollViewModel(QObject* parent = nullptr);
  ~FaceEnrollViewModel() override;

  // Intents
  void startEnroll(int uid, int cameraIndex = 0);
  void stopEnroll();
  void reset();

 signals:
  void stateChanged(const FaceEnrollState& state);
  void enrollSuccess();
  void enrollFailed(const QString& message);
  void frameReceived(const QImage& frame);

 protected:
  void emitStateChanged() override;

 private slots:
  void handleFrameReceived(const QImage& frame);

 private:
  std::shared_ptr<FaceEnrollUseCase> m_faceEnrollUseCase;
};
