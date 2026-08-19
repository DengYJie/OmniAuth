#include "AuthWindow.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

#include <FluentQt/BasicInput.h>
#include <FluentQt/Collections.h>
#include <FluentQt/Foundation.h>
#include <FluentQt/TextFields.h>

#include "MainWindow.h"
#include <QTimer>
#include <FluentQt/StatusInfo.h>
#include "ui/screen/facescan/FaceScannerPage.h"
#include "ui/screen/facescan/FaceScannerViewModel.h"
#include "ui/screen/login/LoginPage.h"
#include "ui/screen/login/LoginViewModel.h"
#include "ui/screen/main/AuthViewModel.h"
#include "ui/screen/recovery/RecoveryPage.h"
#include "ui/screen/recovery/RecoveryViewModel.h"
#include "ui/screen/register/RegisterPage.h"
#include "ui/screen/register/RegisterViewModel.h"
#include "ui/window/TitleBar.h"

namespace fluent_b = fluent::basicinput;
namespace fluent_tf = fluent::textfields;
namespace fluent_col = fluent::collections;

AuthWindow::AuthWindow(QWidget* parent) : WindowBase(parent) {
    m_AuthViewModel = new AuthViewModel(this);
    m_loginViewModel = new LoginViewModel(this);
    m_faceScannerViewModel = new FaceScannerViewModel(this);
    m_registerViewModel = new RegisterViewModel(this);
    m_recoveryViewModel = new RecoveryViewModel(this);

    setupUi();
    bindViewModels();

    // 触发初始状态
    m_AuthViewModel->resetToLogin();
}

void AuthWindow::setupUi() {
    setFixedSize(480, 620);
    setCustomWindowChromeEnabled(true);
    titleBar()->setSystemButtonVisible(ui::window::TitleBar::SystemButtonType::Maximize, false);

#if defined(QT_DEBUG) || !defined(NDEBUG)
    auto* themeBtn = new fluent_b::Button(this);
    themeBtn->setFluentLayout(fluent_b::Button::IconOnly);
    themeBtn->setFluentStyle(fluent_b::Button::Subtle);
    themeBtn->setIconGlyph(Typography::Icons::glyph(QStringLiteral("ic_fluent_weather_moon_20_regular")));
    themeBtn->setFixedSize(32, 32);
    themeBtn->setToolTip(QStringLiteral("切换主题"));
    connect(themeBtn, &fluent_b::Button::clicked, this, [themeBtn]() {
        const bool nextDark =
            FluentElement::currentTheme() == FluentElement::Light;
        FluentElement::setThemeDeferred(
            nextDark ? FluentElement::Dark : FluentElement::Light);
        themeBtn->setIconGlyph(nextDark
            ? Typography::Icons::Sunny
            : Typography::Icons::glyph(QStringLiteral("ic_fluent_weather_moon_20_regular")));
        });
    titleBar()->setRightHeaderWidget(themeBtn);
#endif

    // ── 页面栈 ──
    m_stackHost = new fluent_col::StackView(this);
    m_stackHost->setObjectName(QStringLiteral("AuthStackHost"));
    m_stackHost->setDefaultItemOwnership(fluent::WidgetOwnership::Borrowed);

    m_loginPage = new LoginPage(m_loginViewModel, this);
    m_loginPage->setObjectName(QStringLiteral("AuthLoginPage"));
    m_residentPages.insert(AppRoute::Login, m_loginPage);

    m_stackHost->setInitialItem(m_loginPage, fluent::WidgetOwnership::Borrowed);

    setContentWidget(m_stackHost);

    m_globalInfoBar = new fluent::status_info::InfoBar(this);
    m_globalInfoBar->setSeverity(fluent::status_info::InfoBar::Success);
    m_globalInfoBar->setIsClosable(true);
    m_globalInfoBar->hide();
}

void AuthWindow::showSuccessToast(const QString& msg) {
    if (!m_globalInfoBar) return;
    m_globalInfoBar->setMessage(msg);
    m_globalInfoBar->setSeverity(fluent::status_info::InfoBar::Success);
    m_globalInfoBar->setFixedWidth(width() - 48);
    m_globalInfoBar->move(24, 48); 
    m_globalInfoBar->raise();
    m_globalInfoBar->show();
    QTimer::singleShot(3000, m_globalInfoBar, &QWidget::hide);
}

void AuthWindow::updateTitleBar(const QString& title, bool backVisible) {
    titleBar()->setTitle(title);
    titleBar()->setBackButtonVisible(backVisible);
}

