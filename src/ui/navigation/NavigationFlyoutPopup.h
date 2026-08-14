#pragma once

#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QVector>

#include <FluentQt/Foundation.h>

#include "components/foundation/overlay/OverlayShadow.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "design/Elevation.h"

#include "ui/navigation/NavigationMetrics.h"
#include "ui/navigation/NavigationWidget.h"
#include "ui/navigation/NavigationTreeItem.h"
#include "ui/navigation/NavigationIndicator.h"
#include "ui/navigation/NavigationTreeWidgetBase.h"

class QBoxLayout;
class QPropertyAnimation;
class QParallelAnimationGroup;
class QHideEvent;
class QPaintEvent;

namespace ui::navigation {

class NavigationTreeWidget;

/**
 * @brief 导航菜单专用 Flyout
 *
 * 以独立 Qt::Tool 顶层窗口承载导航 flyout，可越过主窗口边界渲染。作为
 * NavigationTreeWidgetBase 的派生，内部以克隆子树渲染被激活分类的子项（可折叠、带选中态），
 * 叠加 Medium 阴影、滑入动画、屏幕边界 clamp 与上下翻转，以及锚点跟随与 light dismiss。
 */
class NavigationFlyoutPopup : public NavigationTreeWidgetBase {
    Q_OBJECT
public:
    /**
     * @brief 构造独立 flyout 窗口。
     * @param rootTree 原 panel 的导航树（单一权威状态源，切页/选中态统一由其驱动）
     * @param host 宿主（NavigationPanel），用于主题继承 + 锚点跟随 + eventFilter 挂载
     */
    explicit NavigationFlyoutPopup(NavigationTreeWidget* rootTree, QWidget* host);
    ~NavigationFlyoutPopup() override;

    /**
     * @brief 以克隆方式重建指定分类的子树并填充 flyout。
     * @param categoryKey 被激活分类的路由键（其子项及递归子项将被克隆）
     */
    void rebuildSubtree(const QString& categoryKey);

    /**
     * @brief 以克隆方式重建给定溢出条目集合（供 overflow flyout 使用）。
     * @param entries 溢出条目列表（节点连同其递归子项将被克隆，分区标题按序渲染）
     */
    void rebuildSubtreeFromEntries(const QVector<NavigationOverflowEntry>& entries);

    /**
     * @brief 触发浮层内选中项跨窗口飞跃入场（Cross Window Portal In）动画。
     * @param mappedStartRect 映射到浮层坐标系后的顶栏指示条起始位置。
     * @param targetRect 浮层内部选中项的目标位置。
     */
    void playSelectedItemCrossPortal(NavigationTreeItem* selectedItem, const QRectF& mappedStartRect, const QRectF& targetRect);

    /// 浮层内克隆出的选中项（供调度层判定是否需要触发 Portal 动效）。
    NavigationTreeItem* selectedItem() const { return m_selectedItem; }

    /**
     * @brief 定位并显示 flyout。
     * @param globalCardTopLeft 卡片左上角的屏幕坐标（调用方用 panel 局部坐标 mapToGlobal 得到）
     * @param slideInOffset 滑入方向与距离（屏幕坐标，向下/右为正）；QPoint(0,0) 关闭滑入直接显示
     */
    void openAt(const QPoint& globalCardTopLeft, const QPoint& slideInOffset = QPoint(0, 0));

    /**
     * @brief 绑定强关联的锚点按钮（Target 架构），用于全局命中测试与点击防抖。
     */
    void setAnchorWidget(QWidget* anchor) { m_anchorWidget = anchor; }

    /**
     * @brief 设置点击外部时是否吞噬事件（默认为 true）。
     */
    bool lightDismissConsumesPress() const { return m_lightDismissConsumesPress; }
    void setLightDismissConsumesPress(bool consume) { m_lightDismissConsumesPress = consume; }

    /**
     * @brief 穿透白名单。
     * 点击这些控件时，弹窗关闭且事件会直接穿透，不会被吞噬。
     */
    void addLightDismissPassthrough(QWidget* widget) { m_lightDismissPassthrough.append(widget); }
    void removeLightDismissPassthrough(QWidget* widget) { m_lightDismissPassthrough.removeOne(widget); }
    void clearLightDismissPassthrough() { m_lightDismissPassthrough.clear(); }

signals:
    /// flyout 展开动画完成（完全显示）后发出，供调用方延迟播放组合动效。
    void opened();
    /// flyout 关闭（无论点击外部/Esc/主动 close）后发出，供调用方清理激活态。
    void closed();
    /// flyout 内点击了可选中（selectable）的分类项主体，供调用方触发跨窗口 Portal 动效。
    void selectableCategoryClicked(NavigationTreeItem* item);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool event(QEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void onThemeUpdated() override { update(); }

private:
    /// 递归克隆源节点及其子树，挂载到 flyout 内部布局。
    void cloneNode(NavigationTreeWidget* srcNode, NavigationTreeWidget* parentClone, int depth);
    /// 根据克隆行内容锁定外宽并调整高度（宽度自适应最长行文本）。
    void finalizeSize();

    QWidget* m_host = nullptr;
    /// flyout 主布局：承载克隆顶级 item 与分类子容器。
    QBoxLayout* m_contentLayout = nullptr;

    /// 卡片终点：屏幕坐标，不含阴影 margin
    QPoint m_globalCardPos;
    /// 本次滑入偏移：用于动画起点计算
    QPoint m_slideInOffset;
    /// 宿主窗口原点 → 卡片左上角的偏移：宿主移动时跟随重定位
    QPoint m_hostAnchorOffset;
    /// 是否向上翻转显示：滑入方向反转 + 跟随时保持翻转
    bool m_flippedUp = false;

    /// 精准绑定的触发锚点控件
    QPointer<QWidget> m_anchorWidget;

    /// 点击外部时是否吞噬事件：默认行为，吸收点击
    bool m_lightDismissConsumesPress = true;

    /// 穿透白名单：命中这些控件时关闭但放行事件
    QVector<QPointer<QWidget>> m_lightDismissPassthrough;

    /// 现代风格的入场动画组合 (Slide + Fade)
    QParallelAnimationGroup* m_animGroup = nullptr;
    QPropertyAnimation* m_slideAnim = nullptr;
    QPropertyAnimation* m_fadeAnim = nullptr;

    /// 浮层内部的指示条动效载体（Portal In 动画用）。
    NavigationIndicator* m_flyoutIndicator = nullptr;

    /// 克隆出的当前选中 item。
    NavigationTreeItem* m_selectedItem = nullptr;
};

} // namespace ui::navigation
