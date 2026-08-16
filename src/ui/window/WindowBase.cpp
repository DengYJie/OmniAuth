#include "WindowBase.h"

#include <FluentQt/StatusInfo.h>
#include <QColor>
#include <QHBoxLayout>
#include <QMargins>
#include <QPainter>
#include <QShowEvent>

WindowBase::WindowBase(QWidget* parent)
    : QWidget(parent) {
    setupUi();
    onThemeUpdated();
}

void WindowBase::setupUi() {
    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->setSpacing(0);

    // 1. 标题栏
    m_titleBar = new ui::window::TitleBar(this);
    m_rootLayout->addWidget(m_titleBar);

    // 2. 内容区
    m_contentHost = new QWidget(this);
    auto* contentLayout = new QVBoxLayout(m_contentHost);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    m_rootLayout->addWidget(m_contentHost, 1);
}

void WindowBase::setupWindowKit() {
    if (m_windowAgent) return;

    m_windowAgent = new QWK::WidgetWindowAgent(this);
    m_windowAgent->setup(this);

    m_windowAgent->setTitleBar(m_titleBar);
    
    // 连接到 ui::window::TitleBar 自带的系统控制按钮
    if (auto* minimizeBtn = m_titleBar->minimizeButton()) {
        m_windowAgent->setSystemButton(QWK::WindowAgentBase::Minimize, minimizeBtn);
    }
    if (auto* maximizeBtn = m_titleBar->maximizeButton()) {
        m_windowAgent->setSystemButton(QWK::WindowAgentBase::Maximize, maximizeBtn);
    }
    if (auto* closeBtn = m_titleBar->closeButton()) {
        m_windowAgent->setSystemButton(QWK::WindowAgentBase::Close, closeBtn);
    }

    // 保证导航按钮也不被当作标题栏拖拽区域吞噬点击
    if (auto* backBtn = m_titleBar->backButton()) {
        m_windowAgent->setHitTestVisible(backBtn, true);
    }
    if (auto* paneBtn = m_titleBar->paneToggleButton()) {
        m_windowAgent->setHitTestVisible(paneBtn, true);
    }
    if (auto* iconBtn = m_titleBar->iconButton()) {
        m_windowAgent->setHitTestVisible(iconBtn, true);
    }

    applyPlatformWindowAttributes();
}

void WindowBase::applyPlatformWindowAttributes() {
    if (!m_windowAgent)
        return;

    const bool isDark = (effectiveTheme() == FluentElement::Dark);

#if defined(Q_OS_WIN)
    m_windowAgent->setWindowAttribute(QStringLiteral("dark-mode"), isDark);
    m_windowAgent->setWindowAttribute(QStringLiteral("mica"),
        m_backdropEffect == fluent::windowing::BackdropEffect::Mica);
    m_windowAgent->setWindowAttribute(QStringLiteral("mica-alt"), false);
    m_windowAgent->setWindowAttribute(QStringLiteral("acrylic-material"),
        m_backdropEffect == fluent::windowing::BackdropEffect::Acrylic);
    m_windowAgent->setWindowAttribute(QStringLiteral("dwm-blur"), false);
    m_windowAgent->setWindowAttribute(QStringLiteral("dwm-border-color"),
        QColor(QStringLiteral("#3367d6")));
#elif defined(Q_OS_MACOS)
    m_windowAgent->setWindowAttribute(QStringLiteral("no-system-buttons"), false);
    m_windowAgent->setWindowAttribute(QStringLiteral("blur-effect"),
        isDark ? QStringLiteral("dark") : QStringLiteral("light"));
    m_windowAgent->setWindowAttribute(QStringLiteral("glass-effect"), QStringLiteral("regular"));
    m_windowAgent->setWindowAttribute(QStringLiteral("glass-corner-radius"), 24.0);
    m_windowAgent->setWindowAttribute(QStringLiteral("glass-tint-color"),
        QColor(255, 255, 255, 46));
#endif
}

void WindowBase::setContentWidget(QWidget* widget) {
    if (m_contentWidget) {
        m_contentWidget->deleteLater();
    }
    m_contentWidget = widget;
    if (m_contentWidget) {
        m_contentHost->layout()->addWidget(m_contentWidget);
    }
}

void WindowBase::setBackdropEffect(fluent::windowing::BackdropEffect effect) {
    if (m_backdropEffect == effect) return;
    m_backdropEffect = effect;

    if (m_windowAgent) {
        applyPlatformWindowAttributes();
    }
    setAttribute(Qt::WA_TranslucentBackground, effect != fluent::windowing::BackdropEffect::Solid);
}

void WindowBase::setCustomWindowChromeEnabled(bool enabled) {
    if (m_customWindowChromeEnabled == enabled) return;
    m_customWindowChromeEnabled = enabled;

    if (m_titleBar) {
        m_titleBar->setVisible(enabled);
    }

    if (enabled && !m_windowAgent && testAttribute(Qt::WA_WState_Polished)) {
        setupWindowKit();
    }
}

bool WindowBase::event(QEvent* event) {
    if (event->type() == QEvent::Polish) {
        if (m_customWindowChromeEnabled && !m_windowAgent) {
            setupWindowKit();
        }
    }
    return QWidget::event(event);
}

void WindowBase::onThemeUpdated() {
    if (m_windowAgent) {
        applyPlatformWindowAttributes();
    }

    if (m_titleBar) {
        m_titleBar->onThemeUpdated();
    }

    update();
}

void WindowBase::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
}

void WindowBase::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
}

void WindowBase::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    // 启用系统背景时透明擦除，否则绘制主题底色
    if (testAttribute(Qt::WA_TranslucentBackground)) {
        painter.fillRect(rect(), Qt::transparent);
    }
    else {
        painter.fillRect(rect(), themeColorsRef().bgLayer);
    }
}
