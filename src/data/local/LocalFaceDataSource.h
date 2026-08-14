#pragma once

#include "domain/repository/UserRepository.h"
#include "domain/repository/FaceAuthRepository.h"
#include <memory>

namespace Ort {
    struct Env;
}
class RetinaFaceEngine;
class AntiSpoofingEngine;
class ArcFaceEngine;

class LocalFaceDataSource : public FaceAuthRepository {
public:
    explicit LocalFaceDataSource(std::shared_ptr<UserRepository> userRepository);
    ~LocalFaceDataSource() override;

    bool init() override;
    void detectFaceAsync(const cv::Mat& frame,
        std::function<void(std::expected<FaceDetectionResult, AuthError>)> callback) override;

    void checkLivenessAsync(const cv::Mat& frame, const FaceBox& faceBox, float threshold,
        std::function<void(std::expected<float, AuthError>)> callback) override;

    void extractFeatureAsync(const cv::Mat& alignedFace,
        std::function<void(std::expected<std::vector<float>, AuthError>)> callback) override;

    void matchFeatureAsync(const std::vector<float>& feature, float threshold,
        std::function<void(bool matched, int uid)> callback) override;

private:
    std::shared_ptr<UserRepository> m_userRepo;
    std::unique_ptr<Ort::Env> m_ortEnv;
    std::unique_ptr<RetinaFaceEngine> m_retinaEngine;
    std::unique_ptr<AntiSpoofingEngine> m_antiSpoofEngine;
    std::unique_ptr<ArcFaceEngine> m_arcFaceEngine;
    std::vector<std::pair<int, std::vector<float>>> m_featureCache;
    void reloadFaceFeatureCache();
};
