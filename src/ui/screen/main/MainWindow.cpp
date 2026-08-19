#include "MainWindow.h"
#include "MainWindowViewModel.h"
#include "ui/animation/AnimatedSettingsVisualSource.h"
#include "ui/navigation/NavigationPanel.h"
#include <FluentQt/BasicInput.h>
#include <FluentQt/Design.h>
#include <FluentQt/Navigation.h>
#include <FluentQt/TextFields.h>
#include "design/Spacing.h"
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <FluentQt/DialogsFlyouts.h>
#include "data/di/AppContainer.h"
#include "ui/screen/facescan/FaceEnrollDialog.h"
#include <QPointer>
#include <QTimer>

namespace fluent_tf = fluent::textfields;

namespace {

    QWidget* createDummyPage(const QString& titleText) {
        auto* page = new QWidget();
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(Spacing::XLarge, Spacing::Large,
                                   Spacing::XLarge, Spacing::Large);

        auto* title = new fluent_tf::Label(titleText, page);
        title->setFluentTypography(Typography::FontRole::Title);
        layout->addWidget(title);
        layout->addStretch();
        return page;
    }

    QWidget* createSettingsPage(MainWindow* window) {
        auto* page = new QWidget();
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(Spacing::XLarge, Spacing::Large,
                                   Spacing::XLarge, Spacing::Large);
        layout->setSpacing(Spacing::Standard);

        auto* title = new fluent_tf::Label(QStringLiteral("系统设置与个性化"), page);
        title->setFluentTypography(Typography::FontRole::Title);
        layout->addWidget(title);

        auto* sectionTitle = new fluent_tf::Label(QStringLiteral("导航栏布局模式 (Navigation Position)"), page);
        sectionTitle->setFluentTypography(Typography::FontRole::BodyStrong);
        layout->addWidget(sectionTitle);

        auto* descLabel = new fluent_tf::Label(QStringLiteral("切换侧边栏 (Left) 与顶部栏 (Top) 导航展示形态"), page);
        descLabel->setFluentTypography(Typography::FontRole::Caption);
        layout->addWidget(descLabel);

        auto* switchLayout = new QHBoxLayout();
        switchLayout->setContentsMargins(0, 0, 0, 0);

        auto* navPosSwitch = new fluent::basicinput::ToggleSwitch(page);
        navPosSwitch->setOnContent(QStringLiteral("顶栏模式 (Top)"));
        navPosSwitch->setOffContent(QStringLiteral("侧边栏模式 (Left)"));
        navPosSwitch->setIsOn(window->navigationView()->displayMode() == fluent::navigation::NavigationView::DisplayMode::Top);

        QObject::connect(navPosSwitch, &fluent::basicinput::ToggleSwitch::toggled, window, [window](bool isTop) {
            window->navigationView()->setDisplayMode(
                isTop ? fluent::navigation::NavigationView::DisplayMode::Top
                      : fluent::navigation::NavigationView::DisplayMode::Left
            );
        });

        switchLayout->addWidget(navPosSwitch);
        switchLayout->addStretch();
        layout->addLayout(switchLayout);

        // ── 生物识别与安全设置 ──
        auto* bioSectionTitle = new fluent_tf::Label(QStringLiteral("生物识别与安全 (Biometrics & Security)"), page);
        bioSectionTitle->setFluentTypography(Typography::FontRole::BodyStrong);
        layout->addWidget(bioSectionTitle);

        auto* bioDescLabel = new fluent_tf::Label(QStringLiteral("录入或更新您的面部特征，以便在登录界面快速刷脸认证"), page);
        bioDescLabel->setFluentTypography(Typography::FontRole::Caption);
        layout->addWidget(bioDescLabel);

        auto* enrollBtn = new fluent::basicinput::Button(QStringLiteral("录入 / 重新绑定人脸"), page);
        enrollBtn->setFluentStyle(fluent::basicinput::Button::Accent);
        enrollBtn->setFixedWidth(200);
        QObject::connect(enrollBtn, &fluent::basicinput::Button::clicked, window, [window]() {
            if (window->uid() <= 0) {
                auto* warnDialog = new fluent::dialogs_flyouts::ContentDialog(window);
                warnDialog->setTitle(QStringLiteral("提示"));
                warnDialog->setContentText(QStringLiteral("当前未获取到有效用户登录态，无法绑定人脸"));
                warnDialog->setCloseButtonText(QStringLiteral("确定"));
                warnDialog->setAttribute(Qt::WA_DeleteOnClose);
                warnDialog->exec();
                return;
            }
            auto* dialog = new FaceEnrollDialog(window->uid(), window);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->exec();
        });
        layout->addWidget(enrollBtn);

        layout->addStretch();
        return page;
    }

}  // namespace

