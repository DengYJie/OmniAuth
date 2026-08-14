#include "data/di/AppContainer.h"
#include "data/local/LocalRiskDataSource.h"
#include "data/remote/RemoteRiskDataSource.h"
#include "data/repository/BehaviorRiskRepositoryImpl.h"
#include "data/local/LocalUserDataSource.h"
#include "data/remote/RemoteUserDataSource.h"
#include "data/local/LocalCaptchaDataSource.h"
#include "data/remote/RemoteCaptchaDataSource.h"
#include "data/local/LocalFaceDataSource.h"
#include "data/remote/RemoteFaceDataSource.h"
#include "data/repository/UserRepositoryImpl.h"
#include "data/repository/FaceAuthRepositoryImpl.h"
#include "data/repository/CaptchaRepositoryImpl.h"
std::shared_ptr<BehaviorRiskService> AppContainer::s_behaviorRiskService = nullptr;
std::shared_ptr<UserRepository> AppContainer::s_userRepository = nullptr;
std::shared_ptr<CaptchaService> AppContainer::s_captchaService = nullptr;
std::shared_ptr<CaptchaRepository> AppContainer::s_captchaRepository = nullptr;
std::shared_ptr<FaceAuthRepository> AppContainer::s_faceAuthRepository = nullptr;
std::shared_ptr<FaceLoginUseCase> AppContainer::s_faceLoginUseCase = nullptr;
std::shared_ptr<FaceEnrollUseCase> AppContainer::s_faceEnrollUseCase = nullptr;
std::shared_ptr<PasswordLoginUseCase> AppContainer::s_passwordLoginUseCase = nullptr;
std::shared_ptr<SmsLoginUseCase> AppContainer::s_smsLoginUseCase = nullptr;
std::shared_ptr<RegisterUseCase> AppContainer::s_registerUseCase = nullptr;
std::shared_ptr<ResetPasswordUseCase> AppContainer::s_resetPasswordUseCase = nullptr;
void AppContainer::init(bool useRemote) {
  // 1. 实例化 Data Sources
  auto localRiskDS = std::make_unique<LocalRiskDataSource>();
  auto remoteRiskDS = std::make_unique<RemoteRiskDataSource>();

  // 这里可以异步预热本地模型，避免第一次卡顿
  // 由于 LocalRiskDataSource::init() 尚未暴露出线程安全保证，我们现在可以由它内部在 evaluateAsync 时处理或在此处预热
  // 为了安全，我们可以在主线程初始化或者交给后台线程
  localRiskDS->init();

  // 2. 实例化 Repositories
  auto riskRepo = std::make_shared<BehaviorRiskRepositoryImpl>(std::move(localRiskDS), std::move(remoteRiskDS));
  riskRepo->setUseRemote(useRemote);

  // 初始化 User DataSource
  auto localUserDS = std::make_unique<LocalUserDataSource>();
  auto remoteUserDS = std::make_unique<RemoteUserDataSource>();
  localUserDS->init();

  auto userRepo = std::make_shared<UserRepositoryImpl>(std::move(localUserDS), std::move(remoteUserDS));
  userRepo->setUseRemote(useRemote);
  s_userRepository = userRepo;

  // 3. 实例化 Services
  s_behaviorRiskService = std::make_shared<BehaviorRiskService>(riskRepo);

  // 初始化人脸模块
  auto localFaceDS = std::make_unique<LocalFaceDataSource>(s_userRepository);
  auto remoteFaceDS = std::make_unique<RemoteFaceDataSource>();
  localFaceDS->init();
  auto faceRepo = std::make_shared<FaceAuthRepositoryImpl>(std::move(localFaceDS), std::move(remoteFaceDS));
  faceRepo->setUseRemote(useRemote);
  s_faceAuthRepository = faceRepo;

  // 4. 认证用例 (替代旧的统一 AuthService)
  s_faceLoginUseCase = std::make_shared<FaceLoginUseCase>(s_faceAuthRepository, s_userRepository);
  s_faceEnrollUseCase = std::make_shared<FaceEnrollUseCase>(s_faceAuthRepository, s_userRepository);
  s_passwordLoginUseCase = std::make_shared<PasswordLoginUseCase>(s_userRepository);
  s_smsLoginUseCase = std::make_shared<SmsLoginUseCase>(s_userRepository);
  s_registerUseCase = std::make_shared<RegisterUseCase>(s_userRepository);
  s_resetPasswordUseCase = std::make_shared<ResetPasswordUseCase>(s_userRepository);

  // 5. 初始化验证码模块
  auto localCaptchaDS = std::make_unique<LocalCaptchaDataSource>();
  auto remoteCaptchaDS = std::make_unique<RemoteCaptchaDataSource>();
  auto captchaRepo = std::make_shared<CaptchaRepositoryImpl>(std::move(localCaptchaDS), std::move(remoteCaptchaDS));
  captchaRepo->setUseRemote(useRemote);
  s_captchaRepository = captchaRepo;
  s_captchaService = std::make_shared<CaptchaService>(s_captchaRepository);
}

std::shared_ptr<BehaviorRiskService> AppContainer::behaviorRiskService() {
  return s_behaviorRiskService;
}

std::shared_ptr<CaptchaService> AppContainer::captchaService() {
  return s_captchaService;
}

std::shared_ptr<FaceLoginUseCase> AppContainer::faceLoginUseCase() {
  return s_faceLoginUseCase;
}

std::shared_ptr<FaceEnrollUseCase> AppContainer::faceEnrollUseCase() {
  return s_faceEnrollUseCase;
}

std::shared_ptr<PasswordLoginUseCase> AppContainer::passwordLoginUseCase() {
  return s_passwordLoginUseCase;
}

std::shared_ptr<SmsLoginUseCase> AppContainer::smsLoginUseCase() {
  return s_smsLoginUseCase;
}

std::shared_ptr<RegisterUseCase> AppContainer::registerUseCase() {
  return s_registerUseCase;
}

std::shared_ptr<ResetPasswordUseCase> AppContainer::resetPasswordUseCase() {
  return s_resetPasswordUseCase;
}

std::shared_ptr<UserRepository> AppContainer::userRepository() {
  return s_userRepository;
}
