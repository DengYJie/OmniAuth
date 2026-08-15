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

#include "ui/navigation/NavigationFlyout.h"
#include "ui/navigation/NavigationIndicator.h"
#include "ui/navigation/NavigationMetrics.h"
#include "ui/navigation/NavigationPushButton.h"
#include "ui/navigation/NavigationSectionHeader.h"
#include "ui/navigation/NavigationToolButton.h"
#include "ui/navigation/NavigationTreeItem.h"
#include "ui/navigation/NavigationTreeWidget.h"
#include "ui/navigation/NavigationWidget.h"

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
                if (m_navigationView) {
                    using DisplayMode = fluent::navigation::NavigationView::DisplayMode;
                    const auto mode = m_navigationView->effectiveDisplayMode();
                    if (m_navigationView->isPaneOpen()
                        && (mode == DisplayMode::LeftCompact || mode == DisplayMode::LeftMinimal)) {
                        m_navigationView->setPaneOpen(false);
                        return;
                    }
                }
            }
            emit backRequested();
            });
        m_headerLayout->addWidget(m_backButton, 0, Qt::AlignLeft);

        m_menuButton = new NavigationToolButton(Typography::Icons::GlobalNav, this);
        m_menuButton->setAccessibleItemName(QStringLiteral("Toggle navigation pane"));
        connect(m_menuButton, &NavigationPushButton::clicked, this, &NavigationPanel::togglePane);
        m_headerLayout->addWidget(m_menuButton, 0, Qt::AlignLeft);
        m_layout->addLayout(m_headerLayout);

        m_tree = new NavigationTreeWidget(this);
        m_layout->addWidget(m_tree, 1);

        m_indicator = new NavigationIndicator(this);
        m_indicator->hide();

        connect(this, &NavigationPanel::indicatorOwnerChanged, m_tree, &NavigationTreeWidget::onIndicatorOwnerChanged);

        connect(m_tree, &NavigationTreeWidgetBase::expansionChanged, this, [this](const QString& routeKey, bool /*expanded*/) {
            if (m_indicatorOwner && m_tree->isAncestorOf(m_indicatorOwner->routeKey(), routeKey)) {
                refreshIndicatorVisuals(true);
            }
            });

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
        NavigationItemPosition position, bool selectable, const QString& tooltip)
    {
        if (!m_tree)
            return;
        m_tree->addItem(routeKey, iconGlyph, text, parentKey, position, selectable, tooltip);
    }

    void NavigationPanel::addSectionHeader(const QString& text)
    {
        m_tree->addSectionHeader(text);
    }

    void NavigationPanel::addWidget(NavigationWidget* widget, NavigationItemPosition position)
    {
        m_tree->addWidget(widget, position);
    }

    void NavigationPanel::setCurrentItem(const QString& routeKey)
    {
        NavigationTreeItem* prevOwner = m_indicatorOwner;
        m_tree->setCurrentItem(routeKey, false);
        NavigationTreeWidget* node = m_tree->nodeFor(routeKey);
        NavigationTreeItem* curOwner = nullptr;
        if (node && node->itemWidget()) {
            curOwner = qobject_cast<NavigationTreeItem*>(node->itemWidget());
        }
        m_indicatorOwner = curOwner;
        dispatchIndicatorAnimation(prevOwner, curOwner);
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

    void NavigationPanel::setCompacted(bool compacted)
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

        refreshIndicatorVisuals();

        emit compactedChanged(m_isCompacted);
    }

    void NavigationPanel::setNavigationView(fluent::navigation::NavigationView* view)
    {
        if (m_navigationView == view)
            return;
        m_navigationView = view;
        if (!view)
            return;

        connect(view, &fluent::navigation::NavigationView::effectiveDisplayModeChanged,
            this, [this](fluent::navigation::NavigationView::DisplayMode mode) {
                applyDisplayMode(static_cast<int>(mode));
            });
        connect(view, &fluent::navigation::NavigationView::paneOpenChanged,
            this, [this](bool) { applyPaneDensity(); });

        // 注入时立即按当前显示模式同步一次，供初始布局对齐
        applyDisplayMode(static_cast<int>(view->effectiveDisplayMode()));
    }

    void NavigationPanel::applyDisplayMode(int mode)
    {
        using DisplayMode = fluent::navigation::NavigationView::DisplayMode;
        const auto dm = static_cast<DisplayMode>(mode);
        setOrientation(dm == DisplayMode::Top ? Orientation::Horizontal : Orientation::Vertical);
        m_navigationView->setPaneOpen(dm == DisplayMode::Left || dm == DisplayMode::Top);
    }

    void NavigationPanel::applyPaneDensity()
    {
        if (!m_navigationView)
            return;
        setCompacted(!m_navigationView->isPaneOpen());
    }

    void NavigationPanel::togglePane()
    {
        if (m_navigationView)
            m_navigationView->setPaneOpen(!m_navigationView->isPaneOpen());
    }

    void NavigationPanel::setExpandProgress(float progress)
    {
        const float oldP = m_expandProgress;
        m_expandProgress = qBound(0.0f, progress, 1.0f);
        if (m_tree)
            m_tree->setExpandProgress(m_expandProgress);

        // 完全展开 (>= 0.999) 或完全收起 (<= 0.001) 时，直接重算 visual proxy 无动画更新指示条
        if ((m_expandProgress <= 0.001f && oldP > 0.001f) ||
            (m_expandProgress >= 0.999f && oldP < 0.999f)) {
            refreshIndicatorVisuals();
        }
    }

    void NavigationPanel::refreshIndicatorVisuals(bool animated, NavigationTreeItem* prevLogicalOwner)
    {
        if (!m_tree || !m_indicator || !m_indicatorOwner)
            return;

        NavigationTreeItem* prevVisual = m_visualIndicatorOwner.data();
        NavigationTreeItem* curVisual = m_tree->getVisualProxyFor(m_indicatorOwner);

        // 1. 同步物理起点：仅当存在真实逻辑切换，且物理指示条需要滑行时
        if (animated && prevLogicalOwner && prevLogicalOwner != m_indicatorOwner) {
            NavigationTreeItem* prevProxy = m_visualIndicatorOwner ? m_visualIndicatorOwner.data() : m_tree->getVisualProxyFor(prevLogicalOwner);
            if (prevProxy) {
                const QRectF prevRect = m_tree->indicatorRectInHost(prevProxy, this);
                m_indicator->setInitialPosition(prevRect);
            }
        }

        // 2. 瞬时维护替身指针
        if (prevVisual && prevVisual != curVisual) {
            prevVisual->setShowIndicator(false);
        }
        m_visualIndicatorOwner = curVisual;

        if (curVisual) {
            const QRectF targetRect = m_tree->indicatorRectInHost(curVisual, this);

            if (!animated) {
                m_indicator->activateAt(targetRect, false);
                curVisual->setShowIndicator(true);
                // 无动画时立即通知就绪
                if (prevLogicalOwner && prevLogicalOwner != m_indicatorOwner) {
                    emit indicatorOwnerChanged(prevLogicalOwner, false);
                }
                emit indicatorOwnerChanged(m_indicatorOwner, true);
            }
            else {
                // 起飞信号：熄灭旧逻辑节点
                if (prevLogicalOwner && prevLogicalOwner != m_indicatorOwner) {
                    connect(m_indicator, &NavigationIndicator::flightStarted, this, [this, prevLogicalOwner]() {
                        emit indicatorOwnerChanged(prevLogicalOwner, false);
                        }, Qt::SingleShotConnection);
                }

                // 降落信号：点亮新替身，并广播新逻辑节点
                connect(m_indicator, &NavigationIndicator::flightFinished, this, [this, curVisual, prevLogicalOwner]() {
                    if (m_visualIndicatorOwner == curVisual) {
                        curVisual->setShowIndicator(true);
                    }
                    if (prevLogicalOwner) { // 仅当由导航事件触发时，才发射 indicatorOwnerChanged
                        emit indicatorOwnerChanged(m_indicatorOwner, true);
                    }
                    }, Qt::SingleShotConnection);

                m_indicator->activateAt(targetRect, true);
            }
        }
        else {
            m_indicator->hideIndicator();
            if (prevLogicalOwner && prevLogicalOwner != m_indicatorOwner) {
                emit indicatorOwnerChanged(prevLogicalOwner, false);
            }
        }
    }

    void NavigationPanel::moveFocusBy(int delta)
    {
        if (m_tree)
            m_tree->moveFocusBy(delta);
    }

    QSize NavigationPanel::sizeHint() const
    {
        if (m_orientation == Orientation::Horizontal) {
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
        // Horizontal 模式下 panel 需跟随窗口宽度，但自身未必收到 resizeEvent，故监听顶层窗口
        if (QWidget* top = window()) {
            top->installEventFilter(this);
        }

        // 首次展示防御回退：若当前完全没有任何选中的项，自动选中树中第一个可用的项（静默秒开无动画）
        if (!m_indicatorOwner && m_tree) {
            if (NavigationTreeItem* firstSelectable = m_tree->findFirstSelectableItem()) {
                setCurrentItem(firstSelectable->routeKey());
            }
        }
        else if (m_indicatorOwner) {
            refreshIndicatorVisuals(false);
        }
    }

    void NavigationPanel::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);

        if (m_navigationView) {
            const int expandedW = m_navigationView->expandedPaneWidth();
            const int compactW = m_navigationView->compactPaneWidth();
            if (expandedW > compactW) {
                const float progress = qBound(0.0f, float(width() - compactW) / float(expandedW - compactW), 1.0f);
                setExpandProgress(progress);
            }
        }

    }

    void NavigationPanel::setOrientation(Orientation orientation)
    {
        if (m_orientation == orientation)
            return;
        m_orientation = orientation;

        const auto direction = (orientation == Orientation::Horizontal)
            ? QBoxLayout::LeftToRight
            : QBoxLayout::TopToBottom;

        m_layout->setDirection(direction);
        m_headerLayout->setDirection(direction);
        m_userCardLayout->setDirection(direction);

        const auto s = themeSpacing();
        if (orientation == Orientation::Horizontal) {
            m_menuButton->hide();
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            if (window()) {
                setFixedWidth(window()->width());
            }
            else if (parentWidget()) {
                setFixedWidth(parentWidget()->width());
            }
            // Horizontal 模式：外层布局边距清零，使导航项占满顶栏 48px 高度
            m_layout->setContentsMargins(0, 0, 0, 0);
            m_headerLayout->setContentsMargins(s.small, 0, 0, 0);
            // userCard 靠右：在横向布局末尾添加伸缩占位
            m_layout->setStretch(2, 0);
        }
        else {
            m_menuButton->show();
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
            m_layout->setContentsMargins(0, 0, 0, s.small);
            m_headerLayout->setContentsMargins(0, s.xSmall, 0, 0);
        }

        if (m_tree) {
            m_tree->setOrientation(orientation);
        }
        if (m_indicator) {
            m_indicator->setOrientation(orientation);
        }

        refreshIndicatorVisuals(false);
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

    // Horizontal 模式下窗口缩放时跟随其宽度
    bool NavigationPanel::eventFilter(QObject* watched, QEvent* event)
    {
        if (watched == window() && event->type() == QEvent::Resize) {
            if (m_orientation == Orientation::Horizontal) {
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

    void NavigationPanel::triggerCrossWindowPortal(NavigationFlyout* flyout, QWidget* anchorWidget, NavigationTreeItem* prevOwner, NavigationTreeItem* curClone)
    {
        if (m_orientation != Orientation::Horizontal || !m_indicator || !flyout || !curClone)
            return;

        m_flyoutCloseIntent = FlyoutCloseIntent::PortalReturn;

        NavigationTreeItem* owner = m_indicatorOwner;
        NavigationTreeItem* visualOwner = m_tree->getVisualProxyFor(prevOwner ? prevOwner : owner);

        // 顶栏起飞矩形：取指示条当前的真实视觉位置而非选中项几何，保证从任意项起飞都从原位开始
        QRectF topHostStartRect;
        if (m_indicator && m_indicator->currentRect().isValid() && m_indicator->currentRect().width() > 0) {
            topHostStartRect = m_indicator->currentRect();
        }
        else {
            NavigationWidget* topItem = visualOwner ? visualOwner : qobject_cast<NavigationWidget*>(anchorWidget);
            if (!topItem && owner) topItem = owner;
            topHostStartRect = m_tree->indicatorRectInHost(topItem, this);
        }

        // flyout 已定位，用 mapToGlobal 直接取真实全局几何（含翻转/clamp 后的最终位置）
        const QRectF flyoutRect = curClone->indicatorRect();
        const QPointF globalTargetTopLeft = curClone->mapToGlobal(flyoutRect.topLeft().toPoint());
        const QRectF globalTargetRect(globalTargetTopLeft, flyoutRect.size());

        const QPointF globalStartTopLeft = this->mapToGlobal(topHostStartRect.topLeft().toPoint());
        const QRectF globalStartRect(globalStartTopLeft, topHostStartRect.size());

        // 顶栏指示条：从顶栏当前原位置向左收缩（Horizontal 宿主收缩分支）
        const QRectF topHostTargetRect(this->mapFromGlobal(globalTargetRect.topLeft().toPoint()), globalTargetRect.size());

        // flyout 指示条：从目标位置顶部向下生长（Vertical 宿主生长分支）
        const QRectF flyoutHostStartRect(flyout->mapFromGlobal(globalStartRect.topLeft().toPoint()), globalStartRect.size());
        const QRectF itemInFlyoutHost = flyout->indicatorRectInHost(curClone);

        // 顶栏熄灭逻辑：顶栏指示条起飞时，熄灭原节点的常驻指示条
        connect(m_indicator, &NavigationIndicator::flightStarted, this, [this, prevOwner, owner]() {
            NavigationTreeItem* itemToExtinguish = prevOwner ? prevOwner : owner;
            if (itemToExtinguish) emit indicatorOwnerChanged(itemToExtinguish, false);
            if (m_visualIndicatorOwner) {
                m_visualIndicatorOwner->setShowIndicator(false);
                m_visualIndicatorOwner = nullptr;
            }
            }, Qt::SingleShotConnection);

        m_indicator->playCrossWindowPortal(topHostStartRect, topHostTargetRect);

        // 浮层点亮逻辑：浮层指示条降落后，点亮最终节点的常驻指示条
        if (NavigationIndicator* flyoutIndicator = flyout->indicator()) {
            connect(flyoutIndicator, &NavigationIndicator::flightFinished, this, [this, flyout, curClone]() {
                if (m_indicatorOwner) emit indicatorOwnerChanged(m_indicatorOwner, true);
                }, Qt::SingleShotConnection);
        }

        // 使用 indicatorRectInHost 作为真正的降落点，以支持折叠父节点的代理回退
        flyout->playSelectedItemCrossPortal(curClone, flyoutHostStartRect, itemInFlyoutHost);
    }

    void NavigationPanel::showFlyoutMenu(const QString& categoryKey, QWidget* anchorWidget)
    {
        if (!m_tree || !anchorWidget)
            return;

        const QRect anchorRect(anchorWidget->mapTo(this, QPoint(0, 0)), anchorWidget->size());

        // 无动画立即关闭旧的，避免旧关闭动画与新 flyout 重叠
        closeFlyoutMenu(false);

        // 复用 Popup 会残留旧尺寸/布局状态，每次新建干净实例
        auto* flyout = new NavigationFlyout(m_tree, this);

        // 克隆该分类的子树（含递归子分类），保留展开/选中态；flyout 内分类可内联折叠
        flyout->rebuildSubtree(categoryKey);

        m_compactFlyout = flyout;

        connect(this, &NavigationPanel::indicatorOwnerChanged, flyout, &NavigationFlyout::onIndicatorOwnerChanged);

        // flyout 内展开状态改变：如果改变的是当前选中项的祖先，重新派发内部滑动动画
        connect(flyout, &NavigationFlyout::expansionChanged, this,
            [this](const QString& routeKey, bool /*expanded*/) {
                if (m_indicatorOwner && m_tree->isAncestorOf(m_indicatorOwner->routeKey(), routeKey)) {
                    dispatchIndicatorAnimation(m_indicatorOwner, m_indicatorOwner);
                }
            });

        // flyout 关闭（无论何种方式）时统一收尾：重排 + 按意图播放动画
        // 使用 aboutToHide 消除 150ms 滞后感，使主面板指示条与浮层淡出同时归位
        connect(flyout, &NavigationFlyout::aboutToHide, this,
            [this, categoryKey, anchorWidget]() {
                animateFlyoutClosed(categoryKey, anchorWidget);
            });

        // Horizontal 模式下：若选中项属于当前分类子树，待 flyout 完全定位后触发跨窗口联合传送门
        // 须在 opened 后触发：flyout 定位/布局完成前 selectedItem 几何不准，且可能翻转（m_flippedUp）
        if (m_orientation == Orientation::Horizontal && m_indicator && m_tree->isAncestorOf(m_tree->currentRouteKey(), categoryKey)) {
            connect(flyout, &NavigationFlyout::opened, this,
                [this, flyout, anchorWidget]() {
                    if (NavigationTreeItem* curClone = flyout->cloneItemFor(m_tree->currentRouteKey())) {
                        triggerCrossWindowPortal(flyout, anchorWidget, m_indicatorOwner, curClone);
                    }
                }, Qt::SingleShotConnection);
        }

        flyout->addLightDismissPassthrough(this); // 允许侧边栏内的点击直接穿透（实现无缝切换）

        if (m_orientation == Orientation::Horizontal) {
            // Horizontal 模式：flyout 顶部紧贴 panel 下外边缘，X 对齐锚点水平中心，向下滑入
            flyout->setPlacement(NavigationFlyout::Placement::Bottom);
            flyout->showAt(anchorWidget);
        }
        else {
            // Vertical 紧凑模式：flyout 左边紧贴 panel 右外边缘，Y 垂直居中于锚点
            flyout->setPlacement(NavigationFlyout::Placement::Right);
            flyout->showAt(anchorWidget);
        }
    }

    void NavigationPanel::closeFlyoutMenu(bool animated)
    {
        if (!m_compactFlyout)
            return;
        auto* popup = m_compactFlyout.data();
        m_compactFlyout = nullptr;

        if (animated && popup->isVisible()) {
            connect(popup, &NavigationFlyout::closed,
                popup, &QObject::deleteLater);
            popup->close();
        }
        else {
            // 无动画时也必须走 close() 流程以保证 aboutToHide -> closed 生命周期闭环
            popup->setExitAnimationEnabled(false);
            connect(popup, &NavigationFlyout::closed,
                popup, &QObject::deleteLater);
            popup->close();
        }
    }

    void NavigationPanel::showOverflowMenu(QWidget* anchorWidget,
        const QVector<NavigationOverflowEntry>& entries)
    {
        if (!anchorWidget || entries.isEmpty())
            return;

        const QRect anchorRect(anchorWidget->mapTo(this, QPoint(0, 0)), anchorWidget->size());

        closeOverflowMenu(false);

        auto* flyout = new NavigationFlyout(m_tree, this);

        // 克隆溢出条目子树（header 与节点按序渲染，分类可内联折叠）
        flyout->rebuildSubtreeFromEntries(entries);

        m_overflowFlyout = flyout;

        connect(this, &NavigationPanel::indicatorOwnerChanged, flyout, &NavigationFlyout::onIndicatorOwnerChanged);

        flyout->addLightDismissPassthrough(this);

        // flyout 内展开状态改变：如果改变的是当前选中项的祖先，重新派发内部滑动动画
        connect(flyout, &NavigationFlyout::expansionChanged, this,
            [this](const QString& routeKey, bool /*expanded*/) {
                if (m_indicatorOwner && m_tree->isAncestorOf(m_indicatorOwner->routeKey(), routeKey)) {
                    dispatchIndicatorAnimation(m_indicatorOwner, m_indicatorOwner);
                }
            });

        // overflow flyout 关闭后统一收尾：重排 + 按意图播放动画
        connect(flyout, &NavigationFlyout::aboutToHide, this,
            [this, anchorWidget]() {
                animateFlyoutClosed(QString(), anchorWidget);
            });

        // Horizontal 模式下：若选中项在溢出列表中，待 flyout 完全定位后触发跨窗口联合传送门
        if (m_orientation == Orientation::Horizontal && m_indicator) {
            connect(flyout, &NavigationFlyout::opened, this,
                [this, flyout, anchorWidget]() {
                    if (NavigationTreeItem* curClone = flyout->cloneItemFor(m_tree->currentRouteKey())) {
                        triggerCrossWindowPortal(flyout, anchorWidget, m_indicatorOwner, curClone);
                    }
                }, Qt::SingleShotConnection);
        }

        flyout->setPlacement(NavigationFlyout::Placement::Bottom);
        flyout->showAt(anchorWidget);
    }

    void NavigationPanel::closeOverflowMenu(bool animated)
    {
        if (!m_overflowFlyout)
            return;
        auto* popup = m_overflowFlyout.data();
        m_overflowFlyout = nullptr;

        if (animated && popup->isVisible()) {
            connect(popup, &NavigationFlyout::closed,
                popup, &QObject::deleteLater);
            popup->close();
        }
        else {
            popup->setExitAnimationEnabled(false);
            connect(popup, &NavigationFlyout::closed,
                popup, &QObject::deleteLater);
            popup->close();
        }
    }

    void NavigationPanel::dispatchIndicatorAnimation(NavigationTreeItem* prevOwner, NavigationTreeItem* curOwner)
    {
        if (!curOwner) return;

        // 获取当前处于激活状态的 Flyout
        NavigationFlyout* activeFlyout = m_compactFlyout ? m_compactFlyout.data() :
            (m_overflowFlyout ? m_overflowFlyout.data() : nullptr);

        // O(1) 查找 Flyout 内部是否有克隆替身
        NavigationTreeItem* prevClone = activeFlyout && prevOwner ? activeFlyout->cloneItemFor(prevOwner->routeKey()) : nullptr;
        NavigationTreeItem* curClone = activeFlyout ? activeFlyout->cloneItemFor(curOwner->routeKey()) : nullptr;

        if (activeFlyout && curClone && !curOwner->isCategory()) {
            // 判决0（最高优先级）：叶子项点击，Flyout 即将关闭，
            // 不得在 flyout 内发起任何飞跃或绘制，起点终点推迟到 animateFlyoutClosed 统一接管
            m_flyoutCloseIntent = FlyoutCloseIntent::LeafSlide;
            m_flyoutPrevOwner = prevOwner;
            if (prevOwner) emit indicatorOwnerChanged(prevOwner, false);
            return;
        }

        if (activeFlyout && prevClone && curClone) {
            // 判决1：同 flyout 内飞跃（仅针对可选分类项的 Inline Expansion）
            NavigationIndicator* flyoutInd = activeFlyout->indicator();
            if (!flyoutInd) return;

            // 同步起点：如果是同节点展开/折叠代理变更，不强制同步，允许从原视觉位置平滑滑过去
            if (prevOwner && prevOwner != curOwner) {
                const QRectF startRect = activeFlyout->indicatorRectInHost(prevClone);
                flyoutInd->setInitialPosition(startRect);
            }

            connect(flyoutInd, &NavigationIndicator::flightStarted, this, [this, prevOwner]() {
                if (prevOwner) emit indicatorOwnerChanged(prevOwner, false);
                }, Qt::SingleShotConnection);
            connect(flyoutInd, &NavigationIndicator::flightFinished, this, [this, curOwner]() {
                if (m_indicatorOwner == curOwner) emit indicatorOwnerChanged(curOwner, true);
                }, Qt::SingleShotConnection);

            const QRectF targetRect = activeFlyout->indicatorRectInHost(curClone);
            activeFlyout->playInternalFlight(targetRect);
        }
        else if (activeFlyout && !prevClone && curClone) {
            // 判决2：跨窗口传送门（仅针对可选分类项）
            QWidget* anchor = activeFlyout->anchor();
            triggerCrossWindowPortal(activeFlyout, anchor, prevOwner, curClone);
        }
        else {
            // 判决3：主界面常规飞跃或视觉回退
            if (!m_indicator) return;

            if (!prevOwner) {
                refreshIndicatorVisuals(false);
                return;
            }

            // 完全贯通：起点同步与所有动画生命周期的信号分发均收敛到 refreshIndicatorVisuals 中
            refreshIndicatorVisuals(true, prevOwner);
        }
    }

    void NavigationPanel::animateFlyoutClosed(const QString& categoryKey, QWidget* anchorWidget)
    {
        if (!m_tree) {
            m_flyoutCloseIntent = FlyoutCloseIntent::None;
            m_flyoutPrevOwner = nullptr;
            return;
        }

        // ① 先重排（保证后续所有坐标计算基于最新布局）
        if (!categoryKey.isEmpty()) m_tree->dismissCategory();
        m_tree->updateOverflow();

        // ② 实时重读 owner（防止用户在 flyout 动画期间又点了别的）
        NavigationTreeItem* logicOwner = m_indicatorOwner;
        if (!logicOwner) {
            m_flyoutCloseIntent = FlyoutCloseIntent::None;
            m_flyoutPrevOwner = nullptr;
            return;
        }

        // ③ 取出意图与 prevOwner 后复位，防止多开 flyout 状态残留
        const FlyoutCloseIntent intent = m_flyoutCloseIntent;
        NavigationTreeItem* prevOwner = m_flyoutPrevOwner;
        m_flyoutCloseIntent = FlyoutCloseIntent::None;
        m_flyoutPrevOwner = nullptr;

        // ④ 延迟到下一帧计算（等待 updateOverflow 与 dismissCategory 完成布局刷新）
        QTimer::singleShot(0, this, [this, categoryKey, anchorWidget, logicOwner, intent, prevOwner]() {
            // 在延时回调中再次确认 owner 是否改变
            if (m_indicatorOwner != logicOwner) {
                return;
            }

            const bool belongsToFlyout = categoryKey.isEmpty()
                ? m_tree->hasOverflowItems()
                : (logicOwner->routeKey() == categoryKey
                    || m_tree->isAncestorOf(logicOwner->routeKey(), categoryKey));

            if (!belongsToFlyout) {
                emit indicatorOwnerChanged(logicOwner, true);
                refreshIndicatorVisuals(false);
                return;
            }

            // 计算终点：owner 在顶栏上的视觉代理
            NavigationTreeItem* visualOwner = m_tree->getVisualProxyFor(logicOwner);
            NavigationWidget* returnItem = visualOwner
                ? visualOwner
                : qobject_cast<NavigationWidget*>(anchorWidget);
            if (!returnItem) returnItem = logicOwner;
            const QRectF targetRect = m_tree->indicatorRectInHost(returnItem, this);

            // 按意图分流动画
            switch (intent) {
            case FlyoutCloseIntent::LeafSlide: {
                // 完全贯通：交由 refreshIndicatorVisuals 统一处理同步、动画与信号分发
                if (m_indicator) {
                    refreshIndicatorVisuals(true, prevOwner);
                }
                break;
            }
            case FlyoutCloseIntent::PortalReturn: {
                // 仅对需要特殊动画的 PortalReturn 注册单独的降落亮灯
                connect(m_indicator, &NavigationIndicator::flightFinished, this,
                    [this, logicOwner]() {
                        if (m_indicatorOwner == logicOwner) {
                            emit indicatorOwnerChanged(logicOwner, true);
                            refreshIndicatorVisuals(false);
                        }
                    }, Qt::SingleShotConnection);
                m_indicator->playPortalReturn(targetRect);
                break;
            }
            case FlyoutCloseIntent::None: {
                refreshIndicatorVisuals(false, prevOwner);
                break;
            }
            }
            });
    }

} // namespace ui::navigation
