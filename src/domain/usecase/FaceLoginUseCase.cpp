#include "FaceLoginUseCase.h"

#include <QDebug>
#include <chrono>
#include <future>
#include <utility>

#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

// ═══════════════════════════════════════════════════════════════
// 构造 / 析构
// ═══════════════════════════════════════════════════════════════

FaceLoginUseCase::FaceLoginUseCase(std::shared_ptr<FaceAuthRepository> faceRepository,
                                   std::shared_ptr<UserRepository> userRepository)
    : m_faceRepository(std::move(faceRepository)),
      m_userRepository(std::move(userRepository)) {}

FaceLoginUseCase::~FaceLoginUseCase() {
    stopFaceScan();
}

// ═══════════════════════════════════════════════════════════════
// 人脸登录管线 (从 AuthService 原样迁移)
// ═══════════════════════════════════════════════════════════════

bool FaceLoginUseCase::startFaceScan(int cameraIndex) {
    if (m_running) stopFaceScan();

    m_running = true;
    m_lastAuthSuccess = false;
    m_lastFaceBox = cv::Rect();
    m_lastMatchedUid = -1;

    m_captureThread = std::jthread([this, cameraIndex](std::stop_token stopToken) {
        captureWorkerLoop(stopToken, cameraIndex);
    });

    m_isProcessingFrame = false;
    return true;
}

void FaceLoginUseCase::stopFaceScan() {
    m_running = false;
    if (m_captureThread.joinable()) {
        m_captureThread.request_stop();
    }
    if (m_captureThread.joinable()) {
        m_captureThread.join();
    }
}

float FaceLoginUseCase::calculateIoU(const cv::Rect& box1, const cv::Rect& box2) const {
    cv::Rect inter = box1 & box2;
    int interArea = inter.area();
    int unionArea = box1.area() + box2.area() - interArea;
    if (unionArea <= 0) return 0.0f;
    return static_cast<float>(interArea) / static_cast<float>(unionArea);
}

// static
bool FaceLoginUseCase::openCapture(cv::VideoCapture& cap, int cameraIndex) {
#ifdef _WIN32
    if (cap.open(cameraIndex, cv::CAP_DSHOW)) return true;
    if (cap.open(cameraIndex, cv::CAP_MSMF)) return true;
#endif
    return cap.open(cameraIndex, cv::CAP_ANY);
}

