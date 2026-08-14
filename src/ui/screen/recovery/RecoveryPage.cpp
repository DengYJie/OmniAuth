#include "RecoveryPage.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QRegularExpression>
#include <QTimer>
#include "ui/widget/CaptchaOverlay.h"
#include <FluentQt/StatusInfo.h>

namespace fluent_b = fluent::basicinput;
namespace fluent_tf = fluent::textfields;
namespace fluent_nav = fluent::navigation;

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
    bindViewModel();
}

void RecoveryPage::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(76, 32, 76, 32);
    mainLayout->setSpacing(0);

    m_mainStackedWidget = new fluent_nav::StackContentHost(this);

    setupStep1();
    setupStep2();
    setupStep3();

    m_mainStackedWidget->insertPage(0, m_step1Widget);
    m_mainStackedWidget->insertPage(1, m_step2Widget);
    m_mainStackedWidget->insertPage(2, m_step3Widget);
    m_mainStackedWidget->setCurrentIndex(0, 0, false);

    mainLayout->addWidget(m_mainStackedWidget);

    m_captchaOverlay = new CaptchaOverlay(this);
}

void RecoveryPage::setupStep1() {
    m_step1Widget = new QWidget(this);
    auto* layout = new QVBoxLayout(m_step1Widget);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addStretch();

    auto* title = new fluent_tf::Label(QStringLiteral("找回密码"), m_step1Widget);
    title->setFluentTypography(Typography::FontRole::Title);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title, 0, Qt::AlignCenter);
    layout->addSpacing(16);

    m_accountInput = new fluent_tf::LineEdit(m_step1Widget);
    m_accountInput->setPlaceholderText(QStringLiteral("手机号 / 邮箱"));
    m_accountInput->setClearButtonEnabled(true);
    layout->addWidget(m_accountInput);

    m_errorText1 = makeErrorInfoBar(m_step1Widget);
    layout->addWidget(m_errorText1);

    layout->addSpacing(16);

    m_nextBtn = new fluent_b::Button(QStringLiteral("下一步"), m_step1Widget);
    m_nextBtn->setFluentStyle(fluent_b::Button::Accent);
    m_nextBtn->setFixedHeight(36);
    layout->addWidget(m_nextBtn);

    layout->addStretch();
}

