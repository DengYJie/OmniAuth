#include "ui/screen/register/RegisterPage.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QVBoxLayout>

#include <FluentQt/Navigation.h>
#include <FluentQt/StatusInfo.h>

#include "ui/widget/CaptchaOverlay.h"
#include "ui/screen/register/RegisterViewModel.h"

namespace fluent_b = fluent::basicinput;
namespace fluent_tf = fluent::textfields;
namespace fluent_nav = fluent::navigation;

namespace {

// (Prefix icons have been removed as per specification)

fluent_b::HyperlinkButton* makeLinkButton(const QString& text, QWidget* parent) {
  auto* btn = new fluent_b::HyperlinkButton(text, parent);
  btn->setFluentStyle(fluent_b::Button::Subtle);
  btn->setFluentSize(fluent_b::Button::Small);
  btn->setFont(Typography::fontStyle(Typography::FontRole::Caption).toQFont());
  return btn;
}

fluent::status_info::InfoBar* makeErrorInfoBar(QWidget* parent) {
  auto* infoBar = new fluent::status_info::InfoBar(parent);
  infoBar->setSeverity(fluent::status_info::InfoBar::Error);
  infoBar->setSingleLine(true);
  infoBar->setIsClosable(false);
  infoBar->hide();
  return infoBar;
}

}  // namespace

RegisterPage::RegisterPage(RegisterViewModel* viewModel, QWidget* parent)
    : QWidget(parent), m_viewModel(viewModel) {
    setupUi();
    if (m_viewModel) {
        bindViewModel();
        renderState(m_viewModel->state());
    }
}

void RegisterPage::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(76, 32, 76, 32);
    mainLayout->setSpacing(0);

    m_mainStackedWidget = new fluent_nav::StackContentHost(this);

    setupStep1();
    setupStep2();

    m_mainStackedWidget->insertPage(0, m_step1Widget);
    m_mainStackedWidget->insertPage(1, m_step2Widget);
    m_mainStackedWidget->setCurrentIndex(0, 0, false);

    mainLayout->addWidget(m_mainStackedWidget);

    m_captchaOverlay = new CaptchaOverlay(this);
}

