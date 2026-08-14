#pragma once

#include <onnxruntime_cxx_api.h>
#include <expected>
#include <memory>
#include <string>
#include <vector>

#include "domain/model/BehaviorTracker.h"

/**
 * @brief 行为风控 ONNX 模型推理引擎 (Infrastructure Layer)
 * 
 * 纯粹的基础设施组件，负责管理 ONNX Runtime 会话，并提供同步的数学模型推理能力。
 */
class BehaviorRiskEngine {
 public:
  explicit BehaviorRiskEngine(std::string modelPath = "models/behavior_mlp.onnx");
  ~BehaviorRiskEngine() = default;

  bool init();
  
  // 同步执行模型推理
  std::expected<float, std::string> evaluate(const TrajectoryFeature& feature);

 private:
  std::string m_modelPath;
  std::unique_ptr<Ort::Env> m_env;
  std::unique_ptr<Ort::Session> m_session;
  Ort::SessionOptions m_sessionOptions;
  Ort::AllocatorWithDefaultOptions m_allocator;

  bool m_initialized = false;
  bool m_initFailed = false;

  std::string m_inputName;
  std::string m_outputName;
  std::vector<const char*> m_inputNamePtrs;
  std::vector<const char*> m_outputNamePtrs;
};