void AuthWindow::bindViewModels() {
    connect(m_loginPage, &LoginPage::faceLoginRequested, this,
        [this]() { m_AuthViewModel->navigateTo(AppRoute::FaceScan); });
    connect(m_loginPage, &LoginPage::registerRequested, this,
        [this]() { m_AuthViewModel->navigateTo(AppRoute::Register); });
    connect(m_loginPage, &LoginPage::forgotPasswordRequested, this,
        [this]() { m_AuthViewModel->navigateTo(AppRoute::Recovery); });

    connect(m_recoveryViewModel, &RecoveryViewModel::backToLogin, this,
        [this]() { m_AuthViewModel->goBack(); });

    // 注册和重置成功后回到 Login 并弹出提示
    connect(m_registerViewModel, &RegisterViewModel::registerSuccess, this,
        [this]() { 
            m_AuthViewModel->resetToLogin();
            showSuccessToast(QStringLiteral("注册成功，请使用新账号登录！"));
        });
        
    connect(m_recoveryViewModel, &RecoveryViewModel::resetSuccess, this,
        [this]() { 
            m_AuthViewModel->resetToLogin();
            showSuccessToast(QStringLiteral("密码重置成功，请使用新密码登录！"));
        });

    // 联动 TitleBar backButton 点击
    connect(titleBar(), &ui::window::TitleBar::backButtonClicked, this,
        [this]() {
            m_AuthViewModel->goBack();
        });

    // 联动 StackView 忙碌状态到 TitleBar backButton 的可用性
    connect(m_stackHost, &fluent_col::StackView::busyChanged, this,
        [this](bool busy) {
            titleBar()->setBackButtonEnabled(!busy);
        });

    m_AuthViewModel->observe(this, &AuthWindow::renderAuthState);

    connect(m_AuthViewModel, &AuthViewModel::routePushed, this,
        &AuthWindow::handleRoutePushed);
    connect(m_AuthViewModel, &AuthViewModel::routePopped, this,
        &AuthWindow::handleRoutePopped);
    connect(m_AuthViewModel, &AuthViewModel::poppedToRoot, this,
        &AuthWindow::handlePoppedToRoot);

    connect(m_faceScannerViewModel, &FaceScannerViewModel::faceScanSuccess, this,
        [this](int uid, const QString& username) {
            auto mainWindow = new MainWindow(uid, username, /*promptFaceEnroll=*/false);
            mainWindow->setAttribute(Qt::WA_DeleteOnClose);
            mainWindow->show();
            this->close();
        });

    connect(m_loginViewModel, &LoginViewModel::loginSuccess, this,
        [this](int uid, const QString& username) {
            auto mainWindow = new MainWindow(uid, username, /*promptFaceEnroll=*/true);
            mainWindow->setAttribute(Qt::WA_DeleteOnClose);
            mainWindow->show();
            this->close();
        });
}

QWidget* AuthWindow::getOrCreatePage(AppRoute route) {
    if (m_residentPages.contains(route)) {
        return m_residentPages[route];
    }
    
    QWidget* page = nullptr;
    if (route == AppRoute::FaceScan) {
        auto* faceScannerPage = new FaceScannerPage(m_faceScannerViewModel);
        faceScannerPage->setObjectName(QStringLiteral("AuthFacePage"));
        page = faceScannerPage;
    } else if (route == AppRoute::Register) {
        auto* registerPage = new RegisterPage(m_registerViewModel);
        registerPage->setObjectName(QStringLiteral("AuthRegPage"));
        connect(registerPage, &RegisterPage::loginRequested, this,
            [this]() { m_AuthViewModel->goBack(); });
        page = registerPage;
    } else if (route == AppRoute::Recovery) {
        auto* recoveryPage = new RecoveryPage(m_recoveryViewModel);
        recoveryPage->setObjectName(QStringLiteral("AuthRecPage"));
        page = recoveryPage;
    }
    
    if (page) {
        m_residentPages.insert(route, page);
    }
    return page;
}

void AuthWindow::handleRoutePushed(AppRoute route) {
    QWidget* page = getOrCreatePage(route);
    if (page) {
        // 当从缓存中复用页面时，重置 ViewModel 状态
        if (route == AppRoute::Register) {
            m_registerViewModel->resetToInitialState();
        } else if (route == AppRoute::Recovery) {
            m_recoveryViewModel->resetToInitialState();
        }
        
        // 使用 Borrowed，确保 pop 时页面不会被销毁
        m_stackHost->push(page, fluent::WidgetOwnership::Borrowed);
    }
}

void AuthWindow::handleRoutePopped() {
    m_stackHost->pop();
}

void AuthWindow::handlePoppedToRoot() {
    m_stackHost->popToRoot();
}

void AuthWindow::renderAuthState(const AuthState& state) {
    updateTitleBar(state.titleBarText, state.isBackButtonVisible);

    if (state.currentRoute == AppRoute::Login) {
        m_faceScannerViewModel->stopScan();
    }
    else if (state.currentRoute == AppRoute::FaceScan) {
        m_faceScannerViewModel->startScan();
    }
    else if (state.currentRoute == AppRoute::Register) {
        m_faceScannerViewModel->stopScan();
    }
    else if (state.currentRoute == AppRoute::Recovery) {
        m_faceScannerViewModel->stopScan();
    }
}