void RegisterPage::setupStep1() {
    m_step1Widget = new QWidget(this);
    auto* layout = new QVBoxLayout(m_step1Widget);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addStretch();

    auto* title = new fluent_tf::Label(QStringLiteral("选择注册方式"), m_step1Widget);
    title->setFluentTypography(Typography::FontRole::Title);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title, 0, Qt::AlignCenter);
    layout->addSpacing(16);

    m_contactPivot = new fluent_nav::Pivot(m_step1Widget);
    m_contactPivot->addItem(QStringLiteral("邮箱注册"));
    m_contactPivot->addItem(QStringLiteral("手机号注册"));
    m_contactPivot->setFixedWidth(200);
    m_contactPivot->setFixedHeight(36);
    m_contactPivot->setSelectedIndex(0);
    layout->addWidget(m_contactPivot, 0, Qt::AlignCenter);
    layout->addSpacing(16);

    m_contactStack = new fluent_nav::StackContentHost(m_step1Widget);

    // ── 邮箱 tab ──
    auto* emailPage = new QWidget(m_contactStack);
    auto* eLayout = new QVBoxLayout(emailPage);
    eLayout->setContentsMargins(0, 0, 0, 0);
    eLayout->setSpacing(0);

    auto* eRow = new QHBoxLayout();
    eRow->setContentsMargins(0, 0, 0, 0);
    eRow->setSpacing(12);
    m_emailInput = new fluent_tf::LineEdit(emailPage);
    m_emailInput->setPlaceholderText(QStringLiteral("邮箱地址"));
    m_emailInput->setClearButtonEnabled(true);
    m_emailSendBtn = new fluent_b::Button(QStringLiteral("获取验证码"), emailPage);
    m_emailSendBtn->setFluentStyle(fluent_b::Button::Standard);
    m_emailSendBtn->setFixedWidth(92);
    m_emailSendBtn->setFixedHeight(36);
    eRow->addWidget(m_emailInput);
    eRow->addWidget(m_emailSendBtn);
    eLayout->addLayout(eRow);
    eLayout->addSpacing(16);
    m_emailCodeInput = new fluent_tf::LineEdit(emailPage);
    m_emailCodeInput->setPlaceholderText(QStringLiteral("6 位验证码"));
    m_emailCodeInput->hide();
    eLayout->addWidget(m_emailCodeInput);

    m_contactStack->insertPage(0, emailPage);

    auto* phonePage = new QWidget(m_contactStack);
    auto* pLayout = new QVBoxLayout(phonePage);
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->setSpacing(0);

    auto* pRow = new QHBoxLayout();
    pRow->setContentsMargins(0, 0, 0, 0);
    pRow->setSpacing(12);
    m_phoneInput = new fluent_tf::LineEdit(phonePage);
    m_phoneInput->setPlaceholderText(QStringLiteral("手机号码"));
    m_phoneInput->setClearButtonEnabled(true);
    m_phoneSendBtn = new fluent_b::Button(QStringLiteral("获取验证码"), phonePage);
    m_phoneSendBtn->setFluentStyle(fluent_b::Button::Standard);
    m_phoneSendBtn->setFixedWidth(92);
    m_phoneSendBtn->setFixedHeight(36);
    pRow->addWidget(m_phoneInput);
    pRow->addWidget(m_phoneSendBtn);
    pLayout->addLayout(pRow);
    pLayout->addSpacing(16);
    m_phoneCodeInput = new fluent_tf::LineEdit(phonePage);
    m_phoneCodeInput->setPlaceholderText(QStringLiteral("6 位验证码"));
    m_phoneCodeInput->hide();
    pLayout->addWidget(m_phoneCodeInput);

    m_contactStack->insertPage(1, phonePage);
    m_contactStack->setCurrentIndex(0, 0, false);

    layout->addWidget(m_contactStack);
    layout->addSpacing(16);

    m_agreementWidget = new QWidget(m_step1Widget);
    auto* agreementLayout = new QHBoxLayout(m_agreementWidget);
    agreementLayout->setContentsMargins(0, 0, 0, 0);
    agreementLayout->setSpacing(4);

    m_agreementCheckBox = new fluent_b::CheckBox(m_agreementWidget);
    auto* agreePrefix =
        new fluent_tf::Label(QStringLiteral("已阅读并同意 "), m_agreementWidget);
    agreePrefix->setFluentTypography(Typography::FontRole::Caption);
    agreePrefix->setTextColorRole(fluent_tf::Label::TextColorRole::Secondary);
    m_serviceAgreementText =
        makeLinkButton(QStringLiteral("服务协议"), m_agreementWidget);
    auto* andText =
        new fluent_tf::Label(QStringLiteral(" 和 "), m_agreementWidget);
    andText->setFluentTypography(Typography::FontRole::Caption);
    andText->setTextColorRole(fluent_tf::Label::TextColorRole::Secondary);
    m_privacyPolicyText =
        makeLinkButton(QStringLiteral("隐私政策"), m_agreementWidget);

    agreementLayout->addWidget(m_agreementCheckBox);
    agreementLayout->addWidget(agreePrefix);
    agreementLayout->addWidget(m_serviceAgreementText);
    agreementLayout->addWidget(andText);
    agreementLayout->addWidget(m_privacyPolicyText);
    agreementLayout->addStretch();

    layout->addWidget(m_agreementWidget);
#if defined(QT_DEBUG) || !defined(NDEBUG)
    m_agreementWidget->show();
#else
    m_agreementWidget->hide();
