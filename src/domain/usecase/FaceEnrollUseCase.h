#pragma once

#include "domain/repository/FaceAuthRepository.h"
#include "domain/repository/UserRepository.h"
#include "domain/model/FaceTypes.h"

#include <atomic>
#include <functional>
#include <memory>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <QImage>
#include <QString>
#include <thread>

/**
 * @brief 人脸录入用例
 *
 * 在独立线程中循环采集摄像头帧，执行活体检测与特征提取，
 * 成功后保存人脸特征到用户仓库。人脸登录见 FaceLoginUseCase。
 */
class FaceEnrollUseCase {
public:
    using FrameCallback = std::function<void(const QImage& frame)>;

    explicit FaceEnrollUseCase(std::shared_ptr<FaceAuthRepository> faceRepository,
                               std::shared_ptr<UserRepository> userRepository);
    ~FaceEnrollUseCase();

    void setFrameCallback(FrameCallback callback) { m_frameCallback = std::move(callback); }

    bool startFaceEnroll(int uid, int cameraIndex,
                         std::function<void(bool, QString)> callback);
    void stopFaceEnroll();
    [[nodiscard]] bool isEnrolling() const { return m_isEnrolling; }

private:
    static bool openCapture(cv::VideoCapture& cap, int cameraIndex);
    void enrollWorkerLoop(std::stop_token stopToken, int cameraIndex, int uid,
                          std::function<void(bool, QString)> callback);
    void runOnMainThread(std::function<void()> fn);

    std::shared_ptr<FaceAuthRepository> m_faceRepository;
    std::shared_ptr<UserRepository>     m_userRepository;

    std::atomic<bool> m_isEnrolling{false};
    std::jthread m_enrollThread;

    FrameCallback m_frameCallback = nullptr;

    float m_livenessThreshold = 0.85f;
};
