#pragma once

#include <FluentQt/Windowing.h>
#include <FluentQt/BasicInput.h>
#include <QIcon>
#include <QString>
#include <QWidget>

namespace ui::window {

class ElidedLabel;

/**
 * @brief WinUI 3 标准窗口标题栏
 *
 * 遵循 Windows 11 Fluent 标题栏设计规范，提供 32px 标准高度与 48px 扩展高度（容纳搜索框/头像时），
 * 支持 16px 规范安全边距、满出血背板标题按钮、文字溢出截断以及系统窗口事件的原生无缝集成。
 */
class TitleBar : public fluent::windowing::TitleBar {
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString subtitle READ subtitle WRITE setSubtitle NOTIFY subtitleChanged)
    Q_PROPERTY(QIcon icon READ icon WRITE setIcon NOTIFY iconChanged)
    Q_PROPERTY(bool backButtonVisible READ isBackButtonVisible WRITE setBackButtonVisible NOTIFY backButtonVisibleChanged)
    Q_PROPERTY(bool backButtonEnabled READ isBackButtonEnabled WRITE setBackButtonEnabled NOTIFY backButtonEnabledChanged)
    Q_PROPERTY(bool paneToggleButtonVisible READ isPaneToggleButtonVisible WRITE setPaneToggleButtonVisible NOTIFY paneToggleButtonVisibleChanged)
    Q_PROPERTY(HeightOption heightOption READ heightOption WRITE setHeightOption NOTIFY heightOptionChanged)

public:
    /**
     * @brief 标题栏高度选项
     */
    enum class HeightOption {
        Standard, // 标准高度：WinUI 3 默认 32px
        Tall      // 扩展高度：用于容纳搜索框或用户卡片时 48px
    };
    Q_ENUM(HeightOption)

