#include "RecoveryViewModel.h"
#include "data/di/AppContainer.h"
#include <QPointer>

#include <QRegularExpression>
#include <QTimer>

RecoveryViewModel::RecoveryViewModel(QObject* parent) : BaseViewModel<RecoveryViewModel, RecoveryState>(parent) {
    m_countdownTimer = new QTimer(this);
    m_countdownTimer->setInterval(1000);
    connect(m_countdownTimer, &QTimer::timeout, this, [this]() {
        updateState([](RecoveryState& s) {
            if (s.countdownSeconds > 0) {
                s.countdownSeconds--;
            }
        });
        if (state().countdownSeconds == 0) {
            m_countdownTimer->stop();
        }
    });
}

RecoveryViewModel::~RecoveryViewModel() {
    if (m_countdownTimer->isActive()) {
        m_countdownTimer->stop();
    }
}

void RecoveryViewModel::submitAccount(const QString& account) {
    if (account.trimmed().isEmpty()) return;
    // Directly go to step 2
    updateState([](RecoveryState& s) {
        s.currentStep = 2;
        s.errorMsg.clear();
    });
}

void RecoveryViewModel::selectAuthMethod(RecoveryState::AuthMethod method) {
    updateState([&](RecoveryState& s) {
        s.selectedAuthMethod = method;
        s.errorMsg.clear();
    });
}

void RecoveryViewModel::triggerEmailCaptcha() {
    emit requestCaptcha();
}

void RecoveryViewModel::emailCaptchaSuccess() {
    requestEmailCode();
}

void RecoveryViewModel::requestEmailCode(int waitSeconds) {
    updateState([&](RecoveryState& s) {
        s.countdownSeconds = waitSeconds;
        s.errorMsg.clear();
    });
    m_countdownTimer->start();
    // In a real app, send API request here.
}

void RecoveryViewModel::submitEmailAuth(const QString& code) {
    if (code.length() != 6) {
        updateState([](RecoveryState& s) {
            s.errorMsg = QStringLiteral("验证码格式错误，请输入6位验证码");
        });
        return;
    }
    // Simulate API check
    updateState([](RecoveryState& s) {
        s.currentStep = 3;
        s.errorMsg.clear();
    });
}

void RecoveryViewModel::faceAuthSuccess() {
    updateState([](RecoveryState& s) {
        s.currentStep = 3;
        s.errorMsg.clear();
    });
}

void RecoveryViewModel::updatePasswordStrength(const QString& pwd) {
    int strength = 0;
    
    if (pwd.length() >= 6) {
        strength = 1; // Weak
        bool hasLower = pwd.contains(QRegularExpression("[a-z]"));
        bool hasUpper = pwd.contains(QRegularExpression("[A-Z]"));
        bool hasDigit = pwd.contains(QRegularExpression("\\d"));
        bool hasSpecial = pwd.contains(QRegularExpression("[^a-zA-Z\\d]"));
        
        int conditions = 0;
        if (hasLower || hasUpper) conditions++;
        if (hasDigit) conditions++;
        if (hasSpecial) conditions++;
        
        if (pwd.length() >= 8 && conditions >= 2) {
            strength = 2; // Medium
        }
        if (pwd.length() >= 10 && conditions >= 3) {
            strength = 3; // Strong
        }
    }
    
    updateState([strength](RecoveryState& s) {
        s.passwordStrength = strength;
        s.errorMsg.clear();
    });
}

void RecoveryViewModel::submitResetPassword(const QString& account, const QString& newPwd, const QString& confirmPwd) {
    if (newPwd.isEmpty()) return;
    if (newPwd != confirmPwd) return;
    
    const quint64 reqId = beginRequest();
    updateState([](RecoveryState& s) { s.isProcessing = true; });
    
    if (auto resetPassword = AppContainer::resetPasswordUseCase()) {
        QPointer<RecoveryViewModel> weakThis(this);
        resetPassword->resetPasswordAsync(account.trimmed(), newPwd, [weakThis, reqId](bool ok, QString msg) {
            if (!weakThis) return;
            QMetaObject::invokeMethod(weakThis.data(), [weakThis, reqId, ok, msg]() {
                if (!weakThis || !weakThis->isRequestCurrent(reqId)) return;
                weakThis->updateState([](RecoveryState& s) { s.isProcessing = false; });
                if (ok) {
                    weakThis->resetToInitialState();
                    emit weakThis->resetSuccess();
                } else {
                    weakThis->updateState([msg](RecoveryState& s) { s.errorMsg = msg; });
                }
            });
        });
    } else {
        updateState([](RecoveryState& s) { 
            s.isProcessing = false;
            s.errorMsg = "服务未初始化";
        });
    }
}

void RecoveryViewModel::goBack() {
    auto s = state();
    if (s.currentStep == 1) {
        emit backToLogin();
    } else if (s.currentStep == 2) {
        if (s.selectedAuthMethod != RecoveryState::None) {
            updateState([](RecoveryState& st) { 
                st.selectedAuthMethod = RecoveryState::None; 
                st.errorMsg.clear();
            });
        } else {
            updateState([](RecoveryState& st) { 
                st.currentStep = 1; 
                st.errorMsg.clear();
            });
        }
    } else if (s.currentStep == 3) {
        updateState([](RecoveryState& st) { 
            st.currentStep = 2; 
            st.errorMsg.clear();
        });
    }
}

void RecoveryViewModel::resetToInitialState() {
    if (m_countdownTimer->isActive()) {
        m_countdownTimer->stop();
    }
    invalidateRequests();
    updateState([](RecoveryState& s) {
        s = RecoveryState(); 
    });
}

void RecoveryViewModel::emitStateChanged() {
    emit stateChanged(state());
}
