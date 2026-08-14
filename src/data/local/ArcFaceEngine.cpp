#include "ArcFaceEngine.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/calib3d.hpp>
#include <QDebug>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <numeric>

ArcFaceEngine::ArcFaceEngine(Ort::Env& env, std::string modelPath)
    : m_env(env), m_modelPath(std::move(modelPath)) {
    m_sessionOptions.SetIntraOpNumThreads(1);
    m_sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
}

bool ArcFaceEngine::init() {
    try {
        std::ifstream f(m_modelPath.c_str());
        if (!f.good()) {
            qWarning() << "ArcFace model file not found at:" << QString::fromStdString(m_modelPath);
            m_initialized = false;
            return false;
        }
        f.close();

#ifdef _WIN32
        std::wstring wModelPath(m_modelPath.begin(), m_modelPath.end());
        m_session = std::make_unique<Ort::Session>(m_env, wModelPath.c_str(), m_sessionOptions);
#else
        m_session = std::make_unique<Ort::Session>(m_env, m_modelPath.c_str(), m_sessionOptions);
#endif

        size_t numInputNodes = m_session->GetInputCount();
        if (numInputNodes > 0) {
            auto inputName = m_session->GetInputNameAllocated(0, m_allocator);
            m_inputName = inputName.get();
        }

        size_t numOutputNodes = m_session->GetOutputCount();
        m_outputNames.clear();
        for (size_t i = 0; i < numOutputNodes; ++i) {
            auto outputName = m_session->GetOutputNameAllocated(i, m_allocator);
            m_outputNames.push_back(outputName.get());
        }

        m_initialized = true;
        qDebug() << "ArcFaceEngine initialized successfully.";
        return true;
    } catch (const std::exception& e) {
        qWarning() << "Failed to initialize ArcFaceEngine:" << e.what();
        m_initialized = false;
        return false;
    }
}

cv::Mat ArcFaceEngine::alignFace(const cv::Mat& frame, const std::vector<cv::Point2f>& landmarks) {
    if (frame.empty() || landmarks.size() < 5) {
        return cv::Mat();
    }

    // Standard 112x112 ArcFace reference landmarks
    static const std::vector<cv::Point2f> refLandmarks = {
        cv::Point2f(38.2946f, 51.6963f), // Left eye
        cv::Point2f(73.5318f, 51.5014f), // Right eye
        cv::Point2f(56.0252f, 71.7366f), // Nose
        cv::Point2f(41.5493f, 92.3655f), // Left mouth
        cv::Point2f(70.7299f, 92.2041f)  // Right mouth
    };

    cv::Mat transMatrix = cv::estimateAffinePartial2D(landmarks, refLandmarks);
    if (transMatrix.empty()) {
        // Fallback to simple crop/resize if affine estimation fails
        cv::Mat resized;
        cv::resize(frame, resized, cv::Size(112, 112));
        return resized;
    }

    cv::Mat aligned;
    cv::warpAffine(frame, aligned, transMatrix, cv::Size(112, 112), cv::INTER_LINEAR, cv::BORDER_REFLECT);
    return aligned;
}

std::expected<std::vector<float>, AuthError> ArcFaceEngine::extractFeature(const cv::Mat& alignedFace) {
    if (alignedFace.empty()) {
        return std::unexpected(AuthError::NoFace);
    }
    if (m_initFailed) {
        return std::unexpected(AuthError::ModelError);
    }
    if (!m_initialized) {
        if (!init()) {
            m_initFailed = true;
            return std::unexpected(AuthError::ModelError);
        }
    }

    try {
        // Preprocess: BGR -> RGB, 112x112, scale=1.0/127.5, mean=(127.5, 127.5, 127.5) -> normalized to [-1, 1]
        cv::Mat blob;
        cv::dnn::blobFromImage(alignedFace, blob, 1.0 / 127.5, cv::Size(m_inputWidth, m_inputHeight), cv::Scalar(127.5, 127.5, 127.5), true, false, CV_32F);

        // Zero-copy: blob is already continuous NCHW Float32
        std::vector<int64_t> inputShape = {1, 3, m_inputHeight, m_inputWidth};
        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo, blob.ptr<float>(), blob.total(), inputShape.data(), inputShape.size());

        m_inputNamePtrs = {m_inputName.c_str()};
        m_outputNamePtrs.clear();
        for (const auto& name : m_outputNames) {
            m_outputNamePtrs.push_back(name.c_str());
        }

        auto outputTensors = m_session->Run(
            Ort::RunOptions{nullptr},
            m_inputNamePtrs.data(), &inputTensor, 1,
            m_outputNamePtrs.data(), m_outputNamePtrs.size());

        if (outputTensors.empty()) {
            return std::unexpected(AuthError::ModelError);
        }

        const float* featurePtr = outputTensors[0].GetTensorData<float>();
        auto typeInfo = outputTensors[0].GetTensorTypeAndShapeInfo();
        size_t numElements = typeInfo.GetElementCount();

        std::vector<float> feature(featurePtr, featurePtr + numElements);

        // Normalize vector (L2 norm)
        float sumSquare = 0.0f;
        for (float val : feature) {
            sumSquare += val * val;
        }
        float norm = std::sqrt(sumSquare);
        if (norm > 1e-6f) {
            for (float& val : feature) {
                val /= norm;
            }
        }

        return feature;
    } catch (const Ort::Exception& e) {
        qWarning() << "ArcFace ONNX Exception:" << e.what();
        return std::unexpected(AuthError::ModelError);
    } catch (const std::exception& e) {
        qWarning() << "ArcFace Exception:" << e.what();
        return std::unexpected(AuthError::ModelError);
    }
}
