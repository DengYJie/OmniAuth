#pragma once

#include <vector>
#include <expected>
#include <opencv2/core.hpp>
#include <QMetaType>

struct FaceBox {
    cv::Rect box;
    std::vector<cv::Point2f> landmarks; // 5 landmarks: left eye, right eye, nose, left mouth, right mouth
    float confidence;
};

struct FaceDetectionResult {
    FaceBox face;
};

enum class AuthError {
    NoFace,
    MultipleFaces,
    SpoofDetected,
    ModelError
};
enum class AuthResult {
    Success,
    Failed,
    Unrecognized,
    SpoofingDetected,
    Verifying,
    Error
};

Q_DECLARE_METATYPE(AuthError)
Q_DECLARE_METATYPE(AuthResult)