void FaceLoginUseCase::captureWorkerLoop(std::stop_token stopToken, int cameraIndex) {
    cv::VideoCapture cap;
    if (!openCapture(cap, cameraIndex)) {
        qWarning() << "FaceLoginUseCase: Unable to open camera at index" << cameraIndex;
        if (m_authCallback) {
            m_authCallback(AuthResult::Error, "", QStringLiteral("无法打开摄像头设备"));
        }
        m_running = false;
        return;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    cap.set(cv::CAP_PROP_BUFFERSIZE, 1);

    cv::Mat frame;
    while (!stopToken.stop_requested() && m_running) {
        if (!cap.read(frame) || frame.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (m_frameCallback) {
            cv::Mat rgbFrame;
            cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);
            QImage previewImage(rgbFrame.data, rgbFrame.cols, rgbFrame.rows,
                                static_cast<int>(rgbFrame.step), QImage::Format_RGB888);
            m_frameCallback(previewImage.copy());
        }

        if (!m_isProcessingFrame.exchange(true)) {
            processFrameAsync(frame.clone());
        }
    }

    cap.release();
}

void FaceLoginUseCase::processFrameAsync(cv::Mat frame) {
    if (!m_faceRepository) {
        m_isProcessingFrame = false;
        return;
    }

    m_faceRepository->detectFaceAsync(frame, [this, frame](std::expected<FaceDetectionResult, AuthError> detectRes) {
        if (!detectRes.has_value()) {
            m_lastAuthSuccess = false;
            m_lastFaceBox = cv::Rect();
            AuthError err = detectRes.error();
            if (m_authCallback) {
                switch (err) {
                    case AuthError::NoFace:
                        m_authCallback(AuthResult::Failed, "", QStringLiteral("未检测到人脸"));
                        break;
                    case AuthError::MultipleFaces:
                        m_authCallback(AuthResult::Failed, "", QStringLiteral("检测到多张人脸，请保持单人在框内"));
                        break;
                    case AuthError::ModelError:
                        m_authCallback(AuthResult::Error, "", QStringLiteral("人脸检测模型未就绪或运行失败"));
                        m_running = false;
                        break;
                    default:
                        break;
                }
            }
            m_isProcessingFrame = false;
            return;
        }

        FaceBox faceBox = detectRes.value().face;

        if (!m_lastFaceBox.empty()) {
            float iou = calculateIoU(faceBox.box, m_lastFaceBox);
            if (iou >= m_iouThreshold) {
                m_lastFaceBox = faceBox.box;
                if (m_lastAuthSuccess && m_lastMatchedUid != -1) {
                    if (m_authCallback) {
                        m_authCallback(AuthResult::Verifying, "", QStringLiteral("特征匹配中..."));
                    }
                    m_userRepository->getUserByIdAsync(m_lastMatchedUid, [this](std::optional<User> userOpt) {
                        if (userOpt && m_authCallback) {
                            m_authCallback(AuthResult::Success, userOpt->username(), QStringLiteral("识别成功，欢迎回来！"));
                        }
                    });
                }
                m_isProcessingFrame = false;
                return;
            }
        }

        m_lastFaceBox = faceBox.box;

        if (m_authCallback) {
            m_authCallback(AuthResult::Verifying, "", QStringLiteral("正在进行活体检测与特征比对..."));
        }

        m_faceRepository->checkLivenessAsync(frame, faceBox, m_livenessThreshold,
            [this, frame, faceBox](std::expected<float, AuthError> livenessRes) {
                if (!livenessRes.has_value()) {
                    m_lastAuthSuccess = false;
                    if (m_authCallback) {
                        m_authCallback(AuthResult::Error, "", QStringLiteral("活体检测引擎未就绪或异常"));
                    }
                    m_isProcessingFrame = false;
                    return;
                }

                float livenessScore = livenessRes.value();
                if (livenessScore < m_livenessThreshold) {
                    m_lastAuthSuccess = false;
                    if (m_authCallback) {
                        m_authCallback(AuthResult::SpoofingDetected, "",
                                       QStringLiteral("警告：检测到非真人活体（疑似照片或屏幕）"));
                    }
                    m_isProcessingFrame = false;
                    return;
                }

                cv::Mat alignedFace;
                try {
                    alignedFace = frame(faceBox.box).clone();
                } catch (...) {
                    m_isProcessingFrame = false;
                    return;
                }

                m_faceRepository->extractFeatureAsync(alignedFace,
                    [this, faceBox](std::expected<std::vector<float>, AuthError> featureRes) {
                        if (!featureRes.has_value()) {
                            m_lastAuthSuccess = false;
                            if (m_authCallback) {
                                m_authCallback(AuthResult::Error, "", QStringLiteral("特征提取模型未就绪或异常"));
                            }
                            m_isProcessingFrame = false;
                            return;
                        }

                        std::vector<float> feature = featureRes.value();

                        m_faceRepository->matchFeatureAsync(feature, m_similarityThreshold,
                            [this, faceBox](bool isMatched, int matchedUid) {
                                if (isMatched) {
                                    m_userRepository->getUserByIdAsync(matchedUid, [this, faceBox, matchedUid](std::optional<User> userOpt) {
                                        if (userOpt) {
                                            m_lastAuthSuccess = true;
                                            m_lastFaceBox = faceBox.box;
                                            m_lastMatchedUid = matchedUid;
                                            if (m_authCallback) {
                                                m_authCallback(AuthResult::Success, userOpt->username(),
                                                               QStringLiteral("识别成功，欢迎回来！"));
                                            }
                                        }
                                        m_isProcessingFrame = false;
                                    });
                                } else {
                                    m_lastAuthSuccess = false;
                                    m_lastMatchedUid = -1;
                                    if (m_authCallback) {
                                        m_authCallback(AuthResult::Unrecognized, "",
                                                       QStringLiteral("未匹配到注册特征，请重新尝试"));
                                    }
                                    m_isProcessingFrame = false;
                                }
                            });
                    });
            });
    });
}

// ═══════════════════════════════════════════════════════════════
// 人脸绑定查询
// ═══════════════════════════════════════════════════════════════

bool FaceLoginUseCase::hasUserFace(const QString& account) const {
    if (!m_userRepository) return false;
    std::promise<bool> promise;
    m_userRepository->hasUserFaceAsync(account, [&promise](bool result) {
        promise.set_value(result);
    });
    return promise.get_future().get();
}
