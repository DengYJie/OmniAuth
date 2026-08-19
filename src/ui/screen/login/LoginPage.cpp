#include "ui/screen/login/LoginPage.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QVBoxLayout>

#include <FluentQt/Navigation.h>

#include "ui/screen/login/LoginViewModel.h"
#include "ui/widget/CaptchaOverlay.h"

namespace fluent_b = fluent::basicinput;
namespace fluent_tf = fluent::textfields;
namespace fluent_nav = fluent::navigation;
namespace fluent_ly = fluent::layout;

namespace {

    // 使用 FluentQt 图标字体为 LineEdit 创建前缀图标 QIcon。
    // (Prefix icons have been removed as per specification)

    // 构建"—— 或 ——"样式的分隔行。
    QWidget* makeOrDivider(QWidget* parent) {
        auto* row = new QWidget(parent);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(12);

        auto* left = new fluent_ly::Divider(Qt::Horizontal, row);
        auto* orLabel = new fluent_tf::Label(QStringLiteral("或"), row);
        orLabel->setFluentTypography(Typography::FontRole::Caption);
        orLabel->setTextColorRole(fluent_tf::Label::TextColorRole::Secondary);
        auto* right = new fluent_ly::Divider(Qt::Horizontal, row);

        layout->addWidget(left, 1);
        layout->addWidget(orLabel);
        layout->addWidget(right, 1);
        return row;
    }

    // 底部可点击链接（Caption 风格文本按钮）。
    fluent_b::HyperlinkButton* makeLinkButton(const QString& text, QWidget* parent) {
        auto* btn = new fluent_b::HyperlinkButton(text, parent);
        btn->setFluentStyle(fluent_b::Button::Subtle);
        btn->setFluentSize(fluent_b::Button::Small);
        btn->setFont(Typography::fontStyle(Typography::FontRole::Caption).toQFont());
        return btn;
    }

}  // namespace

LoginPage::LoginPage(LoginViewModel* viewModel, QWidget* parent)
    : QWidget(parent), m_viewModel(viewModel) {
    setupUi();
    if (m_viewModel) {
        bindViewModel();
    }
}

void LoginPage::setFaceSectionVisible(bool visible, bool animated) {
    if (!m_faceSection) return;

    if (visible && m_faceSection->isHidden()) {
        m_faceSection->show();
        if (animated && m_faceAnim) {
            m_faceAnim->stop();
            m_faceAnim->setStartValue(m_faceOpacityEffect->opacity());
            m_faceAnim->setEndValue(1.0);
            m_faceAnim->start();
        }
        else {
            m_faceOpacityEffect->setOpacity(1.0);
        }
    }
    else if (!visible && !m_faceSection->isHidden()) {
        if (animated && m_faceAnim) {
            m_faceAnim->stop();
            m_faceAnim->setStartValue(m_faceOpacityEffect->opacity());
            m_faceAnim->setEndValue(0.0);
            m_faceAnim->start();
            
            disconnect(m_faceAnim, &QPropertyAnimation::finished, nullptr, nullptr);
            connect(m_faceAnim, &QPropertyAnimation::finished, this,
                [this]() {
                    if (m_faceOpacityEffect->opacity() < 0.01) {
                        m_faceSection->hide();
                    }
                });
        }
        else {
            m_faceOpacityEffect->setOpacity(0.0);
            m_faceSection->hide();
        }
    }
}