void RecoveryPage::setupStep2() {
    m_step2Widget = new QWidget(this);
    auto* layout = new QVBoxLayout(m_step2Widget);
    layout->setContentsMargins(0, 0, 0, 0);

    m_step2Stacked = new fluent_nav::StackContentHost(m_step2Widget);

    // Step 2.1: Method Selection
    m_methodSelectWidget = new QWidget(m_step2Stacked);
    auto* methodLayout = new QVBoxLayout(m_methodSelectWidget);
    methodLayout->setContentsMargins(0, 0, 0, 0);
    methodLayout->addStretch();

    auto* title = new fluent_tf::Label(QStringLiteral("选择安全验证方式"), m_methodSelectWidget);
    title->setFluentTypography(Typography::FontRole::Title);
    title->setAlignment(Qt::AlignCenter);
    methodLayout->addWidget(title, 0, Qt::AlignCenter);
    methodLayout->addSpacing(16);

    m_emailBtn = new fluent_b::Button(QStringLiteral("邮箱验证码"), m_methodSelectWidget);
    m_emailBtn->setFixedHeight(48);
    m_emailBtn->setFluentStyle(fluent_b::Button::Standard);
    m_emailBtn->setIconGlyph(Typography::Icons::Mail);
    m_emailBtn->setFluentLayout(fluent_b::Button::IconBefore);

    m_faceBtn = new fluent_b::Button(QStringLiteral("人脸识别验证"), m_methodSelectWidget);
    m_faceBtn->setFixedHeight(48);
    m_faceBtn->setFluentStyle(fluent_b::Button::Standard);
    m_faceBtn->setFluentLayout(fluent_b::Button::IconBefore);

    methodLayout->addWidget(m_emailBtn);
    methodLayout->addSpacing(16);
    methodLayout->addWidget(m_faceBtn);
    methodLayout->addSpacing(16);

    m_backBtnMethod = new fluent_b::Button(QStringLiteral("上一步"), m_methodSelectWidget);
    m_backBtnMethod->setFluentStyle(fluent_b::Button::Standard);
    m_backBtnMethod->setFixedHeight(36);
    methodLayout->addWidget(m_backBtnMethod);

    methodLayout->addStretch();

    // Step 2.2: Email Auth
    m_emailAuthWidget = new QWidget(m_step2Stacked);
    auto* emailLayout = new QVBoxLayout(m_emailAuthWidget);
    emailLayout->setContentsMargins(0, 0, 0, 0);
    emailLayout->addStretch();

    auto* emailTitle = new fluent_tf::Label(QStringLiteral("输入邮箱验证码"), m_emailAuthWidget);
    emailTitle->setFluentTypography(Typography::FontRole::Title);
    emailTitle->setAlignment(Qt::AlignCenter);
    emailLayout->addWidget(emailTitle, 0, Qt::AlignCenter);
    emailLayout->addSpacing(16);

    auto* codeLayout = new QHBoxLayout();
    m_emailCodeInput = new fluent_tf::LineEdit(m_emailAuthWidget);
    m_emailCodeInput->setPlaceholderText(QStringLiteral("验证码"));
    m_sendCodeBtn = new fluent_b::Button(QStringLiteral("获取验证码"), m_emailAuthWidget);
    m_sendCodeBtn->setFixedHeight(36);

    codeLayout->addWidget(m_emailCodeInput, 1);
    codeLayout->addSpacing(8);
    codeLayout->addWidget(m_sendCodeBtn);
    emailLayout->addLayout(codeLayout);

    m_errorText2 = makeErrorInfoBar(m_emailAuthWidget);
    emailLayout->addWidget(m_errorText2);

    emailLayout->addSpacing(16);

    m_backBtn2 = new fluent_b::Button(QStringLiteral("上一步"), m_emailAuthWidget);
    m_backBtn2->setFluentStyle(fluent_b::Button::Standard);
    m_backBtn2->setFixedHeight(36);

    m_verifyEmailBtn = new fluent_b::Button(QStringLiteral("验证"), m_emailAuthWidget);
    m_verifyEmailBtn->setFluentStyle(fluent_b::Button::Accent);
    m_verifyEmailBtn->setFixedHeight(36);

    auto* btnLayout2 = new QHBoxLayout();
    btnLayout2->setContentsMargins(0, 0, 0, 0);
    btnLayout2->addWidget(m_backBtn2);
    btnLayout2->addSpacing(12);
    btnLayout2->addWidget(m_verifyEmailBtn);
    emailLayout->addLayout(btnLayout2);
    emailLayout->addStretch();

    // Step 2.3: Face Auth
    m_faceAuthWidget = new QWidget(m_step2Stacked);
    auto* faceLayout = new QVBoxLayout(m_faceAuthWidget);
    faceLayout->setContentsMargins(0, 0, 0, 0);
    faceLayout->addStretch();

    m_faceScanner = new FaceScannerWidget(m_faceAuthWidget);
    m_faceScanner->setFixedSize(240, 240);
    faceLayout->addWidget(m_faceScanner, 0, Qt::AlignCenter);

    faceLayout->addSpacing(28);
    auto* faceHint = new fluent_tf::Label(QStringLiteral("请将面部对准屏幕中央"), m_faceAuthWidget);
    faceHint->setFluentTypography(Typography::FontRole::Body);
    faceHint->setTextColorRole(fluent_tf::Label::TextColorRole::Secondary);
    faceLayout->addWidget(faceHint, 0, Qt::AlignCenter);

    faceLayout->addStretch();

    m_step2Stacked->insertPage(0, m_methodSelectWidget);
    m_step2Stacked->insertPage(1, m_emailAuthWidget);
    m_step2Stacked->insertPage(2, m_faceAuthWidget);
    m_step2Stacked->setCurrentIndex(0, 0, false);

    layout->addWidget(m_step2Stacked);
}

