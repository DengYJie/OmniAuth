#pragma once
#include "domain/repository/FaceAuthRepository.h"
#include <memory>

class FaceAuthRepositoryImpl : public FaceAuthRepository {
public:
    FaceAuthRepositoryImpl(std::unique_ptr<FaceAuthRepository> localSource,
                       std::unique_ptr<FaceAuthRepository> remoteSource);
                       
    void setUseRemote(bool useRemote);
    bool init() override;
    void detectFaceAsync(const cv::Mat& frame, std::function<void(std::expected<FaceDetectionResult, AuthError>)> cb);
    void checkLivenessAsync(const cv::Mat& frame, const FaceBox& faceBox, float threshold, std::function<void(std::expected<float, AuthError>)> cb);
    void extractFeatureAsync(const cv::Mat& alignedFace, std::function<void(std::expected<std::vector<float>, AuthError>)> cb);
    void matchFeatureAsync(const std::vector<float>& feature, float threshold, std::function<void(bool, int)> cb);

private:
    std::unique_ptr<FaceAuthRepository> m_localSource;
    std::unique_ptr<FaceAuthRepository> m_remoteSource;
    bool m_useRemote = false;
};