void LoginPage::updateButtonStates() {
    static const QRegularExpression emailRe(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
    static const QRegularExpression phoneRe(R"(^1[3-9]\d{9}$)");
    QString account = m_accountInput->text().trimmed();
    bool validAccount = emailRe.match(account).hasMatch() || phoneRe.match(account).hasMatch();

    int mode = m_viewModel->state().loginMode;
    bool isLoggingIn = m_viewModel->state().isLoggingIn;
    bool isCountingDown = m_viewModel->state().codeCountdown > 0;

    bool agreementVisible = m_agreementWidget && m_agreementWidget->isVisible();
    bool agreementOk = !agreementVisible || (m_agreementCheckBox && m_agreementCheckBox->isChecked());

    if (mode == 0) {
        bool hasPassword = !m_passwordInput->text().isEmpty();
        m_loginBtn->setEnabled(validAccount && hasPassword && agreementOk && !isLoggingIn);
    }
    else {
        bool hasCode = m_smsCodeInput->text().trimmed().length() >= 6;
        m_loginBtn->setEnabled(validAccount && hasCode && agreementOk && !isLoggingIn);
    }

    m_sendCodeBtn->setEnabled(validAccount && agreementOk && !isCountingDown);
}

// ── 绑定 ViewModel ───────────────────────────────────────────
void LoginPage::bindViewModel() {
    // Pivot 模式切换
    connect(m_loginModePivot, &fluent_nav::Pivot::currentChanged, this, [this](int index) {
        m_viewModel->switchLoginMode(index);
        });

    // 账号输入 → 人脸绑定检测（失焦 + 输入变化触发 500ms 防抖）
    connect(m_accountInput, &fluent_tf::LineEdit::editingFinished, this, [this]() {
        m_viewModel->checkAccountFaceBinding(m_accountInput->text());
        });
    connect(m_accountInput, &fluent_tf::LineEdit::textChanged, this, [this](const QString& text) {
        m_viewModel->checkAccountFaceBinding(text);
        });

    // 输入变化 → 按钮状态
    connect(m_accountInput, &fluent_tf::LineEdit::textChanged, this, [this]() { updateButtonStates(); });
    connect(m_passwordInput, &fluent_tf::LineEdit::textChanged, this, [this]() { updateButtonStates(); });
    connect(m_smsCodeInput, &fluent_tf::LineEdit::textChanged, this, [this]() { updateButtonStates(); });
    if (m_agreementCheckBox) {
        connect(m_agreementCheckBox, &fluent_b::CheckBox::toggled, this, [this]() { updateButtonStates(); });
    }

    // 登录按钮：根据当前模式选择不同的登录方式
    connect(m_loginBtn, &fluent_b::Button::clicked, this, [this]() {
        if (m_viewModel->state().loginMode == 0) {
            m_viewModel->loginClicked(m_accountInput->text(), m_passwordInput->text());
        }
        else {
            m_viewModel->smsLoginClicked(m_accountInput->text(), m_smsCodeInput->text());
        }
        });

    // 验证码模式：获取验证码 → 唤起滑块验证
    connect(m_sendCodeBtn, &fluent_b::Button::clicked, this, [this]() {
        m_viewModel->requestSmsCode();
        });

    // 滑块验证完成
    connect(m_captchaOverlay, &CaptchaOverlay::verified, this, [this]() {
        m_viewModel->captchaVerified();
        });

    // 人脸登录
    connect(m_faceLoginBtn, &fluent_b::Button::clicked, this, &LoginPage::faceLoginRequested);

    // 底部链接
    connect(m_forgotText, &fluent_b::Button::clicked, this, &LoginPage::forgotPasswordRequested);
    connect(m_registerText, &fluent_b::Button::clicked, this, &LoginPage::registerRequested);
    connect(m_serviceAgreementText, &fluent_b::Button::clicked, this, &LoginPage::serviceAgreementRequested);
    connect(m_privacyPolicyText, &fluent_b::Button::clicked, this, &LoginPage::privacyPolicyRequested);

    // ViewModel → View 状态流
    m_viewModel->observe(this, &LoginPage::renderState);

    // 验证码模式下滑块验证唤起
    connect(m_viewModel, &LoginViewModel::requestCaptcha, this, [this]() {
        m_captchaOverlay->showOverlay();
        });
}

// ── 渲染状态 ─────────────────────────────────────────────────
void LoginPage::renderState(const LoginState& state) {
    // 1. 控制验证码蒙层
    if (state.isCaptchaVisible && m_captchaOverlay->isHidden()) {
        m_captchaOverlay->showOverlay();
    }
    else if (!state.isCaptchaVisible && !m_captchaOverlay->isHidden()) {
        m_captchaOverlay->hideOverlay();
    }

    // 2. 模式切换：密码 / 验证码
    if (m_loginModePivot->selectedIndex() != state.loginMode) {
        m_loginModePivot->setSelectedIndex(state.loginMode);
    }

    if (state.loginMode == 0) {
        m_passwordInput->show();
        m_smsCodeSection->hide();
        m_loginBtn->setText(QStringLiteral("登录"));
    }
    else {
        m_passwordInput->hide();
        m_smsCodeSection->show();
        m_loginBtn->setText(QStringLiteral("验证并登录"));

        if (state.isCodeSent) {
            m_smsCodeInput->show();
            if (state.codeCountdown > 0) {
                m_sendCodeBtn->setText(QStringLiteral("重新发送(%1s)").arg(state.codeCountdown));
            }
            else {
                m_sendCodeBtn->setText(QStringLiteral("重新发送"));
            }
        }
        else {
            m_sendCodeBtn->setText(QStringLiteral("获取验证码"));
        }
    }

    // 3. 人脸登录区域
    setFaceSectionVisible(state.hasFaceBound, true);

    // 4. 按钮状态（输入完整性 + 登录中/倒计时）
    updateButtonStates();
}

void LoginPage::setupUi() {
    auto* pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    pageLayout->setAlignment(Qt::AlignCenter);

    auto* card = new fluent_ly::Card(this);
    card->setAppearance(fluent_ly::Card::Layer);
    card->setBorderVisible(true);
    card->setFixedSize(360, 440);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(40, 28, 40, 28);
    cardLayout->setSpacing(14);
    cardLayout->setAlignment(Qt::AlignVCenter);

    m_titleText = new fluent_tf::Label(QStringLiteral("欢迎使用"), card);
    m_titleText->setFluentTypography(Typography::FontRole::Title);
    m_titleText->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(m_titleText);

    m_loginModePivot = new fluent_nav::Pivot(card);
    m_loginModePivot->addItem(QStringLiteral("密码登录"));
    m_loginModePivot->addItem(QStringLiteral("验证码登录"));
    m_loginModePivot->setFixedWidth(200);
    m_loginModePivot->setSelectedIndex(0);
    cardLayout->addWidget(m_loginModePivot, 0, Qt::AlignCenter);

    auto* formWidget = new QWidget(card);
    formWidget->setFixedWidth(260);
    auto* formLayout = new QFormLayout(formWidget);
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setVerticalSpacing(14);
    formLayout->setHorizontalSpacing(0);

    m_accountInput = new fluent_tf::LineEdit(formWidget);
    m_accountInput->setPlaceholderText(QStringLiteral("手机号 / 邮箱"));
    m_accountInput->setClearButtonEnabled(true);
    m_accountInput->setFixedHeight(32);
    formLayout->addRow(m_accountInput);

    m_passwordInput = new fluent_tf::PasswordBox(formWidget);
    m_passwordInput->setPlaceholderText(QStringLiteral("请输入密码"));
    m_passwordInput->setFixedHeight(32);
    formLayout->addRow(m_passwordInput);

    m_smsCodeSection = new QWidget(formWidget);
    m_smsCodeSection->hide();
    m_smsCodeSection->setFixedHeight(32);
    auto* smsLayout = new QHBoxLayout(m_smsCodeSection);
    smsLayout->setContentsMargins(0, 0, 0, 0);
    smsLayout->setSpacing(10);

    m_smsCodeInput = new fluent_tf::LineEdit(m_smsCodeSection);
    m_smsCodeInput->setPlaceholderText(QStringLiteral("请输入验证码"));
    m_smsCodeInput->setFixedHeight(32);

    m_sendCodeBtn = new fluent_b::Button(QStringLiteral("获取验证码"), m_smsCodeSection);
    m_sendCodeBtn->setFluentStyle(fluent_b::Button::Standard);
    m_sendCodeBtn->setFixedWidth(100);
    m_sendCodeBtn->setFixedHeight(32);

    smsLayout->addWidget(m_smsCodeInput);
    smsLayout->addWidget(m_sendCodeBtn);

    formLayout->addRow(m_smsCodeSection);

    cardLayout->addWidget(formWidget, 0, Qt::AlignCenter);

    m_agreementWidget = new QWidget(formWidget);
    auto* agreementLayout = new QHBoxLayout(m_agreementWidget);
    agreementLayout->setContentsMargins(0, 0, 0, 0);
    agreementLayout->setSpacing(0);

    m_agreementCheckBox = new fluent_b::CheckBox(m_agreementWidget);

    auto* agreePrefix = new fluent_tf::Label(QStringLiteral("阅读并同意"), m_agreementWidget);
    agreePrefix->setFluentTypography(Typography::FontRole::Caption);
    agreePrefix->setTextColorRole(fluent_tf::Label::TextColorRole::Secondary);

    m_serviceAgreementText =
        makeLinkButton(QStringLiteral("服务协议"), m_agreementWidget);

    auto* andText = new fluent_tf::Label(QStringLiteral("与"), m_agreementWidget);
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

    m_loginBtn = new fluent_b::Button(QStringLiteral("登录"), card);
    m_loginBtn->setFluentStyle(fluent_b::Button::Accent);
    m_loginBtn->setFixedSize(200, 36);
    cardLayout->addWidget(m_loginBtn, 0, Qt::AlignCenter);

    m_faceSection = new QWidget(card);
    m_faceSection->hide();
    auto* faceLayout = new QVBoxLayout(m_faceSection);
    faceLayout->setContentsMargins(0, 0, 0, 0);
    faceLayout->setSpacing(14);

    faceLayout->addWidget(makeOrDivider(m_faceSection));

    m_faceLoginBtn = new fluent_b::Button(QStringLiteral("刷脸快捷登录"), m_faceSection);
    m_faceLoginBtn->setFluentStyle(fluent_b::Button::Standard);
    m_faceLoginBtn->setIconGlyph(Typography::Icons::glyph(QStringLiteral("ic_fluent_scan_person_20_regular")));
    m_faceLoginBtn->setFluentLayout(fluent_b::Button::IconBefore);
    m_faceLoginBtn->setFixedHeight(36);
    faceLayout->addWidget(m_faceLoginBtn);

    // 透明动画效果
    m_faceOpacityEffect = new QGraphicsOpacityEffect(m_faceSection);
    m_faceOpacityEffect->setOpacity(0.0);
    m_faceSection->setGraphicsEffect(m_faceOpacityEffect);

    m_faceAnim = new QPropertyAnimation(m_faceOpacityEffect, "opacity", this);
    m_faceAnim->setDuration(250);
    m_faceAnim->setEasingCurve(QEasingCurve::OutCubic);

    cardLayout->addWidget(m_faceSection);

    // ── 9. 底部链接 ──
    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(0, 6, 0, 0);
    bottomLayout->addStretch();

    m_registerText = makeLinkButton(QStringLiteral("注册账号"), card);
    auto* sepText = new fluent_tf::Label(QStringLiteral("  |  "), card);
    sepText->setFluentTypography(Typography::FontRole::Caption);
    sepText->setTextColorRole(fluent_tf::Label::TextColorRole::Secondary);
    m_forgotText = makeLinkButton(QStringLiteral("找回密码"), card);

    bottomLayout->addWidget(m_registerText);
    bottomLayout->addWidget(sepText);
    bottomLayout->addWidget(m_forgotText);
    bottomLayout->addStretch();

    cardLayout->addLayout(bottomLayout);

    pageLayout->addWidget(card);

    // ── 10. 验证码悬浮蒙层 ──
    m_captchaOverlay = new CaptchaOverlay(this);
}

void LoginPage::setAgreementVisible(bool visible) {
    if (m_agreementWidget) {
        m_agreementWidget->setVisible(visible);
    }
}

bool LoginPage::isAgreementChecked() const {
    return m_agreementCheckBox && m_agreementCheckBox->isChecked();
}

void LoginPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_captchaOverlay) {
        m_captchaOverlay->resize(size());
    }
}
