#pragma once

#include <QWidget>

#include "ui/window/WindowBase.h"
#include <FluentQt/Navigation.h>
#include <FluentQt/Windowing.h>
#include "ui/window/NavigationWindow.h"

namespace fluent::basicinput { class Button; }
namespace fluent::navigation { class NavigationView; }
namespace fluent::textfields { class Label; }

/**
 * @brief 主界面窗体，基于 FluentQt windowing::Window + NavigationView
 *
 * 左侧自适应导航（Left/LeftCompact/LeftMinimal）+ StackContentHost 页面转场。
 */
class MainWindow : public NavigationWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private:
    void initWindow();
    void buildNavigation();

    QVector<QWidget*> m_pages;
};
