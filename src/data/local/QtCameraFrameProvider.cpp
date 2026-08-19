#include "QtCameraFrameProvider.h"

#include <QCamera>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QThread>
#include <QVideoFrame>
#include <QVideoSink>

#include <opencv2/imgproc.hpp>

QtCameraFrameProvider::QtCameraFrameProvider(QObject* parent)
    : QObject(parent) {}

QtCameraFrameProvider::~QtCameraFrameProvider() {
    stop();
}

std::expected<void, QString> QtCameraFrameProvider::start(int cameraIndex, FrameHandler frameHandler, ErrorHandler errorHandler) {
    std::expected<void, QString> result;
    auto startFn = [this, cameraIndex, frameHandler = std::move(frameHandler), errorHandler = std::move(errorHandler), &result]() mutable {
        result = startOnCurrentThread(cameraIndex, std::move(frameHandler), std::move(errorHandler));
    };

    if (QThread::currentThread() == thread()) {
        startFn();
    } else {
        QMetaObject::invokeMethod(this, std::move(startFn), Qt::BlockingQueuedConnection);
    }

    return result;
}

void QtCameraFrameProvider::stop() {
    auto stopFn = [this]() {
        stopOnCurrentThread();
    };

    if (QThread::currentThread() == thread()) {
        stopFn();
    } else {
        QMetaObject::invokeMethod(this, std::move(stopFn), Qt::BlockingQueuedConnection);
    }
}

bool QtCameraFrameProvider::isRunning() const {
    return m_running;
}

std::expected<void, QString> QtCameraFrameProvider::startOnCurrentThread(int cameraIndex, FrameHandler frameHandler, ErrorHandler errorHandler) {
    stopOnCurrentThread();

    const auto devices = QMediaDevices::videoInputs();
    if (devices.isEmpty()) {
        return std::unexpected(QStringLiteral("未检测到可用摄像头设备"));
    }
    if (cameraIndex < 0 || cameraIndex >= devices.size()) {
        return std::unexpected(QStringLiteral("无效的摄像头索引"));
    }

    {
        std::lock_guard lock(m_callbackMutex);
        m_frameHandler = std::move(frameHandler);
        m_errorHandler = std::move(errorHandler);
    }

    m_captureSession = std::make_unique<QMediaCaptureSession>();
    m_videoSink = std::make_unique<QVideoSink>();
    m_camera = std::make_unique<QCamera>(devices.at(cameraIndex));

    QObject::connect(m_videoSink.get(), &QVideoSink::videoFrameChanged, this, &QtCameraFrameProvider::handleVideoFrame);
    QObject::connect(
        m_camera.get(),
        &QCamera::errorOccurred,
        this,
        [this](QCamera::Error error, const QString& errorString) {
            handleCameraError(static_cast<int>(error), errorString);
        });

    m_captureSession->setCamera(m_camera.get());
    m_captureSession->setVideoSink(m_videoSink.get());
    m_camera->start();
    m_running = true;

    return {};
}

void QtCameraFrameProvider::stopOnCurrentThread() {
    m_running = false;

    if (m_camera) {
        m_camera->stop();
    }
    if (m_captureSession) {
        m_captureSession->setVideoSink(nullptr);
        m_captureSession->setCamera(nullptr);
    }

    m_videoSink.reset();
    m_camera.reset();
    m_captureSession.reset();

    std::lock_guard lock(m_callbackMutex);
    m_frameHandler = nullptr;
    m_errorHandler = nullptr;
}

void QtCameraFrameProvider::handleVideoFrame(const QVideoFrame& frame) {
    if (!m_running) {
        return;
    }

    QImage rawImage = frame.toImage();
    if (rawImage.isNull()) {
        return;
    }

    QImage previewImage = rawImage.convertToFormat(QImage::Format_RGB888);
    if (previewImage.isNull()) {
        return;
    }

    cv::Mat rgbFrame(previewImage.height(),
                     previewImage.width(),
                     CV_8UC3,
                     previewImage.bits(),
                     previewImage.bytesPerLine());

    cv::Mat bgrFrame;
    cv::cvtColor(rgbFrame, bgrFrame, cv::COLOR_RGB2BGR);

    FrameHandler frameHandler;
    {
        std::lock_guard lock(m_callbackMutex);
        frameHandler = m_frameHandler;
    }

    if (frameHandler) {
        frameHandler(previewImage.copy(), bgrFrame.clone());
    }
}

void QtCameraFrameProvider::handleCameraError(int error, const QString& errorString) {
    if (error == 0) {
        return;
    }

    m_running = false;

    ErrorHandler errorHandler;
    {
        std::lock_guard lock(m_callbackMutex);
        errorHandler = m_errorHandler;
    }

    if (errorHandler) {
        errorHandler(errorString.isEmpty() ? QStringLiteral("摄像头启动失败或设备不可用") : errorString);
    }
}
