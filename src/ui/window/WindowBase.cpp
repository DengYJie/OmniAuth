#include "WindowBase.h"

#include <FluentQt/StatusInfo.h>
#include <QColor>
#include <QHBoxLayout>
#include <QMargins>
#include <QPainter>
#include <QShowEvent>

namespace {
    // Fluent Design 2 窗口按钮规范常量
    constexpr int CaptionButtonWidth = 46;
    constexpr int CaptionButtonIconSize = Typography::IconSize::Standard;
    constexpr qreal InactiveCaptionOpacity = 0.55;
}

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
    m_titleBar = new fluent::windowing::TitleBar(this);
    m_rootLayout->addWidget(m_titleBar);
    setupCaptionButtons();

    // 2. 内容区
    m_contentHost = new QWidget(this);
    auto* contentLayout = new QVBoxLayout(m_contentHost);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    m_rootLayout->addWidget(m_contentHost, 1);

    connect(m_titleBar, &fluent::windowing::TitleBar::titleBarHeightChanged, this, [this](int) {
        syncCaptionButtons();
        });
    connect(m_titleBar, &fluent::windowing::TitleBar::windowActiveChanged, this, [this](bool active) {
        syncCaptionButtonActivation(active);
        });
}

void WindowBase::setupCaptionButtons() {
    if (m_captionButtonHost) return;

    m_captionButtonHost = new QWidget(m_titleBar);
    m_captionButtonHost->setObjectName(QStringLiteral("fluentWindowCaptionButtonHost"));
    m_captionButtonHost->setAttribute(Qt::WA_StyledBackground, false);
    m_captionButtonHost->setAutoFillBackground(false);

    auto* buttonLayout = new QHBoxLayout(m_captionButtonHost);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(0);

    auto createCaptionButton = [this](const QString& objectName,
        const QString& glyph,
        const QString& tooltip,
        bool isCloseBtn = false) {
            auto* button = new fluent::basicinput::Button(m_captionButtonHost);
            button->setObjectName(objectName);
            fluent::status_info::ToolTip::attach(button, tooltip);
            button->setAccessibleName(tooltip);
            button->setFluentStyle(fluent::basicinput::Button::Subtle);
            button->setFluentLayout(fluent::basicinput::Button::IconOnly);
            button->setFluentSize(fluent::basicinput::Button::Small);
            button->setIconGlyph(glyph, CaptionButtonIconSize);
            button->setFocusPolicy(Qt::NoFocus);
            if (isCloseBtn) {
                button->setCriticalOnHover(true);
                button->setCornerRadii(QMargins(0, isMaximized() ? 0 : themeRadius().control, 0, 0));
            }
            else {
                button->setCornerRadii(QMargins(0, 0, 0, 0));
            }
            return button;
        };

    m_minimizeButton = createCaptionButton(QStringLiteral("fluentWindowMinimizeButton"),
        Typography::Icons::ChromeMinimize,
        tr("Minimize"));
    m_maximizeButton = createCaptionButton(QStringLiteral("fluentWindowMaximizeButton"),
        Typography::Icons::ChromeMaximize,
        tr("Maximize"));
    m_closeButton = createCaptionButton(QStringLiteral("fluentWindowCloseButton"),
        Typography::Icons::ChromeClose,
        tr("Close"),
        true);

    buttonLayout->addWidget(m_minimizeButton);
    buttonLayout->addWidget(m_maximizeButton);
    buttonLayout->addWidget(m_closeButton);

    connect(m_minimizeButton, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(m_maximizeButton, &QPushButton::clicked, this, [this] {
        isMaximized() ? showNormal() : showMaximized();
        updateMaximizeButtonIcon();
        });
    connect(m_closeButton, &QPushButton::clicked, this, &QWidget::close);

    if (auto* anchorLayout = qobject_cast<fluent::AnchorLayout*>(m_titleBar->layout())) {
        fluent::AnchorLayout::Anchors anchors;
        anchors.right = { m_titleBar, fluent::AnchorLayout::Edge::Right, 0 };
        anchors.verticalCenter = { m_titleBar, fluent::AnchorLayout::Edge::VCenter, 0 };
        anchorLayout->addAnchoredWidget(m_captionButtonHost, anchors);
    }

    syncCaptionButtons();
}

void WindowBase::syncCaptionButtons() {
    if (!m_captionButtonHost || !m_titleBar)
        return;

    const int buttonHeight = m_titleBar->titleBarHeight();
    constexpr int buttonCount = 3;
    m_captionButtonHost->setFixedSize(CaptionButtonWidth * buttonCount, buttonHeight);

    for (auto* button : { m_minimizeButton, m_maximizeButton, m_closeButton }) {
        if (button) {
            button->setFixedSize(CaptionButtonWidth, buttonHeight);
        }
    }
    if (m_closeButton) {
        m_closeButton->setCornerRadii(QMargins(0, isMaximized() ? 0 : themeRadius().control, 0, 0));
    }
    m_titleBar->setSystemReservedTrailingWidth(CaptionButtonWidth * buttonCount);
    updateMaximizeButtonIcon();
}

void WindowBase::syncCaptionButtonActivation(bool active) {
    const qreal opacity = active ? 1.0 : InactiveCaptionOpacity;
    for (auto* button : { m_minimizeButton, m_maximizeButton, m_closeButton }) {
        if (button) {
            button->setContentOpacity(opacity);
        }
    }
}

void WindowBase::updateMaximizeButtonIcon() {
    if (!m_maximizeButton)
        return;

    const QString tooltip = isMaximized() ? tr("Restore") : tr("Maximize");
    fluent::status_info::ToolTip::attach(m_maximizeButton, tooltip);
    m_maximizeButton->setAccessibleName(tooltip);

    m_maximizeButton->setIconGlyph(isMaximized()
        ? Typography::Icons::ChromeRestore
        : Typography::Icons::ChromeMaximize,
        CaptionButtonIconSize);
}

void WindowBase::setupWindowKit() {
    if (m_windowAgent) return;

    m_windowAgent = new QWK::WidgetWindowAgent(this);
    m_windowAgent->setup(this);

    m_windowAgent->setTitleBar(m_titleBar);
    m_windowAgent->setSystemButton(QWK::WindowAgentBase::Minimize, m_minimizeButton);
    m_windowAgent->setSystemButton(QWK::WindowAgentBase::Maximize, m_maximizeButton);
    m_windowAgent->setSystemButton(QWK::WindowAgentBase::Close, m_closeButton);

    m_windowAgent->setHitTestVisible(m_minimizeButton, true);
    m_windowAgent->setHitTestVisible(m_maximizeButton, true);
    m_windowAgent->setHitTestVisible(m_closeButton, true);

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

    if (m_titleBar)
        m_titleBar->onThemeUpdated();
    if (m_minimizeButton)
        m_minimizeButton->onThemeUpdated();
    if (m_maximizeButton)
        m_maximizeButton->onThemeUpdated();
    if (m_closeButton)
        m_closeButton->onThemeUpdated();

    syncCaptionButtons();
    update();
}

void WindowBase::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
}

void WindowBase::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);

    if (event->type() == QEvent::WindowStateChange) {
        syncCaptionButtons();
    }
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
