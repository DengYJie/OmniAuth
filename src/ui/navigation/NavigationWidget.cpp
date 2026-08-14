#include "ui/navigation/NavigationWidget.h"
#include "ui/navigation/NavigationFocusHost.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <FluentQt/Design.h>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

namespace ui::navigation {

bool NavigationWidget::isReducedMotion() {
#ifdef Q_OS_WIN
    BOOL enabled = TRUE;
    if (::SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0)) {
        return enabled == FALSE;
    }
#endif
    return false;
}

NavigationWidget::NavigationWidget(bool isSelectable, QWidget* parent)
    : QWidget(parent)
    , m_isSelectable(isSelectable)
{
    setMouseTracking(true);
    setFocusPolicy(m_isSelectable ? Qt::TabFocus : Qt::NoFocus);
    setFixedHeight(kItemHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_hoverAnimation = new QVariantAnimation(this);
    m_hoverAnimation->setDuration(::Animation::Duration::Fast);
    m_hoverAnimation->setEasingCurve(::Animation::getEasing(::Animation::EasingType::Standard));
    connect(m_hoverAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_hoverProgress = value.toFloat();
        update();
    });
}

void NavigationWidget::setCompacted(bool compacted) {
    if (m_isCompacted == compacted) return;
    m_isCompacted = compacted;
    setExpandProgress(compacted ? 0.0f : 1.0f);
    updateGeometry();
    update();
}

void NavigationWidget::setSelected(bool selected) {
    if (!m_isSelectable || m_isSelected == selected) return;
    m_isSelected = selected;
    update();
    emit selectedChanged(selected);
}

void NavigationWidget::setNodeDepth(int depth) {
    if (m_nodeDepth == depth) return;
    m_nodeDepth = depth;
    updateGeometry();
    update();
}

void NavigationWidget::setHoverProgress(float progress) {
    m_hoverProgress = progress;
    update();
}

void NavigationWidget::setExpandProgress(float progress) {
    if (m_orientation == NavigationOrientation::Horizontal)
        return;
    const float p = qBound(0.0f, progress, 1.0f);
    if (qFuzzyCompare(p, m_expandProgress))
        return;
    m_expandProgress = p;
    m_isCompacted = (p < 0.01f);
    update();
}

fluent::FluentElement::Theme NavigationWidget::cachedEffectiveTheme() const {
    if (!m_themeValid) {
        m_effectiveTheme = effectiveTheme();
        m_themeValid = true;
    }
    return m_effectiveTheme;
}

const fluent::FluentElement::Colors& NavigationWidget::colorsRef() const {
    return fluent::ThemeRegistry::instance().colors(cachedEffectiveTheme() == fluent::FluentElement::Theme::Dark);
}

void NavigationWidget::onThemeUpdated() {
    m_themeValid = false;
    update();
}

