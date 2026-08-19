#include "Stepper.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QAccessible>
#include <QAccessibleEvent>
#include <QFontMetrics>
#include "design/IconCatalog.h"
#include "design/Spacing.h"

namespace ui::widget {

namespace {
    constexpr int kNodeRadius = 10;            ///< 节点半径 (无对应 Spacing token，保留设计值)
    constexpr int kNodeDiameter = kNodeRadius * 2; ///< 节点直径
    constexpr int kTrackHeight = ::Spacing::Border::Focused; ///< 连线高度 (2px)
    constexpr int kLabelGap = ::Spacing::XSmall;  ///< 节点与标签间距 (4px)
    constexpr int kMinSegmentWidth = 64;       ///< 最小步骤段宽
}

/*!
 * \class Stepper
 * \brief 实现由 Fluent 2 驱动的横向步骤指示器组件
 */

Stepper::Stepper(QWidget* parent)
    : QWidget(parent) {
    setFocusPolicy(m_clickable ? Qt::StrongFocus : Qt::NoFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true);

    m_animation = new QVariantAnimation(this);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this, &Stepper::onAnimationValueChanged);
}

Stepper::~Stepper() = default;

void Stepper::addStep(const QString& title, const QString& subtitle) {
    m_steps.append({title, subtitle, false});
    m_currentStep = qBound(0, m_currentStep, static_cast<int>(m_steps.size() - 1));
    m_progressValue = m_currentStep;
    m_focusIndex = m_currentStep;
    updateAccessibleState();
    updateGeometry();
    update();
}

void Stepper::setSteps(const QStringList& steps) {
    m_steps.clear();
    for (const auto& step : steps) {
        m_steps.append({step, QString(), false});
    }
    m_currentStep = qBound(0, m_currentStep, static_cast<int>(m_steps.size() - 1));
    m_progressValue = m_currentStep;
    m_focusIndex = m_currentStep;
    updateAccessibleState();
    updateGeometry();
    update();
}

void Stepper::setSubtitle(int index, const QString& subtitle) {
    if (index >= 0 && index < m_steps.size()) {
        m_steps[index].subtitle = subtitle;
        updateGeometry();
        update();
    }
}

void Stepper::setCurrentStep(int index) {
    if (m_steps.isEmpty()) return;
    index = qBound(0, index, static_cast<int>(m_steps.size() - 1));
    if (m_currentStep == index) return;

    if (!m_animated || (m_animation->state() != QAbstractAnimation::Running && qAbs(m_progressValue - index) < 0.001)) {
        m_currentStep = index;
        m_progressValue = index;
        m_focusIndex = index;
        m_animation->stop();
        updateAccessibleState();
        update();
        return;
    }

    m_currentStep = index;
    m_focusIndex = index;
    updateAccessibleState();

    const qreal from = m_progressValue;
    m_animation->stop();
    m_progressValue = from;
    m_animation->setEasingCurve(themeAnimation().standard);
    m_animation->setStartValue(from);
    m_animation->setEndValue(static_cast<qreal>(index));
    int duration = (index > from) ? themeAnimation().normal : (themeAnimation().normal - 50);
    m_animation->setDuration(duration);
    m_animation->start();
}

void Stepper::setError(int index, bool hasError) {
    if (index >= 0 && index < m_steps.size()) {
        m_steps[index].hasError = hasError;
        updateAccessibleState();
        update();
    }
}