MainWindow::MainWindow(int uid, const QString& username, bool promptFaceEnroll, QWidget* parent)
    : NavigationWindow(parent), m_initPromptRequested(promptFaceEnroll) {
    
    m_viewModel = std::make_shared<MainWindowViewModel>(uid, username);
    m_viewModel->observe(this, &MainWindow::renderState);

    initWindow();
    buildNavigation();
    if (!username.isEmpty()) {
        setWindowTitle(QString("OmniAuth - 主系统 - [%1]").arg(username));
    }
}

int MainWindow::uid() const {
    return m_viewModel->currentState().uid;
}

QString MainWindow::username() const {
    return m_viewModel->currentState().username;
}

void MainWindow::initWindow() {
    setWindowTitle(QStringLiteral("OmniAuth - 主系统"));
    resize(1080, 720);
    setMinimumSize(480, 480);
}

void MainWindow::showEvent(QShowEvent* event) {
    NavigationWindow::showEvent(event);
    if (m_initPromptRequested) {
        m_initPromptRequested = false;
        m_viewModel->checkFaceEnrollment();
    }
}

void MainWindow::renderState(const MainWindowState& state) {
    if (state.showFaceEnrollPrompt) {
        m_viewModel->clearFaceEnrollPrompt();
        QTimer::singleShot(400, this, &MainWindow::promptFaceEnrollDialog);
    }
}

void MainWindow::promptFaceEnrollDialog() {
    auto* dialog = new fluent::dialogs_flyouts::ContentDialog(this);
    dialog->setTitle(QStringLiteral("人脸登录推荐"));
    dialog->setContentText(QStringLiteral("检测到您尚未录入面部数据，是否立即录入以便下次直接刷脸登录？"));
    dialog->setPrimaryButtonText(QStringLiteral("立即录入"));
    dialog->setCloseButtonText(QStringLiteral("稍后再说"));
    dialog->setDefaultButton(fluent::dialogs_flyouts::ContentDialog::Primary);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    if (dialog->exec() == fluent::dialogs_flyouts::ContentDialog::ResultPrimary) {
        auto* enrollDlg = new FaceEnrollDialog(uid(), this);
        enrollDlg->setAttribute(Qt::WA_DeleteOnClose);
        enrollDlg->exec();
    }
}

void MainWindow::buildNavigation() {
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

    // 底部: 系统设置 (带 WinUI 3 原生齿轮微交互旋转动画)
    addSubInterface(
        QStringLiteral("settings"),
        createSettingsPage(this),
        Typography::Icons::Settings,
        QStringLiteral("系统设置"),
        QString(),
        ui::navigation::NavigationItemPosition::Bottom,
        /*selectable=*/true,
        std::make_shared<ui::animation::AnimatedSettingsVisualSource>()
    );
    addSubInterface(QStringLiteral("about"), createDummyPage(QStringLiteral("关于 OmniAuth")), Typography::Icons::Info, QStringLiteral("关于"), QString(), ui::navigation::NavigationItemPosition::Bottom);

    // 初始选中
    switchTo(QStringLiteral("home"));
}
