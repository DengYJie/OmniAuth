#pragma once

#include <QString>

#include "ui/common/BaseViewModel.h"

namespace ui::navigation { class NavigationHistory; }

enum class AppRoute { Login, FaceScan, Register, Recovery };

struct AuthState {
  AppRoute currentRoute = AppRoute::Login;
  QString titleBarText = QStringLiteral("");
  bool isBackButtonVisible = false;

  bool operator==(const AuthState&) const = default;
};

class AuthViewModel : public BaseViewModel<AuthViewModel, AuthState> {
  Q_OBJECT

 public:
  explicit AuthViewModel(QObject* parent = nullptr);
  ~AuthViewModel() override = default;

  // Intents / Actions
  void navigateTo(AppRoute route);
  void goBack();
  void resetToLogin();

 signals:
  void stateChanged(const AuthState& state);
  void routePushed(AppRoute route);
  void routePopped();
  void poppedToRoot();

 protected:
  void emitStateChanged() override;

 private:
  ui::navigation::NavigationHistory* m_navHistory = nullptr;
  AppRoute stringToRoute(const QString& str) const;
  QString routeToString(AppRoute route) const;
  void updateStateForRoute(AppRoute route);
};