void RecoveryPage::setupStep3() {
    m_step3Widget = new QWidget(this);
    auto* layout = new QVBoxLayout(m_step3Widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addStretch();

    auto* title = new fluent_tf::Label(QStringLiteral("重设密码"), m_step3Widget);
    title->setFluentTypography(Typography::FontRole::Title);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title, 0, Qt::AlignCenter);
    layout->addSpacing(16);

    m_newPwdInput = new fluent_tf::PasswordBox(m_step3Widget);
    m_newPwdInput->setPlaceholderText(QStringLiteral("请输入新密码"));
    layout->addWidget(m_newPwdInput);

    layout->addSpacing(8);

    m_strengthBar = new QWidget(m_step3Widget);
    m_strengthBar->setFixedHeight(4);
    m_strengthBar->setStyleSheet("background-color: transparent; border-radius: 2px;");
    layout->addWidget(m_strengthBar);

    layout->addSpacing(16);

    m_confirmPwdInput = new fluent_tf::PasswordBox(m_step3Widget);
    m_confirmPwdInput->setPlaceholderText(QStringLiteral("请再次输入新密码"));
    layout->addWidget(m_confirmPwdInput);

    m_errorText3 = makeErrorInfoBar(m_step3Widget);
    layout->addWidget(m_errorText3);

    layout->addSpacing(16);

    m_backBtn3 = new fluent_b::Button(QStringLiteral("上一步"), m_step3Widget);
    m_backBtn3->setFluentStyle(fluent_b::Button::Standard);
    m_backBtn3->setFixedHeight(36);

    m_resetBtn = new fluent_b::Button(QStringLiteral("确认重置"), m_step3Widget);
    m_resetBtn->setFluentStyle(fluent_b::Button::Accent);
    m_resetBtn->setFixedHeight(36);

    auto* btnLayout3 = new QHBoxLayout();
    btnLayout3->setContentsMargins(0, 0, 0, 0);
    btnLayout3->addWidget(m_backBtn3);
    btnLayout3->addSpacing(12);
    btnLayout3->addWidget(m_resetBtn);
    layout->addLayout(btnLayout3);

    layout->addStretch();
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
    connect(m_backBtn3, &fluent_b::Button::clicked, this, [this]() {
        m_viewModel->goBack();
        });

    connect(m_viewModel, &RecoveryViewModel::requestCaptcha, this, [this]() {
        m_captchaOverlay->showOverlay();
        });
    connect(m_captchaOverlay, &CaptchaOverlay::verified, this, [this]() {
        m_viewModel->emailCaptchaSuccess();
        });

    connect(m_viewModel, &RecoveryViewModel::stateChanged, this, &RecoveryPage::onStateChanged);
}

void RecoveryPage::onStateChanged(const RecoveryState& state) {
    m_mainStackedWidget->setCurrentIndex(state.currentStep - 1);

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
        if (state.passwordStrength == 0) {
            m_strengthBar->setStyleSheet("background-color: transparent; border-radius: 2px;");
        }
        else if (state.passwordStrength == 1) {
            m_strengthBar->setStyleSheet("background-color: #ff3c3c; border-radius: 2px; margin-right: 66%;");
        }
        else if (state.passwordStrength == 2) {
            m_strengthBar->setStyleSheet("background-color: #ffb900; border-radius: 2px; margin-right: 33%;");
        }
        else if (state.passwordStrength == 3) {
            m_strengthBar->setStyleSheet("background-color: #107c10; border-radius: 2px;");
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
