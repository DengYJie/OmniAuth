#include "TitleBar.h"

#include <FluentQt/StatusInfo.h>
#include <FluentQt/Design.h>
#include <QPainter>
#include <QResizeEvent>
#include <QLabel>
#include <QApplication>
#include <QFontMetrics>

namespace ui::window {

namespace {
    // WinUI 3 标准标题栏高度
    constexpr int TitleBarStandardHeight = 32;
    // 包含搜索框或用户卡片时的扩展高度
    constexpr int TitleBarTallHeight = 48;
    // WinUI 3 满出血背板右侧控制按钮宽度 (Min/Max/Close)
    constexpr int CaptionButtonWidth = 46;
    // WinUI 3 左侧导航按钮宽度 (Back/Menu)
    constexpr int AppTitleBarButtonWidth = 40;
    // WinUI 3 规范元素安全间距
    constexpr int ElementSpacing = 16;
    // 标题文本右侧间距
    constexpr int TitleMarginRight = 8;
    // 标准图标像素尺寸
    constexpr int IconSize = 16;
    // 窗口非活动时的内容透明度规范 (TitleBarDeactivatedOpacity)
    constexpr qreal InactiveOpacity = 0.5;
}

// 文本溢出自动末尾截断的标签控件：避免挤压右侧窗口控制按钮
class ElidedLabel : public QLabel {
public:
    explicit ElidedLabel(QWidget* parent = nullptr) : QLabel(parent) {}
    void setText(const QString& text) {
        m_text = text;
        QLabel::setText(text);
        update();
    }
protected:
    // 依据可用宽度通过 QFontMetrics 计算截断文本绘制
    void paintEvent(QPaintEvent* event) override {
        QPainter painter(this);
        painter.setPen(palette().color(QPalette::WindowText));
        QFontMetrics fm(font());
        QString elidedText = fm.elidedText(m_text, Qt::ElideRight, width());
        QRect textRect = rect();
        painter.drawText(textRect, alignment() | Qt::AlignVCenter, elidedText);
    }
private:
    QString m_text;
};

TitleBar::TitleBar(QWidget* parent)
    : fluent::windowing::TitleBar(parent)
{
    setTitleBarHeight(TitleBarStandardHeight);

    m_backButton = new fluent::basicinput::Button(this);
    m_backButton->setObjectName(QStringLiteral("fluentWindowBackButton"));
    m_backButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    m_backButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    m_backButton->setIconGlyph(Typography::Icons::Back, IconSize);
    m_backButton->setFocusPolicy(Qt::NoFocus);
    m_backButton->hide();
    fluent::status_info::ToolTip::attach(m_backButton, tr("Back"));
    m_backButton->setAccessibleName(tr("Back"));
    connect(m_backButton, &fluent::basicinput::Button::clicked, this, &TitleBar::backButtonClicked);

    m_paneToggleButton = new fluent::basicinput::Button(this);
    m_paneToggleButton->setObjectName(QStringLiteral("fluentWindowPaneToggleButton"));
    m_paneToggleButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    m_paneToggleButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    m_paneToggleButton->setIconGlyph(Typography::Icons::GlobalNav, IconSize);
    m_paneToggleButton->setFocusPolicy(Qt::NoFocus);
    m_paneToggleButton->hide();
    fluent::status_info::ToolTip::attach(m_paneToggleButton, tr("Menu"));
    m_paneToggleButton->setAccessibleName(tr("Menu"));
    connect(m_paneToggleButton, &fluent::basicinput::Button::clicked, this, &TitleBar::paneToggleButtonClicked);

    m_iconButton = new fluent::basicinput::Button(this);
    m_iconButton->setObjectName(QStringLiteral("fluentWindowIconButton"));
    m_iconButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    m_iconButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    m_iconButton->setFocusPolicy(Qt::NoFocus);
    m_iconButton->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_iconButton->hide();

    // 穿透鼠标事件以保留标题区域的系统拖拽与右键窗口菜单行为
    m_titleLabel = new ElidedLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("fluentWindowTitleLabel"));
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_titleLabel->setFont(themeFont(Typography::FontRole::Caption).toQFont());
    m_titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_subtitleLabel = new ElidedLabel(this);
    m_subtitleLabel->setObjectName(QStringLiteral("fluentWindowSubtitleLabel"));
    m_subtitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_subtitleLabel->setFont(themeFont(Typography::FontRole::Caption).toQFont());
    m_subtitleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_subtitleLabel->hide();

