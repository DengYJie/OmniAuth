#include "ui/navigation/NavigationFlyout.h"

#include <functional>

#include <FluentQt/Design.h>

#include "components/foundation/overlay/OverlayShadow.h"

#include <QApplication>
#include <QDebug>
#include <QEasingCurve>
#include <QEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QScreen>
#include <QTimer>
#include <QWheelEvent>
#include <QBoxLayout>
#include <QVBoxLayout>

#include <QHash>

#include "ui/navigation/NavigationPushButton.h"
#include "ui/navigation/NavigationSectionHeader.h"
#include "ui/navigation/NavigationTreeWidget.h"
#include "ui/navigation/NavigationWidget.h"

namespace ui::navigation {

namespace {

QPixmap generateGrainTile(qreal devicePixelRatio)
{
    // 缓存在全局，避免每次绘制重新生成噪点
    static QHash<int, QPixmap> s_grainCache;
    const int dprInt = qMax(1, qRound(devicePixelRatio * 100));

    if (s_grainCache.contains(dprInt)) {
        return s_grainCache.value(dprInt);
    }

    const int size = qMax(1, qRound(96 * devicePixelRatio));
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);

    quint32 state = 0x8f3d7a21U ^ static_cast<quint32>(dprInt);
    for (int y = 0; y < size; ++y) {
        auto* scanLine = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < size; ++x) {
            state ^= state << 13U;
            state ^= state >> 17U;
            state ^= state << 5U;
            const int value = static_cast<int>((state >> 24U) & 0xffU);
            scanLine[x] = qRgb(value, value, value);
        }
    }

    QPixmap texture = QPixmap::fromImage(image);
    texture.setDevicePixelRatio(devicePixelRatio);
    s_grainCache.insert(dprInt, texture);
    return texture;
}

