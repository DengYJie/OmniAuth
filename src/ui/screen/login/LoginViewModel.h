#pragma once

#include <QString>
#include <QTimer>

#include "ui/common/BaseViewModel.h"

struct LoginState {
  bool isCaptchaVisible = false;
  bool isLoggingIn = false;
  QString errorMessage;

  // 登录方式: 0 = 密码登录, 1 = 验证码登录
  int loginMode = 0;
  // 当前输入的账号是否已绑定人脸（失焦或停止输入 500ms 后轻量校验）
  bool hasFaceBound = false;
  bool isCheckingAccount = false;
  // 验证码相关
  bool isCodeSent = false;
  int codeCountdown = 0;

  bool operator==(const LoginState&) const = default;
};

class LoginViewModel : public BaseViewModel<LoginViewModel, LoginState> {
  Q_OBJECT

 public:
  explicit LoginViewModel(QObject* parent = nullptr);
  ~LoginViewModel() override;

  // Intents / Actions
  void switchLoginMode(int mode);
  void checkAccountFaceBinding(const QString& account);
  void loginClicked(const QString& account, const QString& password);
  void smsLoginClicked(const QString& account, const QString& code);
  void requestSmsCode();
  void captchaVerified();
  void captchaClosed();
  void resetError();

 signals:
  void stateChanged(const LoginState& state);
  void requestCaptcha();  // 验证码模式下唤起滑块验证
  void loginSuccess(int uid, const QString& username);    // 发送给上层进行页面跳转或处理

 protected:
  void emitStateChanged() override;

 private:
  QString m_pendingAccount;
  QString m_pendingPassword;
  QString m_pendingSmsCode;

  QTimer* m_countdownTimer = nullptr;
  QTimer* m_debounceTimer = nullptr;
};
