#include "ui/navigation/NavigationPanel.h"

#include <QDebug>
#include <QDynamicPropertyChangeEvent>
#include <QEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>

#include <FluentQt/Design.h>
#include <FluentQt/DialogsFlyouts.h>
#include <FluentQt/Navigation.h>
#include <FluentQt/Windowing.h>

#include "ui/navigation/NavigationIndicator.h"
#include "ui/navigation/NavigationMetrics.h"
#include "ui/navigation/NavigationPushButton.h"
#include "ui/navigation/NavigationSectionHeader.h"
#include "ui/navigation/NavigationTreeItem.h"
#include "ui/navigation/NavigationTreeWidget.h"
#include "ui/navigation/NavigationToolButton.h"
#include "ui/navigation/NavigationWidget.h"
#include "ui/navigation/NavigationFlyoutPopup.h"

namespace ui::navigation {

NavigationPanel::NavigationPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void NavigationPanel::setupUi()
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setAutoFillBackground(false);

    const auto s = themeSpacing();
    m_layout = new QBoxLayout(QBoxLayout::TopToBottom, this);
    m_layout->setContentsMargins(0, 0, 0, s.small);
    m_layout->setSpacing(0);

    m_headerLayout = new QBoxLayout(QBoxLayout::TopToBottom);
    m_headerLayout->setContentsMargins(0, s.xSmall, 0, 0);
    m_headerLayout->setSpacing(0);

    m_backButton = new NavigationToolButton(Typography::Icons::Back, this);
    m_backButton->setAccessibleItemName(QStringLiteral("Back"));
    m_backButton->setVisible(false);
    connect(m_backButton, &NavigationPushButton::clicked, this, [this]() {
        if (isVisible()) {
            if (auto* navView = qobject_cast<fluent::navigation::NavigationView*>(parentWidget())) {
                using DisplayMode = fluent::navigation::NavigationView::DisplayMode;
                const auto mode = navView->effectiveDisplayMode();
                if (navView->isPaneOpen()
                    && (mode == DisplayMode::LeftCompact || mode == DisplayMode::LeftMinimal)) {
                    navView->setPaneOpen(false);
                    return;
                }
            }
        }
        emit backRequested();
    });
    m_headerLayout->addWidget(m_backButton, 0, Qt::AlignLeft);

    m_menuButton = new NavigationToolButton(Typography::Icons::GlobalNav, this);
    m_menuButton->setAccessibleItemName(QStringLiteral("Toggle navigation pane"));
    connect(m_menuButton, &NavigationPushButton::clicked, this, &NavigationPanel::togglePaneRequested);
    m_headerLayout->addWidget(m_menuButton, 0, Qt::AlignLeft);
    m_layout->addLayout(m_headerLayout);

    m_tree = new NavigationTreeWidget(this);
    m_layout->addWidget(m_tree, 1);

    m_indicator = new NavigationIndicator(this);
    m_indicator->hide();
    m_tree->setIndicator(m_indicator);

    connect(m_tree, &NavigationTreeWidget::itemSelected, this,
            &NavigationPanel::itemSelected);
    connect(m_tree, &NavigationTreeWidget::categoryActivated, this,
            [this](const QString& categoryKey, QWidget* anchorWidget) {
        showFlyoutMenu(categoryKey, anchorWidget);
    });
    connect(m_tree, &NavigationTreeWidget::categoryDeactivated, this,
            [this](const QString& /*categoryKey*/) {
        closeFlyoutMenu(true);
    });
    connect(m_tree, &NavigationTreeWidget::overflowMenuRequested, this,
            [this](QWidget* anchorWidget,
                   const QVector<NavigationOverflowEntry>& entries) {
        showOverflowMenu(anchorWidget, entries);
    });

    m_userCardContainer = new QWidget(this);
    m_userCardLayout = new QBoxLayout(QBoxLayout::TopToBottom, m_userCardContainer);
    m_userCardLayout->setContentsMargins(0, 0, 0, 0);
    m_userCardLayout->setSpacing(0);
    m_userCardContainer->hide();
    m_layout->addWidget(m_userCardContainer);
}

void NavigationPanel::addItem(const QString& routeKey, const QString& iconGlyph,
                              const QString& text, const QString& parentKey,
                              NavigationItemPosition position, bool selectable)
{
    m_tree->addItem(routeKey, iconGlyph, text, parentKey, position, selectable);
}

void NavigationPanel::addSectionHeader(const QString& text)
{
    m_tree->addSectionHeader(text);
}

void NavigationPanel::addWidget(NavigationWidget* widget, NavigationItemPosition position)
{
    m_tree->addWidget(widget, position);
}

