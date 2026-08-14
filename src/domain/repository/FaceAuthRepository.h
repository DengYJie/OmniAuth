#pragma once
#include <opencv2/core.hpp>
#include <functional>
#include <vector>
#include <expected>
#include <QString>
#include "domain/model/FaceTypes.h"

class FaceAuthRepository {
public:
    virtual ~FaceAuthRepository() = default;
    virtual bool init() = 0;
    // 1. 人脸检测 (异步获取单帧内的人脸框与关键点)
    virtual void detectFaceAsync(const cv::Mat& frame, 
                                 std::function<void(std::expected<FaceDetectionResult, AuthError>)> callback) = 0;

    // 2. 活体检测 (异步评估是否为真人)
    virtual void checkLivenessAsync(const cv::Mat& frame, const FaceBox& faceBox, float threshold,
                                    std::function<void(std::expected<float, AuthError>)> callback) = 0;

    // 3. 特征提取 (异步生成 512 维特征向量)
    virtual void extractFeatureAsync(const cv::Mat& alignedFace, 
                                     std::function<void(std::expected<std::vector<float>, AuthError>)> callback) = 0;

    // 4. 特征匹配 (异步比对已知人脸库)
    virtual void matchFeatureAsync(const std::vector<float>& feature, float threshold,
                                   std::function<void(bool matched, int uid)> callback) = 0;
};
