#include "BehaviorRiskEngine.h"

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <array>

BehaviorRiskEngine::BehaviorRiskEngine(std::string modelPath) 
    : m_modelPath(std::move(modelPath)) {}

bool BehaviorRiskEngine::init() {
  if (m_initialized) {
    return true;
  }
  if (m_initFailed) {
    return false;
  }

  try {
    m_env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "BehaviorRiskEngine");

    m_sessionOptions.SetIntraOpNumThreads(1);
    m_sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

#ifdef _WIN32
    std::wstring wModelPath;
    if (m_modelPath.starts_with("models/")) {
      QString fullPath = QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromStdString(m_modelPath));
      wModelPath = fullPath.toStdWString();
    } else {
      std::string fullStr = QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromStdString(m_modelPath)).toStdString();
      wModelPath = std::wstring(fullStr.begin(), fullStr.end());
    }
    m_session = std::make_unique<Ort::Session>(*m_env, wModelPath.c_str(), m_sessionOptions);
#else
    std::string fullPath = m_modelPath;
    if (m_modelPath.starts_with("models/")) {
      fullPath = QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromStdString(m_modelPath)).toStdString();
    }
    m_session = std::make_unique<Ort::Session>(*m_env, fullPath.c_str(), m_sessionOptions);
#endif

    Ort::TypeInfo inputTypeInfo = m_session->GetInputTypeInfo(0);
    auto inputTensorInfo = inputTypeInfo.GetTensorTypeAndShapeInfo();
    
    Ort::AllocatedStringPtr inputNameStr = m_session->GetInputNameAllocated(0, m_allocator);
    m_inputName = inputNameStr.get();
    m_inputNamePtrs.push_back(m_inputName.c_str());

    Ort::AllocatedStringPtr outputNameStr = m_session->GetOutputNameAllocated(0, m_allocator);
    m_outputName = outputNameStr.get();
    m_outputNamePtrs.push_back(m_outputName.c_str());

    m_initialized = true;
    qInfo() << "[BehaviorRiskEngine] Model initialized successfully from" << QString::fromStdString(m_modelPath);
    return true;

  } catch (const Ort::Exception& e) {
    qWarning() << "[BehaviorRiskEngine] ONNX Runtime init failed:" << e.what();
    m_initFailed = true;
    return false;
  } catch (const std::exception& e) {
    qWarning() << "[BehaviorRiskEngine] Init failed:" << e.what();
    m_initFailed = true;
    return false;
  }
}

std::expected<float, std::string> BehaviorRiskEngine::evaluate(const TrajectoryFeature& feature) {
  if (!m_initialized) {
    if (!init()) {
      return std::unexpected("Model not initialized and failed to init.");
    }
  }

  try {
    std::array<float, 10> featArr = feature.toArray();
    std::vector<float> inputTensorValues(featArr.begin(), featArr.end());
    std::vector<int64_t> inputDims = {1, 10};

    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memoryInfo, 
        inputTensorValues.data(), 
        inputTensorValues.size(),
        inputDims.data(), 
        inputDims.size());

    auto outputTensors = m_session->Run(
        Ort::RunOptions{nullptr}, 
        m_inputNamePtrs.data(), 
        &inputTensor, 1, 
        m_outputNamePtrs.data(), 1);

    if (outputTensors.empty() || !outputTensors.front().IsTensor()) {
      return std::unexpected("ONNX run returned invalid output tensor.");
    }

    const float* floatArr = outputTensors.front().GetTensorMutableData<float>();
    return floatArr[0];

  } catch (const Ort::Exception& e) {
    return std::unexpected(std::string("ONNX Run Error: ") + e.what());
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Evaluate Error: ") + e.what());
  }
}