    m_minimizeButton = createCaptionButton(QStringLiteral("fluentWindowMinimizeButton"), Typography::Icons::ChromeMinimize, tr("Minimize"));
    m_maximizeButton = createCaptionButton(QStringLiteral("fluentWindowMaximizeButton"), Typography::Icons::ChromeMaximize, tr("Maximize"));
    m_closeButton = createCaptionButton(QStringLiteral("fluentWindowCloseButton"), Typography::Icons::ChromeClose, tr("Close"), true);

    connect(m_minimizeButton, &fluent::basicinput::Button::clicked, window(), &QWidget::showMinimized);
    connect(m_maximizeButton, &fluent::basicinput::Button::clicked, window(), [this]() {
        if (window()->isMaximized()) {
            window()->showNormal();
        } else {
            window()->showMaximized();
        }
    });
    connect(m_closeButton, &fluent::basicinput::Button::clicked, window(), &QWidget::close);

    connect(window(), &QWidget::windowIconChanged, this, [this](const QIcon& icon) {
        setIcon(icon);
    });
    connect(window(), &QWidget::windowTitleChanged, this, [this](const QString& title) {
        setTitle(title);
    });

    connect(this, &fluent::windowing::TitleBar::titleBarHeightChanged, this, [this](int) {
        updateCaptionButtonSizes();
        updateLayout();
    });

    connect(this, &fluent::windowing::TitleBar::windowActiveChanged, this, [this](bool active) {
        syncActivationOpacity();
    });

}

TitleBar::~TitleBar() = default;

fluent::basicinput::Button* TitleBar::createCaptionButton(const QString& objectName, const QString& glyph, const QString& tooltip, bool isCloseBtn) {
    auto* button = new fluent::basicinput::Button(this);
    button->setObjectName(objectName);
    fluent::status_info::ToolTip::attach(button, tooltip);
    button->setAccessibleName(tooltip);
    button->setFluentStyle(fluent::basicinput::Button::Subtle);
    button->setFluentLayout(fluent::basicinput::Button::IconOnly);
    button->setIconGlyph(glyph, IconSize);
    button->setFocusPolicy(Qt::NoFocus);
    if (isCloseBtn) {
        button->setCriticalOnHover(true);
    }
    return button;
}

void TitleBar::setTitle(const QString& title) {
    if (m_title == title) return;
    m_title = title;
    m_titleLabel->setText(title);
    updateLayout();
    emit titleChanged(title);
}

QString TitleBar::title() const {
    return m_title;
}

void TitleBar::setSubtitle(const QString& subtitle) {
    if (m_subtitle == subtitle) return;
    m_subtitle = subtitle;
    m_subtitleLabel->setText(subtitle);
    m_subtitleLabel->setVisible(!subtitle.isEmpty());
    updateLayout();
    emit subtitleChanged(subtitle);
}

QString TitleBar::subtitle() const {
    return m_subtitle;
}

void TitleBar::setIcon(const QIcon& icon) {
    m_icon = icon;
    if (m_icon.isNull()) {
        m_iconButton->hide();
    } else {
        m_iconButton->setIcon(icon);
        m_iconButton->setIconSize(QSize(IconSize, IconSize));
        m_iconButton->show();
    }
    updateLayout();
    emit iconChanged(icon);
}

QIcon TitleBar::icon() const {
    return m_icon;
}

void TitleBar::setBackButtonVisible(bool visible) {
    if (m_backButtonVisible == visible) return;
    m_backButtonVisible = visible;
    m_backButton->setVisible(visible);
    updateLayout();
    emit backButtonVisibleChanged(visible);
}

bool TitleBar::isBackButtonVisible() const {
    return m_backButtonVisible;
}

void TitleBar::setBackButtonEnabled(bool enabled) {
    if (m_backButtonEnabled == enabled) return;
    m_backButtonEnabled = enabled;
    m_backButton->setEnabled(enabled);
    emit backButtonEnabledChanged(enabled);
}

