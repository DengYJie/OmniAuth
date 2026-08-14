#pragma once

#include <QString>

#include "ui/common/BaseViewModel.h"

enum class AppRoute { Login, FaceScan, Register, Recovery };

struct AuthState {
  AppRoute currentRoute = AppRoute::Login;
  QString titleBarText = QStringLiteral("");
  bool isBackButtonVisible = false;
};

class AuthViewModel : public BaseViewModel<AuthState> {
  Q_OBJECT

 public:
  explicit AuthViewModel(QObject* parent = nullptr);
  ~AuthViewModel() override = default;

  // Intents / Actions
  void navigateTo(AppRoute route);
  void goBack();

 signals:
  void stateChanged(const AuthState& state);

 protected:
  void emitStateChanged() override;
};
