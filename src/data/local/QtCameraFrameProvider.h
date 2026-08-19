#pragma once

#include <QObject>
#include <QImage>
#include <QString>

#include <expected>
#include <functional>
#include <memory>
#include <mutex>

#include <opencv2/core.hpp>

class QCamera;
class QMediaCaptureSession;
class QVideoFrame;
class QVideoSink;

class QtCameraFrameProvider : public QObject {
public:
    using FrameHandler = std::function<void(const QImage& previewFrame, cv::Mat inferenceFrame)>;
    using ErrorHandler = std::function<void(const QString& message)>;

    explicit QtCameraFrameProvider(QObject* parent = nullptr);
    ~QtCameraFrameProvider() override;

    std::expected<void, QString> start(int cameraIndex, FrameHandler frameHandler, ErrorHandler errorHandler);
    void stop();

    [[nodiscard]] bool isRunning() const;

private:
    std::expected<void, QString> startOnCurrentThread(int cameraIndex, FrameHandler frameHandler, ErrorHandler errorHandler);
    void stopOnCurrentThread();
    void handleVideoFrame(const QVideoFrame& frame);
    void handleCameraError(int error, const QString& errorString);

    mutable std::mutex m_callbackMutex;
    FrameHandler m_frameHandler = nullptr;
    ErrorHandler m_errorHandler = nullptr;

    std::unique_ptr<QMediaCaptureSession> m_captureSession;
    std::unique_ptr<QCamera> m_camera;
    std::unique_ptr<QVideoSink> m_videoSink;

    bool m_running = false;
};
