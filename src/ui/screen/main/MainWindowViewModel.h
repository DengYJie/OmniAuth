#pragma once

#include <QString>
#include "ui/common/BaseViewModel.h"

struct MainWindowState {
    int uid = -1;
    QString username;
    bool showFaceEnrollPrompt = false;

    bool operator==(const MainWindowState& other) const {
        return uid == other.uid &&
               username == other.username &&
               showFaceEnrollPrompt == other.showFaceEnrollPrompt;
    }
};

class MainWindowViewModel : public BaseViewModel<MainWindowViewModel, MainWindowState> {
    Q_OBJECT
public:
    explicit MainWindowViewModel(int uid, const QString& username, QObject* parent = nullptr);

    // Intents
    void checkFaceEnrollment();
    void clearFaceEnrollPrompt();
};
