#pragma once

#include "domain/model/FaceTypes.h"
#include <string>
#include <memory>
#include <vector>
#include <expected>
#include <onnxruntime_cxx_api.h>
#include <opencv2/core.hpp>

class ArcFaceEngine {
public:
    explicit ArcFaceEngine(Ort::Env& env, std::string modelPath = "res/models/arcface.onnx");
    ~ArcFaceEngine() = default;

    bool init();
    bool isInitialized() const { return m_initialized; }

    static cv::Mat alignFace(const cv::Mat& frame, const std::vector<cv::Point2f>& landmarks);
    std::expected<std::vector<float>, AuthError> extractFeature(const cv::Mat& alignedFace);

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

    int m_inputWidth = 112;
    int m_inputHeight = 112;
};
