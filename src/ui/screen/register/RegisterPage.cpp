#include "ui/screen/register/RegisterPage.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QVBoxLayout>

#include <FluentQt/Layout.h>
#include <FluentQt/Navigation.h>
#include <FluentQt/StatusInfo.h>

#include "ui/screen/register/RegisterViewModel.h"
#include "ui/widget/CaptchaOverlay.h"

namespace fluent_b = fluent::basicinput;
namespace fluent_tf = fluent::textfields;
namespace fluent_nav = fluent::navigation;
namespace fluent_ly = fluent::layout;

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
    }
}

void RegisterPage::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
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
    layout->setSpacing(0);
    layout->setAlignment(Qt::AlignCenter);

    auto* card = new fluent_ly::Card(m_step1Widget);
    card->setAppearance(fluent_ly::Card::Layer);
    card->setBorderVisible(true);
    card->setFixedSize(360, 440);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(28, 28, 28, 28);
    cardLayout->setSpacing(14);
    cardLayout->setAlignment(Qt::AlignVCenter);

    auto* title = new fluent_tf::Label(QStringLiteral("选择注册方式"), card);
    title->setFluentTypography(Typography::FontRole::Title);
    title->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(title, 0, Qt::AlignCenter);

    m_contactPivot = new fluent_nav::Pivot(card);
    m_contactPivot->addItem(QStringLiteral("邮箱注册"));
    m_contactPivot->addItem(QStringLiteral("手机号注册"));
    m_contactPivot->setFixedWidth(200);
    m_contactPivot->setSelectedIndex(0);
    cardLayout->addWidget(m_contactPivot, 0, Qt::AlignCenter);

    auto* formWidget = new QWidget(card);
    formWidget->setFixedWidth(260);
    auto* formLayout = new QFormLayout(formWidget);
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setVerticalSpacing(14);
    formLayout->setHorizontalSpacing(0);

    auto* contactRowWidget = new QWidget(formWidget);
    auto* contactRow = new QHBoxLayout(contactRowWidget);
    contactRow->setContentsMargins(0, 0, 0, 0);
    contactRow->setSpacing(12);
    m_contactInput = new fluent_tf::LineEdit(contactRowWidget);
    m_contactInput->setPlaceholderText(QStringLiteral("邮箱地址"));
    m_contactInput->setClearButtonEnabled(true);
    m_contactInput->setFixedHeight(32);
    m_sendBtn = new fluent_b::Button(QStringLiteral("获取验证码"), contactRowWidget);
    m_sendBtn->setFluentStyle(fluent_b::Button::Standard);
    m_sendBtn->setFixedWidth(100);
    m_sendBtn->setFixedHeight(32);
    contactRow->addWidget(m_contactInput);
    contactRow->addWidget(m_sendBtn);
    formLayout->addRow(contactRowWidget);

    m_codeInput = new fluent_tf::LineEdit(formWidget);
    m_codeInput->setPlaceholderText(QStringLiteral("6 位验证码"));
    m_codeInput->setFixedHeight(32);
    m_codeInput->hide();
    formLayout->addRow(m_codeInput);

    cardLayout->addWidget(formWidget, 0, Qt::AlignCenter);

    m_agreementWidget = new QWidget(formWidget);
    auto* agreementLayout = new QHBoxLayout(m_agreementWidget);
    agreementLayout->setContentsMargins(0, 0, 0, 0);
    agreementLayout->setSpacing(0);

    m_agreementCheckBox = new fluent_b::CheckBox(m_agreementWidget);
    auto* agreePrefix =
        new fluent_tf::Label(QStringLiteral("阅读并同意"), m_agreementWidget);
    agreePrefix->setFluentTypography(Typography::FontRole::Caption);
    agreePrefix->setTextColorRole(fluent_tf::Label::TextColorRole::Secondary);
    m_serviceAgreementText =
        makeLinkButton(QStringLiteral("服务协议"), m_agreementWidget);
    auto* andText =
        new fluent_tf::Label(QStringLiteral("与"), m_agreementWidget);
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

    formLayout->addRow(m_agreementWidget);
#if defined(QT_DEBUG) || !defined(NDEBUG)
    m_agreementWidget->show();
#else
    m_agreementWidget->hide();
