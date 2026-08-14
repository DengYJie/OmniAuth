#include "ui/navigation/NavigationFlyoutPopup.h"

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

    NavigationFlyoutPopup::NavigationFlyoutPopup(NavigationTreeWidget* rootTree, QWidget* host)
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
        m_flyoutIndicator->setNavigationPosition(NavigationPosition::Left);
    }

    NavigationFlyoutPopup::~NavigationFlyoutPopup()
    {
        // 防止 host 析构后仍指向本对象触发悬垂崩溃
        if (m_host)
            m_host->removeEventFilter(this);
    }

    void NavigationFlyoutPopup::rebuildSubtree(const QString& categoryKey)
    {
        NavigationTreeWidget* catNode = root()->nodeFor(categoryKey);
        if (!catNode)
            return;
        for (NavigationTreeWidget* child : catNode->children()) {
            if (child && child->itemWidget())
                cloneNode(child, nullptr, 0);
        }
        finalizeSize();
    }

    void NavigationFlyoutPopup::rebuildSubtreeFromEntries(const QVector<NavigationOverflowEntry>& entries)
    {
        // header 与节点按序插入以保留原始分区顺序；header 不参与宽度计算
        for (const auto& entry : entries) {
            if (entry.header) {
                auto* header = new NavigationSectionHeader(entry.header->text(), nullptr);
                header->setOrientation(NavigationOrientation::Vertical);
                header->setExpandProgress(1.0f);
                m_contentLayout->addWidget(header);
            }
            else if (entry.node && entry.node->itemWidget()) {
                cloneNode(entry.node, nullptr, 0);
            }
        }
        finalizeSize();
    }

    void NavigationFlyoutPopup::cloneNode(NavigationTreeWidget* srcNode, NavigationTreeWidget* parentClone, int depth)
    {
        // 克隆节点挂到 flyout 下（widget 父级），m_root 仍指向原树保证切页落到原树。
        auto* node = new NavigationTreeWidget(root());
        node->setParent(this);
        node->m_routeKey = srcNode->routeKey();
        node->m_parentNode = parentClone;
        node->setInlineExpansion(true);

        // flyout 内折叠/展开分类时，仅同步主树源节点的展开状态标志（供下次克隆恢复），
        // 不调用 setExpanded：避免其 show 子容器到 m_mainLayout，容器显隐由导航布局统一管理。
        connect(node, &NavigationTreeWidgetBase::expansionChanged, this,
            [root = root()](const QString& key, bool expanded) {
                if (auto* target = root->nodeFor(key))
                    target->m_isExpanded = expanded;
            });

        const auto* srcItem = srcNode->item();
        auto* item = new NavigationTreeItem(
            srcNode->routeKey(),
            srcItem->iconGlyph(),
            srcItem->text(),
            srcNode->isCategory() ? NavigationTreeItem::Kind::Category : NavigationTreeItem::Kind::Leaf,
            depth,
            srcItem->isSelectable(),
            nullptr);
        item->setOrientation(NavigationOrientation::Vertical);
        item->setExpandProgress(1.0f);
        item->setIconSize(srcItem->iconSize());
        item->setTreeParent(node);
        node->m_itemWidget = item;

        // 决策槽与源树一致：itemClicked 收敛到 onItemClicked；叶子切页后关闭 flyout，
        // 分类走内联展开（onItemClicked 内联分支），不关闭。
        connect(item, &NavigationTreeItem::itemClicked, node,
            [node, item, this](const QString& key, bool chevronClicked) {
                node->onItemClicked(key, chevronClicked);
                if (!node->isCategory()) {
                    close();
                } else if (!chevronClicked && item->isSelectable()) {
                    // 点击 selectable 分类项主体：更新 flyout 内选中项并通知调度层触发 Portal 动效
                    m_selectedItem = item;
                    emit selectableCategoryClicked(item);
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

        // 选中态复制：节点是当前选中项或其祖先时高亮（选中权威仍在原树）。
        const QString currentKey = root()->currentRouteKey();
        if (srcNode->routeKey() == currentKey) {
            item->setSelected(true);
            m_selectedItem = item;
        }
        else if (root()->isAncestorOf(currentKey, srcNode->routeKey())) {
            item->setSelected(true);
        }

        // 递归克隆子节点。
        for (NavigationTreeWidget* child : srcNode->children())
            cloneNode(child, node, depth + 1);

        // 复制展开态：递归完成后子容器已就绪。
        if (node->isCategory() && srcNode->m_isExpanded)
            node->setExpanded(true, false);
    }



    void NavigationFlyoutPopup::playSelectedItemCrossPortal(NavigationTreeItem* selectedItem, const QRectF& mappedStartRect, const QRectF& targetRect)
    {
        if (!m_flyoutIndicator || !selectedItem)
            return;

        if (NavigationWidget::isReducedMotion()) {
            selectedItem->setShowIndicator(true);
            return;
        }

        selectedItem->setShowIndicator(false);
        
        qDebug() << "[NavFlyout] playSelectedItemCrossPortal item:" << selectedItem->routeKey()
                 << "startRect:" << mappedStartRect << "targetRect:" << targetRect;

        // Portal 动画不发 flightFinished，此处按动画时长对齐收尾（恢复原生指示条）
        QTimer::singleShot(themeAnimation().normal, this, [selectedItem, this]() {
            qDebug() << "[NavFlyout] crossPortal flight finished, enabling showIndicator on" << selectedItem->routeKey();
            selectedItem->setShowIndicator(true);
            m_flyoutIndicator->hide();
        });

        m_flyoutIndicator->playCrossWindowPortal(mappedStartRect, targetRect, themeAnimation().normal);
    }

    void NavigationFlyoutPopup::finalizeSize()
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

    void NavigationFlyoutPopup::openAt(const QPoint& globalCardTopLeft, const QPoint& slideInOffset)
    {
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

        qDebug() << "[NavFlyout] openAt cardPos:" << cardPos << "outerTopLeft:" << outerTopLeft
                 << "effectiveOffset:" << effectiveOffset << "flippedUp:" << m_flippedUp;

        if (effectiveOffset.isNull()) {
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
                    qDebug() << "[NavFlyout] open animation finished -> emitting opened()";
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

    bool NavigationFlyoutPopup::eventFilter(QObject* watched, QEvent* event)
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
                if (ke->key() == Qt::Key_Escape) {
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
                if (m_lightDismissConsumesPress) {
                    close();
                    return true;
                }
                else {
                    close();
                    return false;
                }
            }
        }

        return QWidget::eventFilter(watched, event);
    }

    bool NavigationFlyoutPopup::event(QEvent* e)
    {
        // 子控件（克隆节点的 children container）每次改变 fixedHeight 都会向父 widget
        // 发 LayoutRequest。在此捕获并 adjustSize()，使 flyout 高度跟随动画实时伸缩。
        if (e->type() == QEvent::LayoutRequest)
            adjustSize();
        return NavigationTreeWidgetBase::event(e);
    }

    void NavigationFlyoutPopup::hideEvent(QHideEvent* event)
    {
        qApp->removeEventFilter(this);
        QWidget::hideEvent(event);
        emit closed();
    }

    void NavigationFlyoutPopup::paintEvent(QPaintEvent* event)
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
