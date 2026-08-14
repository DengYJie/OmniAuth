#include "ui/screen/login/LoginPage.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QVBoxLayout>

#include <FluentQt/Navigation.h>

#include "ui/widget/CaptchaOverlay.h"
#include "ui/screen/login/LoginViewModel.h"

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
    // 初始渲染
    renderState(m_viewModel->state());
  }
}

// ── 人脸区域淡入/淡出动画 ────────────────────────────────────
void LoginPage::setFaceSectionVisible(bool visible, bool animated) {
  if (!m_faceSection) return;

  if (visible && m_faceSection->isHidden()) {
    m_faceSection->show();
    if (animated && m_faceAnim) {
      m_faceAnim->stop();
      m_faceAnim->setStartValue(0.0);
      m_faceAnim->setEndValue(1.0);
      m_faceAnim->start();
    } else {
      m_faceOpacityEffect->setOpacity(1.0);
    }
  } else if (!visible && !m_faceSection->isHidden()) {
    if (animated && m_faceAnim) {
      m_faceAnim->stop();
      m_faceAnim->setStartValue(m_faceOpacityEffect->opacity());
      m_faceAnim->setEndValue(0.0);
      m_faceAnim->start();
      // 动画结束后隐藏
      connect(m_faceAnim, &QPropertyAnimation::finished, m_faceSection,
              [this]() {
                if (m_faceOpacityEffect->opacity() < 0.01) {
                  m_faceSection->hide();
                }
              });
    } else {
      m_faceOpacityEffect->setOpacity(0.0);
      m_faceSection->hide();
    }
  }
}

// ── 按钮状态更新 ───────────────────────────────────────────
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
  } else {
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
    } else {
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
  connect(m_viewModel, &LoginViewModel::stateChanged, this, &LoginPage::renderState);

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
  } else if (!state.isCaptchaVisible && !m_captchaOverlay->isHidden()) {
    m_captchaOverlay->hideOverlay();
  }

  // 2. 模式切换：密码 / 验证码
  if (m_loginModePivot->selectedIndex() != state.loginMode) {
    m_loginModePivot->setSelectedIndex(state.loginMode);
  }

  if (state.loginMode == 0) {
    m_passwordSection->show();
    m_smsCodeSection->hide();
    m_loginBtn->setText(QStringLiteral("登录"));
  } else {
    m_passwordSection->hide();
    m_smsCodeSection->show();
    m_loginBtn->setText(QStringLiteral("验证并登录"));

    if (state.isCodeSent) {
      m_smsCodeInput->show();
      if (state.codeCountdown > 0) {
        m_sendCodeBtn->setText(QStringLiteral("重新发送(%1s)").arg(state.codeCountdown));
      } else {
        m_sendCodeBtn->setText(QStringLiteral("重新发送"));
      }
    } else {
      m_sendCodeBtn->setText(QStringLiteral("获取验证码"));
    }
  }

  // 3. 人脸登录区域
  setFaceSectionVisible(state.hasFaceBound, true);

  // 4. 按钮状态（输入完整性 + 登录中/倒计时）
  updateButtonStates();
}

