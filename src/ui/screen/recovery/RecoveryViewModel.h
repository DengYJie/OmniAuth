#pragma once

#include <QString>
#include <QTimer>
#include "ui/common/BaseViewModel.h"

struct RecoveryState {
    int currentStep = 1; // 1: Identity, 2: Security Auth, 3: Reset
    
    enum AuthMethod { None, Email, Face } selectedAuthMethod = None;
    
    // Email Auth
    int countdownSeconds = 0; // if > 0, button shows countdown
    
    // Reset Password
    int passwordStrength = 0; // 0: None, 1: Weak (Red), 2: Medium (Yellow), 3: Strong (Green)
    
    bool isProcessing = false;
    QString errorMsg;

    bool operator==(const RecoveryState&) const = default;
};

class RecoveryViewModel : public BaseViewModel<RecoveryViewModel, RecoveryState> {
    Q_OBJECT
public:
    explicit RecoveryViewModel(QObject* parent = nullptr);
    ~RecoveryViewModel() override;

    // Actions for Step 1
    void submitAccount(const QString& account); 
    // Actions for Step 2
    void selectAuthMethod(RecoveryState::AuthMethod method);
    void triggerEmailCaptcha();
    void emailCaptchaSuccess();
    void requestEmailCode(int waitSeconds = 60);
    void submitEmailAuth(const QString& code);
    void faceAuthSuccess();
    
    // Actions for Step 3
    void updatePasswordStrength(const QString& newPwd);
    void submitResetPassword(const QString& account, const QString& newPwd, const QString& confirmPwd);
    
    void goBack(); 
    
    void resetToInitialState();

signals:
    void stateChanged(const RecoveryState& state);
    void requestCaptcha();
    void backToLogin();
    void resetSuccess();

private:
    void calculatePasswordStrength();
    
    QTimer* m_countdownTimer = nullptr;
    
protected:
    void emitStateChanged() override;
};