bool TitleBar::isBackButtonEnabled() const {
    return m_backButtonEnabled;
}

void TitleBar::setPaneToggleButtonVisible(bool visible) {
    if (m_paneToggleButtonVisible == visible) return;
    m_paneToggleButtonVisible = visible;
    m_paneToggleButton->setVisible(visible);
    updateLayout();
    emit paneToggleButtonVisibleChanged(visible);
}

bool TitleBar::isPaneToggleButtonVisible() const {
    return m_paneToggleButtonVisible;
}

void TitleBar::setHeightOption(HeightOption option) {
    if (m_heightOption == option) return;
    m_heightOption = option;
    updateHeight();
    emit heightOptionChanged(option);
}

TitleBar::HeightOption TitleBar::heightOption() const {
    return m_heightOption;
}

void TitleBar::setSearchWidget(QWidget* searchWidget) {
    if (m_searchWidget == searchWidget) return;
    if (m_searchWidget) m_searchWidget->setParent(nullptr);
    m_searchWidget = searchWidget;
    if (m_searchWidget) {
        m_searchWidget->setParent(this);
        m_searchWidget->show();
    }
    updateHeight();
    updateLayout();
}

QWidget* TitleBar::searchWidget() const {
    return m_searchWidget;
}

void TitleBar::setAccountWidget(QWidget* accountWidget) {
    if (m_accountWidget == accountWidget) return;
    if (m_accountWidget) m_accountWidget->setParent(nullptr);
    m_accountWidget = accountWidget;
    if (m_accountWidget) {
        m_accountWidget->setParent(this);
        m_accountWidget->show();
    }
    updateHeight();
    updateLayout();
}

QWidget* TitleBar::accountWidget() const {
    return m_accountWidget;
}

void TitleBar::setLeftHeaderWidget(QWidget* leftHeaderWidget) {
    if (m_leftHeaderWidget == leftHeaderWidget) return;
    if (m_leftHeaderWidget) m_leftHeaderWidget->setParent(nullptr);
    m_leftHeaderWidget = leftHeaderWidget;
    if (m_leftHeaderWidget) {
        m_leftHeaderWidget->setParent(this);
        m_leftHeaderWidget->show();
    }
    updateLayout();
}

QWidget* TitleBar::leftHeaderWidget() const {
    return m_leftHeaderWidget;
}

void TitleBar::setContentWidget(QWidget* contentWidget) {
    if (m_customContentWidget == contentWidget) return;
    if (m_customContentWidget) m_customContentWidget->setParent(nullptr);
    m_customContentWidget = contentWidget;
    if (m_customContentWidget) {
        m_customContentWidget->setParent(this);
        m_customContentWidget->show();
    }
    updateLayout();
}

QWidget* TitleBar::contentWidget() const {
    return m_customContentWidget;
}

void TitleBar::onThemeUpdated() {
    fluent::windowing::TitleBar::onThemeUpdated();
    
    QColor textColor = themeColorsRef().textPrimary;
    QPalette p = m_titleLabel->palette();
    p.setColor(QPalette::WindowText, textColor);
    m_titleLabel->setPalette(p);
    m_subtitleLabel->setPalette(p);
    
    m_titleLabel->setFont(themeFont(Typography::FontRole::Caption).toQFont());
    m_subtitleLabel->setFont(themeFont(Typography::FontRole::Caption).toQFont());

    m_backButton->onThemeUpdated();
    m_paneToggleButton->onThemeUpdated();
    m_iconButton->onThemeUpdated();
    m_minimizeButton->onThemeUpdated();
    m_maximizeButton->onThemeUpdated();
    m_closeButton->onThemeUpdated();

    updateCaptionButtonSizes();
    syncActivationOpacity();
}

bool TitleBar::eventFilter(QObject* watched, QEvent* event) {
    if (watched == window() && event->type() == QEvent::WindowStateChange) {
        syncWindowState();
    }
    return fluent::windowing::TitleBar::eventFilter(watched, event);
}

void TitleBar::showEvent(QShowEvent* event) {
    fluent::windowing::TitleBar::showEvent(event);
    if (window()) {
        window()->installEventFilter(this);
        syncWindowState();
    }
}

