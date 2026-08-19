#pragma once

#include "domain/repository/FaceAuthRepository.h"
#include "domain/repository/UserRepository.h"
#include "domain/model/FaceTypes.h"

#include <atomic>
#include <functional>
#include <memory>
#include <opencv2/core.hpp>
#include <QImage>
#include <QString>
#include <thread>

class QtCameraFrameProvider;

/**
 * @brief 人脸登录用例
 *
 * 负责摄像头采集、活体检测、特征比对的人脸识别登录全流程，
 * 以及账号人脸绑定查询。人脸录入见 FaceEnrollUseCase。
 */
class FaceLoginUseCase {
public:
    using FrameCallback = std::function<void(const QImage& frame)>;
    using AuthCallback  = std::function<void(AuthResult result, int uid, const QString& username, const QString& message)>;

    explicit FaceLoginUseCase(std::shared_ptr<FaceAuthRepository> faceRepository,
                              std::shared_ptr<UserRepository> userRepository);
    ~FaceLoginUseCase();

    void setFrameCallback(FrameCallback callback) { m_frameCallback = std::move(callback); }
    void setAuthCallback(AuthCallback callback)   { m_authCallback = std::move(callback); }

    void setLivenessThreshold(float threshold)   { m_livenessThreshold = threshold; }
    float livenessThreshold() const              { return m_livenessThreshold; }
    void setSimilarityThreshold(float threshold) { m_similarityThreshold = threshold; }
    float similarityThreshold() const            { return m_similarityThreshold; }
    void setIouThreshold(float threshold)        { m_iouThreshold = threshold; }
    float iouThreshold() const                   { return m_iouThreshold; }

    bool startFaceScan(int cameraIndex = 0);
    void stopFaceScan();
    [[nodiscard]] bool isScanning() const { return m_running; }

    // 账号是否已绑定人脸（邮箱/手机号/用户名 或 uid）
    [[nodiscard]] bool hasUserFace(const QString& account) const;
    [[nodiscard]] bool hasUserFace(int uid) const;

private:
    void processFrameAsync(cv::Mat frame);
    float calculateIoU(const cv::Rect& box1, const cv::Rect& box2) const;

    std::shared_ptr<FaceAuthRepository> m_faceRepository;
    std::shared_ptr<UserRepository>     m_userRepository;
    std::unique_ptr<QtCameraFrameProvider> m_cameraProvider;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_isProcessingFrame{false};

    FrameCallback m_frameCallback = nullptr;
    AuthCallback  m_authCallback  = nullptr;

    float m_livenessThreshold    = 0.85f;
    float m_similarityThreshold  = 0.50f;
    float m_iouThreshold         = 0.85f;

    // Face Tracking Cache
    cv::Rect m_lastFaceBox;
    bool m_lastAuthSuccess = false;
    int m_lastMatchedUid = -1;
};