void NavigationPanel::setCurrentItem(const QString& routeKey, bool animated)
{
    m_tree->setCurrentItem(routeKey, animated);
}

void NavigationPanel::setCategoryExpanded(const QString& routeKey, bool expanded, bool animated)
{
    if (m_tree)
        m_tree->setCategoryExpanded(routeKey, expanded, animated);
}

QString NavigationPanel::currentRouteKey() const
{
    return m_tree ? m_tree->currentRouteKey() : QString();
}

void NavigationPanel::setUserInfoCard(NavigationWidget* cardWidget)
{
    if (!cardWidget || !m_userCardLayout)
        return;
    m_userCardLayout->addWidget(cardWidget);
    m_userCardContainer->show();
    updateGeometry();
}

bool NavigationPanel::isBackButtonVisible() const
{
    return m_backButton && m_backButton->isVisible();
}

void NavigationPanel::setBackButtonVisible(bool visible)
{
    if (m_backButton && m_backButton->isVisible() != visible) {
        m_backButton->setVisible(visible);
        updateGeometry();
        emit backButtonVisibleChanged(visible);
    }
}

bool NavigationPanel::isBackEnabled() const
{
    return m_backButton && m_backButton->isEnabled();
}

void NavigationPanel::setBackEnabled(bool enabled)
{
    if (m_backButton && m_backButton->isEnabled() != enabled) {
        m_backButton->setEnabled(enabled);
        emit backEnabledChanged(enabled);
    }
}

bool NavigationPanel::isMenuButtonVisible() const
{
    return m_menuButton && m_menuButton->isVisible();
}

void NavigationPanel::setMenuButtonVisible(bool visible)
{
    if (m_menuButton && m_menuButton->isVisible() != visible) {
        m_menuButton->setVisible(visible);
        updateGeometry();
        emit menuButtonVisibleChanged(visible);
    }
}

void NavigationPanel::setCompacted(bool compacted, bool [[maybe_unused]] /*animated*/)
{
    if (m_isCompacted == compacted)
        return;
    m_isCompacted = compacted;
    if (!compacted)
        closeFlyoutMenu(true);
    if (m_tree)
        m_tree->setCompacted(compacted);
    if (m_menuButton)
        m_menuButton->setCompacted(compacted);
    if (m_backButton)
        m_backButton->setCompacted(compacted);

    emit displayModeChanged(m_isCompacted);
    emit compactedChanged(m_isCompacted);
}

void NavigationPanel::togglePane()
{
    emit togglePaneRequested();
}

void NavigationPanel::setExpandProgress(float progress)
{
    m_expandProgress = qBound(0.0f, progress, 1.0f);
    if (m_tree)
        m_tree->setExpandProgress(m_expandProgress);
}

void NavigationPanel::moveFocusBy(int delta)
{
    if (m_tree)
        m_tree->moveFocusBy(delta);
}

QSize NavigationPanel::sizeHint() const
{
    if (m_position == NavigationPosition::Top) {
        int w = parentWidget() ? parentWidget()->width() : QWidget::sizeHint().width();
        return QSize(w, kTopBarItemHeight);
    }
    return QSize(Breakpoints::NavigationPaneExpandedWidth, QWidget::sizeHint().height());
}

void NavigationPanel::paintEvent(QPaintEvent* event)
{
    // 浮层抽屉模式透出下层毛玻璃，不填充背景
    if (!m_surfaceVisible) {
        const QColor fill = fluent::windowing::windowChromeBackdropFill(
            *this, window(), window() && window()->isActiveWindow());
        if (fill.isValid()) {
            QPainter painter(this);
            painter.fillRect(rect(), fill);
        }
    }
    QWidget::paintEvent(event);
}

void NavigationPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_indicator)
        m_indicator->raise();
    // Top 模式下 panel 需跟随窗口宽度，但自身未必收到 resizeEvent，故监听顶层窗口
    if (QWidget* top = window()) {
        top->installEventFilter(this);
    }
}

void NavigationPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    
    if (auto* navView = qobject_cast<fluent::navigation::NavigationView*>(parentWidget())) {
        const int expandedW = navView->expandedPaneWidth();
        const int compactW = navView->compactPaneWidth();
        if (expandedW > compactW) {
            const float progress = qBound(0.0f, float(width() - compactW) / float(expandedW - compactW), 1.0f);
            setExpandProgress(progress);
        }
    }

    // 延迟一帧，确保深层布局树完成重排后再刷新指示条坐标
    if (m_tree && !event->oldSize().isEmpty()) {
        const bool layoutSnappedInstantly = qAbs(event->size().width() - event->oldSize().width()) > 50;
        QTimer::singleShot(0, m_tree, [tree = m_tree, layoutSnappedInstantly]() {
            tree->refreshIndicator(/*animated=*/false, /*forceSnap=*/layoutSnappedInstantly);
        });
    }
}

