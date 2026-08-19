#include "FaceScannerViewModel.h"
#include "domain/usecase/FaceLoginUseCase.h"
#include "data/di/AppContainer.h"
#include <QMetaObject>
#include <QDebug>
#include <QTimer>

FaceScannerViewModel::FaceScannerViewModel(QObject* parent)
    : BaseViewModel<FaceScannerViewModel, FaceScannerState>(parent) {
    m_faceLoginUseCase = AppContainer::faceLoginUseCase();

    qRegisterMetaType<AuthError>();
    qRegisterMetaType<AuthResult>();

    if (m_faceLoginUseCase) {
        m_faceLoginUseCase->setFrameCallback([this](const QImage& frame) {
            QMetaObject::invokeMethod(this, [this, frame]() {
                handleFrameReceived(frame);
            }, Qt::QueuedConnection);
        });

        m_faceLoginUseCase->setAuthCallback([this](AuthResult result, int uid, const QString& username, const QString& message) {
            QMetaObject::invokeMethod(this, [this, result, uid, username, message]() {
                handleAuthResult(result, uid, username, message);
            }, Qt::QueuedConnection);
        });
    }
}

FaceScannerViewModel::~FaceScannerViewModel() {
    stopScan();
}

void FaceScannerViewModel::emitStateChanged() {
    emit stateChanged(m_state);
}

void FaceScannerViewModel::startScan(int cameraIndex) {
    updateState([&](FaceScannerState& state) {
        state.isScanning = true;
        state.scanSuccess = false;
        state.scanState = FaceScannerWidget::ScanState::Connecting;
        state.message = QStringLiteral("正在初始化摄像头与AI模型...");
        state.authenticatedUser.clear();
    });

    bool started = m_faceLoginUseCase->startFaceScan(cameraIndex);
    if (!started) {
        updateState([&](FaceScannerState& state) {
            state.isScanning = false;
            state.scanState = FaceScannerWidget::ScanState::Error;
            state.message = QStringLiteral("无法启动面部识别服务");
        });
    }
}

void FaceScannerViewModel::stopScan() {
    if (m_faceLoginUseCase) {
        m_faceLoginUseCase->stopFaceScan();
    }
    updateState([&](FaceScannerState& state) {
        state.isScanning = false;
        state.scanSuccess = false;
        state.message.clear();
    });
}

void FaceScannerViewModel::reset() {
    stopScan();
    updateState([&](FaceScannerState& state) {
        state.isScanning = false;
        state.scanSuccess = false;
        state.scanState = FaceScannerWidget::ScanState::Connecting;
        state.message.clear();
        state.authenticatedUser.clear();
    });
}

void FaceScannerViewModel::handleFrameReceived(const QImage& frame) {
    if (!m_state.isScanning) return;
    emit frameReceived(frame);
}
void FaceScannerViewModel::handleAuthResult(AuthResult result, int uid, const QString& username, const QString& message) {
    if (!m_state.isScanning && result != AuthResult::Success) return;

    switch (result) {
        case AuthResult::Success: {
            updateState([&](FaceScannerState& state) {
                state.isScanning = false;
                state.scanSuccess = true;
                state.scanState = FaceScannerWidget::ScanState::Success;
                state.message = message;
                state.authenticatedUser = username;
            });
            if (m_faceLoginUseCase) {
                m_faceLoginUseCase->stopFaceScan();
            }
            emit faceScanSuccess(uid, username);
            break;
        }
        case AuthResult::SpoofingDetected: {
            updateState([&](FaceScannerState& state) {
                state.scanState = FaceScannerWidget::ScanState::Error;
                state.message = message;
            });
            break;
        }
        case AuthResult::Failed: {
            updateState([&](FaceScannerState& state) {
                state.scanState = FaceScannerWidget::ScanState::Scanning;
                state.message = message;
            });
            break;
        }
        case AuthResult::Unrecognized: {
            updateState([&](FaceScannerState& state) {
                state.scanState = FaceScannerWidget::ScanState::Error;
                state.message = message;
            });
            break;
        }
        case AuthResult::Verifying: {
            updateState([&](FaceScannerState& state) {
                state.scanState = FaceScannerWidget::ScanState::Verifying;
                state.message = message;
            });
            break;
        }
        case AuthResult::Error: {
            updateState([&](FaceScannerState& state) {
                state.scanState = FaceScannerWidget::ScanState::Error;
                state.message = message;
            });
            break;
        }
    }
}
