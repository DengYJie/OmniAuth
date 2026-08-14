#pragma once

#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTimer>
#include <QWidget>

#include <FluentQt/BasicInput.h>
#include <FluentQt/Layout.h>
#include <FluentQt/TextFields.h>

namespace fluent::basicinput { class Button; class CheckBox; class HyperlinkButton; }
namespace fluent::navigation { class Pivot; }
namespace fluent::textfields { class Label; class LineEdit; class PasswordBox; }

class CaptchaOverlay;
class LoginViewModel;
struct LoginState;

/**
 * @brief 登录页面 (LoginPage)
 *
 * 支持密码登录 / 验证码登录双模式切换 (Segmented Pivot)，
 * 输入账号后自动检测是否已绑定人脸并展示刷脸快捷入口。
 */
class LoginPage : public QWidget {
  Q_OBJECT

 public:
  explicit LoginPage(LoginViewModel* viewModel, QWidget* parent = nullptr);
  ~LoginPage() override = default;

 signals:
  void faceLoginRequested();        // 转由 MainWindow 的 MainViewModel 处理
  void forgotPasswordRequested();   // 预留
  void registerRequested();         // 预留
  void serviceAgreementRequested(); // 预留
  void privacyPolicyRequested();    // 预留

 public slots:
  void setAgreementVisible(bool visible);
  [[nodiscard]] bool isAgreementChecked() const;

 protected:
  void resizeEvent(QResizeEvent* event) override;

 private:
  void setupUi();
  void bindViewModel();
  void updateButtonStates();
  void renderState(const LoginState& state);
  void setFaceSectionVisible(bool visible, bool animated = true);

  LoginViewModel* m_viewModel = nullptr;

  // 标题
  fluent::textfields::Label* m_titleText = nullptr;

  // 选项卡
  fluent::navigation::Pivot* m_loginModePivot = nullptr;

  // 账号输入
  fluent::textfields::LineEdit* m_accountInput = nullptr;

  // 密码登录区域
  QWidget* m_passwordSection = nullptr;
  fluent::textfields::PasswordBox* m_passwordInput = nullptr;

  // 验证码登录区域
  QWidget* m_smsCodeSection = nullptr;
  fluent::textfields::LineEdit* m_smsCodeInput = nullptr;
  fluent::basicinput::Button* m_sendCodeBtn = nullptr;

  // 登录按钮
  fluent::basicinput::Button* m_loginBtn = nullptr;

  // 协议勾选
  QWidget* m_agreementWidget = nullptr;
  fluent::basicinput::CheckBox* m_agreementCheckBox = nullptr;
  fluent::basicinput::HyperlinkButton* m_serviceAgreementText = nullptr;
  fluent::basicinput::HyperlinkButton* m_privacyPolicyText = nullptr;

  // 人脸登录区域（条件展示）
  QWidget* m_faceSection = nullptr;
  fluent::basicinput::Button* m_faceLoginBtn = nullptr;
  QGraphicsOpacityEffect* m_faceOpacityEffect = nullptr;
  QPropertyAnimation* m_faceAnim = nullptr;

  // 底部链接
  fluent::basicinput::HyperlinkButton* m_forgotText = nullptr;
  fluent::basicinput::HyperlinkButton* m_registerText = nullptr;

  // 验证码悬浮蒙层
  class CaptchaOverlay* m_captchaOverlay = nullptr;
};
