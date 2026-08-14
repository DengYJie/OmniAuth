#include "ui/window/NavigationWindow.h"
#include <FluentQt/Navigation.h>
#include <QTimer>

#include "ui/navigation/NavigationIndicator.h"
#include "ui/navigation/NavigationPanel.h"
#include "ui/navigation/NavigationWidget.h"

NavigationWindow::NavigationWindow(QWidget* parent)
    : WindowBase(parent)
{
    initNavigation();
}

NavigationWindow::~NavigationWindow() = default;

void NavigationWindow::initNavigation() {
    m_navigationView = new fluent::navigation::NavigationView(this);
    m_navigationView->setDisplayMode(fluent::navigation::NavigationView::DisplayMode::Auto);

    // 单一导航面板：同时承载 main + footer（+ 顶部汉堡折叠入口）。
    m_panel = new ui::navigation::NavigationPanel(m_navigationView);
    m_navigationView->setMainChromeWidget(m_panel);


    connect(m_navigationView, &fluent::navigation::NavigationView::paneOpenChanged,
        this, [this](bool) { applyNavigationPaneDensity(); });

    // 汉堡 → setPaneOpen。
    connect(m_panel, &ui::navigation::NavigationPanel::togglePaneRequested,
        this, [this]() {
            if (m_navigationView) {
                m_navigationView->setPaneOpen(!m_navigationView->isPaneOpen());
            }
        });

    connect(m_navigationView, &fluent::navigation::NavigationView::effectiveDisplayModeChanged,
        this, [this](fluent::navigation::NavigationView::DisplayMode mode) {
            if (!m_panel || !m_navigationView) return;
            
            m_panel->setNavigationPosition(
                mode == fluent::navigation::NavigationView::DisplayMode::Top
                    ? ui::navigation::NavigationPosition::Top
                    : ui::navigation::NavigationPosition::Left);

            m_navigationView->setPaneOpen(
                mode == fluent::navigation::NavigationView::DisplayMode::Left
                || mode == fluent::navigation::NavigationView::DisplayMode::Top);
            applyNavigationPaneDensity();
        });

    connect(m_panel, &ui::navigation::NavigationPanel::itemSelected, this, [this](const QString& routeKey) {
        switchTo(routeKey);
        });

    connect(m_navigationView->contentHost(), &fluent::navigation::StackContentHost::currentIndexChanged,
        this, [this](int index) {
            const QString routeKey = m_indexToRouteMap.value(index);
            if (!routeKey.isEmpty() && m_panel)
                m_panel->setCurrentItem(routeKey, true);
        });

    const auto initialMode = m_navigationView->effectiveDisplayMode();
    const bool initialPaneOpen = (initialMode == fluent::navigation::NavigationView::DisplayMode::Left
                                  || initialMode == fluent::navigation::NavigationView::DisplayMode::Top);
    
    m_panel->setNavigationPosition(
        initialMode == fluent::navigation::NavigationView::DisplayMode::Top
            ? ui::navigation::NavigationPosition::Top
            : ui::navigation::NavigationPosition::Left);

    m_navigationView->setPaneOpen(initialPaneOpen);
    setNavigationPanesCompact(!initialPaneOpen, false);

    setContentWidget(m_navigationView);
}

void NavigationWindow::setNavigationPanesCompact(bool compact, bool animated) {
    if (m_panel) {
        m_panel->setCompacted(compact, animated);
    }
}

void NavigationWindow::applyNavigationPaneDensity() {
    if (!m_navigationView) return;

    // 我们不再需要 hacky 的延时定时器，因为现在 NavigationPanel 的 compact 进度
    // 是在其 resizeEvent 中严格通过物理宽度计算的，自然能与布局转场完美同步。
    setNavigationPanesCompact(!m_navigationView->isPaneOpen(), m_navigationView->isAnimationEnabled());
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

