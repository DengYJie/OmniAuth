#include "MainWindow.h"

#include "ui/navigation/NavigationPanel.h"
#include <FluentQt/Design.h>
#include <FluentQt/Navigation.h>
#include <FluentQt/TextFields.h>
#include <QVBoxLayout>

namespace fluent_tf = fluent::textfields;

namespace {

    QWidget* createDummyPage(const QString& titleText) {
        auto* page = new QWidget();
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(32, 24, 32, 24);

        auto* title = new fluent_tf::Label(titleText, page);
        title->setFluentTypography(Typography::FontRole::Title);
        layout->addWidget(title);
        layout->addStretch();
        return page;
    }

}  // namespace

MainWindow::MainWindow(QWidget* parent) : NavigationWindow(parent) {
    initWindow();
    buildNavigation();
}

void MainWindow::initWindow() {
    setWindowTitle(QStringLiteral("OmniAuth - 主系统"));
    resize(1080, 720);
    setMinimumSize(480, 480);
    setBackdropEffect(fluent::windowing::BackdropEffect::Mica);
    setCustomWindowChromeEnabled(true);

    // 主系统采用顶部导航栏
    navigationView()->setDisplayMode(fluent::navigation::NavigationView::DisplayMode::Top);
    panel()->setNavigationPosition(ui::navigation::NavigationPosition::Top);
}

void MainWindow::buildNavigation() {
    panel()->setBackButtonVisible(true);
    // 概览
    addSectionHeader(QStringLiteral("概览"));
    addSubInterface(QStringLiteral("home"), createDummyPage(QStringLiteral("控制台概览")), Typography::Icons::Home, QStringLiteral("首页"));
    addSubInterface(QStringLiteral("dashboard"), createDummyPage(QStringLiteral("实时数据看板")), Typography::Icons::View, QStringLiteral("数据看板"));

    // 管理（二级分类）
    addSectionHeader(QStringLiteral("管理"));
    addSubInterface(QStringLiteral("mgmt"), createDummyPage(QStringLiteral("管理总览")), Typography::Icons::Shield, QStringLiteral("管理中心"));
    addSubInterface(QStringLiteral("users"), createDummyPage(QStringLiteral("用户账号管理")), Typography::Icons::People, QStringLiteral("用户管理"), QStringLiteral("mgmt"));
    // 三级菜单示例：users 为二级分类，其下再挂三级子项
    addSubInterface(QStringLiteral("users/list"), createDummyPage(QStringLiteral("用户列表与检索")), Typography::Icons::ContactInfo, QStringLiteral("用户列表"), QStringLiteral("users"));
    addSubInterface(QStringLiteral("users/roles"), createDummyPage(QStringLiteral("用户角色分配")), Typography::Icons::ContactInfo, QStringLiteral("角色分配"), QStringLiteral("users"));
    addSubInterface(QStringLiteral("roles"), createDummyPage(QStringLiteral("角色与权限分配")), Typography::Icons::ContactInfo, QStringLiteral("角色权限"), QStringLiteral("mgmt"));
    addSubInterface(QStringLiteral("devices"), createDummyPage(QStringLiteral("设备注册与信任管理")), Typography::Icons::Laptop, QStringLiteral("设备管理"), QStringLiteral("mgmt"));
    addSubInterface(QStringLiteral("security"), createDummyPage(QStringLiteral("安全审计与日志")), Typography::Icons::Edit, QStringLiteral("安全中心"), QStringLiteral("mgmt"));

    // 认证服务（纯分类：无自己的页面，仅折叠/展开，nullptr 表示不注册内容页）
    addSectionHeader(QStringLiteral("认证服务"));
    addSubInterface(QStringLiteral("auth"), nullptr, Typography::Icons::Lock, QStringLiteral("认证服务"),
        QString(), ui::navigation::NavigationItemPosition::Top, /*selectable=*/false);
    addSubInterface(QStringLiteral("oauth"), createDummyPage(QStringLiteral("OAuth 2.0 / OIDC 配置中心")), Typography::Icons::Link, QStringLiteral("OAuth 授权"), QStringLiteral("auth"));
    addSubInterface(QStringLiteral("mfa"), createDummyPage(QStringLiteral("多因素认证 (MFA) 设置")), Typography::Icons::Block, QStringLiteral("MFA 验证"), QStringLiteral("auth"));
    addSubInterface(QStringLiteral("sso"), createDummyPage(QStringLiteral("单点登录 (SSO) 域配置")), Typography::Icons::Share, QStringLiteral("单点登录"), QStringLiteral("auth"));
    addSubInterface(QStringLiteral("sessions"), createDummyPage(QStringLiteral("在线会话与吊销")), Typography::Icons::Clock, QStringLiteral("会话管理"), QStringLiteral("auth"));

    // 开发（有自己的页面，同时也是分类）
    addSectionHeader(QStringLiteral("开发"));
    addSubInterface(QStringLiteral("dev"), createDummyPage(QStringLiteral("开发者总览")), Typography::Icons::Grid, QStringLiteral("开发者中心"));
    addSubInterface(QStringLiteral("keys"), createDummyPage(QStringLiteral("API 密钥与凭据")), Typography::Icons::PasswordKeyShow, QStringLiteral("API 密钥"), QStringLiteral("dev"));
    addSubInterface(QStringLiteral("webhooks"), createDummyPage(QStringLiteral("Webhook 订阅与投递")), Typography::Icons::Link, QStringLiteral("Webhook"), QStringLiteral("dev"));
    addSubInterface(QStringLiteral("logs"), createDummyPage(QStringLiteral("调用日志与追踪")), Typography::Icons::Document, QStringLiteral("调用日志"), QStringLiteral("dev"));

    // 消息中心
    addSectionHeader(QStringLiteral("消息中心"));
    addSubInterface(QStringLiteral("notify"), createDummyPage(QStringLiteral("系统通知与公告")), Typography::Icons::ImportantBadge12, QStringLiteral("通知"));
    addSubInterface(QStringLiteral("msg"), createDummyPage(QStringLiteral("私信与对话")), Typography::Icons::Message, QStringLiteral("私信"));
    addSubInterface(QStringLiteral("channels"), nullptr, Typography::Icons::Mail, QStringLiteral("消息渠道"),
        QString(), ui::navigation::NavigationItemPosition::Top, /*selectable=*/false);
    addSubInterface(QStringLiteral("emailCh"), createDummyPage(QStringLiteral("邮件渠道配置")), Typography::Icons::Send, QStringLiteral("邮件渠道"), QStringLiteral("channels"));
    addSubInterface(QStringLiteral("smsCh"), createDummyPage(QStringLiteral("短信渠道配置")), Typography::Icons::Phone, QStringLiteral("短信渠道"), QStringLiteral("channels"));

    // 底部: 系统设置
    addSubInterface(QStringLiteral("settings"), createDummyPage(QStringLiteral("系统参数配置")), Typography::Icons::Settings, QStringLiteral("系统设置"), QString(), ui::navigation::NavigationItemPosition::Bottom);
    addSubInterface(QStringLiteral("about"), createDummyPage(QStringLiteral("关于 OmniAuth")), Typography::Icons::Info, QStringLiteral("关于"), QString(), ui::navigation::NavigationItemPosition::Bottom);

    // 初始选中
    switchTo(QStringLiteral("home"));
}
