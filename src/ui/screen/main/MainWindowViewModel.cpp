#include "MainWindowViewModel.h"
#include "data/di/AppContainer.h"
#include <QPointer>

MainWindowViewModel::MainWindowViewModel(int uid, const QString& username, QObject* parent)
    : BaseViewModel<MainWindowViewModel, MainWindowState>(parent) {
    auto state = currentState();
    state.uid = uid;
    state.username = username;
    updateState(state);
}

void MainWindowViewModel::checkFaceEnrollment() {
    int currentUid = currentState().uid;
    if (currentUid <= 0) return;

    if (auto userRepo = AppContainer::userRepository()) {
        QPointer<MainWindowViewModel> weakThis(this);
        userRepo->hasUserFaceAsync(currentUid, [weakThis](bool hasFace) {
            if (!weakThis) return;
            QMetaObject::invokeMethod(weakThis.data(), [weakThis, hasFace]() {
                if (weakThis && !hasFace) {
                    auto s = weakThis->currentState();
                    s.showFaceEnrollPrompt = true;
                    weakThis->updateState(s);
                }
            }, Qt::QueuedConnection);
        });
    }
}

void MainWindowViewModel::clearFaceEnrollPrompt() {
    auto state = currentState();
    if (state.showFaceEnrollPrompt) {
        state.showFaceEnrollPrompt = false;
        updateState(state);
    }
}