// ── 界面构建 ─────────────────────────────────────────────────
void LoginPage::setupUi() {
  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(76, 32, 76, 32);
  mainLayout->setSpacing(0);

  // 顶部弹簧，使表单居中
  mainLayout->addStretch();

  // ── 1. 顶部标题 ──
  m_titleText = new fluent_tf::Label(QStringLiteral("欢迎使用"), this);
  m_titleText->setFluentTypography(Typography::FontRole::Title);
  m_titleText->setAlignment(Qt::AlignCenter);
  mainLayout->addWidget(m_titleText);
  mainLayout->addSpacing(16);

  // ── 2. 选项卡：密码登录 | 验证码登录 ──
  m_loginModePivot = new fluent_nav::Pivot(this);
  m_loginModePivot->addItem(QStringLiteral("密码登录"));
  m_loginModePivot->addItem(QStringLiteral("验证码登录"));
  m_loginModePivot->setFixedHeight(36);
  m_loginModePivot->setSelectedIndex(0);
  mainLayout->addWidget(m_loginModePivot, 0, Qt::AlignCenter);
  mainLayout->addSpacing(16);

  // ── 3. 账号输入框 ──
  m_accountInput = new fluent_tf::LineEdit(this);
  m_accountInput->setPlaceholderText(QStringLiteral("手机号 / 邮箱"));
  m_accountInput->setClearButtonEnabled(true);
  mainLayout->addWidget(m_accountInput);
  mainLayout->addSpacing(16);

  // ── 4. 密码输入区域（密码登录模式默认可见）──
  m_passwordSection = new QWidget(this);
  auto* pwdLayout = new QVBoxLayout(m_passwordSection);
  pwdLayout->setContentsMargins(0, 0, 0, 0);
  pwdLayout->setSpacing(0);

  m_passwordInput = new fluent_tf::PasswordBox(m_passwordSection);
  m_passwordInput->setPlaceholderText(QStringLiteral("请输入密码"));
  pwdLayout->addWidget(m_passwordInput);

  mainLayout->addWidget(m_passwordSection);

  // ── 5. 验证码输入区域（验证码登录模式可见，默认隐藏）──
  m_smsCodeSection = new QWidget(this);
  m_smsCodeSection->hide();
  auto* smsLayout = new QHBoxLayout(m_smsCodeSection);
  smsLayout->setContentsMargins(0, 0, 0, 0);
  smsLayout->setSpacing(12);

  m_smsCodeInput = new fluent_tf::LineEdit(m_smsCodeSection);
  m_smsCodeInput->setPlaceholderText(QStringLiteral("请输入验证码"));

  m_sendCodeBtn = new fluent_b::Button(QStringLiteral("获取验证码"), m_smsCodeSection);
  m_sendCodeBtn->setFluentStyle(fluent_b::Button::Standard);
  m_sendCodeBtn->setFixedWidth(108);
  m_sendCodeBtn->setFixedHeight(36);

  smsLayout->addWidget(m_smsCodeInput);
  smsLayout->addWidget(m_sendCodeBtn);

  mainLayout->addWidget(m_smsCodeSection);
  mainLayout->addSpacing(16);

  // ── 6. 服务协议与隐私政策勾选 ──
  m_agreementWidget = new QWidget(this);
  auto* agreementLayout = new QHBoxLayout(m_agreementWidget);
  agreementLayout->setContentsMargins(0, 0, 0, 16);
  agreementLayout->setSpacing(4);

  m_agreementCheckBox = new fluent_b::CheckBox(m_agreementWidget);

  auto* agreePrefix =
      new fluent_tf::Label(QStringLiteral("已阅读并同意 "), m_agreementWidget);
  agreePrefix->setFluentTypography(Typography::FontRole::Caption);
  agreePrefix->setTextColorRole(fluent_tf::Label::TextColorRole::Secondary);

  m_serviceAgreementText =
      makeLinkButton(QStringLiteral("服务协议"), m_agreementWidget);

  auto* andText = new fluent_tf::Label(QStringLiteral(" 和 "), m_agreementWidget);
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

  mainLayout->addWidget(m_agreementWidget);
#if defined(QT_DEBUG) || !defined(NDEBUG)
  m_agreementWidget->show();
#else
  m_agreementWidget->hide();
#endif

  // ── 7. 登录按钮 ──
  m_loginBtn = new fluent_b::Button(QStringLiteral("登录"), this);
  m_loginBtn->setFluentStyle(fluent_b::Button::Accent);
  m_loginBtn->setFixedHeight(36);
  mainLayout->addWidget(m_loginBtn);

  // ── 8. 人脸登录区域（初始隐藏，含淡入动画）──
  m_faceSection = new QWidget(this);
  m_faceSection->hide();
  auto* faceLayout = new QVBoxLayout(m_faceSection);
  faceLayout->setContentsMargins(0, 16, 0, 0);
  faceLayout->setSpacing(16);

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

  mainLayout->addWidget(m_faceSection);

  // ── 9. 底部弹簧 + 链接 ──
  mainLayout->addStretch();

  auto* bottomLayout = new QHBoxLayout();
  bottomLayout->setContentsMargins(0, 0, 0, 0);
  bottomLayout->addStretch();

  m_registerText = makeLinkButton(QStringLiteral("注册账号"), this);
  auto* sepText = new fluent_tf::Label(QStringLiteral("  |  "), this);
  sepText->setFluentTypography(Typography::FontRole::Caption);
  sepText->setTextColorRole(fluent_tf::Label::TextColorRole::Secondary);
  m_forgotText = makeLinkButton(QStringLiteral("找回密码"), this);

  bottomLayout->addWidget(m_registerText);
  bottomLayout->addWidget(sepText);
  bottomLayout->addWidget(m_forgotText);
  bottomLayout->addStretch();

  mainLayout->addLayout(bottomLayout);

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
