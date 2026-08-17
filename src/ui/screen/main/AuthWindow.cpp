#include "AuthWindow.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

#include <FluentQt/BasicInput.h>
#include <FluentQt/Foundation.h>
#include <FluentQt/TextFields.h>

#include "ui/screen/facescan/FaceScannerPage.h"
#include "ui/screen/login/LoginPage.h"
#include "ui/screen/recovery/RecoveryPage.h"
#include "ui/screen/register/RegisterPage.h"
#include "ui/screen/facescan/FaceScannerViewModel.h"
#include "ui/screen/login/LoginViewModel.h"
#include "ui/screen/main/AuthViewModel.h"
#include "ui/screen/recovery/RecoveryViewModel.h"
#include "ui/screen/register/RegisterViewModel.h"
#include "MainWindow.h"
#include "ui/window/TitleBar.h"

namespace fluent_b = fluent::basicinput;
namespace fluent_tf = fluent::textfields;
namespace fluent_nav = fluent::navigation;

AuthWindow::AuthWindow(QWidget* parent) : WindowBase(parent) {
  m_AuthViewModel = new AuthViewModel(this);
  m_loginViewModel = new LoginViewModel(this);
  m_faceScannerViewModel = new FaceScannerViewModel(this);
  m_registerViewModel = new RegisterViewModel(this);
  m_recoveryViewModel = new RecoveryViewModel(this);

  setupUi();
  bindViewModels();

  renderAuthState(m_AuthViewModel->state());
}

void AuthWindow::setupUi() {
  setFixedSize(440, 560);
  setWindowTitle(QStringLiteral("OmniAuth"));
  setBackdropEffect(fluent::windowing::BackdropEffect::Mica);
  setCustomWindowChromeEnabled(true);

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
  m_stackHost = new fluent_nav::StackContentHost(this);
  m_stackHost->setObjectName(QStringLiteral("AuthStackHost"));
  
  m_loginPage = new LoginPage(m_loginViewModel, this);
  m_loginPage->setObjectName(QStringLiteral("AuthLoginPage"));
  
  m_faceScannerPage = new FaceScannerPage(m_faceScannerViewModel, this);
  m_faceScannerPage->setObjectName(QStringLiteral("AuthFacePage"));
  
  m_registerPage = new RegisterPage(m_registerViewModel, this);
  m_registerPage->setObjectName(QStringLiteral("AuthRegPage"));
  
  m_recoveryPage = new RecoveryPage(m_recoveryViewModel, this);
  m_recoveryPage->setObjectName(QStringLiteral("AuthRecPage"));

  m_stackHost->insertPage(0, m_loginPage);
  m_stackHost->insertPage(1, m_faceScannerPage);
  m_stackHost->insertPage(2, m_registerPage);
  m_stackHost->insertPage(3, m_recoveryPage);
  m_stackHost->setCurrentIndex(0, 0, false);

  setContentWidget(m_stackHost);
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

  connect(m_registerPage, &RegisterPage::loginRequested, this,
      [this]() { m_AuthViewModel->navigateTo(AppRoute::Login); });

  connect(m_recoveryViewModel, &RecoveryViewModel::backToLogin, this,
      [this]() { m_AuthViewModel->navigateTo(AppRoute::Login); });

  connect(titleBar(), &ui::window::TitleBar::backButtonClicked, this,
      [this]() { m_AuthViewModel->goBack(); });

  connect(m_AuthViewModel, &AuthViewModel::stateChanged, this,
      &AuthWindow::renderAuthState);

  connect(m_faceScannerViewModel, &FaceScannerViewModel::faceScanSuccess, this,
      [this](const QString& username) {
        qDebug() << "Face Auth Success for user:" << username;
        auto mainWindow = new MainWindow();
        mainWindow->show();
        this->close();
      });

  connect(m_loginViewModel, &LoginViewModel::loginSuccess, this,
      [this]() {
        qDebug() << "Password Auth Success";
        auto mainWindow = new MainWindow();
        mainWindow->show();
        this->close();
      });
}

void AuthWindow::renderAuthState(const AuthState& state) {
  updateTitleBar(state.titleBarText, state.isBackButtonVisible);

  if (state.currentRoute == AppRoute::Login) {
    m_faceScannerViewModel->stopScan();
    m_stackHost->setCurrentIndex(0);
  }
  else if (state.currentRoute == AppRoute::FaceScan) {
    m_stackHost->setCurrentIndex(1);
    m_faceScannerViewModel->startScan();
  }
  else if (state.currentRoute == AppRoute::Register) {
    m_faceScannerViewModel->stopScan();
    m_registerViewModel->resetToInitialState();
    m_stackHost->setCurrentIndex(2);
  }
  else if (state.currentRoute == AppRoute::Recovery) {
    m_faceScannerViewModel->stopScan();
    m_recoveryViewModel->resetToInitialState();
    m_stackHost->setCurrentIndex(3);
  }
}
