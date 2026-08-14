#include "ui/navigation/NavigationTreeWidget.h"

#include <functional>

#include <QDateTime>
#include <QDebug>
#include <QEasingCurve>
#include <QEvent>
#include <QGuiApplication>
#include <QHash>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScopeGuard>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QTimer>
#include <QVariantAnimation>
#include <QVBoxLayout>

#include <FluentQt/Design.h>
#include <FluentQt/DialogsFlyouts.h>
#include <FluentQt/Scrolling.h>

#include <components/foundation/overlay/OverlayGeometry.h>

#include "ui/navigation/NavigationFlyoutPopup.h"
#include "ui/navigation/NavigationIndicator.h"
#include "ui/navigation/NavigationMetrics.h"
#include "ui/navigation/NavigationSectionHeader.h"
#include "ui/navigation/NavigationSeparator.h"
#include "ui/navigation/NavigationToolButton.h"
#include "ui/navigation/NavigationTreeItem.h"
#include "ui/navigation/NavigationWidget.h"

namespace ui::navigation {

    NavigationTreeWidget::NavigationTreeWidget(QWidget* parent)
        : NavigationTreeWidgetBase(this, parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setAutoFillBackground(false);
        m_layout = new QBoxLayout(QBoxLayout::TopToBottom, this);
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(0);

        m_mainLayout = new QBoxLayout(QBoxLayout::TopToBottom);
        m_mainLayout->setContentsMargins(0, 0, 0, 0);
        m_mainLayout->setSpacing(0);
        m_mainLayout->addStretch(1); // Keep items packed to the top

        m_footerLayout = new QBoxLayout(QBoxLayout::TopToBottom);
        m_footerLayout->setContentsMargins(0, 0, 0, 0);
        m_footerLayout->setSpacing(0);

        auto* scrollContainer = new QWidget(this);
        scrollContainer->setLayout(m_mainLayout);
        scrollContainer->setObjectName("NavigationScrollContainer");
        scrollContainer->setStyleSheet("background: transparent;");

        m_scrollView = new fluent::scrolling::ScrollView(this);
        m_scrollView->setWidgetResizable(true);
        m_scrollView->setContentWidget(scrollContainer);
        m_scrollView->setFrameShape(QFrame::NoFrame);
        m_scrollView->setVerticalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Visible);
        m_scrollView->viewport()->setStyleSheet("background: transparent;");
        m_scrollView->setStyleSheet("background: transparent; border: none;");

        m_layout->addWidget(m_scrollView, 1);
        m_layout->addLayout(m_footerLayout);