#endif

    m_errorText1 = makeErrorInfoBar(m_step1Widget);
    layout->addWidget(m_errorText1);

    layout->addSpacing(16);

    m_nextBtn1 = new fluent_b::Button(QStringLiteral("验证并继续"), m_step1Widget);
    m_nextBtn1->setFluentStyle(fluent_b::Button::Accent);
    m_nextBtn1->setFixedHeight(36);
    layout->addWidget(m_nextBtn1);

    layout->addSpacing(16);

    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(4);
    bottomLayout->addStretch();

    auto* hintText = new fluent_tf::Label(QStringLiteral("已拥有账号？"), m_step1Widget);
    hintText->setFluentTypography(Typography::FontRole::Caption);
    hintText->setTextColorRole(fluent_tf::Label::TextColorRole::Secondary);
    auto* loginText = makeLinkButton(QStringLiteral("登录"), m_step1Widget);
    bottomLayout->addWidget(hintText);
    bottomLayout->addWidget(loginText);
    bottomLayout->addStretch();

    layout->addLayout(bottomLayout);

    layout->addStretch();

    connect(loginText, &fluent_b::Button::clicked, this, &RegisterPage::loginRequested);
    connect(m_serviceAgreementText, &fluent_b::Button::clicked, this, &RegisterPage::serviceAgreementRequested);
    connect(m_privacyPolicyText, &fluent_b::Button::clicked, this, &RegisterPage::privacyPolicyRequested);
}

void RegisterPage::setupStep2() {
    m_step2Widget = new QWidget(this);
    auto* layout = new QVBoxLayout(m_step2Widget);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addStretch();

    auto* title = new fluent_tf::Label(QStringLiteral("创建您的账号"), m_step2Widget);
    title->setFluentTypography(Typography::FontRole::Title);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title, 0, Qt::AlignCenter);
    layout->addSpacing(16);

    m_accountInput = new fluent_tf::LineEdit(m_step2Widget);
    m_accountInput->setPlaceholderText(QStringLiteral("设置用户名 (中英文、数字)"));
    m_accountInput->setClearButtonEnabled(true);
    layout->addWidget(m_accountInput);

    layout->addSpacing(16);

    m_passwordInput = new fluent_tf::PasswordBox(m_step2Widget);
    m_passwordInput->setPlaceholderText(QStringLiteral("设置密码 (至少 8 位)"));
    layout->addWidget(m_passwordInput);

    layout->addSpacing(8);

    m_strengthBar = new fluent::status_info::ProgressBar(m_step2Widget);
    m_strengthBar->setFixedHeight(4);
    layout->addWidget(m_strengthBar);

    layout->addSpacing(16);

    m_confirmPasswordInput = new fluent_tf::PasswordBox(m_step2Widget);
    m_confirmPasswordInput->setPlaceholderText(QStringLiteral("确认密码"));
    layout->addWidget(m_confirmPasswordInput);

    m_errorText2 = makeErrorInfoBar(m_step2Widget);
    layout->addWidget(m_errorText2);

    layout->addSpacing(16);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(12);

    m_backBtn2 = new fluent_b::Button(QStringLiteral("上一步"), m_step2Widget);
    m_backBtn2->setFluentStyle(fluent_b::Button::Standard);
    m_backBtn2->setFixedHeight(36);

    m_registerBtn = new fluent_b::Button(QStringLiteral("完成注册"), m_step2Widget);
    m_registerBtn->setFluentStyle(fluent_b::Button::Accent);
    m_registerBtn->setFixedHeight(36);

    btnLayout->addWidget(m_backBtn2);
    btnLayout->addWidget(m_registerBtn);
    layout->addLayout(btnLayout);

    layout->addStretch();
}

