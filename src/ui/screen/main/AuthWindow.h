#pragma once

#include <FluentQt/Navigation.h>
#include <FluentQt/Windowing.h>
#include <QMap>
#include "ui/window/WindowBase.h"

namespace fluent::basicinput { class Button; }
namespace fluent::collections { class StackView; }
namespace fluent::textfields { class Label; }
namespace fluent::status_info { class InfoBar; }

class AuthViewModel;
class LoginViewModel;
class FaceScannerViewModel;
class RegisterViewModel;
class RecoveryViewModel;
class LoginPage;
class FaceScannerPage;
class RegisterPage;
class RecoveryPage;
enum class AppRoute;
struct AuthState;

/**
 * @brief 认证入口窗体，基于 FluentQt windowing::Window
 *
 * 由 FluentQt 统一提供无边框标题栏、Mica 背景与系统按钮；
 * 页面栈使用 collections::StackView（带转场）。
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

    QWidget* getOrCreatePage(AppRoute route);
    void handleRoutePushed(AppRoute route);
    void handleRoutePopped();
    void handlePoppedToRoot();

    AuthViewModel* m_AuthViewModel = nullptr;
    LoginViewModel* m_loginViewModel = nullptr;
    FaceScannerViewModel* m_faceScannerViewModel = nullptr;
    RegisterViewModel* m_registerViewModel = nullptr;
    RecoveryViewModel* m_recoveryViewModel = nullptr;

    fluent::collections::StackView* m_stackHost = nullptr;
    LoginPage* m_loginPage = nullptr;
    QMap<AppRoute, QWidget*> m_residentPages;

    fluent::status_info::InfoBar* m_globalInfoBar = nullptr;
    void showSuccessToast(const QString& msg);
};