void NavigationPanel::setNavigationPosition(NavigationPosition position)
{
    if (m_position == position)
        return;
    m_position = position;

    const auto direction = (position == NavigationPosition::Top)
        ? QBoxLayout::LeftToRight
        : QBoxLayout::TopToBottom;

    m_layout->setDirection(direction);
    m_headerLayout->setDirection(direction);
    m_userCardLayout->setDirection(direction);

    const auto s = themeSpacing();
    if (position == NavigationPosition::Top) {
        m_menuButton->hide();
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        if (window()) {
            setFixedWidth(window()->width());
        } else if (parentWidget()) {
            setFixedWidth(parentWidget()->width());
        }
        // Top 模式：外层布局边距清零，使导航项占满顶栏 48px 高度
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_headerLayout->setContentsMargins(s.small, 0, 0, 0);
        // userCard 靠右：在横向布局末尾添加伸缩占位
        m_layout->setStretch(2, 0);
    } else {
        m_menuButton->show();
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        m_layout->setContentsMargins(0, 0, 0, s.small);
        m_headerLayout->setContentsMargins(0, s.xSmall, 0, 0);
    }

    if (m_tree) {
        m_tree->setNavigationPosition(position);
    }
    if (m_indicator) {
        m_indicator->setNavigationPosition(position);
    }
    
    updateGeometry();
}

bool NavigationPanel::event(QEvent* event)
{
    if (event->type() == QEvent::WindowDeactivate) {
        closeFlyoutMenu(true);
    }
    if (event->type() == QEvent::DynamicPropertyChange) {
        if (auto* change = static_cast<QDynamicPropertyChangeEvent*>(event);
            change && change->propertyName() == "fluentNavPaneFloating") {
            setSurfaceVisible(property("fluentNavPaneFloating").toBool());
        }
    }
    return QWidget::event(event);
}

// Top 模式下窗口缩放时跟随其宽度
bool NavigationPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == window() && event->type() == QEvent::Resize) {
        if (m_position == NavigationPosition::Top) {
            setFixedWidth(static_cast<QResizeEvent*>(event)->size().width());
        }
    }
    return QWidget::eventFilter(watched, event);
}

void NavigationPanel::setSurfaceVisible(bool visible)
{
    if (m_surfaceVisible == visible)
        return;
    m_surfaceVisible = visible;
    setAttribute(Qt::WA_NoSystemBackground, visible);
    setAttribute(Qt::WA_TranslucentBackground, visible);
    update();
}

void NavigationPanel::triggerCrossWindowPortal(NavigationFlyoutPopup* flyout, QWidget* anchorWidget)
{
    // Portal 动效仅在 Top 模式存在，Left 模式无 overflow/跨窗口指示条，直接跳过
    if (m_position != NavigationPosition::Top || !m_indicator || !flyout || !flyout->selectedItem())
        return;

    NavigationTreeItem* owner = m_tree->indicatorOwner();
    const QRectF topBarRect = m_tree->indicatorRectInHost(
        owner ? owner : qobject_cast<NavigationWidget*>(anchorWidget));
    m_tree->releaseIndicatorOwner();

    // flyout 已定位，用 mapToGlobal 直接取真实全局几何（含翻转/clamp 后的最终位置）
    const QRectF flyoutRect = flyout->selectedItem()->indicatorRect();
    const QPointF globalTargetTopLeft = flyout->selectedItem()->mapToGlobal(flyoutRect.topLeft().toPoint());
    const QRectF globalTargetRect(globalTargetTopLeft, flyoutRect.size());

    const QPointF globalStartTopLeft = m_tree->mapToGlobal(topBarRect.topLeft().toPoint());
    const QRectF globalStartRect(globalStartTopLeft, topBarRect.size());

    // 顶栏指示条：从顶栏原位置向左收缩（Top 宿主收缩分支）
    const QRectF topHostStartRect = topBarRect;
    const QRectF topHostTargetRect(m_tree->mapFromGlobal(globalTargetRect.topLeft().toPoint()), globalTargetRect.size());

    // flyout 指示条：从目标位置顶部向下生长（Left 宿主生长分支）
    const QRectF flyoutHostStartRect(flyout->mapFromGlobal(globalStartRect.topLeft().toPoint()), globalStartRect.size());
    const QRectF flyoutHostTargetRect(flyout->mapFromGlobal(globalTargetRect.topLeft().toPoint()), globalTargetRect.size());

    m_indicator->playCrossWindowPortal(topHostStartRect, topHostTargetRect, themeAnimation().normal);
    flyout->playSelectedItemCrossPortal(flyout->selectedItem(), flyoutHostStartRect, flyoutHostTargetRect);
}

