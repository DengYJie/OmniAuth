#pragma once

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <FluentQt/BasicInput.h>
#include <FluentQt/Collections.h>
#include <FluentQt/Navigation.h>
#include <FluentQt/TextFields.h>

#include "ui/widget/FaceScannerWidget.h"
#include "ui/screen/recovery/RecoveryViewModel.h"
#include "ui/widget/Stepper.h"

namespace fluent::basicinput { class Button; }
namespace fluent::textfields { class Label; class LineEdit; class PasswordBox; }
namespace fluent::status_info { class InfoBar; }

class CaptchaOverlay;

class RecoveryPage : public QWidget {
    Q_OBJECT
public:
    explicit RecoveryPage(RecoveryViewModel* viewModel, QWidget* parent = nullptr);
    
protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onStateChanged(const RecoveryState& state);

private:
    void setupUi();
    void bindViewModel();
    void updateButtonStates();
    
    // Step 1: Identity
    void setupStep1();
    QWidget* m_step1Widget;
    fluent::textfields::LineEdit* m_accountInput;
    fluent::basicinput::Button* m_nextBtn;
    fluent::status_info::InfoBar* m_errorText1;
    
    // Step 2: Security Auth
    void setupStep2();
    QWidget* m_step2Widget;
    fluent::collections::StackView* m_step2Stacked;
    
    // Step 2.1: Method Selection
    QWidget* m_methodSelectWidget;
    fluent::basicinput::Button* m_emailBtn;
    fluent::basicinput::Button* m_faceBtn;
    fluent::basicinput::Button* m_backBtnMethod;
    
    // Step 2.2: Email Auth
    QWidget* m_emailAuthWidget;
    fluent::textfields::LineEdit* m_emailCodeInput;
    fluent::basicinput::Button* m_sendCodeBtn;
    fluent::basicinput::Button* m_backBtn2;
    fluent::basicinput::Button* m_verifyEmailBtn;
    fluent::status_info::InfoBar* m_errorText2;
    
    // Step 2.3: Face Auth
    QWidget* m_faceAuthWidget;
    FaceScannerWidget* m_faceScanner;
    
    // Step 3: Reset Password
    void setupStep3();
    QWidget* m_step3Widget;
    fluent::textfields::PasswordBox* m_newPwdInput;
    fluent::textfields::PasswordBox* m_confirmPwdInput;
    QWidget* m_strengthBar;
    fluent::basicinput::Button* m_resetBtn;
    fluent::status_info::InfoBar* m_errorText3;
    
    fluent::collections::StackView* m_mainStackedWidget;
    CaptchaOverlay* m_captchaOverlay;
    
    ui::widget::Stepper* m_stepper;
    
    RecoveryViewModel* m_viewModel;
};
