#pragma once
#include "domain/repository/FaceAuthRepository.h"

class RemoteFaceDataSource : public FaceAuthRepository {
public:
    ~RemoteFaceDataSource() override = default;

    bool init() override;

    void detectFaceAsync(const cv::Mat& frame, 
                         std::function<void(std::expected<FaceDetectionResult, AuthError>)> callback) override;

    void checkLivenessAsync(const cv::Mat& frame, const FaceBox& faceBox, float threshold,
                            std::function<void(std::expected<float, AuthError>)> callback) override;

    void extractFeatureAsync(const cv::Mat& alignedFace, 
                             std::function<void(std::expected<std::vector<float>, AuthError>)> callback) override;

    void matchFeatureAsync(const std::vector<float>& feature, float threshold,
                           std::function<void(bool matched, int uid)> callback) override;
};
