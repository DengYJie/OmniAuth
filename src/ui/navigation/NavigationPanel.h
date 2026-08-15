#pragma once

#include <QPointer>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QWidget>

#include <QRect>

#include <FluentQt/Foundation.h>

#include "ui/navigation/NavigationFlyout.h"
#include "ui/navigation/NavigationFocusHost.h"
#include "ui/navigation/NavigationMetrics.h"

class QPaintEvent;
class QShowEvent;

namespace fluent::navigation {
    class NavigationView;
}

namespace ui::navigation {

    class NavigationIndicator;
    class NavigationTreeItem;
    class NavigationTreeWidget;
    class NavigationToolButton;
    class NavigationWidget;

    /**
     * @brief 导航整合面板
     *
     * 作为 NavigationView 的 mainChromeWidget，集中承载顶部操作区（返回与汉堡按钮）、主导航树（Main）、
     * 底部操作区（Footer）及用户卡片。通过 resizeEvent 实时监听物理宽度推算折叠进度并派发至子树，
     * 保证指示条与布局在 Auto 断点切换时的精确同步。
     */
    class NavigationPanel : public QWidget, public fluent::FluentElement,
        public INavigationFocusHost {
        Q_OBJECT
            Q_PROPERTY(bool backButtonVisible READ isBackButtonVisible WRITE setBackButtonVisible NOTIFY backButtonVisibleChanged)
            Q_PROPERTY(bool backEnabled READ isBackEnabled WRITE setBackEnabled NOTIFY backEnabledChanged)
            Q_PROPERTY(bool menuButtonVisible READ isMenuButtonVisible WRITE setMenuButtonVisible NOTIFY menuButtonVisibleChanged)
            Q_PROPERTY(bool compacted READ isCompacted WRITE setCompacted NOTIFY compactedChanged)

    public:
        explicit NavigationPanel(QWidget* parent = nullptr);
        ~NavigationPanel() override = default;

        /**
         * @brief 添加导航路由项
         * @param routeKey 唯一路由标识
         * @param iconGlyph Fluent 图标字形
         * @param text 显示文本
         * @param parentKey 父级分类标识，为空时作为顶级节点
         * @param position 条目归属区域（Top 主列表 / Bottom 页脚），默认 Top
         * @param selectable 是否可选中，默认 true
         */
        void addItem(const QString& routeKey, const QString& iconGlyph,
            const QString& text, const QString& parentKey = QString(),
            NavigationItemPosition position = NavigationItemPosition::Top,
            bool selectable = true,
            const QString& tooltip = QString());

        /**
         * @brief 添加分区标题
         * @param text 分区文本
         */
        void addSectionHeader(const QString& text);

        /**
         * @brief 添加自定义导航部件（透传到树）
         * @param widget 已构造的导航部件
         * @param position 条目归属区域（Top 主列表 / Bottom 页脚）
         */
        void addWidget(NavigationWidget* widget,
            NavigationItemPosition position = NavigationItemPosition::Top);

        /**
         * @brief 设置当前激活路由
         * @param routeKey 目标路由标识
         * @param animated 是否启用指示条平滑滑行动画
         */
        void setCurrentItem(const QString& routeKey);
        QString currentRouteKey() const;

        /**
         * @brief 编程式展开/收起指定分类（紧凑模式下忽略）。
         * @param routeKey 分类节点路由键
         * @param expanded 是否展开
         * @param animated 是否启用折叠高度过渡动画
         */
        void setCategoryExpanded(const QString& routeKey, bool expanded, bool animated = true);

        /**
         * @brief 设置底部固定用户卡片部件
         * @param cardWidget 用户信息卡片部件指针
         */
        void setUserInfoCard(NavigationWidget* cardWidget);

        NavigationIndicator* indicator() const { return m_indicator; }
        NavigationTreeWidget* tree() const { return m_tree; }
        NavigationToolButton* menuButton() const { return m_menuButton; }
        NavigationToolButton* backButton() const { return m_backButton; }
        NavigationToolButton* paneToggleButton() const { return m_menuButton; }

        /**
         * @brief 显式注入关联的 NavigationView，替代 parentWidget 隐式查找
         * @param view 关联的 NavigationView，可为 nullptr 以解除关联
         */
        void setNavigationView(fluent::navigation::NavigationView* view);

        bool isBackButtonVisible() const;
        void setBackButtonVisible(bool visible);

        bool isBackEnabled() const;
        void setBackEnabled(bool enabled);

        bool isMenuButtonVisible() const;
        void setMenuButtonVisible(bool visible);
        bool isPaneToggleButtonVisible() const { return isMenuButtonVisible(); }
        void setPaneToggleButtonVisible(bool visible) { setMenuButtonVisible(visible); }

        bool isCompacted() const { return m_isCompacted; }
        void setCompacted(bool compacted);

        void setOrientation(Orientation orientation);
        Orientation orientation() const { return m_orientation; }

        void togglePane();

        void moveFocusBy(int delta) override;

        QSize sizeHint() const override;

        void setExpandProgress(float progress);
        float expandProgress() const { return m_expandProgress; }


    signals:
        /// 切页的唯一驱动源，由树统一广播
        void itemSelected(const QString& routeKey);
        void backRequested();
        void compactedChanged(bool compacted);
        void backButtonVisibleChanged(bool visible);
        void backEnabledChanged(bool enabled);
        void menuButtonVisibleChanged(bool visible);
        void indicatorOwnerChanged(NavigationTreeItem* item, bool isOwner);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        bool event(QEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

        void onThemeUpdated() override { update(); }

    private:
        void setupUi();
        void setSurfaceVisible(bool visible);
        void applyDisplayMode(int mode);
        void applyPaneDensity();
        void showFlyoutMenu(const QString& categoryKey, QWidget* anchorWidget);
        void showOverflowMenu(QWidget* anchorWidget, const QVector<NavigationOverflowEntry>& entries);
        void closeFlyoutMenu(bool animated = true);
        void closeOverflowMenu(bool animated = true);
        void triggerCrossWindowPortal(NavigationFlyout* flyout, QWidget* anchorWidget, NavigationTreeItem* prevOwner, NavigationTreeItem* curClone = nullptr);
        void dispatchIndicatorAnimation(NavigationTreeItem* prevOwner, NavigationTreeItem* curOwner);
        void animateFlyoutClosed(const QString& categoryKey, QWidget* anchorWidget);
        void refreshIndicatorVisuals(bool animated = false, NavigationTreeItem* prevOwner = nullptr);

        /// Flyout 关闭后应执行的动画意图
        enum class FlyoutCloseIntent {
            None,          // 无特殊动画（light-dismiss、owner 不属于此 flyout）
            LeafSlide,     // 叶子点击：顶栏指示条从 prevOwner 滑向 curOwner 的视觉代理
            PortalReturn,  // 指示条正在 flyout 内部：播放 Portal 归位动画
        };

        void internalSetOrientation(Orientation orientation);
        void internalSetCompacted(bool compacted);

    private:
        bool m_isCompacted = false;
        bool m_surfaceVisible = false;
        float m_expandProgress = 1.0f;
        Orientation m_orientation = Orientation::Vertical;

        QBoxLayout* m_layout = nullptr;
        QBoxLayout* m_headerLayout = nullptr;
        NavigationToolButton* m_backButton = nullptr;
        NavigationToolButton* m_menuButton = nullptr;
        NavigationTreeWidget* m_tree = nullptr;
        NavigationIndicator* m_indicator = nullptr;
        QPointer<fluent::navigation::NavigationView> m_navigationView = nullptr;
        QWidget* m_userCardContainer = nullptr;
        QBoxLayout* m_userCardLayout = nullptr;
        QPointer<NavigationTreeItem> m_indicatorOwner;
        QPointer<NavigationTreeItem> m_visualIndicatorOwner;

        QPointer<NavigationFlyout> m_compactFlyout;
        QPointer<NavigationFlyout> m_overflowFlyout;
        FlyoutCloseIntent m_flyoutCloseIntent = FlyoutCloseIntent::None;
        QPointer<NavigationTreeItem> m_flyoutPrevOwner;
    };

} // namespace ui::navigation