void TitleBar::syncWindowState() {
    if (!window()) return;
    const bool isMaximized = window()->isMaximized();
    m_maximizeButton->setIconGlyph(isMaximized ? Typography::Icons::ChromeRestore : Typography::Icons::ChromeMaximize, IconSize);
    m_maximizeButton->setAccessibleName(isMaximized ? tr("Restore") : tr("Maximize"));
    fluent::status_info::ToolTip::attach(m_maximizeButton, isMaximized ? tr("Restore") : tr("Maximize"));
    // 最大化时铺满右上角不设置圆角，普通窗口状态保留控制区圆角
    m_closeButton->setCornerRadii(QMargins(0, isMaximized ? 0 : themeRadius().control, 0, 0));
}

void TitleBar::resizeEvent(QResizeEvent* event) {
    fluent::windowing::TitleBar::resizeEvent(event);
    updateLayout();
}

void TitleBar::updateHeight() {
    // 存在搜索框或用户卡片时强制使用扩展高度
    bool requiresTall = (m_searchWidget != nullptr || m_accountWidget != nullptr);
    int targetHeight = (m_heightOption == HeightOption::Tall || requiresTall) ? TitleBarTallHeight : TitleBarStandardHeight;
    setTitleBarHeight(targetHeight);
}

void TitleBar::updateCaptionButtonSizes() {
    const int h = titleBarHeight();
    m_backButton->setFixedSize(AppTitleBarButtonWidth, h);
    m_paneToggleButton->setFixedSize(AppTitleBarButtonWidth, h);
    m_iconButton->setFixedSize(AppTitleBarButtonWidth, h);
    m_minimizeButton->setFixedSize(CaptionButtonWidth, h);
    m_maximizeButton->setFixedSize(CaptionButtonWidth, h);
    m_closeButton->setFixedSize(CaptionButtonWidth, h);
}

