#include "AntiSpoofingEngine.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>
#include <QDebug>
#include <fstream>
#include <cmath>
#include <algorithm>

AntiSpoofingEngine::AntiSpoofingEngine(Ort::Env& env, std::string modelPath)
    : m_env(env), m_modelPath(std::move(modelPath)) {
    m_sessionOptions.SetIntraOpNumThreads(1);
    m_sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
}

bool AntiSpoofingEngine::init() {
    try {
        std::ifstream f(m_modelPath.c_str());
        if (!f.good()) {
            qWarning() << "MiniFASNet model file not found at:" << QString::fromStdString(m_modelPath);
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
        qDebug() << "AntiSpoofingEngine initialized successfully.";
        return true;
    } catch (const std::exception& e) {
        qWarning() << "Failed to initialize AntiSpoofingEngine:" << e.what();
        m_initialized = false;
        return false;
    }
}

std::expected<float, AuthError> AntiSpoofingEngine::checkLiveness(const cv::Mat& frame, const FaceBox& faceBox, float threshold) {
    if (frame.empty()) {
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
        // Expand bounding box for MiniFASNet
        const cv::Rect& rect = faceBox.box;
        int cx = rect.x + rect.width / 2;
        int cy = rect.y + rect.height / 2;
        int maxSide = std::max(rect.width, rect.height);

        int expandedSide = static_cast<int>(maxSide * m_scaleParam);
        int x1 = std::max(0, cx - expandedSide / 2);
        int y1 = std::max(0, cy - expandedSide / 2);
        int x2 = std::min(frame.cols, cx + expandedSide / 2);
        int y2 = std::min(frame.rows, cy + expandedSide / 2);

        cv::Rect expandedRect(x1, y1, x2 - x1, y2 - y1);
        if (expandedRect.width <= 0 || expandedRect.height <= 0) {
            return std::unexpected(AuthError::NoFace);
        }

        cv::Mat crop = frame(expandedRect);

        // Preprocess: keep BGR (official MiniFASNet expects BGR), resize to 80x80,
        // normalize pixel values to [0, 1] via scale = 1/255
        cv::Mat blob;
        cv::dnn::blobFromImage(crop, blob, 1.0 / 255.0, cv::Size(m_inputWidth, m_inputHeight), cv::Scalar(0, 0, 0), false, false, CV_32F);

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

        const float* logits = outputTensors[0].GetTensorData<float>();
        // MiniFASNet outputs 3 classes (0: Fake/Spoof, 1: Real face, 2: Spoof/Other) or 2 classes (0: Spoof, 1: Real)
        // Apply softmax over logits
        float scoreReal = 0.0f;
        auto typeInfo = outputTensors[0].GetTensorTypeAndShapeInfo();
        size_t numClasses = typeInfo.GetElementCount();

        if (numClasses >= 3) {
            float expSum = 0.0f;
            std::vector<float> exps(numClasses);
            for (size_t i = 0; i < numClasses; ++i) {
                exps[i] = std::exp(logits[i]);
                expSum += exps[i];
            }
            if (expSum > 0.00001f) {
                // In Silent-Face-Anti-Spoofing (MiniFASNet), Class 2 is Real face, Class 0/1 are 2D/Screen attacks
                scoreReal = exps[2] / expSum;
            }
        } else if (numClasses == 2) {
            float expSum = std::exp(logits[0]) + std::exp(logits[1]);
            scoreReal = std::exp(logits[1]) / expSum;
        } else {
            scoreReal = logits[0];
        }

        return scoreReal;
    } catch (const Ort::Exception& e) {
        qWarning() << "MiniFASNet ONNX Exception:" << e.what();
        return std::unexpected(AuthError::ModelError);
    } catch (const std::exception& e) {
        qWarning() << "AntiSpoofing Exception:" << e.what();
        return std::unexpected(AuthError::ModelError);
    }
}
