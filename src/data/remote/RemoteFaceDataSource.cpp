#include "RemoteFaceDataSource.h"

bool RemoteFaceDataSource::init() {
    return true;
}

void RemoteFaceDataSource::detectFaceAsync(const cv::Mat& frame, 
                                           std::function<void(std::expected<FaceDetectionResult, AuthError>)> callback) {
    if (callback) callback(std::unexpected(AuthError::ModelError));
}

void RemoteFaceDataSource::checkLivenessAsync(const cv::Mat& frame, const FaceBox& faceBox, float threshold,
                                              std::function<void(std::expected<float, AuthError>)> callback) {
    if (callback) callback(std::unexpected(AuthError::ModelError));
}

void RemoteFaceDataSource::extractFeatureAsync(const cv::Mat& alignedFace, 
                                               std::function<void(std::expected<std::vector<float>, AuthError>)> callback) {
    if (callback) callback(std::unexpected(AuthError::ModelError));
}

void RemoteFaceDataSource::matchFeatureAsync(const std::vector<float>& feature, float threshold,
                                             std::function<void(bool matched, int uid)> callback) {
    if (callback) callback(false, -1);
}
