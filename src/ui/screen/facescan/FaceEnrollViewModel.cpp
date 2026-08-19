#include "FaceEnrollViewModel.h"
#include "domain/usecase/FaceEnrollUseCase.h"
#include "data/di/AppContainer.h"
#include <QMetaObject>
#include <QPointer>

FaceEnrollViewModel::FaceEnrollViewModel(QObject* parent)
    : BaseViewModel<FaceEnrollViewModel, FaceEnrollState>(parent) {
    m_faceEnrollUseCase = AppContainer::faceEnrollUseCase();

    if (m_faceEnrollUseCase) {
        m_faceEnrollUseCase->setFrameCallback([this](const QImage& frame) {
            QMetaObject::invokeMethod(this, [this, frame]() {
                handleFrameReceived(frame);
            }, Qt::QueuedConnection);
        });
    }
}

FaceEnrollViewModel::~FaceEnrollViewModel() {
    stopEnroll();
    if (m_faceEnrollUseCase) {
        m_faceEnrollUseCase->setFrameCallback(nullptr);
    }
}

void FaceEnrollViewModel::emitStateChanged() {
    emit stateChanged(m_state);
}

void FaceEnrollViewModel::startEnroll(int uid, int cameraIndex) {
    if (uid <= 0) {
        updateState([&](FaceEnrollState& state) {
            state.isEnrolling = false;
            state.scanState = FaceScannerWidget::ScanState::Error;
            state.message = QStringLiteral("无效的用户 ID，无法录入人脸");
        });
        return;
    }

    const quint64 reqId = beginRequest();
    updateState([&](FaceEnrollState& state) {
        state.isEnrolling = true;
        state.enrollSuccess = false;
        state.scanState = FaceScannerWidget::ScanState::Scanning;
        state.message = QStringLiteral("正在初始化摄像头，请保持面部位于框内...");
    });

    if (!m_faceEnrollUseCase) {
        updateState([&](FaceEnrollState& state) {
            state.isEnrolling = false;
            state.scanState = FaceScannerWidget::ScanState::Error;
            state.message = QStringLiteral("人脸录入服务未就绪");
        });
        return;
    }

    QPointer<FaceEnrollViewModel> weakThis(this);
    bool started = m_faceEnrollUseCase->startFaceEnroll(uid, cameraIndex, [weakThis, reqId](bool ok, QString msg) {
        if (!weakThis) return;
        QMetaObject::invokeMethod(weakThis.data(), [weakThis, reqId, ok, msg]() {
            if (!weakThis || !weakThis->isRequestCurrent(reqId)) return;
            if (ok) {
                weakThis->updateState([msg](FaceEnrollState& state) {
                    state.isEnrolling = false;
                    state.enrollSuccess = true;
                    state.scanState = FaceScannerWidget::ScanState::Success;
                    state.message = msg;
                });
                emit weakThis->enrollSuccess();
            } else {
                weakThis->updateState([msg](FaceEnrollState& state) {
                    state.isEnrolling = false;
                    state.enrollSuccess = false;
                    state.scanState = FaceScannerWidget::ScanState::Error;
                    state.message = msg;
                });
                emit weakThis->enrollFailed(msg);
            }
        }, Qt::QueuedConnection);
    });

    if (!started) {
        updateState([&](FaceEnrollState& state) {
            state.isEnrolling = false;
            state.scanState = FaceScannerWidget::ScanState::Error;
            state.message = QStringLiteral("无法启动人脸录入，可能设备被占用或正在录入中");
        });
    }
}

void FaceEnrollViewModel::stopEnroll() {
    if (m_faceEnrollUseCase) {
        m_faceEnrollUseCase->stopFaceEnroll();
    }
    updateState([&](FaceEnrollState& state) {
        state.isEnrolling = false;
        state.enrollSuccess = false;
        state.message.clear();
    });
}

void FaceEnrollViewModel::reset() {
    stopEnroll();
    updateState([&](FaceEnrollState& state) {
        state.isEnrolling = false;
        state.enrollSuccess = false;
        state.scanState = FaceScannerWidget::ScanState::Connecting;
        state.message.clear();
    });
}

void FaceEnrollViewModel::handleFrameReceived(const QImage& frame) {
    if (!m_state.isEnrolling) return;
    emit frameReceived(frame);
}