void TitleBar::updateLayout() {
    const int w = width();
    const int h = titleBarHeight();

    int currentX = 0;
    
    // 左侧按钮区域
    if (m_backButtonVisible) {
        m_backButton->setGeometry(currentX, 0, AppTitleBarButtonWidth, h);
        currentX += AppTitleBarButtonWidth;
    }
    if (m_paneToggleButtonVisible) {
        m_paneToggleButton->setGeometry(currentX, 0, AppTitleBarButtonWidth, h);
        currentX += AppTitleBarButtonWidth;
    }

    // LeftHeaderPresenter: 原生 WinUI 3 左侧自定义内容
    if (m_leftHeaderWidget) {
        m_leftHeaderWidget->setGeometry(currentX, (h - m_leftHeaderWidget->height()) / 2, m_leftHeaderWidget->width(), m_leftHeaderWidget->height());
        currentX += m_leftHeaderWidget->width();
    }

    // 左侧内容区域的起始需要一个安全间距（如果左侧没有按钮/LeftHeader，默认左边距为16）
    if (!m_backButtonVisible && !m_paneToggleButtonVisible && !m_leftHeaderWidget) {
        currentX += ElementSpacing;
    } else if (m_leftHeaderWidget) {
        currentX += ElementSpacing; // WinUI 规范中，如果有 LeftHeader，它和 Icon 之间有间距
    }

    if (!m_icon.isNull()) {
        m_iconButton->setGeometry(currentX, 0, AppTitleBarButtonWidth, h);
        currentX += AppTitleBarButtonWidth;
    }

    int rightOffset = w;
    rightOffset -= CaptionButtonWidth;
    m_closeButton->setGeometry(rightOffset, 0, CaptionButtonWidth, h);
    rightOffset -= CaptionButtonWidth;
    m_maximizeButton->setGeometry(rightOffset, 0, CaptionButtonWidth, h);
    rightOffset -= CaptionButtonWidth;
    m_minimizeButton->setGeometry(rightOffset, 0, CaptionButtonWidth, h);

    if (m_accountWidget) {
        rightOffset -= m_accountWidget->width();
        m_accountWidget->setGeometry(rightOffset, (h - m_accountWidget->height()) / 2, m_accountWidget->width(), m_accountWidget->height());
        rightOffset -= ElementSpacing;
    }

    // 搜索框基于窗口几何中心绝对居中
    int searchLeft = w;
    if (m_searchWidget) {
        int sw = m_searchWidget->width();
        int sh = m_searchWidget->height();
        searchLeft = (w - sw) / 2;
        m_searchWidget->setGeometry(searchLeft, (h - sh) / 2, sw, sh);
    }

    int titleMaxRight = qMin(rightOffset, m_searchWidget ? searchLeft - ElementSpacing : rightOffset) - ElementSpacing;

    // --- WinUI 3 Compact Mode 折叠避让逻辑 ---
    int titleRequiredWidth = 0;
    if (!m_title.isEmpty()) {
        titleRequiredWidth += m_titleLabel->fontMetrics().horizontalAdvance(m_titleLabel->text()) + 4 + TitleMarginRight;
    }
    if (!m_subtitle.isEmpty()) {
        titleRequiredWidth += m_subtitleLabel->fontMetrics().horizontalAdvance(m_subtitleLabel->text()) + 4 + ElementSpacing;
    }

    bool shouldCompact = false;
    if (m_searchWidget != nullptr || m_customContentWidget != nullptr) {
        if (!m_isCompact) {
            // 检测是否发生挤压
            bool isSqueezed = false;
            if (m_searchWidget && (currentX + titleRequiredWidth > searchLeft - ElementSpacing)) {
                isSqueezed = true;
            } else if (m_customContentWidget) {
                int avail = titleMaxRight - currentX;
                if (avail < titleRequiredWidth + m_customContentWidget->minimumSizeHint().width()) {
                    isSqueezed = true;
                }
            }
            if (isSqueezed) {
                m_compactModeThresholdWidth = w;
                m_isCompact = true;
                shouldCompact = true;
            }
        } else {
            // 已处于折叠态，如果宽度恢复到记录的临界值，则展开
            if (w >= m_compactModeThresholdWidth) {
                m_compactModeThresholdWidth = 0;
                m_isCompact = false;
                shouldCompact = false;
            } else {
                shouldCompact = true;
            }
        }
    } else {
        m_isCompact = false;
    }

    if (shouldCompact) {
        m_titleLabel->hide();
        m_subtitleLabel->hide();
    } else {
        if (!m_title.isEmpty()) m_titleLabel->show();
        if (!m_subtitle.isEmpty()) m_subtitleLabel->show();

        if (!m_title.isEmpty()) {
            int titleWidth = m_titleLabel->fontMetrics().horizontalAdvance(m_titleLabel->text()) + 4;
            int availWidth = qMax(0, titleMaxRight - currentX);
            m_titleLabel->setGeometry(currentX, 0, qMin(titleWidth, availWidth), h);
            currentX += qMin(titleWidth, availWidth) + TitleMarginRight;
        }

        if (!m_subtitle.isEmpty()) {
            int subtitleWidth = m_subtitleLabel->fontMetrics().horizontalAdvance(m_subtitleLabel->text()) + 4;
            int availWidth = qMax(0, titleMaxRight - currentX);
            m_subtitleLabel->setGeometry(currentX, 0, qMin(subtitleWidth, availWidth), h);
            currentX += qMin(subtitleWidth, availWidth) + ElementSpacing;
        }
    }

    // 处理自适应延伸的自定义内容（非绝对居中）
    if (m_customContentWidget) {
        int availWidth = qMax(0, titleMaxRight - currentX);
        m_customContentWidget->setGeometry(currentX, (h - m_customContentWidget->height()) / 2, availWidth, m_customContentWidget->height());
    }

    // 同步基类以正确排除系统按钮区域的拖拽命中
    setSystemReservedTrailingWidth(w - rightOffset);
}

void TitleBar::syncActivationOpacity() {
    qreal opacity = isWindowActive() ? 1.0 : InactiveOpacity;
    m_backButton->setContentOpacity(opacity);
    m_paneToggleButton->setContentOpacity(opacity);
    m_iconButton->setContentOpacity(opacity);
    m_minimizeButton->setContentOpacity(opacity);
    m_maximizeButton->setContentOpacity(opacity);
    m_closeButton->setContentOpacity(opacity);
}

} // namespace ui::window
