#include "RegisterViewModel.h"

#include <QDebug>
#include <QRegularExpression>
#include <QTimer>
#include "data/di/AppContainer.h"
#include <QPointer>

RegisterViewModel::RegisterViewModel(QObject* parent)
    : BaseViewModel<RegisterState>(parent) {
  m_countdownTimer = new QTimer(this);
  m_countdownTimer->setInterval(1000);
  connect(m_countdownTimer, &QTimer::timeout, this, [this]() {
    auto s = state();
    bool isEmail = s.contactType == RegisterState::Email;
    int cd = isEmail ? s.emailCountdown : s.phoneCountdown;
    if (cd > 1) {
      updateState([isEmail](RegisterState& st) {
        if (isEmail) st.emailCountdown--;
        else        st.phoneCountdown--;
      });
    } else {
      m_countdownTimer->stop();
      updateState([isEmail](RegisterState& st) {
        if (isEmail) st.emailCountdown = 0;
        else        st.phoneCountdown = 0;
      });
    }
  });
}

RegisterViewModel::~RegisterViewModel() {
  if (m_countdownTimer && m_countdownTimer->isActive()) {
    m_countdownTimer->stop();
  }
}

void RegisterViewModel::switchContactType(RegisterState::ContactType type) {
  if (m_state.contactType == type) return;
  updateState([type](RegisterState& s) {
    s.contactType = type;
    s.errorMessage.clear();
  });
  // 恢复新 tab 的倒计时状态
  auto s = state();
  int cd = (type == RegisterState::Email) ? s.emailCountdown : s.phoneCountdown;
  if (cd > 0) m_countdownTimer->start();
  else        m_countdownTimer->stop();
}

void RegisterViewModel::triggerCaptcha(RegisterState::ContactType type) {
  m_pendingContactType = type;
  emit requestCaptcha();
}

void RegisterViewModel::captchaVerified() {
  requestVerificationCode(m_pendingContactType);
}

void RegisterViewModel::captchaClosed() {
  updateState([](RegisterState& s) { s.isCaptchaVisible = false; });
}

void RegisterViewModel::requestVerificationCode(RegisterState::ContactType type, int waitSeconds) {
  bool isEmail = type == RegisterState::Email;
  updateState([isEmail, waitSeconds](RegisterState& s) {
    if (isEmail) {
      s.emailCodeSent = true;
      s.emailCountdown = waitSeconds;
    } else {
      s.phoneCodeSent = true;
      s.phoneCountdown = waitSeconds;
    }
    s.errorMessage.clear();
  });
  m_countdownTimer->start();
}

void RegisterViewModel::submitContactVerification(RegisterState::ContactType type,
                                                   const QString& contact, const QString& code) {

  updateState([](RegisterState& s) {
    s.currentStep = 2;
    s.errorMessage.clear();
  });
}

void RegisterViewModel::updatePasswordStrength(const QString& pwd) {
  int strength = 0;
  if (pwd.length() >= 6) {
    strength = 1;
    bool hasLower = pwd.contains(QRegularExpression("[a-z]"));
    bool hasUpper = pwd.contains(QRegularExpression("[A-Z]"));
    bool hasDigit = pwd.contains(QRegularExpression("\\d"));
    bool hasSpecial = pwd.contains(QRegularExpression("[^a-zA-Z\\d]"));

    int conditions = 0;
    if (hasLower || hasUpper) conditions++;
    if (hasDigit) conditions++;
    if (hasSpecial) conditions++;

    if (pwd.length() >= 8 && conditions >= 2) strength = 2;
    if (pwd.length() >= 10 && conditions >= 3) strength = 3;
  }

  updateState([strength](RegisterState& s) {
    s.passwordStrength = strength;
    s.errorMessage.clear();
  });
}

void RegisterViewModel::submitRegister(const QString& account,
                                      RegisterState::ContactType contactType,
                                      const QString& contact,
                                      const QString& password,
                                      const QString& confirmPassword,
                                      bool isAgreementChecked) {
  QString trimmedAcc = account.trimmed();
  QString email = (contactType == RegisterState::Email) ? contact.trimmed() : QString();
  QString phone = (contactType == RegisterState::Phone) ? contact.trimmed() : QString();

  updateState([](RegisterState& s) { s.isRegistering = true; });

  if (auto registerUseCase = AppContainer::registerUseCase()) {
      QPointer<RegisterViewModel> weakThis(this);
      registerUseCase->registerUserAsync(trimmedAcc, password, email, phone, [weakThis](bool ok, QString msg) {
          if (!weakThis) return;
          QMetaObject::invokeMethod(weakThis.data(), [weakThis, ok, msg]() {
              if (!weakThis) return;
              weakThis->updateState([](RegisterState& s) { s.isRegistering = false; });
              if (ok) {
                weakThis->resetToInitialState();
                emit weakThis->registerSuccess();
              } else {
                weakThis->updateState([msg](RegisterState& s) { s.errorMessage = msg; });
              }
          });
      });
  } else {
      updateState([](RegisterState& s) {
          s.isRegistering = false;
          s.errorMessage = QStringLiteral("服务未初始化");
      });
  }
}

void RegisterViewModel::goBack() {
  auto s = state();
  if (s.currentStep == 1) {
    resetToInitialState();
    emit backToLogin();
  } else if (s.currentStep == 2) {
    updateState([](RegisterState& st) {
      st.currentStep = 1;
      st.errorMessage.clear();
    });
  }
}

void RegisterViewModel::resetToInitialState() {
  if (m_countdownTimer && m_countdownTimer->isActive()) {
    m_countdownTimer->stop();
  }
  updateState([](RegisterState& s) { s = RegisterState(); });
}

void RegisterViewModel::resetError() {
  updateState([](RegisterState& s) { s.errorMessage.clear(); });
}

void RegisterViewModel::emitStateChanged() {
  emit stateChanged(state());
}