#endif

    m_errorText1 = makeErrorInfoBar(card);
    cardLayout->addWidget(m_errorText1);

    m_nextBtn1 = new fluent_b::Button(QStringLiteral("验证并继续"), card);
    m_nextBtn1->setFluentStyle(fluent_b::Button::Accent);
    m_nextBtn1->setFixedWidth(260);
    m_nextBtn1->setFixedHeight(36);
    cardLayout->addWidget(m_nextBtn1, 0, Qt::AlignCenter);

    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(0, 4, 0, 0);
    bottomLayout->setSpacing(4);
    bottomLayout->addStretch();

    auto* hintText = new fluent_tf::Label(QStringLiteral("已拥有账号？"), card);
    hintText->setFluentTypography(Typography::FontRole::Caption);
    hintText->setTextColorRole(fluent_tf::Label::TextColorRole::Secondary);
    auto* loginText = makeLinkButton(QStringLiteral("登录"), card);
    bottomLayout->addWidget(hintText);
    bottomLayout->addWidget(loginText);
    bottomLayout->addStretch();

    cardLayout->addLayout(bottomLayout);

    layout->addWidget(card);

    connect(loginText, &fluent_b::Button::clicked, this, &RegisterPage::loginRequested);
    connect(m_serviceAgreementText, &fluent_b::Button::clicked, this, &RegisterPage::serviceAgreementRequested);
    connect(m_privacyPolicyText, &fluent_b::Button::clicked, this, &RegisterPage::privacyPolicyRequested);
}

