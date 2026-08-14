#pragma once

#include "WindowBase.h"
#include "ui/navigation/NavigationMetrics.h"
#include <QHash>
#include <QString>

class QTimer;

namespace ui::navigation {
class NavigationPanel;
class NavigationIndicator;
class NavigationWidget;
}

namespace fluent::navigation {
class NavigationView;
}


class NavigationWindow : public WindowBase {
    Q_OBJECT

public:
    explicit NavigationWindow(QWidget* parent = nullptr);
    ~NavigationWindow() override;

    // Adds a navigation section header
    void addSectionHeader(const QString& text);

    // Adds a custom navigation widget
    void addWidget(ui::navigation::NavigationWidget* widget,
                   ui::navigation::NavigationItemPosition position =
                       ui::navigation::NavigationItemPosition::Top);

    // Adds a sub-interface (page) and its navigation item
    void addSubInterface(
        const QString& routeKey,
        QWidget* interfaceWidget,
        const QString& iconGlyph,
        const QString& text,
        const QString& parentRouteKey = QString(),
        ui::navigation::NavigationItemPosition pos = ui::navigation::NavigationItemPosition::Top,
        bool selectable = true
    );

    // Switches the current page
    void switchTo(const QString& routeKey);

    // Sets the user info card (avatar) at the bottom
    void setUserInfoCard(QWidget* cardWidget);

    ui::navigation::NavigationPanel* panel() const { return m_panel; }
    fluent::navigation::NavigationView* navigationView() const { return m_navigationView; }

protected:
    void initNavigation();
    /// 根据 paneOpen 决定面板展开/折叠，展开时若宽度动画未完成则延迟。
    void applyNavigationPaneDensity();
    /// 下发 compact 状态到面板。
    void setNavigationPanesCompact(bool compact, bool animated);

protected:
    ui::navigation::NavigationPanel* m_panel = nullptr;
    fluent::navigation::NavigationView* m_navigationView = nullptr;

    // Maps routeKey to page index in StackContentHost
    QHash<QString, int> m_routeToIndexMap;
    // currentIndexChanged 只携带 index，需反查 routeKey，故维护反向表避免 O(n) 遍历
    QHash<int, QString> m_indexToRouteMap;
};