    /**
     * @brief 构造函数
     * @param parent 父容器控件
     */
    explicit TitleBar(QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~TitleBar() override;

    /**
     * @brief 设置应用窗口主标题
     * @param title 主标题文本
     */
    void setTitle(const QString& title);

    /**
     * @brief 获取应用窗口主标题
     * @return 当前主标题文本
     */
    QString title() const;

    /**
     * @brief 设置应用窗口副标题
     * @param subtitle 副标题文本
     */
    void setSubtitle(const QString& subtitle);

    /**
     * @brief 获取应用窗口副标题
     * @return 当前副标题文本
     */
    QString subtitle() const;

    /**
     * @brief 设置应用窗口图标
     * @param icon 图标对象
     */
    void setIcon(const QIcon& icon);

    /**
     * @brief 获取应用窗口图标
     * @return 当前图标对象
     */
    QIcon icon() const;

    /**
     * @brief 设置后退导航按钮的可见性
     * @param visible 是否可见
     */
    void setBackButtonVisible(bool visible);

    /**
     * @brief 查询后退导航按钮的可见性
     * @return 是否可见
     */
    bool isBackButtonVisible() const;

    /**
     * @brief 设置后退导航按钮的交互可用性
     * @param enabled 是否可用
     */
    void setBackButtonEnabled(bool enabled);

    /**
     * @brief 查询后退导航按钮的交互可用性
     * @return 是否可用
     */
    bool isBackButtonEnabled() const;

    /**
     * @brief 设置侧边栏展开折叠按钮的可见性
     * @param visible 是否可见
     */
    void setPaneToggleButtonVisible(bool visible);

    /**
     * @brief 查询侧边栏展开折叠按钮的可见性
     * @return 是否可见
     */
    bool isPaneToggleButtonVisible() const;

    /**
     * @brief 设置标题栏高度模式
     * @param option 目标高度模式
     */
    void setHeightOption(HeightOption option);

    /**
     * @brief 获取当前标题栏高度模式
     * @return 当前高度模式
     */
    HeightOption heightOption() const;

    /**
     * @brief 注入全局搜索框控件：绝对居中于窗口
     * @param searchWidget 搜索框实例指针，为 nullptr 时移除并根据规则恢复标准高度
     */
    void setSearchWidget(QWidget* searchWidget);

    /**
     * @brief 获取全局搜索框控件
     * @return 搜索框控件指针
     */
    QWidget* searchWidget() const;

    /**
     * @brief 注入用户账户头像控件：停靠于系统控制按钮左侧
     * @param accountWidget 账户头像控件指针，为 nullptr 时移除并根据规则恢复标准高度
     */
    void setAccountWidget(QWidget* accountWidget);

    /**
     * @brief 获取用户账户头像控件
     * @return 账户头像控件指针
     */
    QWidget* accountWidget() const;

    /**
     * @brief 注入自定义内容控件：填充标题文本与右侧控件之间的空白区
     * @param contentWidget 自定义内容控件指针
     */
    void setContentWidget(QWidget* contentWidget);

    /**
     * @brief 获取自定义内容控件
     * @return 自定义内容控件指针
     */
    QWidget* contentWidget() const;

    /**
     * @brief 获取窗口系统图标按钮：用于 QWindowKit 注册原生系统菜单与双击关闭
     * @return 窗口图标按钮指针
     */
    fluent::basicinput::Button* iconButton() const { return m_iconButton; }

    /**
     * @brief 获取最小化按钮：用于 QWindowKit 注册系统最小化动作
     * @return 最小化按钮指针
     */
    fluent::basicinput::Button* minimizeButton() const { return m_minimizeButton; }

    /**
     * @brief 获取最大化/还原按钮：用于 QWindowKit 注册系统最大化/还原动作
     * @return 最大化/还原按钮指针
     */
    fluent::basicinput::Button* maximizeButton() const { return m_maximizeButton; }

    /**
     * @brief 获取关闭按钮：用于 QWindowKit 注册系统关闭动作
     * @return 关闭按钮指针
     */
    fluent::basicinput::Button* closeButton() const { return m_closeButton; }

    /**
     * @brief 获取后退导航按钮：用于 QWindowKit 声明免拖拽吞噬的客户区点击
     * @return 后退按钮指针
     */
    fluent::basicinput::Button* backButton() const { return m_backButton; }

    /**
     * @brief 获取侧边栏折叠按钮：用于 QWindowKit 声明免拖拽吞噬的客户区点击
     * @return 侧边栏折叠按钮指针
     */
    fluent::basicinput::Button* paneToggleButton() const { return m_paneToggleButton; }

    /**
     * @brief 主题刷新回调：同步前景色、圆角及非活动态透明度
     */
    void onThemeUpdated() override;

signals:
    /**
     * @brief 后退按钮点击信号
     */
    void backButtonClicked();

    /**
     * @brief 侧边栏切换按钮点击信号
     */
    void paneToggleButtonClicked();

    /**
     * @brief 主标题变更信号
     * @param title 新标题内容
     */
    void titleChanged(const QString& title);

    /**
     * @brief 副标题变更信号
     * @param subtitle 新副标题内容
     */
    void subtitleChanged(const QString& subtitle);

    /**
     * @brief 图标变更信号
     * @param icon 新图标
     */
    void iconChanged(const QIcon& icon);

    /**
     * @brief 后退按钮可见性变更信号
     * @param visible 新可见性状态
     */
    void backButtonVisibleChanged(bool visible);

    void backButtonEnabledChanged(bool enabled);

    /**
     * @brief 侧边栏切换按钮可见性变更信号
     * @param visible 新可见性状态
     */
    void paneToggleButtonVisibleChanged(bool visible);

    /**
     * @brief 标题栏高度模式变更信号
     * @param option 新高度模式
     */
    void heightOptionChanged(HeightOption option);

protected:
    // 重写 showEvent 以确保在已加入窗口树时正确安装事件过滤器
    void showEvent(QShowEvent* event) override;

    // 监听尺寸变化以动态计算绝对居中搜索框及溢出截断区域
    void resizeEvent(QResizeEvent* event) override;

    // 监听宿主窗口状态以同步最大化还原图标与右上角圆角裁切
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    // 同步当前窗口的最大化/还原状态到 UI
    void syncWindowState();

private:
    // 根据子控件存在情况与高度模式计算并应用物理高度
    void updateHeight();

    // 计算各分区绝对像素坐标并更新拖拽安全区
    void updateLayout();

    // 保证背板填满标题栏高度
    void updateCaptionButtonSizes();

    // 窗口非活动时降低交互元素不透明度
    void syncActivationOpacity();

    // 统一构造符合 WinUI 3 交互态与提示气泡的系统控制按钮
    fluent::basicinput::Button* createCaptionButton(const QString& objectName, const QString& glyph, const QString& tooltip, bool isCloseBtn = false);

    QString m_title;
    QString m_subtitle;
    QIcon m_icon;
    bool m_backButtonVisible = false;
    bool m_backButtonEnabled = true;
    bool m_paneToggleButtonVisible = false;
    HeightOption m_heightOption = HeightOption::Standard;

    // 自定义注入控件
    QWidget* m_searchWidget = nullptr;
    QWidget* m_accountWidget = nullptr;
    QWidget* m_customContentWidget = nullptr;

    // 标题与导航控件
    fluent::basicinput::Button* m_backButton = nullptr;
    fluent::basicinput::Button* m_paneToggleButton = nullptr;
    fluent::basicinput::Button* m_iconButton = nullptr;
    ElidedLabel* m_titleLabel = nullptr;
    ElidedLabel* m_subtitleLabel = nullptr;

    // 窗口系统控制按钮
    fluent::basicinput::Button* m_minimizeButton = nullptr;
    fluent::basicinput::Button* m_maximizeButton = nullptr;
    fluent::basicinput::Button* m_closeButton = nullptr;
};

} // namespace ui::window
