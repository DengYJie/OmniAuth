#pragma once

#include "domain/model/FaceTypes.h"
#include <string>
#include <memory>
#include <vector>
#include <expected>
#include <onnxruntime_cxx_api.h>
#include <opencv2/core.hpp>

class RetinaFaceEngine {
public:
    explicit RetinaFaceEngine(Ort::Env& env, std::string modelPath = "res/models/retinaface.onnx");
    ~RetinaFaceEngine() = default;

    bool init();
    bool isInitialized() const { return m_initialized; }

    std::expected<FaceDetectionResult, AuthError> detect(const cv::Mat& frame);

private:
    std::string m_modelPath;
    bool m_initialized = false;
    bool m_initFailed = false;
    Ort::Env& m_env;
    Ort::SessionOptions m_sessionOptions;
    std::unique_ptr<Ort::Session> m_session;
    Ort::AllocatorWithDefaultOptions m_allocator;

    std::string m_inputName;
    std::vector<std::string> m_outputNames;
    std::vector<const char*> m_inputNamePtrs;
    std::vector<const char*> m_outputNamePtrs;

    int m_inputWidth = 640;
    int m_inputHeight = 640;
    float m_scoreThreshold = 0.5f;
    float m_nmsThreshold = 0.4f;

    void generateAnchors();
    struct Anchor {
        float cx, cy, w, h;
    };
    std::vector<Anchor> m_anchors;

    // Pre-allocated buffers for zero per-frame allocation
    cv::Mat m_blob;
    std::vector<int> m_indices;
    std::vector<cv::Rect> m_candidateBoxes;
    std::vector<float> m_candidateScores;
    std::vector<std::vector<cv::Point2f>> m_candidateLandmarks;
};