void NavigationWidget::setOrientation(NavigationOrientation orientation) {
    if (m_orientation == orientation)
        return;
    m_orientation = orientation;
    if (orientation == NavigationOrientation::Horizontal) {
        m_isCompacted = false;
        m_expandProgress = 1.0f;
        setFixedHeight(kTopBarItemHeight);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    } else {
        setFixedHeight(kItemHeight);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    updateGeometry();
    update();
}

QRectF NavigationWidget::indicatorRect() const {
    const auto s = themeSpacing();
    if (m_orientation == NavigationOrientation::Horizontal) {
        // Horizontal：底部居中水平条
        const qreal w = qMin(static_cast<qreal>(width()), static_cast<qreal>(kTopSelectionIndicatorWidth));
        const qreal x = (width() - w) / 2.0;
        const qreal y = height() - kTopSelectionIndicatorHeight - 2; // 贴近底边
        return QRectF(x, y, w, kTopSelectionIndicatorHeight);
    }
    const int x = s.xSmall + m_nodeDepth * s.large;
    return QRectF(x, (height() - kSelectionIndicatorHeight) / 2.0,
                  kSelectionIndicatorWidth, kSelectionIndicatorHeight);
}

void NavigationWidget::click() {
    emit clicked(true);
}

INavigationFocusHost* NavigationWidget::navigationFocusHost() const {
    for (QWidget* p = parentWidget(); p; p = p->parentWidget()) {
        if (auto* host = dynamic_cast<INavigationFocusHost*>(p)) {
            return host;
        }
    }
    return nullptr;
}

void NavigationWidget::setAccessibleItemName(const QString& name) {
    setAccessibleName(name);
}

void NavigationWidget::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Up:
    case Qt::Key_Down:
        // 向上委托宿主，保持列表焦点闭环
        if (auto* host = navigationFocusHost()) {
            host->moveFocusBy(event->key() == Qt::Key_Up ? -1 : 1);
            event->accept();
            return;
        }
        break;
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        emit clicked(true);
        event->accept();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void NavigationWidget::focusInEvent(QFocusEvent* event) {
    // 仅键盘 Tab/快捷键到达时渲染视觉焦点环，指针点击不渲染
    switch (event->reason()) {
    case Qt::TabFocusReason:
    case Qt::BacktabFocusReason:
    case Qt::ShortcutFocusReason:
        m_keyboardFocused = true;
        break;
    default:
        m_keyboardFocused = false;
        break;
    }
    update();
    QWidget::focusInEvent(event);
}

void NavigationWidget::focusOutEvent(QFocusEvent* event) {
    m_keyboardFocused = false;
    update();
    QWidget::focusOutEvent(event);
}

void NavigationWidget::drawFocusVisual(QPainter& painter, const QRectF& rect) const {
    if (!m_keyboardFocused) return;
    const auto& colors = colorsRef();
    const auto r = themeRadius();
    constexpr int kFocusRingGap = 2;
    painter.setBrush(Qt::NoBrush);
    const QRectF outer = rect.adjusted(kFocusRingGap, kFocusRingGap,
                                       -kFocusRingGap, -kFocusRingGap);
    painter.setPen(QPen(colors.strokeFocusOuter, 1));
    painter.drawRoundedRect(outer, r.control, r.control);
    painter.setPen(QPen(colors.strokeFocusInner, 1));
    const qreal innerRadius = qMax<qreal>(0.0, qreal(r.control) - 1.0);
    painter.drawRoundedRect(outer.adjusted(1, 1, -1, -1), innerRadius, innerRadius);
}

void NavigationWidget::enterEvent(FluentEnterEvent* event) {
    QWidget::enterEvent(event);
    m_isHovered = true;
    if (isReducedMotion()) {
        m_hoverProgress = 1.0f;
        update();
        return;
    }
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(1.0f);
    m_hoverAnimation->start();
}

void NavigationWidget::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    m_isHovered = false;
    m_isPressed = false;
    if (isReducedMotion()) {
        m_hoverProgress = 0.0f;
        update();
        return;
    }
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(0.0f);
    m_hoverAnimation->start();
}

void NavigationWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_keyboardFocused = false;
        setFocus(Qt::MouseFocusReason);
        m_isPressed = true;
        update();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void NavigationWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_isPressed) {
        m_isPressed = false;
        update();
        if (rect().contains(event->pos())) {
            emit clicked(true);
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

QColor NavigationWidget::currentBackgroundColor() const {
    const auto& colors = colorsRef();
    if (m_isPressed) {
        return colors.subtleTertiary;
    }
    if (m_isSelected) {
        if (m_orientation == NavigationOrientation::Horizontal)
            return Qt::transparent;
        return colors.subtleSecondary;
    }
    if (m_hoverProgress > 0.0f) {
        QColor target = colors.subtleSecondary;
        target.setAlphaF(target.alphaF() * m_hoverProgress);
        return target;
    }
    return Qt::transparent;
}

float NavigationWidget::currentTextAlpha() const {
    if (m_orientation == NavigationOrientation::Horizontal) {
        return 1.0f;
    }
    if (m_expandProgress < 0.2f) return 0.0f;
    return qBound(0.0f, (m_expandProgress - 0.2f) / 0.8f, 1.0f);
}

} // namespace ui::navigation
