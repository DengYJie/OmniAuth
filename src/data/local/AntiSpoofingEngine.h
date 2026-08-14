#pragma once

#include "domain/model/FaceTypes.h"
#include <string>
#include <memory>
#include <vector>
#include <expected>
#include <onnxruntime_cxx_api.h>
#include <opencv2/core.hpp>

class AntiSpoofingEngine {
public:
    explicit AntiSpoofingEngine(Ort::Env& env, std::string modelPath = "res/models/minifasnet.onnx");
    ~AntiSpoofingEngine() = default;

    bool init();
    bool isInitialized() const { return m_initialized; }

    std::expected<float, AuthError> checkLiveness(const cv::Mat& frame, const FaceBox& faceBox, float threshold = 0.85f);

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

    int m_inputWidth = 80;
    int m_inputHeight = 80;
    float m_scaleParam = 2.7f; // Expanded scale factor for MiniFASNet
};