void Stepper::setStepsClickable(bool clickable) {
    if (m_clickable != clickable) {
        m_clickable = clickable;
        setFocusPolicy(m_clickable ? Qt::StrongFocus : Qt::NoFocus);
        if (!m_clickable) {
            m_hoveredStep = -1;
            m_pressedStep = -1;
        }
        setCursor(m_clickable ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void Stepper::setLabelsVisible(bool visible) {
    if (m_labelsVisible != visible) {
        m_labelsVisible = visible;
        updateGeometry();
        update();
    }
}

void Stepper::setAnimated(bool animated) {
    if (m_animated != animated) {
        m_animated = animated;
        if (!m_animated && m_animation->state() == QAbstractAnimation::Running) {
            m_animation->stop();
            m_progressValue = m_currentStep;
            update();
        }
    }
}

void Stepper::clear() {
    m_steps.clear();
    m_currentStep = 0;
    m_progressValue = 0;
    m_hoveredStep = -1;
    m_pressedStep = -1;
    m_focusIndex = 0;
    m_animation->stop();
    updateAccessibleState();
    updateGeometry();
    update();
}

void Stepper::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    if (m_steps.isEmpty()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto& colors = themeColorsRef();
    const bool isRtl = layoutDirection() == Qt::RightToLeft;
    const int n = m_steps.size();
    const qreal segmentWidth = static_cast<qreal>(width()) / n;
    const qreal centerY = ::Spacing::Small + kNodeRadius; // Top padding 8

    auto getCx = [&](int i) -> qreal {
        qreal x = (i + 0.5) * segmentWidth;
        return isRtl ? width() - x : x;
    };

    // 1. Draw tracks
    for (int i = 0; i < n - 1; ++i) {
        qreal cx1 = getCx(i);
        qreal cx2 = getCx(i + 1);

        qreal x1 = isRtl ? cx1 - kNodeRadius : cx1 + kNodeRadius;
        qreal x2 = isRtl ? cx2 + kNodeRadius : cx2 - kNodeRadius;

        QPainterPath trackPath;
        trackPath.moveTo(x1, centerY);
        trackPath.lineTo(x2, centerY);

        QPen trackPen(colors.strokeDefault, kTrackHeight);
        trackPen.setCapStyle(Qt::RoundCap);
        
        bool isErrorLeft = m_steps[isRtl ? i + 1 : i].hasError;
        bool isErrorRight = m_steps[isRtl ? i : i + 1].hasError;

        if (m_progressValue > i && !isErrorLeft && !isErrorRight) {
            qreal fillRatio = qBound(0.0, m_progressValue - i, 1.0);
            qreal xMid = x1 + (x2 - x1) * fillRatio;
            
            QPainterPath fillPath;
            fillPath.moveTo(x1, centerY);
            fillPath.lineTo(xMid, centerY);

            QPainterPath remainPath;
            remainPath.moveTo(xMid, centerY);
            remainPath.lineTo(x2, centerY);

            QPen fillPen(colors.accentDefault, kTrackHeight);
            fillPen.setCapStyle(Qt::RoundCap);

            painter.strokePath(remainPath, trackPen);
            painter.strokePath(fillPath, fillPen);
        } else {
            painter.strokePath(trackPath, trackPen);
        }
    }

    QFont bodyFont = themeFont(Typography::FontRole::Body).toQFont();
    QFont bodyStrongFont = themeFont(Typography::FontRole::BodyStrong).toQFont();
    QFont captionFont = themeFont(Typography::FontRole::Caption).toQFont();
    
    QFont numFont = bodyStrongFont;
    numFont.setPixelSize(12);

    // 2. Draw nodes & labels
    for (int i = 0; i < n; ++i) {
        qreal cx = getCx(i);
        QRectF nodeRect(cx - kNodeRadius, centerY - kNodeRadius, kNodeDiameter, kNodeDiameter);

        bool isActive = (i == m_currentStep);
        bool isCompleted = (i < m_progressValue);
        bool hasError = m_steps[i].hasError;
        bool isDisabled = !isEnabled();
        bool isHovered = (i == m_hoveredStep) && m_clickable && !isDisabled;
        bool isPressed = (i == m_pressedStep) && m_clickable && !isDisabled;

        QColor fillColor = colors.bgLayer;
        QColor strokeColor = colors.strokeDefault;
        int strokeWidth = 1;
        QColor iconColor = colors.textTertiary;
        QColor labelColor = colors.textTertiary;
        QFont labelFont = captionFont;
        QString iconText = QString::number(i + 1);

        if (isDisabled) {
            fillColor = colors.bgLayer;
            strokeColor = colors.controlDisabled;
            iconColor = colors.textDisabled;
            labelColor = colors.textDisabled;
        } else if (hasError) {
            fillColor = colors.systemCritical;
            strokeColor = Qt::transparent;
            strokeWidth = 0;
            iconColor = colors.textOnAccent;
            labelColor = colors.systemCritical;
            iconText = Typography::Icons::Dismiss;
        } else if (isActive) {
            strokeColor = colors.accentDefault;
            strokeWidth = 2;
            iconColor = colors.textAccentPrimary;
            labelColor = colors.textAccentPrimary;
            labelFont = captionFont;
            labelFont.setBold(true);
        } else if (isCompleted) {
            fillColor = colors.accentDefault;
            strokeColor = Qt::transparent;
            strokeWidth = 0;
            iconColor = colors.textOnAccent;
            labelColor = colors.textPrimary;
            iconText = Typography::Icons::CheckMark;
        } else {
            // Inactive uses defaults
        }

        if (isHovered && !hasError && !isActive) {
            if (isCompleted) {
                fillColor = colors.accentSecondary;
            } else {
                fillColor = colors.subtleSecondary;
                strokeColor = colors.strokeStrong;
                labelColor = colors.textAccentPrimary;
            }
        }
        if (isPressed && !hasError && !isActive) {
            if (isCompleted) {
                fillColor = colors.accentTertiary;
            } else {
                fillColor = colors.subtleTertiary;
            }
        }

        // Draw Focus Ring
        if (m_focused && i == m_focusIndex) {
            QPen focusPen(colors.strokeFocusOuter, 1);
            painter.setPen(focusPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(nodeRect.adjusted(-2, -2, 2, 2));
            QPen focusInnerPen(colors.strokeFocusInner, 1);
            painter.setPen(focusInnerPen);
            painter.drawEllipse(nodeRect.adjusted(-1, -1, 1, 1));
        }

        // Draw Node Background & Stroke
        painter.setBrush(fillColor);
        if (strokeWidth > 0) {
            painter.setPen(QPen(strokeColor, strokeWidth));
        } else {
            painter.setPen(Qt::NoPen);
        }
        painter.drawEllipse(nodeRect);

        // Draw Icon / Number
        painter.setPen(iconColor);
        if (hasError || isCompleted) {
            Typography::Icons::paintGlyph(painter, nodeRect, iconText, 10);
        } else {
            numFont.setPixelSize((i >= 9) ? 9 : 11);
            painter.setFont(numFont);
            painter.drawText(nodeRect, Qt::AlignCenter, iconText);
        }

        // Draw Labels
        if (m_labelsVisible) {
            qreal labelY = centerY + kNodeRadius + kLabelGap;
            qreal labelWidth = qMax(0.0, segmentWidth - ::Spacing::Small);
            QRectF labelRect(cx - labelWidth / 2, labelY, labelWidth, Typography::LineHeight::Body);

            QFontMetrics fm(labelFont);
            QString elidedTitle = fm.elidedText(m_steps[i].title, Qt::ElideRight, labelWidth);
            painter.setFont(labelFont);
            painter.setPen(labelColor);
            painter.drawText(labelRect, Qt::AlignCenter, elidedTitle);

            if (!m_steps[i].subtitle.isEmpty()) {
                labelY += Typography::LineHeight::Body;
                QRectF subRect(cx - labelWidth / 2, labelY, labelWidth, Typography::LineHeight::Caption);
                QFontMetrics fmSub(captionFont);
                QString elidedSub = fmSub.elidedText(m_steps[i].subtitle, Qt::ElideRight, labelWidth);
                painter.setFont(captionFont);
                painter.setPen(colors.textSecondary);
                painter.drawText(subRect, Qt::AlignCenter, elidedSub);
            }
        }
    }
}

void Stepper::mousePressEvent(QMouseEvent* event) {
    if (m_clickable && event->button() == Qt::LeftButton) {
        int index = hitTest(event->pos());
        if (index != -1) {
            m_pressedStep = index;
            update();
        }
    }
    QWidget::mousePressEvent(event);
}

void Stepper::mouseMoveEvent(QMouseEvent* event) {
    if (m_clickable) {
        int index = hitTest(event->pos());
        if (index != m_hoveredStep) {
            m_hoveredStep = index;
            update();
        }
    }
    QWidget::mouseMoveEvent(event);
}

void Stepper::mouseReleaseEvent(QMouseEvent* event) {
    if (m_clickable && event->button() == Qt::LeftButton) {
        int index = hitTest(event->pos());
        if (index != -1 && index == m_pressedStep) {
            emit stepClicked(index);
        }
        m_pressedStep = -1;
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void Stepper::leaveEvent(QEvent* event) {
    if (m_hoveredStep != -1) {
        m_hoveredStep = -1;
        m_pressedStep = -1;
        update();
    }
    QWidget::leaveEvent(event);
}

void Stepper::keyPressEvent(QKeyEvent* event) {
    if (!m_clickable || m_steps.isEmpty()) {
        QWidget::keyPressEvent(event);
        return;
    }
    
    if (event->key() == Qt::Key_Left) {
        m_focusIndex = qMax(0, m_focusIndex - 1);
        update();
    } else if (event->key() == Qt::Key_Right) {
        m_focusIndex = qMin(static_cast<int>(m_steps.size() - 1), m_focusIndex + 1);
        update();
    } else if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return) {
        emit stepClicked(m_focusIndex);
    } else {
        QWidget::keyPressEvent(event);
    }
}

void Stepper::focusInEvent(QFocusEvent* event) {
    m_focused = (event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason);
    if (m_focused && m_focusIndex < 0) {
        m_focusIndex = m_currentStep;
    }
    update();
    QWidget::focusInEvent(event);
}

void Stepper::focusOutEvent(QFocusEvent* event) {
    m_focused = false;
    update();
    QWidget::focusOutEvent(event);
}

QSize Stepper::sizeHint() const {
    if (m_steps.isEmpty()) return QSize(0, 0);
    int h = 2 * ::Spacing::Small + kNodeDiameter; // 8 top, 8 bottom
    if (m_labelsVisible) {
        h += kLabelGap + Typography::LineHeight::Body; 
        bool hasAnySubtitle = false;
        for (const auto& step : m_steps) {
            if (!step.subtitle.isEmpty()) {
                hasAnySubtitle = true;
                break;
            }
        }
        if (hasAnySubtitle) {
            h += Typography::LineHeight::Caption;
        }
    }
    int w = qMax(kMinSegmentWidth * static_cast<int>(m_steps.size()), 200);
    return QSize(w, h);
}

QSize Stepper::minimumSizeHint() const {
    return sizeHint();
}

void Stepper::onThemeUpdated() {
    update();
}

void Stepper::onAnimationValueChanged(const QVariant& value) {
    m_progressValue = value.toReal();
    update();
}

int Stepper::hitTest(const QPoint& pos) const {
    if (m_steps.isEmpty()) return -1;
    const int n = m_steps.size();
    const qreal segmentWidth = static_cast<qreal>(width()) / n;
    
    for (int i = 0; i < n; ++i) {
        qreal xStart = i * segmentWidth;
        qreal xEnd = (i + 1) * segmentWidth;
        if (pos.x() >= xStart && pos.x() < xEnd) {
            return layoutDirection() == Qt::RightToLeft ? n - 1 - i : i;
        }
    }
    return -1;
}

void Stepper::updateAccessibleState() {
    if (m_steps.isEmpty()) {
        setAccessibleName(QString());
        QAccessibleEvent ev(this, QAccessible::NameChanged);
        QAccessible::updateAccessibility(&ev);
        return;
    }
    
    QString stateStr = m_steps[m_currentStep].hasError ? QStringLiteral("错误") : QStringLiteral("进行中");
    QString name = QStringLiteral("步骤导航，当前第 %1 步，共 %2 步：%3，%4")
        .arg(m_currentStep + 1).arg(m_steps.size())
        .arg(m_steps[m_currentStep].title).arg(stateStr);
    
    setAccessibleName(name);
    QAccessibleEvent ev(this, QAccessible::NameChanged);
    QAccessible::updateAccessibility(&ev);
}

} // namespace ui::widget
