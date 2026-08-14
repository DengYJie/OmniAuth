#pragma once

#include <FluentQt/Navigation.h>
#include <FluentQt/Windowing.h>
#include "ui/window/WindowBase.h"
namespace fluent::basicinput { class Button; }
namespace fluent::navigation { class StackContentHost; }
namespace fluent::textfields { class Label; }

class AuthViewModel;
class LoginViewModel;
class FaceScannerViewModel;
class RegisterViewModel;
class RecoveryViewModel;
class LoginPage;
class FaceScannerPage;
class RegisterPage;
class RecoveryPage;
struct AuthState;

/**
 * @brief 认证入口窗体，基于 FluentQt windowing::Window
 *
 * 由 FluentQt 统一提供无边框标题栏、Mica 背景与系统按钮；
 * 页面栈使用 navigation::StackContentHost（带转场）。
 */
class AuthWindow : public WindowBase {
    Q_OBJECT

public:
    explicit AuthWindow(QWidget* parent = nullptr);
    ~AuthWindow() override = default;

private:
    void setupUi();
    void bindViewModels();
    void renderAuthState(const AuthState& state);
    void updateTitleBar(const QString& title, bool backVisible);

    AuthViewModel* m_AuthViewModel = nullptr;
    LoginViewModel* m_loginViewModel = nullptr;
    FaceScannerViewModel* m_faceScannerViewModel = nullptr;
    RegisterViewModel* m_registerViewModel = nullptr;
    RecoveryViewModel* m_recoveryViewModel = nullptr;

    fluent::navigation::StackContentHost* m_stackHost = nullptr;
    LoginPage* m_loginPage = nullptr;
    FaceScannerPage* m_faceScannerPage = nullptr;
    RegisterPage* m_registerPage = nullptr;
    RecoveryPage* m_recoveryPage = nullptr;

    fluent::basicinput::Button* m_backButton = nullptr;
    fluent::textfields::Label* m_titleLabel = nullptr;
};
