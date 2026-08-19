#include "AuthViewModel.h"
#include "ui/navigation/NavigationHistory.h"

AuthViewModel::AuthViewModel(QObject* parent)
    : BaseViewModel<AuthViewModel, AuthState>(parent) {
    m_navHistory = new ui::navigation::NavigationHistory(this);

    // 将逻辑历史的后退操作与状态更新绑定
    connect(m_navHistory, &ui::navigation::NavigationHistory::navigatedBack, this, [this](const QString& routeKey) {
        updateStateForRoute(stringToRoute(routeKey));
        emit routePopped();
        });

    // 绑定逻辑历史的后退能力到标题栏返回按钮的可见性
    connect(m_navHistory, &ui::navigation::NavigationHistory::canGoBackChanged, this, [this](bool canGoBack) {
        updateState([&](AuthState& state) {
            state.isBackButtonVisible = canGoBack;
            });
        });

    // 初始化状态
    m_navHistory->push(routeToString(AppRoute::Login));
    updateStateForRoute(AppRoute::Login);
}

void AuthViewModel::emitStateChanged() { emit stateChanged(m_state); }

QString AuthViewModel::routeToString(AppRoute route) const {
    switch (route) {
    case AppRoute::Login: return QStringLiteral("Login");
    case AppRoute::FaceScan: return QStringLiteral("FaceScan");
    case AppRoute::Register: return QStringLiteral("Register");
    case AppRoute::Recovery: return QStringLiteral("Recovery");
    }
    return QStringLiteral("Login");
}

AppRoute AuthViewModel::stringToRoute(const QString& str) const {
    if (str == QStringLiteral("FaceScan")) return AppRoute::FaceScan;
    if (str == QStringLiteral("Register")) return AppRoute::Register;
    if (str == QStringLiteral("Recovery")) return AppRoute::Recovery;
    return AppRoute::Login;
}

void AuthViewModel::updateStateForRoute(AppRoute route) {
    updateState([&](AuthState& state) {
        state.currentRoute = route;
        if (route == AppRoute::FaceScan) {
            state.titleBarText = QStringLiteral("人脸登录");
        }
        else if (route == AppRoute::Register) {
            state.titleBarText = QStringLiteral("注册账号");
        }
        else if (route == AppRoute::Recovery) {
            state.titleBarText = QStringLiteral("找回密码");
        }
        else if (route == AppRoute::Login) {
            state.titleBarText = QStringLiteral("");
        }
        });
}

void AuthViewModel::navigateTo(AppRoute route) {
    if (m_state.currentRoute == route) return;

    m_navHistory->push(routeToString(route));
    updateStateForRoute(route);
    emit routePushed(route);
}

void AuthViewModel::goBack() {
    if (m_navHistory->canGoBack()) {
        m_navHistory->goBack();
    }
    else {
        resetToLogin();
    }
}

void AuthViewModel::resetToLogin() {
    if (m_state.currentRoute == AppRoute::Login) return;

    m_navHistory->clear();
    updateStateForRoute(AppRoute::Login);
    emit poppedToRoot();
}
