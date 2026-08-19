#include "RecoveryPage.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QRegularExpression>
#include <QTimer>
#include "design/Spacing.h"
#include "ui/widget/CaptchaOverlay.h"
#include <FluentQt/Layout.h>
#include <FluentQt/StatusInfo.h>
#include <FluentQt/Design.h>
#include <FluentQt/Foundation.h>

namespace fluent_b = fluent::basicinput;
namespace fluent_tf = fluent::textfields;
namespace fluent_nav = fluent::navigation;
namespace fluent_ly = fluent::layout;

namespace {

// (Prefix icons have been removed as per specification)

fluent::status_info::InfoBar* makeErrorInfoBar(QWidget* parent) {
  auto* infoBar = new fluent::status_info::InfoBar(parent);
  infoBar->setSeverity(fluent::status_info::InfoBar::Error);
  infoBar->setSingleLine(true);
  infoBar->setIsClosable(false);
  infoBar->hide();
  return infoBar;
}

}  // namespace

RecoveryPage::RecoveryPage(RecoveryViewModel* viewModel, QWidget* parent)
    : QWidget(parent), m_viewModel(viewModel) {
    setupUi();
    if (m_viewModel) {
        bindViewModel();
    }
}

void RecoveryPage::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->setAlignment(Qt::AlignCenter);

    auto* globalCard = new fluent_ly::Card(this);
    globalCard->setAppearance(fluent_ly::Card::Layer);
    globalCard->setBorderVisible(true);
    globalCard->setFixedSize(360, 440);

    auto* cardLayout = new QVBoxLayout(globalCard);
    cardLayout->setContentsMargins(Spacing::XLarge, Spacing::Large,
                                   Spacing::XLarge, Spacing::Large);
    cardLayout->setSpacing(Spacing::Standard);
    cardLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    m_stepper = new ui::widget::Stepper(globalCard);
    m_stepper->setSteps({QStringLiteral("确认账号"), QStringLiteral("安全验证"), QStringLiteral("重置密码")});
    m_stepper->setFixedWidth(280);
    cardLayout->addWidget(m_stepper, 0, Qt::AlignHCenter);
    cardLayout->addSpacing(Spacing::XXLarge);

    m_mainStackedWidget = new fluent::collections::StackView(globalCard);
    cardLayout->addWidget(m_mainStackedWidget);

    mainLayout->addWidget(globalCard, 0, Qt::AlignCenter);

    setupStep1();
    setupStep2();
    setupStep3();

    m_mainStackedWidget->adoptWidget(m_step1Widget);
    m_mainStackedWidget->adoptWidget(m_step2Widget);
    m_mainStackedWidget->adoptWidget(m_step3Widget);
    m_mainStackedWidget->setCurrentIndex(0);

    m_captchaOverlay = new CaptchaOverlay(this);
}

void RecoveryPage::setupStep1() {
    m_step1Widget = new QWidget(this);
    auto* layout = new QVBoxLayout(m_step1Widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(Spacing::Standard);
    layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto* title = new fluent_tf::Label(QStringLiteral("找回密码"), m_step1Widget);
    title->setFluentTypography(Typography::FontRole::Title);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title, 0, Qt::AlignCenter);
    layout->addSpacing(24);

    auto* formWidget = new QWidget(m_step1Widget);
    formWidget->setFixedWidth(260);
    auto* formLayout = new QFormLayout(formWidget);
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setVerticalSpacing(14);
    formLayout->setHorizontalSpacing(0);

    m_accountInput = new fluent_tf::LineEdit(formWidget);
    m_accountInput->setPlaceholderText(QStringLiteral("手机号 / 邮箱"));
    m_accountInput->setClearButtonEnabled(true);
    m_accountInput->setFixedHeight(Spacing::ControlHeight::Standard);
    formLayout->addRow(m_accountInput);

    layout->addWidget(formWidget, 0, Qt::AlignCenter);

    m_errorText1 = makeErrorInfoBar(m_step1Widget);
    layout->addWidget(m_errorText1);

    m_nextBtn = new fluent_b::Button(QStringLiteral("下一步"), m_step1Widget);
    m_nextBtn->setFluentStyle(fluent_b::Button::Accent);
    m_nextBtn->setFixedWidth(200);
    m_nextBtn->setFixedHeight(Spacing::ControlHeight::Standard);
    layout->addWidget(m_nextBtn, 0, Qt::AlignCenter);
}

