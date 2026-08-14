#include "FaceEnrollUseCase.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <chrono>
#include <utility>

#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

// ═══════════════════════════════════════════════════════════════
// 构造 / 析构
// ═══════════════════════════════════════════════════════════════

FaceEnrollUseCase::FaceEnrollUseCase(std::shared_ptr<FaceAuthRepository> faceRepository,
                                     std::shared_ptr<UserRepository> userRepository)
    : m_faceRepository(std::move(faceRepository)),
      m_userRepository(std::move(userRepository)) {}

FaceEnrollUseCase::~FaceEnrollUseCase() {
    stopFaceEnroll();
}

// ═══════════════════════════════════════════════════════════════
// 人脸录入 (从 AuthService 原样迁移)
// ═══════════════════════════════════════════════════════════════

void FaceEnrollUseCase::runOnMainThread(std::function<void()> fn) {
    QMetaObject::invokeMethod(qApp, std::move(fn), Qt::QueuedConnection);
}

bool FaceEnrollUseCase::startFaceEnroll(const QString& account, int cameraIndex,
                                        std::function<void(bool, QString)> callback) {
    if (m_isEnrolling) return false; // 已在录入中

    // 先查账号是否存在
    m_userRepository->getUserByAccountAsync(account.trimmed(),
        [this, cameraIndex, callback](std::optional<UserAuthDTO> userOpt) {
            if (!userOpt.has_value()) {
                callback(false, QStringLiteral("账户不存在"));
                return;
            }
            int uid = userOpt->user.uid();
            m_isEnrolling = true;
            m_enrollThread = std::jthread([this, cameraIndex, uid, callback](std::stop_token stopToken) {
                enrollWorkerLoop(stopToken, cameraIndex, uid, callback);
            });
        });
    return true;
}

void FaceEnrollUseCase::stopFaceEnroll() {
    m_isEnrolling = false;
    if (m_enrollThread.joinable()) {
        m_enrollThread.request_stop();
    }
    if (m_enrollThread.joinable()) {
        m_enrollThread.join();
    }
}

// static
bool FaceEnrollUseCase::openCapture(cv::VideoCapture& cap, int cameraIndex) {
#ifdef _WIN32
    if (cap.open(cameraIndex, cv::CAP_DSHOW)) return true;
    if (cap.open(cameraIndex, cv::CAP_MSMF)) return true;
#endif
    return cap.open(cameraIndex, cv::CAP_ANY);
}

void FaceEnrollUseCase::enrollWorkerLoop(std::stop_token stopToken, int cameraIndex, int uid,
                                         std::function<void(bool, QString)> callback) {
    cv::VideoCapture cap;
    if (!openCapture(cap, cameraIndex)) {
        runOnMainThread([callback]() { callback(false, QStringLiteral("无法打开摄像头")); });
        m_isEnrolling = false;
        return;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    auto startTime = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(20);
    cv::Mat frame;

    // 共享状态（跨嵌套 lambda 访问）
    bool detectionDone = false;
    bool detectionSuccess = false;
    std::vector<float> capturedFeature;

    while (!stopToken.stop_requested() && m_isEnrolling) {
        if (std::chrono::steady_clock::now() - startTime > timeout) {
            runOnMainThread([callback]() { callback(false, QStringLiteral("录入超时，请重试")); });
            break;
        }

        if (!cap.read(frame) || frame.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // 重置本轮状态
        detectionDone = false;
        detectionSuccess = false;
        capturedFeature.clear();

        // 检测人脸
        m_faceRepository->detectFaceAsync(frame,
            [&](std::expected<FaceDetectionResult, AuthError> res) {
                if (!res.has_value()) {
                    detectionDone = true;
                    return;
                }
                FaceBox faceBox = res.value().face;

                // 活体检测
                bool livenessDone = false;
                bool livenessOk = false;
                m_faceRepository->checkLivenessAsync(frame, faceBox, m_livenessThreshold,
                    [&](std::expected<float, AuthError> lr) {
                        livenessDone = true;
                        livenessOk = lr.has_value() && lr.value() >= m_livenessThreshold;
                    });

                while (!livenessDone && !stopToken.stop_requested() && m_isEnrolling) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                if (!livenessOk) {
                    detectionDone = true;
                    return;
                }

                // 提取特征
                cv::Mat alignedFace;
                try {
                    alignedFace = frame(faceBox.box).clone();
                } catch (...) {
                    detectionDone = true;
                    return;
                }

                bool extractDone = false;
                m_faceRepository->extractFeatureAsync(alignedFace,
                    [&](std::expected<std::vector<float>, AuthError> er) {
                        extractDone = true;
                        if (er.has_value()) {
                            capturedFeature = er.value();
                            detectionSuccess = true;
                        }
                    });

                while (!extractDone && !stopToken.stop_requested() && m_isEnrolling) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                detectionDone = true;
            });

        // 等待检测完成
        while (!detectionDone && !stopToken.stop_requested() && m_isEnrolling) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        if (detectionSuccess && !capturedFeature.empty()) {
            runOnMainThread([this, uid, feature = std::move(capturedFeature), callback]() {
                m_userRepository->saveUserFaceFeatureAsync(uid, feature,
                    [callback](bool success) {
                        if (success) {
                            callback(true, QStringLiteral("人脸录入成功"));
                        } else {
                            callback(false, QStringLiteral("保存人脸特征失败"));
                        }
                    });
            });
            m_isEnrolling = false;
            cap.release();
            return;
        }
    }

    cap.release();
    m_isEnrolling = false;
}
