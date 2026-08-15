#include "ui/window/NavigationWindow.h"
#include <FluentQt/Navigation.h>

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

    m_panel = new ui::navigation::NavigationPanel(m_navigationView);
    m_navigationView->setMainChromeWidget(m_panel);
    m_panel->setNavigationView(m_navigationView);

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