void RegisterPage::updateButtonStates() {
    static const QRegularExpression emailRe(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
    static const QRegularExpression phoneRe(R"(^1[3-9]\d{9}$)");

    bool agreementVisible = m_agreementWidget && m_agreementWidget->isVisible();
    bool agreed = !agreementVisible || (m_agreementCheckBox && m_agreementCheckBox->isChecked());

    bool validEmail = emailRe.match(m_emailInput->text().trimmed()).hasMatch();
    bool validPhone = phoneRe.match(m_phoneInput->text().trimmed()).hasMatch();
    bool isCountingDown = m_viewModel->state().emailCountdown > 0
        || m_viewModel->state().phoneCountdown > 0;

    m_emailSendBtn->setEnabled(validEmail && agreed && !isCountingDown);
    m_phoneSendBtn->setEnabled(validPhone && agreed && !isCountingDown);

    auto type = (m_contactPivot->selectedIndex() == 0)
        ? RegisterState::Email : RegisterState::Phone;
    bool validContact = (type == RegisterState::Email) ? validEmail : validPhone;
    QString code = (type == RegisterState::Email)
        ? m_emailCodeInput->text() : m_phoneCodeInput->text();
    m_nextBtn1->setEnabled(validContact && code.trimmed().length() >= 6 && agreed);

    bool hasUsername = !m_accountInput->text().trimmed().isEmpty();
    bool hasPassword = !m_passwordInput->text().isEmpty();
    bool passwordsMatch = !m_passwordInput->text().isEmpty()
        && m_passwordInput->text() == m_confirmPasswordInput->text();
    bool isRegistering = m_viewModel->state().isRegistering;
    m_registerBtn->setEnabled(hasUsername && hasPassword && passwordsMatch && agreed && !isRegistering);
}

void RegisterPage::bindViewModel() {
    connect(m_contactPivot, &fluent_nav::Pivot::currentChanged, this, [this](int index) {
        auto type = (index == 0) ? RegisterState::Email : RegisterState::Phone;
        m_contactStack->setCurrentIndex(index);
        m_viewModel->switchContactType(type);
        });

    connect(m_emailSendBtn, &fluent_b::Button::clicked, this, [this]() {
        m_viewModel->triggerCaptcha(RegisterState::Email);
        });
    connect(m_phoneSendBtn, &fluent_b::Button::clicked, this, [this]() {
        m_viewModel->triggerCaptcha(RegisterState::Phone);
        });

    connect(m_emailInput, &fluent_tf::LineEdit::textChanged, this, [this]() { updateButtonStates(); });
    connect(m_phoneInput, &fluent_tf::LineEdit::textChanged, this, [this]() { updateButtonStates(); });
    connect(m_emailCodeInput, &fluent_tf::LineEdit::textChanged, this, [this]() { updateButtonStates(); });
    connect(m_phoneCodeInput, &fluent_tf::LineEdit::textChanged, this, [this]() { updateButtonStates(); });

    connect(m_nextBtn1, &fluent_b::Button::clicked, this, [this]() {
        auto type = (m_contactPivot->selectedIndex() == 0)
            ? RegisterState::Email : RegisterState::Phone;
        QString contact = (type == RegisterState::Email)
            ? m_emailInput->text() : m_phoneInput->text();
        QString code = (type == RegisterState::Email)
            ? m_emailCodeInput->text() : m_phoneCodeInput->text();
        m_viewModel->submitContactVerification(type, contact, code);
        });

    auto checkMismatch = [this]() {
        bool isPasswordMismatch = !m_confirmPasswordInput->text().isEmpty() &&
            (m_passwordInput->text() != m_confirmPasswordInput->text());
        m_viewModel->updatePasswordStrength(m_passwordInput->text());
        updateButtonStates();
        };
    connect(m_passwordInput, &fluent_tf::LineEdit::textChanged, this, checkMismatch);
    connect(m_confirmPasswordInput, &fluent_tf::LineEdit::textChanged, this, checkMismatch);
    connect(m_accountInput, &fluent_tf::LineEdit::textChanged, this, [this]() { updateButtonStates(); });
    connect(m_agreementCheckBox, &fluent_b::CheckBox::toggled, this, [this]() { updateButtonStates(); });

    connect(m_backBtn2, &fluent_b::Button::clicked, this, [this]() { m_viewModel->goBack(); });
    connect(m_registerBtn, &fluent_b::Button::clicked, this, [this]() {
        auto contactType = (m_contactPivot->selectedIndex() == 0)
            ? RegisterState::Email : RegisterState::Phone;
        QString contact = (contactType == RegisterState::Email)
            ? m_emailInput->text() : m_phoneInput->text();
        m_viewModel->submitRegister(m_accountInput->text(), contactType, contact,
            m_passwordInput->text(), m_confirmPasswordInput->text(),
            isAgreementChecked());
        });

    connect(m_viewModel, &RegisterViewModel::requestCaptcha, this, [this]() {
        m_captchaOverlay->showOverlay();
        });
    connect(m_captchaOverlay, &CaptchaOverlay::verified, this, [this]() {
        m_viewModel->captchaVerified();
        });

    connect(m_viewModel, &RegisterViewModel::stateChanged, this, &RegisterPage::renderState);
}

void RegisterPage::renderState(const RegisterState& state) {
    m_mainStackedWidget->setCurrentIndex(state.currentStep - 1);

    int targetIdx = (state.contactType == RegisterState::Email) ? 0 : 1;
    if (m_contactPivot->selectedIndex() != targetIdx) {
        m_contactPivot->setSelectedIndex(targetIdx);
    }
    if (m_contactStack->currentIndex() != targetIdx) {
        m_contactStack->setCurrentIndex(targetIdx);
    }

    if (state.currentStep == 1) {
        m_errorText1->setMessage(state.errorMessage);
        m_errorText1->setVisible(!state.errorMessage.isEmpty());
    } else {
        m_errorText1->hide();
    }

    if (state.currentStep == 2) {
        m_errorText2->setMessage(state.errorMessage);
        m_errorText2->setVisible(!state.errorMessage.isEmpty());
    } else {
        m_errorText2->hide();
    }

    if (state.emailCodeSent) {
        m_emailCodeInput->show();
        m_emailInput->setEnabled(false);
        if (state.emailCountdown > 0) {
            m_emailSendBtn->setEnabled(false);
            m_emailSendBtn->setText(QStringLiteral("重新发送(%1s)").arg(state.emailCountdown));
        }
        else {
            m_emailSendBtn->setEnabled(true);
            m_emailSendBtn->setText(QStringLiteral("重新发送"));
        }
    }
    else {
        m_emailCodeInput->hide();
        m_emailInput->setEnabled(true);
        m_emailSendBtn->setText(QStringLiteral("获取验证码"));
        m_emailSendBtn->setEnabled(true);
    }

    if (state.phoneCodeSent) {
        m_phoneCodeInput->show();
        m_phoneInput->setEnabled(false);
        if (state.phoneCountdown > 0) {
            m_phoneSendBtn->setEnabled(false);
            m_phoneSendBtn->setText(QStringLiteral("重新发送(%1s)").arg(state.phoneCountdown));
        }
        else {
            m_phoneSendBtn->setEnabled(true);
            m_phoneSendBtn->setText(QStringLiteral("重新发送"));
        }
    }
    else {
        m_phoneCodeInput->hide();
        m_phoneInput->setEnabled(true);
        m_phoneSendBtn->setText(QStringLiteral("获取验证码"));
        m_phoneSendBtn->setEnabled(true);
    }

    if (state.currentStep == 2) {
        switch(state.passwordStrength) {
        case 0: m_strengthBar->setValue(0); break;
        case 1: m_strengthBar->setValue(33); break;
        case 2: m_strengthBar->setValue(66); break;
        case 3: m_strengthBar->setValue(100); break;
        default: m_strengthBar->setValue(0); break;
        }
    }

    if (state.isCaptchaVisible && m_captchaOverlay->isHidden()) {
        m_captchaOverlay->showOverlay();
    }
    else if (!state.isCaptchaVisible && !m_captchaOverlay->isHidden()) {
        m_captchaOverlay->hideOverlay();
    }

    updateButtonStates();
}

void RegisterPage::setAgreementVisible(bool visible) {
    if (m_agreementWidget) {
        m_agreementWidget->setVisible(visible);
    }
}

bool RegisterPage::isAgreementChecked() const {
    return m_agreementCheckBox && m_agreementCheckBox->isChecked();
}

void RegisterPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_captchaOverlay) {
        m_captchaOverlay->resize(size());
    }
}
