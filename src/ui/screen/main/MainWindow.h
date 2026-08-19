#pragma once

#include <QWidget>
#include <memory>

#include "ui/window/WindowBase.h"
#include <FluentQt/Navigation.h>
#include <FluentQt/Windowing.h>
#include "ui/window/NavigationWindow.h"

namespace fluent::basicinput { class Button; }
namespace fluent::navigation { class NavigationView; }
namespace fluent::textfields { class Label; }

struct MainWindowState;
class MainWindowViewModel;

/**
 * @brief 主界面窗体，基于 FluentQt windowing::Window + NavigationView
 *
 * 左侧自适应导航（Left/LeftCompact/LeftMinimal）+ StackContentHost 页面转场。
 */
class MainWindow : public NavigationWindow {
    Q_OBJECT

public:
    explicit MainWindow(int uid = -1, const QString& username = QString(),
                        bool promptFaceEnroll = false, QWidget* parent = nullptr);
    ~MainWindow() override = default;

    int uid() const;
    QString username() const;

protected:
    void showEvent(QShowEvent* event) override;

private:
    void initWindow();
    void buildNavigation();
    void promptFaceEnrollDialog();
    void renderState(const MainWindowState& state);

    std::shared_ptr<MainWindowViewModel> m_viewModel;
    bool m_initPromptRequested = false;
    QVector<QWidget*> m_pages;
};