void paintFlyoutGrain(QPainter& painter, const QRect& rect, qreal opacity = 0.030)
{
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.setOpacity(opacity);

    qreal dpr = 1.0;
    if (painter.device()) {
        dpr = painter.device()->devicePixelRatioF();
    }

    painter.drawTiledPixmap(rect, generateGrainTile(dpr), QPoint(0, 0));
    painter.restore();
}

} // namespace

    NavigationFlyout::NavigationFlyout(NavigationTreeWidget* rootTree, QWidget* host)
        : NavigationTreeWidgetBase(rootTree, nullptr)
        , m_host(host)
    {
        // 独立顶层窗口透明背景支持；WindowDoesNotAcceptFocus 防止点击内部控件时抢焦点
        // 导致主窗口收到 WindowDeactivate 而误关闭 flyout（WinUI3 flyout 同规范）
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint
                       | Qt::WindowDoesNotAcceptFocus);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_ShowWithoutActivating, true);
        setAutoFillBackground(false);

        // 同步宿主主题覆盖，使 popup 与 panel 主题保持一致
        ::fluent::overlay::syncInheritedThemeOverride(this, host);

        // 四周留出阴影缓冲，使子部件严格限制在卡片内部
        m_contentLayout = new QBoxLayout(QBoxLayout::TopToBottom, this);
        m_contentLayout->setContentsMargins(kShadowMargin, kShadowMargin + themeSpacing().xSmall,
            kShadowMargin, kShadowMargin + themeSpacing().xSmall);
        m_contentLayout->setSpacing(0);

        if (m_host)
            m_host->installEventFilter(this);

        // 浮层内部指示条载体：配置为垂直模式，用于 Portal In 动画
        m_flyoutIndicator = new NavigationIndicator(this);
        m_flyoutIndicator->setOrientation(Orientation::Vertical);
    }

    NavigationFlyout::~NavigationFlyout()
    {
        // 防止 host 析构后仍指向本对象触发悬垂崩溃
        if (m_host)
            m_host->removeEventFilter(this);
    }

    void NavigationFlyout::rebuildSubtree(const QString& categoryKey)
    {
        m_itemIndex.clear();
        NavigationTreeWidget* catNode = root()->nodeFor(categoryKey);
        if (!catNode)
            return;
        for (NavigationTreeWidget* child : catNode->children()) {
            if (child && child->itemWidget())
                cloneNode(child, nullptr, 0);
        }
        finalizeSize();
    }

    void NavigationFlyout::rebuildSubtreeFromEntries(const QVector<NavigationOverflowEntry>& entries)
    {
        m_itemIndex.clear();
        // header 与节点按序插入以保留原始分区顺序；header 不参与宽度计算
        for (const auto& entry : entries) {
            if (entry.header) {
                auto* header = new NavigationSectionHeader(entry.header->text(), nullptr);
                header->setOrientation(Orientation::Vertical);
                header->setExpandProgress(1.0f);
                m_contentLayout->addWidget(header);
            }
            else if (entry.node && entry.node->itemWidget()) {
                cloneNode(entry.node, nullptr, 0);
            }
        }
        finalizeSize();
    }

    void NavigationFlyout::cloneNode(NavigationTreeWidget* srcNode, NavigationTreeWidget* parentClone, int depth)
    {
        // 克隆节点挂到 flyout 下（widget 父级），m_root 仍指向原树保证切页落到原树
        auto* node = new NavigationTreeWidget(root());
        node->setParent(this);
        node->m_routeKey = srcNode->routeKey();
        node->m_parentNode = parentClone;
        node->setInlineExpansion(true);

        // flyout 内折叠/展开分类时，仅同步主树源节点的展开状态标志（供下次克隆恢复），
        // 不调用 setExpanded：避免其 show 子容器到 m_mainLayout，容器显隐由导航布局统一管理
        connect(node, &NavigationTreeWidgetBase::expansionChanged, this,
            [this, root = root()](const QString& key, bool expanded) {
                if (auto* target = root->nodeFor(key))
                    target->m_isExpanded = expanded;
                emit expansionChanged(key, expanded);
            });

        const auto* srcItem = srcNode->item();
        auto* item = new NavigationTreeItem(
            srcNode->routeKey(),
            srcItem->iconGlyph(),
            srcItem->text(),
            srcItem->tooltipText(),
            srcNode->isCategory() ? NavigationTreeItem::Kind::Category : NavigationTreeItem::Kind::Leaf,
            depth,
            srcItem->isSelectable(),
            nullptr);
        item->setOrientation(Orientation::Vertical);
        item->setExpandProgress(1.0f);
        item->setIconSize(srcItem->iconSize());
        item->setTreeParent(node);
        node->m_itemWidget = item;
        m_itemIndex.insert(item->routeKey(), item);

        // 决策槽与源树一致：叶子切页后关闭 flyout，
        // 分类走内联展开（onItemClicked 内联分支），不关闭
        connect(item, &NavigationTreeItem::itemClicked, node,
            [node, this](const QString& key, bool chevronClicked) {
                node->onItemClicked(key, chevronClicked);
                if (!node->isCategory()) {
                    close();
                }
            });

        if (parentClone) {
            parentClone->m_children.append(node);
            if (!parentClone->m_childrenContainer) {
                auto* container = new QWidget(nullptr);
                container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                container->setAutoFillBackground(false);
                auto* childLayout = new QVBoxLayout(container);
                childLayout->setContentsMargins(0, 0, 0, 0);
                childLayout->setSpacing(0);
                parentClone->m_childrenContainer = container;
                parentClone->m_childrenLayout = childLayout;
                const int idx = m_contentLayout->indexOf(parentClone->m_itemWidget) + 1;
                m_contentLayout->insertWidget(idx, container);
                container->setVisible(parentClone->m_isExpanded);
            }
            parentClone->m_childrenLayout->addWidget(item);
        }
        else {
            m_contentLayout->addWidget(item);
            m_children.append(node);
        }

        // 选中态复制：节点是当前选中项或其祖先时高亮（选中权威仍在原树）
        const QString currentKey = root()->currentRouteKey();
        if (srcNode->routeKey() == currentKey) {
            item->setSelected(true);
        }
        else if (root()->isAncestorOf(currentKey, srcNode->routeKey())) {
            item->setSelected(true);
        }

        // 递归克隆子节点
        for (NavigationTreeWidget* child : srcNode->children())
            cloneNode(child, node, depth + 1);

        // 复制展开态：递归完成后子容器已就绪
        if (node->isCategory() && srcNode->m_isExpanded)
            node->setExpanded(true, false);
    }

    void NavigationFlyout::playSelectedItemCrossPortal(NavigationTreeItem* selectedItem, const QRectF& mappedStartRect, const QRectF& targetRect)
    {
        if (!m_flyoutIndicator || !selectedItem)
            return;

        if (NavigationWidget::isReducedMotion()) {
            return;
        }

        m_flyoutIndicator->show();
        m_flyoutIndicator->playCrossWindowPortal(mappedStartRect, targetRect);
    }

    NavigationTreeItem* NavigationFlyout::getVisualProxyFor(NavigationTreeItem* item) const
    {
        if (!item) return nullptr;
        NavigationTreeItem* clone = m_itemIndex.value(item->routeKey(), item);
        NavigationTreeWidget* node = clone ? clone->treeParent() : nullptr;
        if (!node) return clone;

        auto isReachable = [](const NavigationTreeWidget* n) -> bool {
            for (const NavigationTreeWidget* p = n->m_parentNode; p; p = p->m_parentNode) {
                if (p->isCategory() && !p->m_isExpanded) return false;
            }
            return true;
        };

        if (isReachable(node)) {
            return qobject_cast<NavigationTreeItem*>(node->itemWidget());
        }

        NavigationTreeWidget* proxy = node->m_parentNode;
        while (proxy && proxy->isCategory()) {
            if (isReachable(proxy)) {
                return qobject_cast<NavigationTreeItem*>(proxy->itemWidget());
            }
            proxy = proxy->m_parentNode;
        }

        if (proxy && proxy->itemWidget()) {
            return qobject_cast<NavigationTreeItem*>(proxy->itemWidget());
        }

        return clone;
    }

    QRectF NavigationFlyout::indicatorRectInHost(NavigationTreeItem* item) const
    {
        if (!item || !m_flyoutIndicator) return QRectF();

        NavigationTreeItem* visualItem = getVisualProxyFor(item);
        if (!visualItem) return QRectF();

        const QWidget* host = m_flyoutIndicator->parentWidget();
        const QPoint itemOriginInHost = visualItem->mapTo(host ? host : this, QPoint(0, 0));
        return QRectF(QPointF(itemOriginInHost) + visualItem->indicatorRect().topLeft(), visualItem->indicatorRect().size());
    }

    void NavigationFlyout::playInternalFlight(const QRectF& targetRect)
    {
        if (!m_flyoutIndicator) return;
        m_flyoutIndicator->show();
        m_flyoutIndicator->activateAt(targetRect, true);
    }

    void NavigationFlyout::onIndicatorOwnerChanged(NavigationTreeItem* item, bool isOwner)
    {
        // 浮层正在关闭时，忽略任何所有权变更，防止在淡出时突然重新绘制出指示条
        if (m_isClosing) return;

        if (!item) return;
        NavigationTreeItem* clone = m_itemIndex.value(item->routeKey());
        if (!clone) {
            return;
        }

        if (isOwner) {
            NavigationTreeItem* visualItem = getVisualProxyFor(clone);
            if (visualItem) {
                visualItem->setShowIndicator(true);
            }
        }
        else {
            clone->setShowIndicator(false);
            NavigationTreeWidget* node = clone->treeParent();
            while (node && node->m_parentNode) {
                node = node->m_parentNode;
                if (node->m_itemWidget) {
                    if (auto* proxyItem = qobject_cast<NavigationTreeItem*>(node->itemWidget())) {
                        proxyItem->setShowIndicator(false);
                    }
                }
            }
        }
    }

    void NavigationFlyout::finalizeSize()
    {
        const QFontMetrics fm(themeFont(Typography::FontRole::Body).toQFont());
        const int leftPadding = kTextLeftOffset;
        const int rightPadding = leftPadding + themeSpacing().medium;

        int maxContentW = 0;
        std::function<void(const NavigationTreeWidget*)> visit = [&](const NavigationTreeWidget* n) {
            if (!n || !n->itemWidget())
                return;
            const auto* btn = qobject_cast<const NavigationPushButton*>(n->itemWidget());
            const int indent = n->itemWidget()->nodeDepth() * themeSpacing().large;
            const int w = leftPadding + indent
                + (btn ? fm.horizontalAdvance(btn->text()) : 0)
                + rightPadding;
            maxContentW = qMax(maxContentW, w);
            for (auto* c : n->children())
                visit(c);
        };
        for (auto* top : m_children)
            visit(top);

        maxContentW = qMax(maxContentW, kCompactFlyoutRowWidth);

        // 外宽需含阴影 margin，否则阴影被裁剪
        const int outerW = maxContentW + 2 * kShadowMargin;
        setMinimumWidth(outerW);
        setMaximumWidth(outerW);
        if (m_contentLayout)
            m_contentLayout->activate();
        adjustSize();
    }

    void NavigationFlyout::setIsOpen(bool open)
    {
        if (m_isOpen == open) return;
        m_isOpen = open;
        emit isOpenChanged(open);
        if (open) {
            this->open();
        } else {
            close();
        }
    }

    void NavigationFlyout::showAt(QWidget* anchor)
    {
        setAnchor(anchor);
        if (!anchor || !m_host) return;

        const QRect anchorRect(anchor->mapToGlobal(QPoint(0, 0)), anchor->size());
        const QSize cardSize = ::fluent::overlay::visibleCardRect(rect(), kShadowMargin).size();
        
        QPoint globalCardTopLeft;
        QPoint slideInOffset;

        if (m_placement == Placement::Right) {
            // Compact 模式: 在 anchor 右侧弹出，Y方向居中
            const int panelRightX = m_host->mapToGlobal(QPoint(m_host->width(), 0)).x();
            const int anchorCenterY = anchorRect.y() + anchorRect.height() / 2;
            const int yPos = anchorCenterY - cardSize.height() / 2;
            globalCardTopLeft = QPoint(panelRightX + m_anchorOffset, yPos);
            slideInOffset = QPoint(8, 0); // kCompactFlyoutSlideOffset = 8
        } else {
            // Top 模式 (Bottom / BottomRight): 在 anchor 下方弹出
            const int panelBottomY = m_host->mapToGlobal(QPoint(0, m_host->height())).y();
            
            int flyoutX = 0;
            if (m_placement == Placement::BottomRight) {
                // 右对齐：Flyout 右边缘与 Anchor 右边缘对齐
                const int anchorRightX = anchorRect.right();
                flyoutX = anchorRightX - cardSize.width() + 1; // +1 to compensate rect().right() logic
            } else {
                // 默认居中：X方向居中
                const int anchorCenterX = anchorRect.x() + anchorRect.width() / 2;
                flyoutX = anchorCenterX - cardSize.width() / 2;
            }
            
            globalCardTopLeft = QPoint(flyoutX, panelBottomY + m_anchorOffset);
            slideInOffset = QPoint(0, 16);
        }

        openAt(globalCardTopLeft, slideInOffset);
    }

    void NavigationFlyout::open()
    {
        if (m_anchorWidget) {
            showAt(m_anchorWidget);
        } else {
            openAt(m_globalCardPos, m_slideInOffset);
        }
    }

    void NavigationFlyout::openAt(const QPoint& globalCardTopLeft, const QPoint& slideInOffset)
    {
        if (!m_isOpen) {
            m_isOpen = true;
            emit isOpenChanged(true);
            emit aboutToShow();
        }

        m_globalCardPos = globalCardTopLeft;
        m_slideInOffset = slideInOffset;

        const QSize cardSize = ::fluent::overlay::visibleCardRect(rect(), kShadowMargin).size();
        QPoint cardPos = m_globalCardPos;

        // 屏幕边界约束：下方不足则向上翻转，左右 clamp 到可用区
        QScreen* screen = QGuiApplication::screenAt(m_globalCardPos);
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        m_flippedUp = false;
        if (screen) {
            const QRect avail = screen->availableGeometry();
            if (cardPos.y() + cardSize.height() > avail.bottom()) {
                cardPos.setY(avail.bottom() - cardSize.height() + 1);
                if (cardPos.y() < avail.top())
                    cardPos.setY(avail.top());
                m_flippedUp = true;
            }
            cardPos = ::fluent::overlay::clampCardTopLeft(cardPos, cardSize, avail, 0);
        }

        // 记录卡片相对宿主原点的偏移，供宿主移动时跟随重定位
        const QPoint outerTopLeft = cardPos - QPoint(kShadowMargin, kShadowMargin);
        m_hostAnchorOffset = cardPos - m_host->mapToGlobal(QPoint(0, 0));

        // 滑入方向反转：向上翻转后从下往上滑入，Y 分量取反
        const QPoint effectiveOffset = m_flippedUp
            ? QPoint(m_slideInOffset.x(), -m_slideInOffset.y())
            : m_slideInOffset;

        if (effectiveOffset.isNull() || NavigationWidget::isReducedMotion()) {
            move(outerTopLeft);
            setWindowOpacity(1.0);
            show();
            emit opened();
        }
        else {
            const QPoint endPos = outerTopLeft;
            const QPoint startPos = endPos - effectiveOffset;
            if (!m_animGroup) {
                m_animGroup = new QParallelAnimationGroup(this);

                // 位移 (Slide)
                m_slideAnim = new QPropertyAnimation(this, "pos", this);
                m_slideAnim->setEasingCurve(themeAnimation().decelerate);
                m_slideAnim->setDuration(themeAnimation().normal);

                // 渐显 (Fade)
                m_fadeAnim = new QPropertyAnimation(this, "windowOpacity", this);
                m_fadeAnim->setEasingCurve(QEasingCurve::Linear);
                m_fadeAnim->setDuration(themeAnimation().normal);

                m_animGroup->addAnimation(m_slideAnim);
                m_animGroup->addAnimation(m_fadeAnim);

                connect(m_animGroup, &QParallelAnimationGroup::finished, this, [this]() {
                    emit opened();
                });
            }
            m_animGroup->stop();
            move(startPos);
            setWindowOpacity(0.0);
            show();

            m_slideAnim->setStartValue(startPos);
            m_slideAnim->setEndValue(endPos);

            m_fadeAnim->setStartValue(0.0);
            m_fadeAnim->setEndValue(1.0);

            m_animGroup->start();
        }

        // 全局拦截外部点击，接管 Light Dismiss 关闭与事件穿透决策
        qApp->installEventFilter(this);

        raise();
    }

    void NavigationFlyout::close()
    {
        if (m_isClosing) return;
        m_isClosing = true;

        emit aboutToHide();

        if (m_isOpen) {
            m_isOpen = false;
            emit isOpenChanged(false);
        }

        if (!m_exitAnimationEnabled || NavigationWidget::isReducedMotion()) {
            hide();
            emit closed();
            return;
        }

        if (m_animGroup) {
            m_animGroup->stop(); // stop any ongoing entry animation
        }

        QParallelAnimationGroup* closeGroup = new QParallelAnimationGroup(this);
        const auto& animTokens = themeAnimation();

        QPropertyAnimation* fadeOut = new QPropertyAnimation(this, "windowOpacity", closeGroup);
        fadeOut->setStartValue(windowOpacity());
        fadeOut->setEndValue(0.0);
        fadeOut->setDuration(animTokens.fast);
        fadeOut->setEasingCurve(animTokens.exit);

        QPropertyAnimation* slideOut = new QPropertyAnimation(this, "pos", closeGroup);
        slideOut->setStartValue(pos());

        // 根据入场位移计算退场位移（进退对称原则）
        QPoint effectiveOffset = m_flippedUp
            ? QPoint(m_slideInOffset.x(), -m_slideInOffset.y())
            : m_slideInOffset;

        QPoint exitOffset(0, -8); // 默认退场上浮
        if (effectiveOffset.y() > 0) exitOffset = QPoint(0, -8);      // 从上向下滑入 -> 向上回缩
        else if (effectiveOffset.y() < 0) exitOffset = QPoint(0, 8);  // 从下向上滑入 -> 向下回缩
        else if (effectiveOffset.x() > 0) exitOffset = QPoint(-8, 0); // 从左向右滑入 -> 向左回缩
        else if (effectiveOffset.x() < 0) exitOffset = QPoint(8, 0);  // 从右向左滑入 -> 向右回缩

        slideOut->setEndValue(pos() + exitOffset);
        slideOut->setDuration(animTokens.fast);
        slideOut->setEasingCurve(animTokens.exit);

        closeGroup->addAnimation(fadeOut);
        closeGroup->addAnimation(slideOut);

        connect(closeGroup, &QParallelAnimationGroup::finished, this, [this, closeGroup]() {
            hide();
            emit closed();
            closeGroup->deleteLater();
        });

        closeGroup->start();
    }

    bool NavigationFlyout::eventFilter(QObject* watched, QEvent* event)
    {
        QWidget* topLevel = m_host ? m_host->window() : nullptr;

        // 宏主、锁点或顶级窗口发生变化时联动
        if (watched == m_host || watched == m_anchorWidget || (topLevel && watched == topLevel)) {
            switch (event->type()) {
            case QEvent::Move:
                if (isVisible()) {
                    // 顶级窗口或宿主移动时，跟随重定位
                    const QPoint hostTopLeft = m_host->mapToGlobal(QPoint(0, 0));
                    move(hostTopLeft + m_hostAnchorOffset - QPoint(kShadowMargin, kShadowMargin));
                }
                break;
            case QEvent::Resize:
                if (isVisible()) {
                    // 顶级窗口或宿主尺寸改变时，内部排版可能巨变，安全起见关闭 Flyout
                    close();
                }
                break;
            case QEvent::WindowDeactivate:
                // 仅当顶级窗口失焦时才关闭（避免内部子控件失焦导致误杀）
                if (watched == topLevel) {
                    close();
                }
                break;
            case QEvent::Hide:
            case QEvent::Destroy:
                close();
                break;
            default:
                break;
            }
        }

        if (isVisible()) {
            // 全局失焦（点击其他应用程序）
            if (event->type() == QEvent::ApplicationDeactivate) {
                close();
            }
            // ESC 键关闭
            else if (event->type() == QEvent::KeyPress) {
                auto* ke = static_cast<QKeyEvent*>(event);
                if (ke->key() == Qt::Key_Escape && (m_closePolicy & CloseOnEscape)) {
                    close();
                    return true;
                }
            }
        }

        // 仅在弹窗外执行命中测试；命中后的吞噬/放行决定点击是否穿透
        if (isVisible() && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::Wheel)) {
            QPoint globalPos;
            if (event->type() == QEvent::MouseButtonPress) {
                globalPos = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
            }
            else {
                globalPos = static_cast<QWheelEvent*>(event)->globalPosition().toPoint();
            }

            const QRect visibleCard = ::fluent::overlay::visibleCardRect(rect(), kShadowMargin);
            const QPoint localPos = mapFromGlobal(globalPos);
            const bool insideCard = visibleCard.contains(localPos);

            if (!insideCard) {
                if (event->type() == QEvent::Wheel) {
                    close();
                    // 滚轮事件一律放行（让底下能够正常滚动）
                    return false;
                }

                // 层级判定优先，几何判定兜底，增强命中健壮性
                QWidget* hitWidget = QApplication::widgetAt(globalPos);
                auto isHit = [&](QWidget* target) {
                    if (!target) return false;
                    if (hitWidget && (hitWidget == target || target->isAncestorOf(hitWidget)))
                        return true;
                    return target->rect().contains(target->mapFromGlobal(globalPos));
                    };

                // 1. 目标排除：命中锚点则关闭并吞噬，避免穿透后重新激活
                if (isHit(m_anchorWidget)) {
                    close();
                    return true;
                }

                // 2. 穿透白名单：关闭但放行事件，让目标控件正常响应
                for (const auto& ptWidget : std::as_const(m_lightDismissPassthrough)) {
                    if (isHit(ptWidget)) {
                        // 赶在 Qt 原生 Popup 拦截前主动关闭，破坏其拦截条件
                        close();
                        // 【放行】，让该控件响应该点击事件
                        return false;
                    }
                }

                // 3. 默认 LightDismiss：按策略决定吞噬（默认吸收）或放行
                if (m_closePolicy & CloseOnPressOutside) {
                    close();
                    return m_lightDismissConsumesPress;
                }
            }
        }

        return QWidget::eventFilter(watched, event);
    }

    bool NavigationFlyout::event(QEvent* e)
    {
        // 子控件（克隆节点的 children container）每次改变 fixedHeight 都会向父 widget
        // 发 LayoutRequest。在此捕获并 adjustSize()，使 flyout 高度跟随动画实时伸缩
        if (e->type() == QEvent::LayoutRequest)
            adjustSize();
        return NavigationTreeWidgetBase::event(e);
    }

    void NavigationFlyout::hideEvent(QHideEvent* event)
    {
        qApp->removeEventFilter(this);
        QWidget::hideEvent(event);
        if (m_isOpen) {
            // 如果外部直接调用 hide() 或系统强制隐藏，补发 aboutToHide 以保证信号成对闭环
            emit aboutToHide();
            m_isOpen = false;
            emit isOpenChanged(false);
        }
        emit closed();
    }

    void NavigationFlyout::keyPressEvent(QKeyEvent* event)
    {
        if (event->key() == Qt::Key_Escape && (m_closePolicy & CloseOnEscape)) {
            close();
            event->accept();
            return;
        }
        NavigationTreeWidgetBase::keyPressEvent(event);
    }

    void NavigationFlyout::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)
            QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        // 独立顶层窗口需先用 CompositionMode_Source 将整个窗口透明清屏（与 FluentMenu 一致），
        // 确保 alpha 通道置 0，避免 DWM 渲染脏数据或不透明黑底
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

        const QRect cardRect = ::fluent::overlay::visibleCardRect(rect(), kShadowMargin);
        const int r = themeRadius().overlay;

        // Medium 阴影
        ::fluent::overlay::paintLayeredShadow(painter, cardRect, r,
            themeShadow(Elevation::Medium));

        const auto& colors = themeColorsRef();

        // 设置圆角裁剪区域，确保所有 4 层效果都不会溢出卡片边缘
        QPainterPath clipPath;
        clipPath.addRoundedRect(QRectF(cardRect).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
        painter.save();
        painter.setClipPath(clipPath);

        const bool isDark = effectiveTheme() == Dark;
        const auto acrylicToken = ::Material::Acrylic::get(isDark);

        // Layer 1: Base Background (底层)
        // 使用当前主体的背景色作为亚克力的基础（无需截图）
        painter.fillRect(cardRect, colors.bgLayer);

        // Layer 2: Exclusion Blend / Luminosity (排他/明度混合层)
        // 降低底层对比度，保证文字的绝对可读性。直接从主题系统拉取 Luminosity 透明度
        painter.setCompositionMode(QPainter::CompositionMode_Exclusion);
        QColor luminosityColor = Qt::white;
        // 因为是实色底，降低原明度参数的权重，防止过曝/过暗
        luminosityColor.setAlphaF(acrylicToken.luminosityOpacity * 0.25);
        painter.fillRect(cardRect, luminosityColor);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

        // Layer 3: Color / Tint Overlay (着色层)
        // 赋予亚克力具体的主题色调，直接从 Fluent 设计系统拉取 Tint 颜色
        QColor tintColor = acrylicToken.tintColor;
        // In-App Flyout 专属高覆盖率 (约80%)，保证悬浮层可读性
        tintColor.setAlphaF(0.80);
        painter.fillRect(cardRect, tintColor);

        // Layer 4: Noise / Grain (噪点层)
        // 防止色带断层，提供磨砂玻璃特有的物理质感
        paintFlyoutGrain(painter, cardRect, isDark ? 0.04 : 0.03);

        painter.restore();

        // 绘制 3. 1px 外边框
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(colors.strokeSurface, 1.0));
        painter.drawRoundedRect(QRectF(cardRect).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
    }

} // namespace ui::navigation