void RecoveryPage::setupStep2() {
    m_step2Widget = new QWidget(this);
    auto* layout = new QVBoxLayout(m_step2Widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_step2Stacked = new fluent::collections::StackView(m_step2Widget);

    // Step 2.1: Method Selection
    m_methodSelectWidget = new QWidget(m_step2Stacked);
    auto* methodLayout = new QVBoxLayout(m_methodSelectWidget);
    methodLayout->setContentsMargins(0, 0, 0, 0);
    methodLayout->setSpacing(Spacing::Standard);
    methodLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto* title = new fluent_tf::Label(QStringLiteral("选择安全验证方式"), m_methodSelectWidget);
    title->setFluentTypography(Typography::FontRole::Title);
    title->setAlignment(Qt::AlignCenter);
    methodLayout->addWidget(title, 0, Qt::AlignCenter);
    methodLayout->addSpacing(24);

    m_emailBtn = new fluent_b::Button(QStringLiteral("邮箱验证码"), m_methodSelectWidget);
    m_emailBtn->setFixedWidth(200);
    m_emailBtn->setFixedHeight(Spacing::ControlHeight::Standard);
    m_emailBtn->setFluentStyle(fluent_b::Button::Standard);
    m_emailBtn->setIconGlyph(Typography::Icons::Mail);
    m_emailBtn->setFluentLayout(fluent_b::Button::IconBefore);

    m_faceBtn = new fluent_b::Button(QStringLiteral("人脸识别验证"), m_methodSelectWidget);
    m_faceBtn->setFixedWidth(200);
    m_faceBtn->setFixedHeight(Spacing::ControlHeight::Standard);
    m_faceBtn->setFluentStyle(fluent_b::Button::Standard);
    m_faceBtn->setIconGlyph(Typography::Icons::glyph(QStringLiteral("ic_fluent_scan_person_16_regular")));
    m_faceBtn->setFluentLayout(fluent_b::Button::IconBefore);

    methodLayout->addWidget(m_emailBtn, 0, Qt::AlignCenter);
    methodLayout->addWidget(m_faceBtn, 0, Qt::AlignCenter);

    m_backBtnMethod = new fluent_b::Button(QStringLiteral("上一步"), m_methodSelectWidget);
    m_backBtnMethod->setFluentStyle(fluent_b::Button::Standard);
    m_backBtnMethod->setFixedWidth(200);
    m_backBtnMethod->setFixedHeight(Spacing::ControlHeight::Standard);
    methodLayout->addWidget(m_backBtnMethod, 0, Qt::AlignCenter);

    // Step 2.2: Email Auth
    m_emailAuthWidget = new QWidget(m_step2Stacked);
    auto* emailLayout = new QVBoxLayout(m_emailAuthWidget);
    emailLayout->setContentsMargins(0, 0, 0, 0);
    emailLayout->setSpacing(Spacing::Standard);
    emailLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto* emailTitle = new fluent_tf::Label(QStringLiteral("输入邮箱验证码"), m_emailAuthWidget);
    emailTitle->setFluentTypography(Typography::FontRole::Title);
    emailTitle->setAlignment(Qt::AlignCenter);
    emailLayout->addWidget(emailTitle, 0, Qt::AlignCenter);
    emailLayout->addSpacing(24);

    auto* formWidget2 = new QWidget(m_emailAuthWidget);
    formWidget2->setFixedWidth(260);
    auto* formLayout2 = new QFormLayout(formWidget2);
    formLayout2->setContentsMargins(0, 0, 0, 0);
    formLayout2->setVerticalSpacing(14);
    formLayout2->setHorizontalSpacing(0);

    auto* codeWidget = new QWidget(formWidget2);
    auto* codeLayout = new QHBoxLayout(codeWidget);
    codeLayout->setContentsMargins(0, 0, 0, 0);
    codeLayout->setSpacing(Spacing::Small);
    m_emailCodeInput = new fluent_tf::LineEdit(codeWidget);
    m_emailCodeInput->setPlaceholderText(QStringLiteral("验证码"));
    m_emailCodeInput->setFixedHeight(Spacing::ControlHeight::Standard);
    m_sendCodeBtn = new fluent_b::Button(QStringLiteral("获取验证码"), codeWidget);
    m_sendCodeBtn->setFixedHeight(Spacing::ControlHeight::Standard);
    m_sendCodeBtn->setFixedWidth(100);

    codeLayout->addWidget(m_emailCodeInput, 1);
    codeLayout->addWidget(m_sendCodeBtn);
    formLayout2->addRow(codeWidget);

    emailLayout->addWidget(formWidget2, 0, Qt::AlignCenter);

    m_errorText2 = makeErrorInfoBar(m_emailAuthWidget);
    emailLayout->addWidget(m_errorText2);

    auto* btnWidget2 = new QWidget(m_emailAuthWidget);
    btnWidget2->setFixedWidth(260);
    auto* btnLayout2 = new QHBoxLayout(btnWidget2);
    btnLayout2->setContentsMargins(0, 0, 0, 0);
    btnLayout2->setSpacing(Spacing::Medium);

    m_backBtn2 = new fluent_b::Button(QStringLiteral("上一步"), btnWidget2);
    m_backBtn2->setFluentStyle(fluent_b::Button::Standard);
    m_backBtn2->setFixedWidth(100);
    m_backBtn2->setFixedHeight(Spacing::ControlHeight::Standard);

    m_verifyEmailBtn = new fluent_b::Button(QStringLiteral("验证"), btnWidget2);
    m_verifyEmailBtn->setFluentStyle(fluent_b::Button::Accent);
    m_verifyEmailBtn->setFixedWidth(100);
    m_verifyEmailBtn->setFixedHeight(Spacing::ControlHeight::Standard);

    btnLayout2->addWidget(m_backBtn2);
    btnLayout2->addWidget(m_verifyEmailBtn);
    emailLayout->addWidget(btnWidget2, 0, Qt::AlignCenter);

    // Step 2.3: Face Auth
    m_faceAuthWidget = new QWidget(m_step2Stacked);
    auto* faceLayout = new QVBoxLayout(m_faceAuthWidget);
    faceLayout->setContentsMargins(0, 0, 0, 0);
    faceLayout->setSpacing(Spacing::Standard);
    faceLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    m_faceScanner = new FaceScannerWidget(m_faceAuthWidget);
    m_faceScanner->setFixedSize(240, 240);
    faceLayout->addWidget(m_faceScanner, 0, Qt::AlignCenter);

    auto* faceHint = new fluent_tf::Label(QStringLiteral("请将面部对准屏幕中央"), m_faceAuthWidget);
    faceHint->setFluentTypography(Typography::FontRole::Body);
    faceHint->setTextColorRole(fluent_tf::Label::TextColorRole::Secondary);
    faceLayout->addWidget(faceHint, 0, Qt::AlignCenter);

    m_step2Stacked->adoptWidget(m_methodSelectWidget);
    m_step2Stacked->adoptWidget(m_emailAuthWidget);
    m_step2Stacked->adoptWidget(m_faceAuthWidget);
    m_step2Stacked->setCurrentIndex(0);

    layout->addWidget(m_step2Stacked);
}

void RecoveryPage::setupStep3() {
    m_step3Widget = new QWidget(this);
    auto* layout = new QVBoxLayout(m_step3Widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(Spacing::Standard);
    layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto* title = new fluent_tf::Label(QStringLiteral("重设密码"), m_step3Widget);
    title->setFluentTypography(Typography::FontRole::Title);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title, 0, Qt::AlignCenter);
    layout->addSpacing(24);

    auto* formWidget3 = new QWidget(m_step3Widget);
    formWidget3->setFixedWidth(260);
    auto* formLayout3 = new QFormLayout(formWidget3);
    formLayout3->setContentsMargins(0, 0, 0, 0);
    formLayout3->setVerticalSpacing(14);
    formLayout3->setHorizontalSpacing(0);

    m_newPwdInput = new fluent_tf::PasswordBox(formWidget3);
    m_newPwdInput->setPlaceholderText(QStringLiteral("请输入新密码"));
    m_newPwdInput->setFixedHeight(Spacing::ControlHeight::Standard);
    formLayout3->addRow(m_newPwdInput);

    m_strengthBar = new QWidget(formWidget3);
    m_strengthBar->setFixedHeight(Spacing::XSmall);
    m_strengthBar->setStyleSheet("background-color: transparent; border-radius: 2px;");
    formLayout3->addRow(m_strengthBar);

    m_confirmPwdInput = new fluent_tf::PasswordBox(formWidget3);
    m_confirmPwdInput->setPlaceholderText(QStringLiteral("请再次输入新密码"));
    m_confirmPwdInput->setFixedHeight(Spacing::ControlHeight::Standard);
    formLayout3->addRow(m_confirmPwdInput);

    layout->addWidget(formWidget3, 0, Qt::AlignCenter);

    m_errorText3 = makeErrorInfoBar(m_step3Widget);
    layout->addWidget(m_errorText3);

    m_resetBtn = new fluent_b::Button(QStringLiteral("确认重置"), m_step3Widget);
    m_resetBtn->setFluentStyle(fluent_b::Button::Accent);
    m_resetBtn->setFixedWidth(200);
    m_resetBtn->setFixedHeight(Spacing::ControlHeight::Standard);
    layout->addWidget(m_resetBtn, 0, Qt::AlignCenter);
}

void RecoveryPage::updateButtonStates() {
    static const QRegularExpression accountRe(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$|^1[3-9]\d{9}$)");
    auto s = m_viewModel->state();

    bool validAccount = accountRe.match(m_accountInput->text().trimmed()).hasMatch();
    m_nextBtn->setEnabled(validAccount);

    bool validCode = m_emailCodeInput->text().trimmed().length() >= 6;
    m_verifyEmailBtn->setEnabled(validCode);

    bool hasPwd = !m_newPwdInput->text().isEmpty();
    bool pwdMatch = hasPwd && m_newPwdInput->text() == m_confirmPwdInput->text();
    m_resetBtn->setEnabled(hasPwd && pwdMatch && !s.isProcessing);
}

void RecoveryPage::bindViewModel() {
    connect(m_nextBtn, &fluent_b::Button::clicked, this, [this]() {
        m_viewModel->submitAccount(m_accountInput->text());
    });

    connect(m_emailBtn, &fluent_b::Button::clicked, this, [this]() {
        m_viewModel->selectAuthMethod(RecoveryState::Email);
    });
    connect(m_faceBtn, &fluent_b::Button::clicked, this, [this]() {
        m_viewModel->selectAuthMethod(RecoveryState::Face);
    });

    connect(m_sendCodeBtn, &fluent_b::Button::clicked, this, [this]() {
        m_viewModel->triggerEmailCaptcha();
    });
    connect(m_verifyEmailBtn, &fluent_b::Button::clicked, this, [this]() {
        m_viewModel->submitEmailAuth(m_emailCodeInput->text());
    });

    connect(m_newPwdInput, &fluent_tf::LineEdit::textChanged, this, [this](const QString& text) {
        m_viewModel->updatePasswordStrength(text);
        updateButtonStates();
    });
    connect(m_confirmPwdInput, &fluent_tf::LineEdit::textChanged, this, [this]() { updateButtonStates(); });
    connect(m_resetBtn, &fluent_b::Button::clicked, this, [this]() {
        m_viewModel->submitResetPassword(m_accountInput->text(), m_newPwdInput->text(), m_confirmPwdInput->text());
    });

    connect(m_accountInput, &fluent_tf::LineEdit::textChanged, this, [this]() { updateButtonStates(); });
    connect(m_emailCodeInput, &fluent_tf::LineEdit::textChanged, this, [this]() { updateButtonStates(); });

    connect(m_backBtnMethod, &fluent_b::Button::clicked, this, [this]() {
        m_viewModel->goBack();
        });
    connect(m_backBtn2, &fluent_b::Button::clicked, this, [this]() {
        m_viewModel->goBack();
        });


    connect(m_viewModel, &RecoveryViewModel::requestCaptcha, this, [this]() {
        m_captchaOverlay->showOverlay();
        });
    connect(m_captchaOverlay, &CaptchaOverlay::verified, this, [this]() {
        m_viewModel->emailCaptchaSuccess();
        });

    m_viewModel->observe(this, &RecoveryPage::onStateChanged);
}

void RecoveryPage::onStateChanged(const RecoveryState& state) {
    m_mainStackedWidget->setCurrentIndex(state.currentStep - 1);
    
    m_stepper->setCurrentStep(state.currentStep - 1);
    m_stepper->setError(state.currentStep - 1, !state.errorMsg.isEmpty());

    if (state.currentStep == 2) {
        if (state.selectedAuthMethod == RecoveryState::None) {
            m_step2Stacked->setCurrentIndex(0);
        }
        else if (state.selectedAuthMethod == RecoveryState::Email) {
            m_step2Stacked->setCurrentIndex(1);
        }
        else if (state.selectedAuthMethod == RecoveryState::Face) {
            m_step2Stacked->setCurrentIndex(2);
            if (m_faceScanner->state() != FaceScannerWidget::ScanState::Scanning &&
                m_faceScanner->state() != FaceScannerWidget::ScanState::Success) {
                m_faceScanner->startScan();
                QTimer::singleShot(3000, this, [this]() {
                    m_faceScanner->setScanResult(true);
                    QTimer::singleShot(1000, this, [this]() {
                        m_viewModel->faceAuthSuccess();
                        });
                    });
            }
        }
    }
    else {
        if (m_faceScanner->state() == FaceScannerWidget::ScanState::Scanning) {
            m_faceScanner->stopScan();
        }
    }

    if (state.currentStep == 1) {
        m_errorText1->setMessage(state.errorMsg);
        m_errorText1->setVisible(!state.errorMsg.isEmpty());
    } else {
        m_errorText1->hide();
    }

    if (state.currentStep == 2 && state.selectedAuthMethod == RecoveryState::Email) {
        m_errorText2->setMessage(state.errorMsg);
        m_errorText2->setVisible(!state.errorMsg.isEmpty());
    } else {
        m_errorText2->hide();
    }

    if (state.currentStep == 3) {
        m_errorText3->setMessage(state.errorMsg);
        m_errorText3->setVisible(!state.errorMsg.isEmpty());
    } else {
        m_errorText3->hide();
    }

    if (state.countdownSeconds > 0) {
        m_sendCodeBtn->setEnabled(false);
        m_sendCodeBtn->setText(QString("重新获取(%1s)").arg(state.countdownSeconds));
    }
    else {
        m_sendCodeBtn->setEnabled(true);
        m_sendCodeBtn->setText("获取验证码");
    }

    if (state.currentStep == 3) {
        bool isLight = fluent::FluentElement::currentTheme() == fluent::FluentElement::Light;
        if (state.passwordStrength == 0) {
            m_strengthBar->setStyleSheet("background-color: transparent; border-radius: 2px;");
        }
        else if (state.passwordStrength == 1) {
            QString color = isLight ? ThemeColors::Light::System::Critical.name() : ThemeColors::Dark::System::Critical.name();
            m_strengthBar->setStyleSheet(QString("background-color: %1; border-radius: 2px; margin-right: 66%;").arg(color));
        }
        else if (state.passwordStrength == 2) {
            QString color = isLight ? ThemeColors::Light::System::Caution.name() : ThemeColors::Dark::System::Caution.name();
            m_strengthBar->setStyleSheet(QString("background-color: %1; border-radius: 2px; margin-right: 33%;").arg(color));
        }
        else if (state.passwordStrength == 3) {
            QString color = isLight ? ThemeColors::Light::System::Success.name() : ThemeColors::Dark::System::Success.name();
            m_strengthBar->setStyleSheet(QString("background-color: %1; border-radius: 2px;").arg(color));
        }
    }

    updateButtonStates();
}

void RecoveryPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_captchaOverlay) {
        m_captchaOverlay->resize(size());
    }
}