void NavigationPanel::showFlyoutMenu(const QString& categoryKey, QWidget* anchorWidget)
{
    if (!m_tree || !anchorWidget)
        return;

    const QRect anchorRect(anchorWidget->mapTo(this, QPoint(0, 0)), anchorWidget->size());

    // 无动画立即关闭旧的，避免旧关闭动画与新 flyout 重叠
    closeFlyoutMenu(false);

    // 复用 Popup 会残留旧尺寸/布局状态，每次新建干净实例
    auto* flyout = new NavigationFlyoutPopup(m_tree, this);

    // 克隆该分类的子树（含递归子分类），保留展开/选中态；flyout 内分类可内联折叠
    flyout->rebuildSubtree(categoryKey);

    m_compactFlyout = flyout;

    // flyout 内点击 selectable 分类项主体：切页（延迟 overflow）后触发跨窗口 Portal 动效
    connect(flyout, &NavigationFlyoutPopup::selectableCategoryClicked, this,
            [this, flyout, anchorWidget](NavigationTreeItem* /*item*/) {
        triggerCrossWindowPortal(flyout, anchorWidget);
    });

    // flyout 关闭（无论何种方式）时，通知树清空激活态并还原指示条归属。
    connect(flyout, &NavigationFlyoutPopup::closed, this,
            [this, flyout, categoryKey]() {
        if (m_tree) {
            m_tree->dismissCategory();

            const QString currentKey = m_tree->currentRouteKey();
            const bool isPortalActive = (m_position == NavigationPosition::Top && m_indicator && flyout->selectedItem() && m_tree->isAncestorOf(currentKey, categoryKey));

            if (isPortalActive) {
                m_tree->restoreIndicatorOwner(); // 先恢复以便获取正确的 targetRect
                if (NavigationTreeItem* owner = m_tree->indicatorOwner()) {
                    owner->setShowIndicator(false); // 暂时隐藏原生，交由飞行件播放 PortalReturn
                    const QRectF targetRect = m_tree->indicatorRectInHost(owner);
                    m_indicator->playPortalReturn(targetRect, themeAnimation().normal);
                }
            } else {
                m_tree->restoreIndicatorOwner();
            }

            // flyout 内点击 selectable 分类项时延迟了 overflow 重算，此处 flyout 关闭后补齐重排
            m_tree->updateOverflow();
        }
    });

    // 锚点为 panel 局部坐标，统一用 panel 的 mapToGlobal 换算为屏幕坐标（卡片左上角）
    const int cardW = flyout->width() - 2 * kShadowMargin;
    const int cardH = flyout->height() - 2 * kShadowMargin;

    // Top 模式下：若选中项属于当前分类子树，待 flyout 完全定位后触发跨窗口联合传送门。
    // 必须在 opened 后触发：flyout 定位/布局完成前 selectedItem 几何不准，且可能翻转（m_flippedUp）。
    if (m_position == NavigationPosition::Top && m_indicator && flyout->selectedItem()
        && m_tree->isAncestorOf(m_tree->currentRouteKey(), categoryKey)) {
        connect(flyout, &NavigationFlyoutPopup::opened, this,
            [this, flyout, anchorWidget]() {
                triggerCrossWindowPortal(flyout, anchorWidget);
            }, Qt::SingleShotConnection);
    }

    if (m_position == NavigationPosition::Top) {
        // Top 模式：flyout 顶部紧贴 panel 下外边缘，X 对齐锚点水平中心，向下滑入
        const int panelBottomY = mapToGlobal(QPoint(0, height())).y();
        const int anchorCenterX = mapToGlobal(QPoint(anchorRect.center().x(), 0)).x();
        const int flyoutCenterX = anchorCenterX - cardW / 2;

        flyout->setAnchorWidget(anchorWidget);
        flyout->addLightDismissPassthrough(this); // 允许侧边栏内的点击直接穿透（实现无缝切换）
        flyout->openAt(QPoint(flyoutCenterX, panelBottomY), QPoint(0, 16));
    } else {
        // Left 紧凑模式：flyout 左边紧贴 panel 右外边缘，Y 垂直居中于锚点
        // openAt 自管阴影 margin，定位直接用可见卡片高居中，无需 margin 补偿
        const int panelRightX = mapToGlobal(QPoint(width(), 0)).x();
        const int anchorCenterY = mapToGlobal(anchorRect.topLeft()).y() + anchorRect.height() / 2;
        const int yPos = anchorCenterY - cardH / 2;

        flyout->setAnchorWidget(anchorWidget);
        flyout->addLightDismissPassthrough(this); // 允许侧边栏内的点击直接穿透
        flyout->openAt(QPoint(panelRightX, yPos), QPoint(kCompactFlyoutSlideOffset, 0));
    }
}

