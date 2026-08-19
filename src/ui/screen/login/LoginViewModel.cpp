#include "data/di/AppContainer.h"
#include "core/CryptoUtils.h"
#include "ui/screen/login/LoginViewModel.h"

#include <QPointer>

LoginViewModel::LoginViewModel(QObject* parent)
    : BaseViewModel<LoginViewModel, LoginState>(parent) {
    // 验证码倒计时
    m_countdownTimer = new QTimer(this);
    m_countdownTimer->setInterval(1000);
    connect(m_countdownTimer, &QTimer::timeout, this, [this]() {
        auto current = state();
        if (current.codeCountdown > 1) {
            updateState([](LoginState& s) { s.codeCountdown--; });
        }
        else {
            m_countdownTimer->stop();
            updateState([](LoginState& s) { s.codeCountdown = 0; });
        }
        });

    // 账号人脸绑定检查防抖 (500ms)
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(500);
    connect(m_debounceTimer, &QTimer::timeout, this, [this]() {
        const QString account = m_pendingAccount;
        if (account.trimmed().isEmpty()) {
            updateState([](LoginState& s) {
                s.hasFaceBound = false;
                s.isCheckingAccount = false;
                });
            return;
        }

        // 轻量级校验：通过邮箱/手机号直接检查是否绑定人脸
        if (auto faceLogin = AppContainer::faceLoginUseCase()) {
            bool hasFace = faceLogin->hasUserFace(account.trimmed());
            updateState([hasFace](LoginState& s) {
                s.hasFaceBound = hasFace;
                s.isCheckingAccount = false;
            });
        } else {
            updateState([](LoginState& s) {
                s.hasFaceBound = false;
                s.isCheckingAccount = false;
            });
        }
        });
}

LoginViewModel::~LoginViewModel() {
    if (m_countdownTimer && m_countdownTimer->isActive()) {
        m_countdownTimer->stop();
    }
    if (m_debounceTimer && m_debounceTimer->isActive()) {
        m_debounceTimer->stop();
    }
}

void LoginViewModel::emitStateChanged() { emit stateChanged(m_state); }

void LoginViewModel::switchLoginMode(int mode) {
    if (mode == m_state.loginMode) return;
    updateState([mode](LoginState& s) {
        s.loginMode = mode;
        s.errorMessage.clear();
        });
}

void LoginViewModel::checkAccountFaceBinding(const QString& account) {
    m_pendingAccount = account;
    if (account.trimmed().isEmpty()) {
        m_debounceTimer->stop();
        updateState([](LoginState& s) {
            s.hasFaceBound = false;
            s.isCheckingAccount = false;
            });
        return;
    }
    updateState([](LoginState& s) { s.isCheckingAccount = true; });
    m_debounceTimer->start();
}

void LoginViewModel::loginClicked(const QString& account, const QString& password) {
    m_pendingAccount = account;
    m_pendingPassword = password;
    updateState([&](LoginState& state) {
        state.errorMessage.clear();
        state.isCaptchaVisible = true;
    });
}

void LoginViewModel::smsLoginClicked(const QString& account, const QString& code) {
    m_pendingAccount = account.trimmed();
    m_pendingSmsCode = code.trimmed();

    const quint64 reqId = beginRequest();
    updateState([&](LoginState& s) {
        s.errorMessage.clear();
        s.isLoggingIn = true;
        });

    // 验证码登录
    if (auto smsLogin = AppContainer::smsLoginUseCase()) {
        QPointer<LoginViewModel> weakThis(this);
        smsLogin->smsLoginAsync(m_pendingAccount, m_pendingSmsCode,
            [weakThis, reqId](bool ok, int uid, QString username, QString msg) {
                if (!weakThis) return;
                QMetaObject::invokeMethod(weakThis.data(), [weakThis, reqId, ok, uid, username, msg]() {
                    if (!weakThis || !weakThis->isRequestCurrent(reqId)) return;
                    weakThis->m_pendingAccount.clear();
                    weakThis->m_pendingSmsCode.clear();

                    weakThis->updateState([](LoginState& s) {
                        s.isLoggingIn = false;
                        s.isCodeSent = false;
                        s.codeCountdown = 0;
                        });

                    if (ok) {
                        emit weakThis->loginSuccess(uid, username);
                    }
                    else {
                        weakThis->updateState([msg](LoginState& s) {
                            s.errorMessage = msg;
                            });
                    }
                    });
            });
    }
    else {
        updateState([](LoginState& s) {
            s.isLoggingIn = false;
            s.errorMessage = QStringLiteral("服务未初始化");
            });
    }
}

void LoginViewModel::requestSmsCode() {
    emit requestCaptcha();
}

void LoginViewModel::captchaClosed() {
    updateState([&](LoginState& state) { state.isCaptchaVisible = false; });
}

void LoginViewModel::captchaVerified() {
    if (m_state.loginMode == 1) {
        // 验证码登录模式：滑块验证通过后发送验证码
        m_countdownTimer->start();
        updateState([](LoginState& state) {
            state.isCaptchaVisible = false;
            state.isCodeSent = true;
            state.codeCountdown = 60;
            state.errorMessage.clear();
        });
    } else {
        // 密码登录模式：滑块验证通过后执行登录
        updateState([](LoginState& state) {
            state.isCaptchaVisible = false;
            state.isLoggingIn = true;
        });

        const quint64 reqId = beginRequest();
        if (auto passwordLogin = AppContainer::passwordLoginUseCase()) {
            QPointer<LoginViewModel> weakThis(this);
            passwordLogin->loginAsync(m_pendingAccount, m_pendingPassword, [weakThis, reqId](bool isAuth, int uid, QString username) {
                if (!weakThis) return;
                QMetaObject::invokeMethod(weakThis.data(), [weakThis, reqId, isAuth, uid, username]() {
                    if (!weakThis || !weakThis->isRequestCurrent(reqId)) return;
                    weakThis->m_pendingAccount.clear();
                    weakThis->m_pendingPassword.clear();

                    weakThis->updateState([](LoginState& state) { state.isLoggingIn = false; });

                    if (isAuth) {
                        emit weakThis->loginSuccess(uid, username);
                    }
                    else {
                        weakThis->updateState([](LoginState& state) {
                            state.errorMessage = QStringLiteral("账号或密码错误");
                            });
                    }
                    });
                });
        }
        else {
            updateState([](LoginState& state) {
                state.isLoggingIn = false;
                state.errorMessage = QStringLiteral("服务未初始化");
                });
        }
    }
}

void LoginViewModel::resetError() {
    updateState([&](LoginState& state) { state.errorMessage.clear(); });
}