void RegisterPage::setupStep2() {
    m_step2Widget = new QWidget(this);
    auto* layout = new QVBoxLayout(m_step2Widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->setAlignment(Qt::AlignCenter);

    auto* card = new fluent_ly::Card(m_step2Widget);
    card->setAppearance(fluent_ly::Card::Layer);
    card->setBorderVisible(true);
    card->setFixedSize(360, 440);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(28, 28, 28, 28);
    cardLayout->setSpacing(14);
    cardLayout->setAlignment(Qt::AlignVCenter);

    auto* title = new fluent_tf::Label(QStringLiteral("创建您的账号"), card);
    title->setFluentTypography(Typography::FontRole::Title);
    title->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(title, 0, Qt::AlignCenter);

    auto* formWidget2 = new QWidget(card);
    formWidget2->setFixedWidth(260);
    auto* formLayout2 = new QFormLayout(formWidget2);
    formLayout2->setContentsMargins(0, 0, 0, 0);
    formLayout2->setVerticalSpacing(14);
    formLayout2->setHorizontalSpacing(0);

    m_accountInput = new fluent_tf::LineEdit(formWidget2);
    m_accountInput->setPlaceholderText(QStringLiteral("设置用户名 (中英文、数字)"));
    m_accountInput->setClearButtonEnabled(true);
    m_accountInput->setFixedHeight(32);
    formLayout2->addRow(m_accountInput);

    m_passwordInput = new fluent_tf::PasswordBox(formWidget2);
    m_passwordInput->setPlaceholderText(QStringLiteral("设置密码 (至少 8 位)"));
    m_passwordInput->setFixedHeight(32);
    formLayout2->addRow(m_passwordInput);

    m_strengthBar = new fluent::status_info::ProgressBar(formWidget2);
    m_strengthBar->setFixedHeight(4);
    formLayout2->addRow(m_strengthBar);

    m_confirmPasswordInput = new fluent_tf::PasswordBox(formWidget2);
    m_confirmPasswordInput->setPlaceholderText(QStringLiteral("确认密码"));
    m_confirmPasswordInput->setFixedHeight(32);
    formLayout2->addRow(m_confirmPasswordInput);

    cardLayout->addWidget(formWidget2, 0, Qt::AlignCenter);

    m_errorText2 = makeErrorInfoBar(card);
    cardLayout->addWidget(m_errorText2);

    auto* btnWidget = new QWidget(card);
    btnWidget->setFixedWidth(260);
    auto* btnLayout = new QHBoxLayout(btnWidget);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(12);

    m_backBtn2 = new fluent_b::Button(QStringLiteral("上一步"), btnWidget);
    m_backBtn2->setFluentStyle(fluent_b::Button::Standard);
    m_backBtn2->setFixedHeight(36);

    m_registerBtn = new fluent_b::Button(QStringLiteral("完成注册"), btnWidget);
    m_registerBtn->setFluentStyle(fluent_b::Button::Accent);
    m_registerBtn->setFixedHeight(36);

    btnLayout->addWidget(m_backBtn2);
    btnLayout->addWidget(m_registerBtn);
    cardLayout->addWidget(btnWidget, 0, Qt::AlignCenter);

    layout->addWidget(card);
}

void RegisterPage::updateButtonStates() {
    static const QRegularExpression emailRe(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
    static const QRegularExpression phoneRe(R"(^1[3-9]\d{9}$)");

    bool agreementVisible = m_agreementWidget && m_agreementWidget->isVisible();
    bool agreed = !agreementVisible || (m_agreementCheckBox && m_agreementCheckBox->isChecked());

    const RegisterState& state = m_viewModel->state();
    bool isEmail = state.contactType == RegisterState::Email;

    QString contact = m_contactInput->text().trimmed();
    bool validContact = isEmail
        ? emailRe.match(contact).hasMatch()
        : phoneRe.match(contact).hasMatch();

    int countdown = isEmail ? state.emailCountdown : state.phoneCountdown;
    m_sendBtn->setEnabled(validContact && agreed && countdown == 0);

    QString code = m_codeInput->text();
    m_nextBtn1->setEnabled(validContact && code.trimmed().length() >= 6 && agreed);

    bool hasUsername = !m_accountInput->text().trimmed().isEmpty();
    bool hasPassword = !m_passwordInput->text().isEmpty();
    bool passwordsMatch = !m_passwordInput->text().isEmpty()
        && m_passwordInput->text() == m_confirmPasswordInput->text();
    m_registerBtn->setEnabled(hasUsername && hasPassword && passwordsMatch && agreed && !state.isRegistering);
}

void RegisterPage::applyContactMode() {
    if (!m_contactInput || !m_sendBtn || !m_codeInput || !m_viewModel) return;

    const RegisterState& state = m_viewModel->state();
    bool isEmail = state.contactType == RegisterState::Email;

    // 动态切换输入框语义 (占位符 / 是否可用 / 验证码可见性)
    m_contactInput->setPlaceholderText(isEmail
        ? QStringLiteral("邮箱地址") : QStringLiteral("手机号码"));
    m_codeInput->setPlaceholderText(QStringLiteral("6 位验证码"));

    bool codeSent = isEmail ? state.emailCodeSent : state.phoneCodeSent;
    int countdown = isEmail ? state.emailCountdown : state.phoneCountdown;

    m_contactInput->setEnabled(!codeSent);
    m_codeInput->setVisible(codeSent);

    if (codeSent) {
        m_sendBtn->setText(countdown > 0
            ? QStringLiteral("重新发送(%1s)").arg(countdown)
            : QStringLiteral("重新发送"));
    }
    else {
        m_sendBtn->setText(QStringLiteral("获取验证码"));
    }
}

void RegisterPage::bindViewModel() {
    connect(m_contactPivot, &fluent_nav::Pivot::currentChanged, this, [this](int index) {
        auto type = (index == 0) ? RegisterState::Email : RegisterState::Phone;
        if (m_viewModel->state().contactType != type) {
            // 切换注册方式时清空输入，避免残留另一类型的无效内容
            m_contactInput->clear();
            m_codeInput->clear();
        }
        m_viewModel->switchContactType(type);
        applyContactMode();   // 根据 Pivot 选择动态更新 UI
        updateButtonStates();
        });

    connect(m_sendBtn, &fluent_b::Button::clicked, this, [this]() {
        auto type = (m_contactPivot->selectedIndex() == 0)
            ? RegisterState::Email : RegisterState::Phone;
        m_viewModel->triggerCaptcha(type);
        });

    connect(m_contactInput, &fluent_tf::LineEdit::textChanged, this, [this]() { updateButtonStates(); });
    connect(m_codeInput, &fluent_tf::LineEdit::textChanged, this, [this]() { updateButtonStates(); });

    connect(m_nextBtn1, &fluent_b::Button::clicked, this, [this]() {
        auto type = (m_contactPivot->selectedIndex() == 0)
            ? RegisterState::Email : RegisterState::Phone;
        m_viewModel->submitContactVerification(type, m_contactInput->text(),
            m_codeInput->text());
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
        m_viewModel->submitRegister(m_accountInput->text(), contactType,
            m_contactInput->text(), m_passwordInput->text(),
            m_confirmPasswordInput->text(), isAgreementChecked());
        });

    connect(m_viewModel, &RegisterViewModel::requestCaptcha, this, [this]() {
        m_captchaOverlay->showOverlay();
        });
    connect(m_captchaOverlay, &CaptchaOverlay::verified, this, [this]() {
        m_viewModel->captchaVerified();
        });

    m_viewModel->observe(this, &RegisterPage::renderState);
}

void RegisterPage::renderState(const RegisterState& state) {
    m_mainStackedWidget->setCurrentIndex(state.currentStep - 1);

    int targetIdx = (state.contactType == RegisterState::Email) ? 0 : 1;
    if (m_contactPivot->selectedIndex() != targetIdx) {
        m_contactPivot->setSelectedIndex(targetIdx);  // 触发 currentChanged 动态更新
    }

    if (state.currentStep == 1) {
        m_errorText1->setMessage(state.errorMessage);
        m_errorText1->setVisible(!state.errorMessage.isEmpty());
    }
    else {
        m_errorText1->hide();
    }

    if (state.currentStep == 2) {
        m_errorText2->setMessage(state.errorMessage);
        m_errorText2->setVisible(!state.errorMessage.isEmpty());
    }
    else {
        m_errorText2->hide();
    }

    // 根据当前注册方式与倒计时状态动态更新 UI
    applyContactMode();

    if (state.currentStep == 2) {
        switch (state.passwordStrength) {
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
