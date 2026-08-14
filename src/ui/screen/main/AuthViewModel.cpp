#include "AuthViewModel.h"

AuthViewModel::AuthViewModel(QObject* parent)
    : BaseViewModel<AuthState>(parent) {}

void AuthViewModel::emitStateChanged() { emit stateChanged(m_state); }

void AuthViewModel::navigateTo(AppRoute route) {
  updateState([&](AuthState& state) {
    state.currentRoute = route;

    if (route == AppRoute::FaceScan) {
      state.titleBarText = QStringLiteral("人脸登录");
      state.isBackButtonVisible = true;
    } else if (route == AppRoute::Register) {
      state.titleBarText = QStringLiteral("注册账号");
      state.isBackButtonVisible = true;
    } else if (route == AppRoute::Recovery) {
      state.titleBarText = QStringLiteral("找回密码");
      state.isBackButtonVisible = true;
    } else if (route == AppRoute::Login) {
      state.titleBarText = QStringLiteral("");
      state.isBackButtonVisible = false;
    }
  });
}

void AuthViewModel::goBack() {
  // 简单的路由回退逻辑：回到 Login
  if (m_state.currentRoute != AppRoute::Login) {
    navigateTo(AppRoute::Login);
  }
}
