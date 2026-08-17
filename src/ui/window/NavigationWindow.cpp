#include "ui/window/NavigationWindow.h"
#include <FluentQt/Navigation.h>

#include "ui/navigation/NavigationIndicator.h"
#include "ui/navigation/NavigationPanel.h"
#include "ui/navigation/NavigationToolButton.h"
#include "ui/navigation/NavigationWidget.h"
#include "ui/window/TitleBar.h"

NavigationWindow::NavigationWindow(QWidget* parent)
    : WindowBase(parent)
{
    initNavigation();
}

NavigationWindow::~NavigationWindow() = default;

void NavigationWindow::initNavigation() {
    m_navigationView = new fluent::navigation::NavigationView(this);
    m_navigationView->setDisplayMode(fluent::navigation::NavigationView::DisplayMode::Auto);

    m_panel = new ui::navigation::NavigationPanel(m_navigationView);
    m_navigationView->setMainChromeWidget(m_panel);

    // 隐藏 NavigationPanel 自带的按钮，由 TitleBar 接管
    m_panel->setPaneToggleButtonVisible(false);
    m_panel->setBackButtonVisible(false);

    // 同步 DisplayMode 变化至 Panel 的排列方向、NavigationView 的 Pane 展开状态及 TitleBar
    auto syncDisplayMode = [this](fluent::navigation::NavigationView::DisplayMode mode) {
        using DisplayMode = fluent::navigation::NavigationView::DisplayMode;
        if (m_panel) {
            m_panel->setOrientation(mode == DisplayMode::Top
                ? ui::navigation::Orientation::Horizontal
                : ui::navigation::Orientation::Vertical);
        }
        if (m_navigationView) {
            m_navigationView->setPaneOpen(mode == DisplayMode::Left || mode == DisplayMode::Top);
        }
        if (titleBar()) {
            titleBar()->setPaneToggleButtonVisible(mode != DisplayMode::Top);
        }
        };

    connect(m_navigationView, &fluent::navigation::NavigationView::effectiveDisplayModeChanged,
        this, syncDisplayMode);

    // 同步 Pane 展开/收起状态至 Panel 的紧凑（折叠）模式
    connect(m_navigationView, &fluent::navigation::NavigationView::paneOpenChanged,
        this, [this](bool open) {
            if (m_panel) {
                m_panel->setCompacted(!open);
            }
        });

    // Panel 的返回按钮逻辑：若在 Minimal/Compact 模式下抽屉展开，则返回先收起抽屉
    connect(m_panel, &ui::navigation::NavigationPanel::backRequested, this, [this]() {
        if (m_navigationView) {
            using DisplayMode = fluent::navigation::NavigationView::DisplayMode;
            const auto mode = m_navigationView->effectiveDisplayMode();
            if (m_navigationView->isPaneOpen()
                && (mode == DisplayMode::LeftCompact || mode == DisplayMode::LeftMinimal)) {
                m_navigationView->setPaneOpen(false);
            }
        }
        });

    // Panel 自带的汉堡菜单按钮点击联动
    if (m_panel->paneToggleButton()) {
        connect(m_panel->paneToggleButton(), &ui::navigation::NavigationToolButton::clicked, this, [this]() {
            if (m_navigationView) {
                m_navigationView->setPaneOpen(!m_navigationView->isPaneOpen());
            }
            });
    }

    // 启用 TitleBar 中的导航控件
    if (titleBar()) {
        titleBar()->setBackButtonVisible(false);

        connect(titleBar(), &ui::window::TitleBar::paneToggleButtonClicked, this, [this]() {
            if (m_navigationView) {
                m_navigationView->setPaneOpen(!m_navigationView->isPaneOpen());
            }
            });
    }

    // 初始状态同步
    syncDisplayMode(m_navigationView->effectiveDisplayMode());
    if (m_panel) {
        m_panel->setCompacted(!m_navigationView->isPaneOpen());
    }

    connect(m_panel, &ui::navigation::NavigationPanel::itemSelected, this, [this](const QString& routeKey) {
        switchTo(routeKey);
        });

    connect(m_navigationView->contentHost(), &fluent::navigation::StackContentHost::currentIndexChanged,
        this, [this](int index) {
            const QString routeKey = m_indexToRouteMap.value(index);
            if (!routeKey.isEmpty() && m_panel)
                m_panel->setCurrentItem(routeKey);
        });

    setContentWidget(m_navigationView);
}

void NavigationWindow::addSectionHeader(const QString& text) {
    if (m_panel) {
        m_panel->addSectionHeader(text);
    }
}

void NavigationWindow::addWidget(ui::navigation::NavigationWidget* widget,
    ui::navigation::NavigationItemPosition position) {
    if (m_panel) {
        m_panel->addWidget(widget, position);
    }
}

void NavigationWindow::addSubInterface(
    const QString& routeKey,
    QWidget* interfaceWidget,
    const QString& iconGlyph,
    const QString& text,
    const QString& parentRouteKey,
    ui::navigation::NavigationItemPosition pos,
    bool selectable)
{
    if (!m_navigationView || !m_panel) return;

    if (interfaceWidget) {
        auto* contentHost = m_navigationView->contentHost();
        const int index = contentHost->count();
        contentHost->insertPage(index, interfaceWidget);
        m_routeToIndexMap.insert(routeKey, index);
        m_indexToRouteMap.insert(index, routeKey);
    }

    m_panel->addItem(routeKey, iconGlyph, text, parentRouteKey, pos,
        selectable && (interfaceWidget != nullptr));
}

void NavigationWindow::switchTo(const QString& routeKey) {
    if (!m_routeToIndexMap.contains(routeKey) || !m_navigationView) return;

    auto* contentHost = m_navigationView->contentHost();
    const int targetIndex = m_routeToIndexMap.value(routeKey);
    const int currentIndex = contentHost->currentIndex();
    const int direction = targetIndex >= currentIndex ? 1 : -1;

    contentHost->setCurrentIndex(targetIndex, direction, true);
}

void NavigationWindow::setUserInfoCard(QWidget* cardWidget) {
    if (!cardWidget || !m_panel) return;

    if (auto* navWidget = qobject_cast<ui::navigation::NavigationWidget*>(cardWidget)) {
        m_panel->setUserInfoCard(navWidget);
    }
}