void NavigationPanel::closeFlyoutMenu(bool animated)
{
    if (!m_compactFlyout)
        return;
    auto* popup = m_compactFlyout.data();
    m_compactFlyout = nullptr;

    if (animated && popup->isVisible()) {
        connect(popup, &NavigationFlyoutPopup::closed,
                popup, &QObject::deleteLater);
        popup->close();
    } else {
        // 立即销毁，避免旧关闭动画与新 flyout 重叠
        popup->hide();
        popup->deleteLater();
    }
}

void NavigationPanel::showOverflowMenu(QWidget* anchorWidget,
                                       const QVector<NavigationOverflowEntry>& entries)
{
    if (!anchorWidget || entries.isEmpty())
        return;

    const QRect anchorRect(anchorWidget->mapTo(this, QPoint(0, 0)), anchorWidget->size());

    closeOverflowMenu(false);

    auto* flyout = new NavigationFlyoutPopup(m_tree, this);

    // 克隆溢出条目子树（header 与节点按序渲染，分类可内联折叠）
    flyout->rebuildSubtreeFromEntries(entries);

    m_overflowFlyout = flyout;

    // 与 showFlyoutMenu 相同的屏幕坐标换算：卡片左上角由 panel 局部坐标 mapToGlobal 得到
    const int panelBottomY = mapToGlobal(QPoint(0, height())).y();
    const int anchorCenterX = mapToGlobal(QPoint(anchorRect.center().x(), 0)).x();
    const int cardW = flyout->width() - 2 * kShadowMargin;
    const int flyoutCenterX = anchorCenterX - cardW / 2;

    flyout->setAnchorWidget(anchorWidget);
    flyout->addLightDismissPassthrough(this);

    // flyout 内点击 selectable 分类项主体：切页（延迟 overflow）后触发跨窗口 Portal 动效
    connect(flyout, &NavigationFlyoutPopup::selectableCategoryClicked, this,
            [this, flyout, anchorWidget](NavigationTreeItem* /*item*/) {
        triggerCrossWindowPortal(flyout, anchorWidget);
    });

    // overflow flyout 关闭后补齐延迟的 overflow 重算（flyout 内点击 selectable 分类项时跳过）
    connect(flyout, &NavigationFlyoutPopup::closed, this,
            [this]() {
        if (m_tree)
            m_tree->updateOverflow();
    });

    // Top 模式下：若选中项在溢出列表中，待 flyout 完全定位后触发跨窗口联合传送门。
    if (m_position == NavigationPosition::Top && m_indicator && flyout->selectedItem()) {
        connect(flyout, &NavigationFlyoutPopup::opened, this,
            [this, flyout, anchorWidget]() {
                triggerCrossWindowPortal(flyout, anchorWidget);
            }, Qt::SingleShotConnection);
    }

    flyout->openAt(QPoint(flyoutCenterX, panelBottomY), QPoint(0, 16));
}

void NavigationPanel::closeOverflowMenu(bool animated)
{
    if (!m_overflowFlyout)
        return;
    auto* popup = m_overflowFlyout.data();
    m_overflowFlyout = nullptr;

    const bool isPortalActive = (m_position == NavigationPosition::Top && m_indicator && popup->selectedItem());

    if (animated && popup->isVisible()) {
        connect(popup, &NavigationFlyoutPopup::closed,
                popup, &QObject::deleteLater);
        popup->close();
    } else {
        popup->hide();
        popup->deleteLater();
    }

    // 还原指示条归属，若处于 Portal 状态则播放 PortalReturn
    if (m_tree) {
        if (isPortalActive) {
            m_tree->restoreIndicatorOwner();
            if (NavigationTreeItem* owner = m_tree->indicatorOwner()) {
                owner->setShowIndicator(false); // 暂时隐藏原生，交由飞行件播放 PortalReturn
                const QRectF targetRect = m_tree->indicatorRectInHost(owner);
                m_indicator->playPortalReturn(targetRect, themeAnimation().normal);
            }
        } else {
            m_tree->restoreIndicatorOwner();
        }
    }
}

} // namespace ui::navigation
