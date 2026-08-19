#include "MainWindowViewModel.h"
#include "data/di/AppContainer.h"
#include <QPointer>

MainWindowViewModel::MainWindowViewModel(int uid, const QString& username, QObject* parent)
    : BaseViewModel<MainWindowViewModel, MainWindowState>(parent) {
    auto s = this->state();
    s.uid = uid;
    s.username = username;
    updateState(s);
}

void MainWindowViewModel::checkFaceEnrollment() {
    int currentUid = this->state().uid;
    if (currentUid <= 0) return;

    if (auto userRepo = AppContainer::userRepository()) {
        QPointer<MainWindowViewModel> weakThis(this);
        userRepo->hasUserFaceAsync(currentUid, [weakThis](bool hasFace) {
            if (!weakThis) return;
            QMetaObject::invokeMethod(weakThis.data(), [weakThis, hasFace]() {
                if (weakThis && !hasFace) {
                    auto s = weakThis->state();
                    s.showFaceEnrollPrompt = true;
                    weakThis->updateState(s);
                }
            }, Qt::QueuedConnection);
        });
    }
}

void MainWindowViewModel::clearFaceEnrollPrompt() {
    auto s = this->state();
    if (s.showFaceEnrollPrompt) {
        s.showFaceEnrollPrompt = false;
        updateState(s);
    }
}
