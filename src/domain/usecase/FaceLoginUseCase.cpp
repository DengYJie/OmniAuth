#include "FaceLoginUseCase.h"
#include "data/local/QtCameraFrameProvider.h"

#include <QDebug>
#include <future>
#include <utility>

FaceLoginUseCase::FaceLoginUseCase(std::shared_ptr<FaceAuthRepository> faceRepository,
                                   std::shared_ptr<UserRepository> userRepository)
    : m_faceRepository(std::move(faceRepository)),
      m_userRepository(std::move(userRepository)),
      m_cameraProvider(std::make_unique<QtCameraFrameProvider>()) {}

FaceLoginUseCase::~FaceLoginUseCase() {
    stopFaceScan();
}

bool FaceLoginUseCase::startFaceScan(int cameraIndex) {
    if (m_running) {
        stopFaceScan();
    }

    m_running = true;
    m_isProcessingFrame = false;
    m_lastAuthSuccess = false;
    m_lastFaceBox = cv::Rect();
    m_lastMatchedUid = -1;

    auto startResult = m_cameraProvider->start(
        cameraIndex,
        [this](const QImage& previewFrame, cv::Mat inferenceFrame) {
            if (!m_running) {
                return;
            }

            if (m_frameCallback) {
                m_frameCallback(previewFrame);
            }

            if (!m_isProcessingFrame.exchange(true)) {
                std::thread([this, frame = std::move(inferenceFrame)]() mutable {
                    processFrameAsync(std::move(frame));
                }).detach();
            }
        },
        [this](const QString& message) {
            m_running = false;
            m_isProcessingFrame = false;
            if (m_authCallback) {
                m_authCallback(AuthResult::Error, -1, "", message);
            }
        });

    if (!startResult.has_value()) {
        m_running = false;
        return false;
    }

    return true;
}

void FaceLoginUseCase::stopFaceScan() {
    m_running = false;
    m_isProcessingFrame = false;
    if (m_cameraProvider) {
        m_cameraProvider->stop();
    }
}

float FaceLoginUseCase::calculateIoU(const cv::Rect& box1, const cv::Rect& box2) const {
    cv::Rect inter = box1 & box2;
    int interArea = inter.area();
    int unionArea = box1.area() + box2.area() - interArea;
    if (unionArea <= 0) return 0.0f;
    return static_cast<float>(interArea) / static_cast<float>(unionArea);
}

void FaceLoginUseCase::processFrameAsync(cv::Mat frame) {
    if (!m_running || !m_faceRepository) {
        m_isProcessingFrame = false;
        return;
    }

    m_faceRepository->detectFaceAsync(frame, [this, frame](std::expected<FaceDetectionResult, AuthError> detectRes) {
        if (!m_running) {
            m_isProcessingFrame = false;
            return;
        }
        if (!detectRes.has_value()) {
            m_lastAuthSuccess = false;
            m_lastFaceBox = cv::Rect();
            AuthError err = detectRes.error();
            if (m_authCallback) {
                switch (err) {
                    case AuthError::NoFace:
                        m_authCallback(AuthResult::Failed, -1, "", QStringLiteral("未检测到人脸"));
                        break;
                    case AuthError::MultipleFaces:
                        m_authCallback(AuthResult::Failed, -1, "", QStringLiteral("检测到多张人脸，请保持单人在框内"));
                        break;
                    case AuthError::ModelError:
                        m_authCallback(AuthResult::Error, -1, "", QStringLiteral("人脸检测模型未就绪或运行失败"));
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
                        m_authCallback(AuthResult::Verifying, -1, "", QStringLiteral("特征匹配中..."));
                    }
                    m_userRepository->getUserByIdAsync(m_lastMatchedUid, [this](std::optional<User> userOpt) {
                        if (userOpt && m_authCallback) {
                            m_authCallback(AuthResult::Success, userOpt->uid(), userOpt->username(), QStringLiteral("识别成功，欢迎回来！"));
                        }
                    });
                }
                m_isProcessingFrame = false;
                return;
            }
        }

        m_lastFaceBox = faceBox.box;

        if (m_authCallback) {
            m_authCallback(AuthResult::Verifying, -1, "", QStringLiteral("正在进行活体检测与特征比对..."));
        }

        m_faceRepository->checkLivenessAsync(frame, faceBox, m_livenessThreshold,
            [this, frame, faceBox](std::expected<float, AuthError> livenessRes) {
                if (!m_running) {
                    m_isProcessingFrame = false;
                    return;
                }
                if (!livenessRes.has_value()) {
                    m_lastAuthSuccess = false;
                    if (m_authCallback) {
                        m_authCallback(AuthResult::Error, -1, "", QStringLiteral("活体检测引擎未就绪或异常"));
                    }
                    m_isProcessingFrame = false;
                    return;
                }

                float livenessScore = livenessRes.value();
                if (livenessScore < m_livenessThreshold) {
                    m_lastAuthSuccess = false;
                    if (m_authCallback) {
                        m_authCallback(AuthResult::SpoofingDetected, -1, "",
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
                        if (!m_running) {
                            m_isProcessingFrame = false;
                            return;
                        }
                        if (!featureRes.has_value()) {
                            m_lastAuthSuccess = false;
                            if (m_authCallback) {
                                m_authCallback(AuthResult::Error, -1, "", QStringLiteral("特征提取模型未就绪或异常"));
                            }
                            m_isProcessingFrame = false;
                            return;
                        }

                        std::vector<float> feature = featureRes.value();

                        m_faceRepository->matchFeatureAsync(feature, m_similarityThreshold,
                            [this, faceBox](bool isMatched, int matchedUid) {
                                if (!m_running) {
                                    m_isProcessingFrame = false;
                                    return;
                                }
                                if (isMatched) {
                                    m_userRepository->getUserByIdAsync(matchedUid, [this, faceBox, matchedUid](std::optional<User> userOpt) {
                                        if (!m_running) {
                                            m_isProcessingFrame = false;
                                            return;
                                        }
                                        if (userOpt) {
                                            m_lastAuthSuccess = true;
                                            m_lastFaceBox = faceBox.box;
                                            m_lastMatchedUid = matchedUid;
                                            if (m_authCallback) {
                                                m_authCallback(AuthResult::Success, userOpt->uid(), userOpt->username(),
                                                               QStringLiteral("识别成功，欢迎回来！"));
                                            }
                                        }
                                        m_isProcessingFrame = false;
                                    });
                                } else {
                                    m_lastAuthSuccess = false;
                                    m_lastMatchedUid = -1;
                                    if (m_authCallback) {
                                        m_authCallback(AuthResult::Unrecognized, -1, "",
                                                       QStringLiteral("未匹配到注册特征，请重新尝试"));
                                    }
                                    m_isProcessingFrame = false;
                                }
                            });
                    });
            });
    });
}

bool FaceLoginUseCase::hasUserFace(const QString& account) const {
    if (!m_userRepository) return false;
    std::promise<bool> promise;
    m_userRepository->hasUserFaceAsync(account, [&promise](bool result) {
        promise.set_value(result);
    });
    return promise.get_future().get();
}

bool FaceLoginUseCase::hasUserFace(int uid) const {
    if (!m_userRepository) return false;
    std::promise<bool> promise;
    m_userRepository->hasUserFaceAsync(uid, [&promise](bool result) {
        promise.set_value(result);
    });
    return promise.get_future().get();
}
