#include "RetinaFaceEngine.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>
#include <QDebug>
#include <fstream>
#include <cmath>
#include <algorithm>

RetinaFaceEngine::RetinaFaceEngine(Ort::Env& env, std::string modelPath)
    : m_env(env), m_modelPath(std::move(modelPath)) {
    m_sessionOptions.SetIntraOpNumThreads(1);
    m_sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
}

bool RetinaFaceEngine::init() {
    try {
        std::ifstream f(m_modelPath.c_str());
        if (!f.good()) {
            qWarning() << "RetinaFace model file not found at:" << QString::fromStdString(m_modelPath);
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

        generateAnchors();
        m_initialized = true;
        qDebug() << "RetinaFaceEngine initialized successfully.";
        return true;
    } catch (const std::exception& e) {
        qWarning() << "Failed to initialize RetinaFaceEngine:" << e.what();
        m_initialized = false;
        return false;
    }
}

void RetinaFaceEngine::generateAnchors() {
    m_anchors.clear();
    std::vector<int> strides = {8, 16, 32};
    std::vector<std::vector<int>> minSizes = {{16, 32}, {64, 128}, {256, 512}};

    for (size_t i = 0; i < strides.size(); ++i) {
        int stride = strides[i];
        int featureW = std::ceil((float)m_inputWidth / stride);
        int featureH = std::ceil((float)m_inputHeight / stride);

        for (int y = 0; y < featureH; ++y) {
            for (int x = 0; x < featureW; ++x) {
                for (int minSize : minSizes[i]) {
                    Anchor anchor;
                    anchor.cx = (x + 0.5f) * stride / m_inputWidth;
                    anchor.cy = (y + 0.5f) * stride / m_inputHeight;
                    anchor.w = (float)minSize / m_inputWidth;
                    anchor.h = (float)minSize / m_inputHeight;
                    m_anchors.push_back(anchor);
                }
            }
        }
    }
}

std::expected<FaceDetectionResult, AuthError> RetinaFaceEngine::detect(const cv::Mat& frame) {
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
        // Preprocessing: keep BGR, resize to 640x640, mean subtraction (104, 117, 123)
        cv::Mat blob;
        cv::dnn::blobFromImage(frame, blob, 1.0, cv::Size(m_inputWidth, m_inputHeight), cv::Scalar(104.0, 117.0, 123.0), false, false, CV_32F);

        // Zero-copy: blob is already continuous NCHW Float32. Use its pointer directly.
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

        // Decode boxes, scores, and landmarks
        float frameW = static_cast<float>(frame.cols);
        float frameH = static_cast<float>(frame.rows);

        m_candidateBoxes.clear();
        m_candidateScores.clear();
        m_candidateLandmarks.clear();

        const float* locPtr = nullptr;
        const float* confPtr = nullptr;
        const float* landmPtr = nullptr;

        for (size_t i = 0; i < outputTensors.size(); ++i) {
            auto shapeInfo = outputTensors[i].GetTensorTypeAndShapeInfo();
            auto shape = shapeInfo.GetShape();
            int64_t dim = shape.empty() ? 0 : shape.back();
            if (dim == 4) {
                locPtr = outputTensors[i].GetTensorData<float>();
            } else if (dim == 2) {
                confPtr = outputTensors[i].GetTensorData<float>();
            } else if (dim == 10) {
                landmPtr = outputTensors[i].GetTensorData<float>();
            }
        }

        if (!locPtr || !confPtr) {
            qWarning() << "[RetinaFace] Missing locPtr or confPtr in model outputs!";
            return std::unexpected(AuthError::ModelError);
        }

        size_t numAnchors = m_anchors.size();
        for (size_t i = 0; i < numAnchors; ++i) {
            float score = confPtr[i * 2 + 1]; // background vs face score
            if (score > m_scoreThreshold) {
                const auto& anchor = m_anchors[i];
                    
                    // Decode box: loc = [dx, dy, dw, dh]
                    float dx = locPtr[i * 4 + 0];
                    float dy = locPtr[i * 4 + 1];
                    float dw = locPtr[i * 4 + 2];
                    float dh = locPtr[i * 4 + 3];

                    float cx = anchor.cx + dx * 0.1f * anchor.w;
                    float cy = anchor.cy + dy * 0.1f * anchor.h;
                    float w = anchor.w * std::exp(dw * 0.2f);
                    float h = anchor.h * std::exp(dh * 0.2f);

                    float x1 = (cx - w / 2.0f) * frameW;
                    float y1 = (cy - h / 2.0f) * frameH;
                    float boxW = w * frameW;
                    float boxH = h * frameH;

                    cv::Rect box(
                        std::max(0, static_cast<int>(x1)),
                        std::max(0, static_cast<int>(y1)),
                        std::min(static_cast<int>(boxW), static_cast<int>(frameW - x1)),
                        std::min(static_cast<int>(boxH), static_cast<int>(frameH - y1))
                    );

                    std::vector<cv::Point2f> landmarks(5);
                    if (landmPtr) {
                        for (int k = 0; k < 5; ++k) {
                            float lx = anchor.cx + landmPtr[i * 10 + k * 2] * 0.1f * anchor.w;
                            float ly = anchor.cy + landmPtr[i * 10 + k * 2 + 1] * 0.1f * anchor.h;
                            landmarks[k] = cv::Point2f(lx * frameW, ly * frameH);
                        }
                    } else {
                        // Estimate landmarks if absent
                        landmarks[0] = cv::Point2f(box.x + box.width * 0.3f, box.y + box.height * 0.35f);
                        landmarks[1] = cv::Point2f(box.x + box.width * 0.7f, box.y + box.height * 0.35f);
                        landmarks[2] = cv::Point2f(box.x + box.width * 0.5f, box.y + box.height * 0.55f);
                        landmarks[3] = cv::Point2f(box.x + box.width * 0.35f, box.y + box.height * 0.75f);
                        landmarks[4] = cv::Point2f(box.x + box.width * 0.65f, box.y + box.height * 0.75f);
                    }
                    m_candidateBoxes.push_back(box);
                    m_candidateScores.push_back(score);
                    m_candidateLandmarks.push_back(landmarks);
                }
            }

        // Apply NMS
        std::vector<int> indices;
        cv::dnn::NMSBoxes(m_candidateBoxes, m_candidateScores, m_scoreThreshold, m_nmsThreshold, indices);

        if (indices.empty()) {
            return std::unexpected(AuthError::NoFace);
        }
        if (indices.size() > 1) {
            return std::unexpected(AuthError::MultipleFaces);
        }

        int bestIdx = indices[0];
        FaceBox faceBox;
        faceBox.box = m_candidateBoxes[bestIdx];
        faceBox.landmarks = m_candidateLandmarks[bestIdx];
        faceBox.confidence = m_candidateScores[bestIdx];

        return FaceDetectionResult{faceBox};
    } catch (const Ort::Exception& e) {
        qWarning() << "RetinaFace ONNX Exception:" << e.what();
        return std::unexpected(AuthError::ModelError);
    } catch (const std::exception& e) {
        qWarning() << "RetinaFace Exception:" << e.what();
        return std::unexpected(AuthError::ModelError);
    }
}
