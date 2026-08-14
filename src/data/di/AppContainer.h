#pragma once

#include <memory>
#include "domain/usecase/FaceLoginUseCase.h"
#include "domain/usecase/FaceEnrollUseCase.h"
#include "domain/usecase/PasswordLoginUseCase.h"
#include "domain/usecase/SmsLoginUseCase.h"
#include "domain/usecase/RegisterUseCase.h"
#include "domain/usecase/ResetPasswordUseCase.h"
#include "domain/usecase/BehaviorRiskService.h"
#include "domain/usecase/CaptchaService.h"
/**
 * @brief 依赖注入中心 / 服务定位器 (Service Locator)
 *
 * 在应用启动时组装所有 DataSources, Repositories, 和 UseCases。
 */
class AppContainer {
 public:
  // 初始化全局依赖，可配置是否启用远程模式
  static void init(bool useRemote = false);

  // 获取行为风控网域服务
  static std::shared_ptr<BehaviorRiskService> behaviorRiskService();

  // 获取验证码服务
  static std::shared_ptr<CaptchaService> captchaService();

  // 获取各认证用例
  static std::shared_ptr<FaceLoginUseCase> faceLoginUseCase();
  static std::shared_ptr<FaceEnrollUseCase> faceEnrollUseCase();
  static std::shared_ptr<PasswordLoginUseCase> passwordLoginUseCase();
  static std::shared_ptr<SmsLoginUseCase> smsLoginUseCase();
  static std::shared_ptr<RegisterUseCase> registerUseCase();
  static std::shared_ptr<ResetPasswordUseCase> resetPasswordUseCase();

  // 获取用户仓库 (保留供直接数据访问场景使用)
  static std::shared_ptr<UserRepository> userRepository();

  AppContainer() = default;

  static std::shared_ptr<BehaviorRiskService> s_behaviorRiskService;
  static std::shared_ptr<UserRepository> s_userRepository;
  static std::shared_ptr<CaptchaService> s_captchaService;
  static std::shared_ptr<CaptchaRepository> s_captchaRepository;

  static std::shared_ptr<FaceAuthRepository> s_faceAuthRepository;

  static std::shared_ptr<FaceLoginUseCase> s_faceLoginUseCase;
  static std::shared_ptr<FaceEnrollUseCase> s_faceEnrollUseCase;
  static std::shared_ptr<PasswordLoginUseCase> s_passwordLoginUseCase;
  static std::shared_ptr<SmsLoginUseCase> s_smsLoginUseCase;
  static std::shared_ptr<RegisterUseCase> s_registerUseCase;
  static std::shared_ptr<ResetPasswordUseCase> s_resetPasswordUseCase;
};
