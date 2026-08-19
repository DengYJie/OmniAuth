#pragma once

#include <QString>

#include "ui/common/BaseViewModel.h"

#include <QTimer>

struct RegisterState {
  int currentStep = 1; // 1: Contact Auth, 2: Account & Password

  enum ContactType { Email, Phone } contactType = Email;

  // 邮箱 tab 独立状态
  bool emailCodeSent = false;
  int emailCountdown = 0;

  // 手机号 tab 独立状态
  bool phoneCodeSent = false;
  int phoneCountdown = 0;

  int passwordStrength = 0; // 0: None, 1: Weak, 2: Medium, 3: Strong

  bool isCaptchaVisible = false;
  bool isRegistering = false;
  QString errorMessage;

  bool operator==(const RegisterState&) const = default;
};

class RegisterViewModel : public BaseViewModel<RegisterViewModel, RegisterState> {
  Q_OBJECT

 public:
  explicit RegisterViewModel(QObject* parent = nullptr);
  ~RegisterViewModel() override;

  // Step 1 Actions
  void switchContactType(RegisterState::ContactType type);
  void triggerCaptcha(RegisterState::ContactType type);
  void captchaVerified();
  void captchaClosed();
  void requestVerificationCode(RegisterState::ContactType type, int waitSeconds = 60);
  void submitContactVerification(RegisterState::ContactType type,
                                 const QString& contact, const QString& code);

  // Step 2 Actions
  void updatePasswordStrength(const QString& pwd);
  void submitRegister(const QString& account,
                      RegisterState::ContactType contactType,
                      const QString& contact,
                      const QString& password,
                      const QString& confirmPassword,
                      bool isAgreementChecked);

  void goBack();
  void resetToInitialState();
  void resetError();

 signals:
  void stateChanged(const RegisterState& state);
  void requestCaptcha();
  void backToLogin();
  void registerSuccess();

 protected:
  void emitStateChanged() override;

 private:
  QTimer* m_countdownTimer = nullptr;
  RegisterState::ContactType m_pendingContactType = RegisterState::Email;
};
