#pragma once

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <FluentQt/BasicInput.h>
#include <FluentQt/Navigation.h>
#include <FluentQt/StatusInfo.h>
#include <FluentQt/TextFields.h>

namespace fluent::basicinput { class Button; class CheckBox; class HyperlinkButton; }
namespace fluent::navigation { class Pivot; class StackContentHost; }
namespace fluent::textfields { class LineEdit; class PasswordBox; }

class CaptchaOverlay;
class RegisterViewModel;
struct RegisterState;

/**
 * @brief 多步骤引导式注册页面 (RegisterPage)
 */
class RegisterPage : public QWidget {
  Q_OBJECT

 public:
  explicit RegisterPage(RegisterViewModel* viewModel, QWidget* parent = nullptr);
  ~RegisterPage() override = default;

 signals:
  void loginRequested();           // 跳转回登录页面
  void serviceAgreementRequested(); // 服务协议
  void privacyPolicyRequested();    // 隐私政策

 public slots:
  void setAgreementVisible(bool visible);
  [[nodiscard]] bool isAgreementChecked() const;

 protected:
  void resizeEvent(QResizeEvent* event) override;

 private:
  void setupUi();
  void bindViewModel();
  void updateButtonStates();
  void renderState(const RegisterState& state);

  // Step 1: 选择注册方式 + 验证邮箱/手机号
  void setupStep1();
  QWidget* m_step1Widget = nullptr;
  fluent::navigation::Pivot* m_contactPivot = nullptr;
  // 动态联系方式输入 (根据 Pivot 选择切换邮箱/手机号)
  void applyContactMode();
  fluent::textfields::LineEdit* m_contactInput = nullptr;
  fluent::basicinput::Button* m_sendBtn = nullptr;
  fluent::textfields::LineEdit* m_codeInput = nullptr;
  fluent::basicinput::Button* m_nextBtn1 = nullptr;
  fluent::status_info::InfoBar* m_errorText1 = nullptr;

  // Step 2: 用户名 + 密码 + 协议签署
  void setupStep2();
  QWidget* m_step2Widget = nullptr;
  fluent::textfields::LineEdit* m_accountInput = nullptr;
  fluent::textfields::PasswordBox* m_passwordInput = nullptr;
  fluent::textfields::PasswordBox* m_confirmPasswordInput = nullptr;
  fluent::status_info::ProgressBar* m_strengthBar = nullptr;
  QWidget* m_agreementWidget = nullptr;
  fluent::basicinput::CheckBox* m_agreementCheckBox = nullptr;
  fluent::basicinput::HyperlinkButton* m_serviceAgreementText = nullptr;
  fluent::basicinput::HyperlinkButton* m_privacyPolicyText = nullptr;
  fluent::basicinput::Button* m_backBtn2 = nullptr;
  fluent::basicinput::Button* m_registerBtn = nullptr;
  fluent::status_info::InfoBar* m_errorText2 = nullptr;

  fluent::navigation::StackContentHost* m_mainStackedWidget = nullptr;
  CaptchaOverlay* m_captchaOverlay = nullptr;

  RegisterViewModel* m_viewModel = nullptr;
};
