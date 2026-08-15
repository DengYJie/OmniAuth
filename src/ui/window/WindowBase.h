#pragma once

#include <QWidget>
#include <QVBoxLayout>

#include <FluentQt/Foundation.h>
#include <FluentQt/Design.h>
#include <FluentQt/Windowing.h>
#include <FluentQt/BasicInput.h>

#include "TitleBar.h"

#include <QWKCore/windowagentbase.h>
#include <QWKWidgets/widgetwindowagent.h>

/**
 * @brief 自定义窗口基类，基于 QWidget + QWindowKit 实现现代无边框和原生系统特效。
 */
class WindowBase : public QWidget, public fluent::FluentElement, public fluent::QMLPlus {
    Q_OBJECT
    Q_PROPERTY(QWidget* contentWidget READ contentWidget WRITE setContentWidget)
    Q_PROPERTY(fluent::windowing::BackdropEffect backdropEffect READ backdropEffect WRITE setBackdropEffect)

public:
    /**
     * @brief 构造函数。初始化基础布局。
     */
    explicit WindowBase(QWidget* parent = nullptr);
    ~WindowBase() override = default;

    /**
     * @brief 获取当前托管的主内容控件。
     */
    QWidget* contentWidget() const { return m_contentWidget; }

    /**
     * @brief 设置主内容控件。通常在构造窗口 UI 时调用。
     * @param widget 要嵌入的主界面控件，生命周期归 WindowBase 管理。
     */
    void setContentWidget(QWidget* widget);
    
    /**
     * @brief 获取标题栏控件。用于自定义添加额外标题栏动作或组件。
     */
    ui::window::TitleBar* titleBar() const { return m_titleBar; }

    /**
     * @brief 获取内容区容器控件。
     */
    QWidget* contentHost() const { return m_contentHost; }

    /**
     * @brief 设置窗口背景效果（Mica / Acrylic / Solid）。
     * @note 建议在窗口初始化或主题切换时调用。
     * @param effect 目标背景效果类型。
     */
    void setBackdropEffect(fluent::windowing::BackdropEffect effect);

    /**
     * @brief 获取当前生效的背景效果。
     */
    fluent::windowing::BackdropEffect backdropEffect() const { return m_backdropEffect; }

    /**
     * @brief 启用或禁用 Fluent 自绘标题栏。
     * @note 请在窗口调用 show() 之前设置。若设置为 false，则窗口保留原生系统边框。
     * @param enabled 是否开启自绘沉浸式标题栏。
     */
    void setCustomWindowChromeEnabled(bool enabled);

    /**
     * @brief 获取是否启用了自绘标题栏。
     */
    bool customWindowChromeEnabled() const { return m_customWindowChromeEnabled; }

    /**
     * @brief 主题更新回调。在系统或应用切换深浅色模式时自动触发。
     */
    void onThemeUpdated() override;

protected:
    /**
     * @brief 窗口绘制事件。用于处理背景擦除与底色填充。
     */
    void paintEvent(QPaintEvent* event) override;

    /**
     * @brief 窗口显示事件。确保在首次显示前完成 QWindowKit 最终初始化。
     */
    void showEvent(QShowEvent* event) override;

    /**
     * @brief 状态改变事件。用于窗口最大化/还原时切换图标与圆角。
     */
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void setupWindowKit();
    void applyPlatformWindowAttributes();

    bool event(QEvent* event) override;

    QVBoxLayout* m_rootLayout = nullptr;
    ui::window::TitleBar* m_titleBar = nullptr;
    QWidget* m_contentHost = nullptr;
    QWidget* m_contentWidget = nullptr;

    fluent::windowing::BackdropEffect m_backdropEffect = fluent::windowing::BackdropEffect::Solid;
    bool m_customWindowChromeEnabled = true;

    QWK::WidgetWindowAgent* m_windowAgent = nullptr;
};