        connect(m_scrollView->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
            refreshIndicator(false, false);
            });

        m_overflowButton = new NavigationToolButton(Typography::Icons::More, this);
        m_overflowButton->setObjectName(QStringLiteral("NavigationOverflowButton"));
        m_overflowButton->setOrientation(NavigationOrientation::Horizontal);
        m_overflowButton->setAnimatedMove(true);
        m_overflowButton->hide();
        connect(m_overflowButton, &NavigationToolButton::clicked, this, [this]() {
            emit overflowMenuRequested(m_overflowButton, overflowEntries());
            });
        m_mainLayout->insertWidget(m_mainLayout->count() - 1, m_overflowButton);
    }

    NavigationTreeWidget::NavigationTreeWidget(NavigationTreeWidget* rootNode)
        : NavigationTreeWidgetBase(rootNode, rootNode)
    {}

    NavigationTreeWidget* NavigationTreeWidget::nodeFor(const QString& routeKey)
    {
        return m_root->m_nodeIndex.value(routeKey, nullptr);
    }

    const NavigationTreeWidget* NavigationTreeWidget::nodeFor(const QString& routeKey) const
    {
        return m_root->m_nodeIndex.value(routeKey, nullptr);
    }

    void NavigationTreeWidget::addItem(const QString& routeKey, const QString& iconGlyph,
        const QString& text, const QString& parentKey,
        NavigationItemPosition position, bool selectable)
    {
        auto* node = new NavigationTreeWidget(m_root);
        node->m_routeKey = routeKey;
        node->m_parentNode = parentKey.isEmpty() ? nullptr : nodeFor(parentKey);

        if (node->m_parentNode) {
            if (node->m_parentNode->m_itemWidget)
                node->m_parentNode->item()->setKind(NavigationTreeItem::Kind::Category);
            node->m_parentNode->m_children.append(node);
        }

        const int depth = node->m_parentNode ? node->m_parentNode->m_itemWidget->nodeDepth() + 1 : 0;
        auto* item = new NavigationTreeItem(routeKey, iconGlyph, text,
            NavigationTreeItem::Kind::Leaf, depth, selectable, nullptr);
        item->setOrientation(orientationFor(m_position));
        item->setExpandProgress(m_expandProgress);
        item->setItemPosition(position);
        item->setTreeParent(node);
        node->m_itemWidget = item;

        // 统一决策槽：itemWidget 只发一个信号，展开/选中/flyout 判定收敛于此。
        connect(item, &NavigationTreeItem::itemClicked, node,
            [node](const QString& key, bool chevronClicked) {
                node->onItemClicked(key, chevronClicked);
            });

        // 展开事件上报：根统一处理指示条归属副作用。
        connect(node, &NavigationTreeWidgetBase::expansionChanged, this,
            [this](const QString& key, bool expanded) {
                onExpansionChanged(key, expanded);
            });

        m_root->m_nodeIndex.insert(routeKey, node);

        if (node->m_parentNode) {
            // 确保父分类的子容器存在并插入布局（紧随父项之后）。
            NavigationTreeWidget* parentNode = node->m_parentNode;
            if (!parentNode->m_childrenContainer) {
                auto* container = new QWidget(nullptr);
                container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                container->setAutoFillBackground(false);
                auto* childLayout = new QVBoxLayout(container);
                childLayout->setContentsMargins(0, 0, 0, 0);
                childLayout->setSpacing(0);
                parentNode->m_childrenContainer = container;
                parentNode->m_childrenLayout = childLayout;
                QBoxLayout* hostLayout = qobject_cast<QBoxLayout*>(
                    parentNode->m_itemWidget->parentWidget()->layout());
                if (hostLayout) {
                    const int idx = hostLayout->indexOf(parentNode->m_itemWidget) + 1;
                    hostLayout->insertWidget(idx, container);
                }
                container->setVisible(parentNode->m_isExpanded);
            }
            parentNode->m_childrenLayout->addWidget(item);
        }
        else {
            addWidget(item, position);
            m_children.append(node);
        }
    }

    bool NavigationTreeWidget::removeItem(const QString& routeKey)
    {
        NavigationTreeWidget* node = nodeFor(routeKey);
        if (!node)
            return false;

        std::function<void(NavigationTreeWidget*)> detach = [&](NavigationTreeWidget* n) {
            for (NavigationTreeWidget* child : n->m_children)
                detach(child);
            n->m_children.clear();
            if (n->m_childrenContainer) {
                n->m_childrenContainer->deleteLater();
                n->m_childrenContainer = nullptr;
                n->m_childrenLayout = nullptr;
            }
            if (n->m_itemWidget)
                n->m_itemWidget->deleteLater();
            m_root->m_nodeIndex.remove(n->m_routeKey);
            n->deleteLater();
            };

        if (node->m_parentNode)
            node->m_parentNode->m_children.removeOne(node);
        else
            m_children.removeOne(node);

        detach(node);

        if (m_currentRouteKey == routeKey) {
            m_currentRouteKey.clear();
            setIndicatorOwner(nullptr);
        }

        return true;
    }

    void NavigationTreeWidget::addSectionHeader(const QString& text)
    {
        addWidget(new NavigationSectionHeader(text, this));
    }

    void NavigationTreeWidget::addWidget(NavigationWidget* widget, NavigationItemPosition position)
    {
        if (!widget)
            return;

        widget->setOrientation(orientationFor(m_position));
        widget->setExpandProgress(m_expandProgress);

        if (auto* header = qobject_cast<NavigationSectionHeader*>(widget))
            m_headers.append(header);

        if (position == NavigationItemPosition::Bottom) {
            if (m_position == NavigationPosition::Left && !m_addingFooter) {
                m_addingFooter = true;
                auto* sep = new NavigationSeparator(this);
                sep->setOrientation(orientationFor(m_position));
                m_footerLayout->addWidget(sep);
            }
            m_footerLayout->addWidget(widget);
        }
        else {
            const int insertIdx = (m_overflowButton && m_mainLayout->indexOf(m_overflowButton) >= 0)
                ? m_mainLayout->indexOf(m_overflowButton)
                : m_mainLayout->count() - 1;
            m_mainLayout->insertWidget(insertIdx, widget);
        }
    }

    bool NavigationTreeWidget::contains(const QString& routeKey) const
    {
        return m_root->m_nodeIndex.contains(routeKey);
    }

    bool NavigationTreeWidget::isRoot(const QString& routeKey) const
    {
        const NavigationTreeWidget* node = nodeFor(routeKey);
        return node && node->m_parentNode == nullptr;
    }

    bool NavigationTreeWidget::isLeaf(const QString& routeKey) const
    {
        const NavigationTreeWidget* node = nodeFor(routeKey);
        return node && node->m_children.isEmpty();
    }

    void NavigationTreeWidget::collapseCategoryChevron(const QString& categoryKey)
    {
        NavigationTreeWidget* node = nodeFor(categoryKey);
        if (!node || !node->m_itemWidget)
            return;
        node->item()->animateChevron(0.0f);
    }

    void NavigationTreeWidget::dismissCategory()
    {
        if (m_activeCategoryKey.isEmpty())
            return;
        const QString key = m_activeCategoryKey;
        m_activeCategoryKey.clear();
        collapseCategoryChevron(key);
        emit categoryDeactivated(key);
    }

    void NavigationTreeWidget::updateOverflow()
    {
        computeOverflow(true);
    }

    void NavigationTreeWidget::activateCategory(const QString& categoryKey, QWidget* anchorWidget)
    {
        if (m_activeCategoryKey == categoryKey)
            return;
        // 若之前有别的分类激活，先反激活。
        if (!m_activeCategoryKey.isEmpty())
            dismissCategory();
        m_activeCategoryKey = categoryKey;
        emit categoryActivated(categoryKey, anchorWidget);
    }

    bool NavigationTreeWidget::isAncestorOf(const QString& routeKey, const QString& ancestorKey) const
    {
        const NavigationTreeWidget* node = nodeFor(routeKey);
        const NavigationTreeWidget* ancestor = nodeFor(ancestorKey);
        if (!node || !ancestor || node == ancestor)
            return false;
        for (const NavigationTreeWidget* p = node->m_parentNode; p; p = p->m_parentNode) {
            if (p == ancestor)
                return true;
        }
        return false;
    }

    void NavigationTreeWidget::setCompacted(bool compacted)
    {
        if (m_isCompacted == compacted)
            return;
        m_isCompacted = compacted;
    }

    QVector<NavigationTreeWidget::ChildDescriptor> NavigationTreeWidget::childrenOf(const QString& categoryKey) const
    {
        QVector<ChildDescriptor> result;
        const NavigationTreeWidget* node = nodeFor(categoryKey);
        if (!node)
            return result;
        for (const NavigationTreeWidget* child : node->m_children) {
            if (!child->m_itemWidget)
                continue;
            auto* btn = qobject_cast<NavigationPushButton*>(child->m_itemWidget);
            result.append({ child->m_routeKey,
                            btn ? btn->iconGlyph() : QString(),
                            btn ? btn->text() : QString() });
        }
        return result;
    }

    void NavigationTreeWidget::setCurrentItem(const QString& routeKey, bool animated, bool updateOverflow)
    {
        if (m_settingCurrentItem)
            return;
        m_settingCurrentItem = true;
        auto cleanup = qScopeGuard([this] { m_settingCurrentItem = false; });

        NavigationTreeWidget* node = nodeFor(routeKey);
        if (!node || !node->m_itemWidget)
            return;

        if (m_isCompacted || m_position == NavigationPosition::Top) {
            NavigationTreeWidget* anchor = node;
            while (anchor->m_parentNode && anchor->m_parentNode != m_root) {
                anchor = anchor->m_parentNode;
            }
            if (!anchor->m_itemWidget)
                return;

            qDebug() << "[NavTree] setCurrentItem (Compact/Top) routeKey:" << routeKey
                << "anchor:" << anchor->routeKey() << "animated:" << animated
                << "prevRouteKey:" << m_currentRouteKey;

            if (routeKey == m_currentRouteKey) {
                moveIndicatorTo(anchor->m_itemWidget, animated);
                return;
            }

            if (NavigationTreeWidget* old = nodeFor(m_currentRouteKey); old && old->m_itemWidget)
                old->m_itemWidget->setSelected(false);

            m_currentRouteKey = routeKey;
            node->m_itemWidget->setSelected(true);

            // 无论选中叶子还是深层子项，若其所属顶级分类处于 overflow（不可见），则将该顶级分类进行粘性固定
            if (m_overflowNodes.contains(anchor) || !anchor->m_itemWidget->isVisible()) {
                m_pinnedCategoryKey = anchor->routeKey();
                qDebug() << "[NavTree] setCurrentItem: pinned root category from overflow ->" << m_pinnedCategoryKey;
            }

            // 选中态变化后重算 overflow，按末端吸附策略排布条目，并应用动画状态；
            // flyout 内点击 selectable 分类项时延迟重算（updateOverflow=false），待 flyout 关闭再触发
            bool overflowChanged = false;
            if (updateOverflow)
                overflowChanged = computeOverflow(animated);

            // 溢出项发生变动时指示条应直接瞬移，不启动飞行动画
            refreshIndicator(overflowChanged ? false : animated, false);
            emit itemSelected(routeKey);
            return;
        }

        qDebug() << "[NavTree] setCurrentItem (Left) routeKey:" << routeKey << "animated:" << animated;

        if (m_position != NavigationPosition::Top)
            expandAncestors(node);

        if (routeKey == m_currentRouteKey) {
            moveIndicatorTo(node->m_itemWidget, animated);
            return;
        }

        if (NavigationTreeWidget* old = nodeFor(m_currentRouteKey); old && old->m_itemWidget)
            old->m_itemWidget->setSelected(false);
        m_currentRouteKey = routeKey;
        node->m_itemWidget->setSelected(true);

        moveIndicatorTo(node->m_itemWidget, animated);
        emit itemSelected(routeKey);
    }

    QRectF NavigationTreeWidget::indicatorRectInHost(NavigationWidget* item) const
    {
        const QRectF local = item->indicatorRect();
        if (!m_indicator)
            return local;
        const QWidget* host = m_indicator->parentWidget();
        const QPoint itemOriginInHost = item->mapTo(host ? host : this, QPoint(0, 0));
        const QPointF topLeft = QPointF(itemOriginInHost) + local.topLeft();
        return QRectF(topLeft, local.size());
    }

    void NavigationTreeWidget::moveIndicatorTo(NavigationWidget* item, bool animated)
    {
        if (!m_indicator)
            return;

        NavigationWidget* targetItem = item;
        if (m_position == NavigationPosition::Top) {
            if (m_overflowButton && m_overflowButton->isVisible() && !item->isVisible())
                targetItem = m_overflowButton;
        }

        const QRectF target = indicatorRectInHost(targetItem);
        const int duration = animated ? themeAnimation().normal : 0;

        qDebug() << "[NavTree] moveIndicatorTo targetItem:" << targetItem
            << "origItem:" << item << "origVisible:" << item->isVisible()
            << "targetRect:" << target << "animated:" << animated << "duration:" << duration;

        const QWidget* host = m_indicator->parentWidget();
        if (m_scrollView && m_scrollView->viewport() && host && m_scrollView->isAncestorOf(targetItem)) {
            const QRect viewportInHost = QRect(
                m_scrollView->viewport()->mapTo(host, QPoint(0, 0)),
                m_scrollView->viewport()->size());
            if (!viewportInHost.isEmpty()) {
                const QRectF viewRect(viewportInHost);
                const QRectF visible = viewRect.intersected(target);

                if (m_position == NavigationPosition::Top) {
                    if (visible.width() < kTopSelectionIndicatorWidth * 0.5) {
                        m_indicator->hideIndicator();
                        return;
                    }
                    QRectF clamped = target;
                    clamped.setLeft(qMax(target.left(), viewRect.left()));
                    clamped.setRight(qMin(target.right(), viewRect.right()));
                    m_indicator->activateAt(clamped, duration, animated);
                    return;
                }
                else {
                    if (visible.height() < kSelectionIndicatorHeight * 0.5) {
                        m_indicator->hideIndicator();
                        return;
                    }
                    QRectF clamped = target;
                    clamped.setTop(qMax(target.top(), viewRect.top()));
                    clamped.setBottom(qMin(target.bottom(), viewRect.bottom()));
                    m_indicator->activateAt(clamped, duration, animated);
                    return;
                }
            }
        }

        m_indicator->activateAt(target, duration, animated);
    }

    void NavigationTreeWidget::refreshIndicator(bool animated, bool /*forceSnap*/)
    {
        if (!m_indicator || m_currentRouteKey.isEmpty()) {
            qDebug() << "[NavTree] refreshIndicator skipped (no indicator or empty key)";
            return;
        }
        NavigationTreeWidget* node = nodeFor(m_currentRouteKey);
        if (!node || !node->m_itemWidget) {
            qDebug() << "[NavTree] refreshIndicator node not found for" << m_currentRouteKey;
            return;
        }
        if (m_isCompacted || m_position == NavigationPosition::Top) {
            NavigationTreeWidget* anchor = node;
            while (anchor->m_parentNode && anchor->m_parentNode != m_root) {
                anchor = anchor->m_parentNode;
            }
            qDebug() << "[NavTree] refreshIndicator (Compact/Top) node:" << m_currentRouteKey
                << "anchor:" << (anchor ? anchor->routeKey() : "null") << "animated:" << animated;
            if (anchor->m_itemWidget)
                moveIndicatorTo(anchor->m_itemWidget, animated);
            return;
        }
        qDebug() << "[NavTree] refreshIndicator (Left) node:" << m_currentRouteKey << "animated:" << animated;
        moveIndicatorTo(node->m_itemWidget, animated);
    }

    QVector<NavigationWidget*> NavigationTreeWidget::visibleItems() const
    {
        QVector<NavigationWidget*> result;
        collectVisible(m_root, result);
        return result;
    }

    void NavigationTreeWidget::collectVisible(NavigationTreeWidget* node, QVector<NavigationWidget*>& out) const
    {
        if (!node)
            return;
        if (node->m_itemWidget)
            out.append(node->m_itemWidget);
        if (node->isCategory() && node->m_isExpanded) {
            for (NavigationTreeWidget* child : node->m_children)
                collectVisible(child, out);
        }
    }

    void NavigationTreeWidget::moveFocusBy(int delta)
    {
        const QVector<NavigationWidget*> items = visibleItems();
        if (items.isEmpty())
            return;
        int idx = -1;
        for (int i = 0; i < items.size(); ++i) {
            if (auto* item = qobject_cast<NavigationTreeItem*>(items.at(i))) {
                if (item->routeKey() == m_currentRouteKey) { idx = i; break; }
            }
        }
        if (idx < 0)
            idx = (delta > 0) ? -1 : items.size();
        const int next = (idx + delta + items.size()) % items.size();
        NavigationWidget* target = items.at(next);
        target->setFocus(Qt::ShortcutFocusReason);
        if (auto* item = qobject_cast<NavigationTreeItem*>(target))
            setCurrentItem(item->routeKey());
    }

    void NavigationTreeWidget::setIndicator(NavigationIndicator* indicator)
    {
        if (m_indicator == indicator)
            return;
        if (m_indicator) {
            disconnect(m_indicator, &NavigationIndicator::flightFinished, this, &NavigationTreeWidget::onFlightFinished);
            disconnect(m_indicator, &NavigationIndicator::flightStarted, this, nullptr);
        }
        m_indicator = indicator;
        if (m_indicator) {
            connect(m_indicator, &NavigationIndicator::flightFinished, this, &NavigationTreeWidget::onFlightFinished);
            connect(m_indicator, &NavigationIndicator::flightStarted, this,
                [this]() { setIndicatorOwner(nullptr); });
        }
    }

    void NavigationTreeWidget::onFlightFinished()
    {
        NavigationTreeWidget* node = nodeFor(m_currentRouteKey);
        if (!node || !node->m_itemWidget)
            return;

        qDebug() << "[NavTree] onFlightFinished currentRouteKey:" << m_currentRouteKey;

        if (m_isCompacted || m_position == NavigationPosition::Top) {
            NavigationTreeWidget* anchor = node;
            while (anchor->m_parentNode && anchor->m_parentNode != m_root) {
                anchor = anchor->m_parentNode;
            }
            if (anchor && anchor->m_itemWidget) {
                if (m_position == NavigationPosition::Top && !anchor->item()->isVisible()) {
                    qDebug() << "[NavTree] onFlightFinished: anchor is in overflow/hidden, clearing indicator owner";
                    setIndicatorOwner(nullptr);
                }
                else {
                    setIndicatorOwner(anchor->item());
                }
            }
            return;
        }

        auto isReachable = [](const NavigationTreeWidget* n) -> bool {
            for (const NavigationTreeWidget* p = n->m_parentNode; p; p = p->m_parentNode) {
                if (p->isCategory() && !p->m_isExpanded)
                    return false;
            }
            return true;
            };

        if (isReachable(node)) {
            setIndicatorOwner(node->item());
        }
        else {
            NavigationTreeWidget* proxy = node->m_parentNode;
            while (proxy && proxy->isCategory()) {
                if (isReachable(proxy)) {
                    if (proxy->m_itemWidget) {
                        setIndicatorOwner(proxy->item());
                    }
                    return;
                }
                proxy = proxy->m_parentNode;
            }
            if (proxy && proxy->m_itemWidget) {
                setIndicatorOwner(proxy->item());
            }
        }
    }

    void NavigationTreeWidget::onExpansionChanged(const QString& routeKey, bool expanded)
    {
        // 只有展开态变化影响当前选中项所在分类链时才需调整指示条归属。
        if (m_currentRouteKey.isEmpty() || !isAncestorOf(m_currentRouteKey, routeKey))
            return;

        NavigationTreeWidget* node = nodeFor(routeKey);
        if (!node)
            return;

        if (expanded) {
            // 展开：指示条交还到实际选中子项。
            if (node->m_itemWidget && node->item() == m_indicatorOwner)
                setIndicatorOwner(nullptr);
            if (NavigationTreeWidget* sel = nodeFor(m_currentRouteKey); sel && sel->m_itemWidget)
                moveIndicatorTo(sel->m_itemWidget, true);
        }
        else {
            // 折叠：选中项被隐藏，指示条回退到最近的可见分类。
            setIndicatorOwner(nullptr);
            if (node->m_itemWidget)
                moveIndicatorTo(node->m_itemWidget, false);
        }
    }

    void NavigationTreeWidget::setIndicatorOwner(NavigationTreeItem* owner)
    {
        if (m_indicatorOwner == owner)
            return;
        qDebug() << "[NavTree] setIndicatorOwner from"
            << (m_indicatorOwner ? m_indicatorOwner->routeKey() : "nullptr")
            << "to" << (owner ? owner->routeKey() : "nullptr");
        if (m_indicatorOwner)
            m_indicatorOwner->setShowIndicator(false);
        m_indicatorOwner = owner;
        if (m_indicatorOwner)
            m_indicatorOwner->setShowIndicator(true);
    }

    void NavigationTreeWidget::releaseIndicatorOwner()
    {
        setIndicatorOwner(nullptr);
    }

    void NavigationTreeWidget::restoreIndicatorOwner()
    {
        // 飞行期间不得设置常驻 owner（飞行件与常驻指示条会重叠），
        // 待 flightFinished 后由 onFlightFinished 自然恢复归属。
        if (m_indicator && m_indicator->isFlying()) {
            releaseIndicatorOwner();
            return;
        }
        // 复用飞行结束的归属判定：选中项可达则归其自身，否则回退到最近可见分类。
        onFlightFinished();
    }

    void NavigationTreeWidget::setCategoryExpanded(const QString& routeKey, bool expanded, bool animated)
    {
        if (m_isCompacted)
            return;
        NavigationTreeWidget* node = nodeFor(routeKey);
        if (!node || !node->isCategory())
            return;
        node->setExpanded(expanded, animated);
    }

    void NavigationTreeWidget::setRememberExpandState(const QString& routeKey, bool remember)
    {
        if (NavigationTreeWidget* node = nodeFor(routeKey))
            node->m_rememberExpandState = remember;
    }

    void NavigationTreeWidget::saveExpandState(const QString& routeKey)
    {
        NavigationTreeWidget* node = nodeFor(routeKey);
        if (node && node->m_rememberExpandState)
            node->m_savedExpanded = node->m_isExpanded;
    }

    void NavigationTreeWidget::restoreExpandState(const QString& routeKey, bool animated)
    {
        NavigationTreeWidget* node = nodeFor(routeKey);
        if (!node || !node->m_rememberExpandState || !node->isCategory())
            return;
        node->setExpanded(node->m_savedExpanded, animated);
    }

    void NavigationTreeWidget::propagateExpandProgress(float progress)
    {
        std::function<void(NavigationTreeWidget*)> visit = [&](NavigationTreeWidget* node) {
            if (!node)
                return;
            if (node->m_itemWidget)
                node->m_itemWidget->setExpandProgress(progress);
            for (NavigationTreeWidget* child : node->m_children)
                visit(child);
            };
        visit(m_root);

        for (NavigationSectionHeader* header : m_headers)
            if (header)
                header->setExpandProgress(progress);
    }

    void NavigationTreeWidget::propagateNavigationPosition(NavigationPosition position)
    {
        // 沿逻辑树一次递归同时完成：itemWidget 设方向 + 子容器显隐/最大尺寸。
        std::function<void(NavigationTreeWidget*)> visit = [&](NavigationTreeWidget* node) {
            if (!node)
                return;
            if (node->m_itemWidget)
                node->m_itemWidget->setOrientation(orientationFor(position));
            if (node->m_childrenContainer) {
                if (position == NavigationPosition::Top) {
                    node->m_childrenContainer->hide();
                    node->m_childrenContainer->setMaximumSize(0, 0);
                }
                else {
                    node->m_childrenContainer->show();
                    node->m_childrenContainer->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
                }
            }
            for (NavigationTreeWidget* child : node->m_children)
                visit(child);
            };
        visit(m_root);

        for (NavigationSectionHeader* header : m_headers) {
            header->setOrientation(orientationFor(position));
            header->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
            header->setVisible(true);
        }
    }

    void NavigationTreeWidget::setExpandProgress(float progress)
    {
        if (m_position == NavigationPosition::Top)
            return;

        const float p = qBound(0.0f, progress, 1.0f);
        if (qFuzzyCompare(p, m_expandProgress))
            return;
        m_expandProgress = p;

        propagateExpandProgress(p);
        updateActiveAncestorContainersProgress(p);

        // 仅在动画终态做全树捕获/恢复，避免逐帧遍历所有节点。
        if (p <= 0.001f)
            collapseAllCategories();
        else if (p >= 0.999f)
            restoreAllCategories();
    }

    void NavigationTreeWidget::updateActiveAncestorContainersProgress(float progress)
    {
        NavigationTreeWidget* cur = nodeFor(m_currentRouteKey);
        if (!cur)
            return;

        // 进度区间映射：面板展开至 20% 宽度以上时才开始展开子项，避免极窄阶段子项内容挤压
        constexpr float kExpandStartProgress = 0.2f;
        const float childProgress = (progress <= kExpandStartProgress)
            ? 0.0f
            : qBound(0.0f, (progress - kExpandStartProgress) / (1.0f - kExpandStartProgress), 1.0f);

        static const QEasingCurve easeCurve(QEasingCurve::OutQuad);
        const float eased = easeCurve.valueForProgress(childProgress);

        NavigationTreeWidget* parentCategory = (cur->m_parentNode && cur->m_parentNode->isCategory())
            ? cur->m_parentNode : nullptr;

        for (NavigationTreeWidget* pNode = cur->m_parentNode; pNode; pNode = pNode->m_parentNode) {
            if (!pNode->isCategory() || !pNode->m_childrenContainer)
                continue;

            QWidget* container = pNode->m_childrenContainer;

            if (pNode->m_heightAnimation)
                pNode->m_heightAnimation->stop();

            if (childProgress <= 0.001f) {
                // 只收容器，不改 m_isExpanded：保留 flyout 克隆的数据源。
                container->setFixedHeight(0);
                container->hide();
                pNode->item()->setArrowAngle(0.0f);
                pNode->item()->setExpanded(false, /*animated=*/false);
            }
            else {
                pNode->m_isExpanded = true;
                if (!container->isVisible())
                    container->show();

                const int targetHeight = container->layout()
                    ? container->layout()->sizeHint().height()
                    : container->sizeHint().height();

                pNode->item()->setArrowAngle(180.0f * eased);

                if (childProgress >= 0.999f) {
                    container->setMinimumHeight(0);
                    container->setMaximumHeight(QWIDGETSIZE_MAX);
                    pNode->item()->setExpanded(true, /*animated=*/false);
                }
                else {
                    container->setFixedHeight(qRound(targetHeight * eased));
                }
            }
        }

        if (parentCategory && cur->m_itemWidget) {
            if (!m_indicator || !m_indicator->isFlying()) {
                if (childProgress <= 0.001f) {
                    setIndicatorOwner(parentCategory->item());
                }
                else {
                    setIndicatorOwner(cur->item());
                }
            }
        }
    }

    void NavigationTreeWidget::collapseAllCategories()
    {
        // 折叠终态一次遍历：捕获所有分类展开态快照并收起子容器/箭头，不改 m_isExpanded
        // （其保留为 flyout 克隆的数据源），动画中间帧不重复遍历。
        m_savedExpandStates.clear();
        std::function<void(NavigationTreeWidget*)> visit = [&](NavigationTreeWidget* node) {
            if (!node)
                return;
            if (node->isCategory() && node->m_childrenContainer) {
                m_savedExpandStates.insert(node->m_routeKey, node->m_isExpanded);
                node->m_childrenContainer->setFixedHeight(0);
                node->m_childrenContainer->hide();
                node->item()->setArrowAngle(0.0f);
                node->item()->setExpanded(false, /*animated=*/false);
            }
            for (NavigationTreeWidget* child : node->m_children)
                visit(child);
            };
        visit(m_root);
    }

    void NavigationTreeWidget::restoreAllCategories()
    {
        // 展开终态一次遍历：按折叠前快照恢复所有分类的展开态（容器、箭头、m_isExpanded）。
        std::function<void(NavigationTreeWidget*)> visit = [&](NavigationTreeWidget* node) {
            if (!node)
                return;
            if (node->isCategory() && node->m_childrenContainer) {
                const bool expanded = m_savedExpandStates.value(node->m_routeKey, node->m_isExpanded);
                node->m_isExpanded = expanded;
                if (expanded) {
                    node->m_childrenContainer->setMinimumHeight(0);
                    node->m_childrenContainer->setMaximumHeight(QWIDGETSIZE_MAX);
                    node->m_childrenContainer->show();
                    node->item()->setExpanded(true, /*animated=*/false);
                    node->item()->setArrowAngle(180.0f);
                }
            }
            for (NavigationTreeWidget* child : node->m_children)
                visit(child);
            };
        visit(m_root);
    }

    void NavigationTreeWidget::paintEvent(QPaintEvent* event)
    {
        QWidget::paintEvent(event);
    }

    void NavigationTreeWidget::showEvent(QShowEvent* event)
    {
        QWidget::showEvent(event);
        QTimer::singleShot(0, this, [this]() {
            if (m_position == NavigationPosition::Top)
                computeOverflow();
            NavigationTreeWidget* node = nodeFor(m_currentRouteKey);
            if (node && node->m_itemWidget && m_indicator)
                m_indicator->activateAt(indicatorRectInHost(node->m_itemWidget), 0, false);
            });
    }

    void NavigationTreeWidget::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);
        if (m_position == NavigationPosition::Top
            && event->size().width() != event->oldSize().width()) {
            computeOverflow();
        }
    }

    void NavigationTreeWidget::setNavigationPosition(NavigationPosition position)
    {
        if (m_position == position)
            return;
        m_position = position;

        const auto direction = (position == NavigationPosition::Top)
            ? QBoxLayout::LeftToRight
            : QBoxLayout::TopToBottom;

        m_layout->setDirection(direction);
        m_mainLayout->setDirection(direction);
        m_footerLayout->setDirection(direction);

        if (m_scrollView) {
            if (position == NavigationPosition::Top) {
                m_scrollView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                m_scrollView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                m_scrollView->setHorizontalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Hidden);
                m_scrollView->setVerticalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Hidden);
                m_scrollView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            }
            else {
                m_scrollView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                m_scrollView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
                m_scrollView->setHorizontalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Hidden);
                m_scrollView->setVerticalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Visible);
                m_scrollView->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
            }
        }

        propagateNavigationPosition(position);

        if (m_indicator)
            m_indicator->setNavigationPosition(position);

        if (position == NavigationPosition::Top) {
            computeOverflow();
        }
        else {
            if (m_overflowButton) {
                m_overflowButton->hide();
                m_overflowButton->setSelected(false);
            }
            for (NavigationTreeWidget* node : m_children)
                if (node->m_itemWidget)
                    node->m_itemWidget->show();
            for (auto* header : m_headers)
                header->show();
        }

        update();
    }

    bool NavigationTreeWidget::computeOverflow(bool animated)
    {
        if (m_updatingOverflow || m_position != NavigationPosition::Top)
            return false;
        m_updatingOverflow = true;

        const int footerW = m_footerLayout ? m_footerLayout->sizeHint().width() : 0;
        const int availableWidth = qMax(0, width() - footerW);

        // overflow 按钮构造时已 setFixedSize(固定尺寸)，width() 反映其不可压缩宽度。
        const int overflowBtnWidth = m_overflowButton ? m_overflowButton->width() : kTopBarItemHeight;

        // 窗口一缩小即立即撤销粘性保护——不等到放不下才取消
        if (m_lastTopAvailableWidth > 0 && availableWidth < m_lastTopAvailableWidth) {
            if (!m_pinnedCategoryKey.isEmpty()) {
                qDebug() << "[NavTree] computeOverflow: window shrunk -> unpinning" << m_pinnedCategoryKey;
                m_pinnedCategoryKey.clear();
            }
        }

        // 收集顶级条目（O(n)，预建哈希替代 O(n×m) 双层线性搜索）
        struct Entry {
            QWidget* widget = nullptr;
            NavigationTreeWidget* node = nullptr;
            NavigationSectionHeader* header = nullptr;
            int                      width = 0;
        };

        QHash<QWidget*, NavigationTreeWidget*> nodeLookup;
        nodeLookup.reserve(m_children.size());
        for (NavigationTreeWidget* child : m_children)
            if (child->itemWidget())
                nodeLookup.insert(child->itemWidget(), child);

        QHash<QWidget*, NavigationSectionHeader*> headerLookup;
        headerLookup.reserve(m_headers.size());
        for (auto* h : m_headers)
            headerLookup.insert(h, h);

        QVector<Entry> entries;
        entries.reserve(m_mainLayout->count());
        for (int i = 0; i < m_mainLayout->count(); ++i) {
            QLayoutItem* li = m_mainLayout->itemAt(i);
            QWidget* w = li ? li->widget() : nullptr;
            if (!w || w == m_overflowButton)
                continue;
            NavigationTreeWidget* foundNode = nodeLookup.value(w, nullptr);
            NavigationSectionHeader* foundHeader = headerLookup.value(w, nullptr);
            if (foundNode || foundHeader)
                entries.append({ w, foundNode, foundHeader, w->sizeHint().width() });
        }

        // 找出当前选中项所在的顶级分类索引
        int selectedIdx = -1;
        for (int i = 0; i < entries.size(); ++i) {
            if (entries[i].node && isSelectedUnder(entries[i].node)) {
                selectedIdx = i;
                break;
            }
        }

        // 宽度不足的边界（条目宽度或按钮自身宽度缺失）：按钮不得隐藏
        // 即使 availableWidth ≤ 0，overflow 按钮仍须可见：让布局为按钮保留空间。
        if (availableWidth <= 0) {
            for (auto& e : entries)
                e.widget->setVisible(false);
            if (m_overflowButton) {
                m_overflowButton->show();
                m_overflowButton->setSelected(selectedIdx >= 0);
            }
            m_lastTopAvailableWidth = availableWidth;
            m_updatingOverflow = false;
            return true; // 项全部隐藏，视为布局变动
        }

        int totalItemsWidth = 0;
        for (const auto& e : entries)
            totalItemsWidth += e.width;

        m_overflowNodes.clear();
        m_overflowHeaders.clear();

        bool layoutChanged = false;

        qDebug() << "[NavTree] computeOverflow BEGIN: availableWidth=" << availableWidth
            << "totalItemsWidth=" << totalItemsWidth
            << "overflowBtnWidth=" << overflowBtnWidth
            << "entriesCount=" << entries.size()
            << "selectedIdx=" << selectedIdx
            << "requestedAnimated=" << animated;

        if (totalItemsWidth <= availableWidth) {
            // 全部放得下：全部可见，隐藏 overflow 按钮
            qDebug() << "[NavTree] computeOverflow: ALL items fit in top bar!";
            for (const auto& e : entries) {
                if (!e.widget->isVisible()) {
                    layoutChanged = true;
                    qDebug() << "[NavTree] computeOverflow: item became visible ->" << (e.node ? e.node->routeKey() : "header");
                }
                e.widget->setVisible(true);
            }
            if (m_overflowButton) {
                if (m_overflowButton->isVisible()) {
                    layoutChanged = true;
                    qDebug() << "[NavTree] computeOverflow: overflowButton hidden";
                }
                m_overflowButton->hide();
                m_overflowButton->setSelected(false);
            }
        }
        else {
            // 放不下：按优先级分配宽度
            // 按钮最高优先、不可压缩：条目只能使用扣除按钮宽度后的剩余空间。
            int remainingWidth = availableWidth - overflowBtnWidth;
            QVector<bool> isVisible(entries.size(), false);

            // 第一遍：按自然顺序线性填充（从 0 到 N-1）
            for (int i = 0; i < entries.size(); ++i) {
                if (entries[i].width <= remainingWidth) {
                    isVisible[i] = true;
                    remainingWidth -= entries[i].width;
                }
                else {
                    qDebug() << "[NavTree] computeOverflow: natural cutoff at"
                        << (entries[i].node ? entries[i].node->routeKey() : "header")
                        << "width=" << entries[i].width << "remainingWidth=" << remainingWidth;
                    break;
                }
            }

            // 第二遍：末端吸附策略（End Pinning）
            // 目标 1（核心保护）：当前选中项所在的根节点（selectedIdx）。
            // 无论选中的是顶级项还是深层子项，只要该顶级根节点未能自然进入栏内，
            // 必须从末尾倒序挤出非选中项，将其固定在栏内最右端！
            if (selectedIdx >= 0 && !isVisible[selectedIdx]) {
                const int needed = entries[selectedIdx].width;
                qDebug() << "[NavTree] computeOverflow: active selected root"
                    << (entries[selectedIdx].node ? entries[selectedIdx].node->routeKey() : "unknown")
                    << "is in overflow, needs" << needed << "px, current remainingWidth=" << remainingWidth;

                // 倒序挤出末端条目直到空间足够
                for (int i = entries.size() - 1; i >= 0; --i) {
                    if (remainingWidth >= needed)
                        break;
                    if (isVisible[i] && i != selectedIdx) {
                        isVisible[i] = false;
                        remainingWidth += entries[i].width;
                        qDebug() << "[NavTree] computeOverflow: evicted tail item"
                            << (entries[i].node ? entries[i].node->routeKey() : "header")
                            << "width=" << entries[i].width << "new remainingWidth=" << remainingWidth;
                    }
                }

                if (remainingWidth >= needed) {
                    isVisible[selectedIdx] = true;
                    remainingWidth -= needed;
                    qDebug() << "[NavTree] computeOverflow: active selected root pinned to tail successfully!";
                }
                else {
                    qDebug() << "[NavTree] computeOverflow: cannot even fit active selected root alone!";
                }
            }

            // 目标 2（次级粘性保留）：非当前选中的历史粘性项（m_pinnedCategoryKey）。
            // 当用户切换到栏内自然可见项时，历史粘性项在未缩放窗口的前提下继续保留在栏内。
            int pinIdx = -1;
            if (!m_pinnedCategoryKey.isEmpty()) {
                for (int i = 0; i < entries.size(); ++i) {
                    if (entries[i].node && entries[i].node->routeKey() == m_pinnedCategoryKey) {
                        pinIdx = i;
                        break;
                    }
                }
            }

            if (pinIdx >= 0 && !isVisible[pinIdx] && pinIdx != selectedIdx) {
                const int needed = entries[pinIdx].width;
                qDebug() << "[NavTree] computeOverflow: secondary sticky pinned item" << m_pinnedCategoryKey
                    << "is in overflow, needs" << needed << "px, current remainingWidth=" << remainingWidth;

                // 倒序挤出末端条目（避开 active 选中项和 sticky 项自身）直到空间足够
                for (int i = entries.size() - 1; i >= 0; --i) {
                    if (remainingWidth >= needed)
                        break;
                    if (isVisible[i] && i != pinIdx && i != selectedIdx) {
                        isVisible[i] = false;
                        remainingWidth += entries[i].width;
                        qDebug() << "[NavTree] computeOverflow: evicted tail item"
                            << (entries[i].node ? entries[i].node->routeKey() : "header")
                            << "width=" << entries[i].width << "new remainingWidth=" << remainingWidth;
                    }
                }

                if (remainingWidth >= needed) {
                    isVisible[pinIdx] = true;
                    remainingWidth -= needed;
                    qDebug() << "[NavTree] computeOverflow: secondary sticky item placed at tail successfully!";
                }
                else {
                    qDebug() << "[NavTree] computeOverflow: cannot fit secondary sticky item, unpinning!";
                    m_pinnedCategoryKey.clear();
                }
            }

            // 落地：更新各 widget 可见性，记录进 overflow 的条目
            for (int i = 0; i < entries.size(); ++i) {
                if (entries[i].widget->isVisible() != isVisible[i]) {
                    layoutChanged = true;
                    qDebug() << "[NavTree] computeOverflow: visibility changed for"
                        << (entries[i].node ? entries[i].node->routeKey() : "header")
                        << "from" << entries[i].widget->isVisible() << "to" << isVisible[i];
                }
                entries[i].widget->setVisible(isVisible[i]);
                if (!isVisible[i]) {
                    if (entries[i].node)   m_overflowNodes.append(entries[i].node);
                    if (entries[i].header) m_overflowHeaders.append(entries[i].header);
                }
            }

            if (m_overflowButton) {
                if (!m_overflowButton->isVisible()) {
                    layoutChanged = true;
                    qDebug() << "[NavTree] computeOverflow: overflowButton shown";
                }
                m_overflowButton->show();
                // 若选中项当前在菜单里，overflow 按钮呈 selected 态。
                const bool overflowSelected = (selectedIdx >= 0 && !isVisible[selectedIdx]);
                m_overflowButton->setSelected(overflowSelected);
                qDebug() << "[NavTree] computeOverflow: overflowButton selectedState=" << overflowSelected;
            }
        }

        if (layoutChanged) {
            qDebug() << "[NavTree] computeOverflow: layoutChanged detected, forcing animated=false";
            animated = false;
        }

        m_lastTopAvailableWidth = availableWidth;
        qDebug() << "[NavTree] computeOverflow END: overflowNodesCount=" << m_overflowNodes.size()
            << "overflowHeadersCount=" << m_overflowHeaders.size()
            << "finalAnimated=" << animated;
        m_updatingOverflow = false;
        return layoutChanged;
    }

    bool NavigationTreeWidget::isSelectedUnder(NavigationTreeWidget* node) const
    {
        if (!node)
            return false;
        return node->routeKey() == m_currentRouteKey
            || isAncestorOf(m_currentRouteKey, node->routeKey());
    }

    QVector<NavigationOverflowEntry> NavigationTreeWidget::overflowEntries() const
    {
        // O(n+m)：先建 widget→entry 哈希，再单次扫描布局，避免双层线性搜索
        QHash<QWidget*, NavigationOverflowEntry> lookup;
        lookup.reserve(m_overflowNodes.size() + m_overflowHeaders.size());
        for (auto* node : m_overflowNodes)
            if (node->itemWidget())
                lookup.insert(node->itemWidget(), { node, nullptr });
        for (auto* h : m_overflowHeaders)
            lookup.insert(h, { nullptr, h });

        QVector<NavigationOverflowEntry> result;
        result.reserve(lookup.size());
        for (int i = 0; i < m_mainLayout->count(); ++i) {
            QWidget* w = m_mainLayout->itemAt(i) ? m_mainLayout->itemAt(i)->widget() : nullptr;
            if (!w || w == m_overflowButton) continue;
            auto it = lookup.find(w);
            if (it != lookup.end())
                result.append(*it);
        }
        return result;
    }

} // namespace ui::navigation
