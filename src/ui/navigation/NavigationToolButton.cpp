#include "ui/navigation/NavigationToolButton.h"

#include <QMoveEvent>
#include <QShowEvent>
#include <QVariantAnimation>

#include "ui/navigation/NavigationMetrics.h"

namespace ui::navigation {

    NavigationToolButton::NavigationToolButton(const QString& iconGlyph, QWidget* parent)
        : NavigationPushButton(iconGlyph, QString(), false, parent)
    {
        setFixedSize(kTopBarItemHeight, kTopBarItemHeight);
        setFocusPolicy(Qt::TabFocus);
    }

    void NavigationToolButton::setCompacted(bool /*compacted*/) {
        // 纯图标按钮在 compact 下尺寸/图标位置不变
    }

    void NavigationToolButton::setOrientation(Orientation orientation) {
        NavigationPushButton::setOrientation(orientation);
        // 基类按方向 setFixedHeight 会覆盖固定尺寸，这里重设回 48x48 方形热区
        setFixedSize(kTopBarItemHeight, kTopBarItemHeight);
    }

    int NavigationToolButton::iconDrawX() const {
        return qMax(0, (width() - m_iconSize) / 2);
    }

    void NavigationToolButton::showEvent(QShowEvent* event) {
        NavigationPushButton::showEvent(event);
        if (!m_animatedMove || NavigationWidget::isReducedMotion())
            return;

        const QPoint targetPos = pos();
        if (targetPos.x() > 0) {
            const QPoint startPos = targetPos + QPoint(20, 0); // 从右侧 20px 处平滑滑入
            m_isMovingProgrammatically = true;
            move(startPos);
            m_isMovingProgrammatically = false;

            if (!m_slideAnimation) {
                m_slideAnimation = new QVariantAnimation(this);
                m_slideAnimation->setEasingCurve(themeAnimation().decelerate);
                connect(m_slideAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
                    m_isMovingProgrammatically = true;
                    move(val.toPoint());
                    m_isMovingProgrammatically = false;
                });
            }
            m_slideAnimation->stop();
            m_slideAnimation->setDuration(themeAnimation().normal);
            m_slideAnimation->setStartValue(startPos);
            m_slideAnimation->setEndValue(targetPos);
            m_slideAnimation->start();
        }
    }

    void NavigationToolButton::moveEvent(QMoveEvent* event) {
        NavigationPushButton::moveEvent(event);
        if (!m_animatedMove || !isVisible() || NavigationWidget::isReducedMotion() || m_isMovingProgrammatically)
            return;

        const QPoint oldP = event->oldPos();
        const QPoint newP = event->pos();
        if (oldP.isNull() || oldP == newP || oldP.x() < 0)
            return;

        if (!m_slideAnimation) {
            m_slideAnimation = new QVariantAnimation(this);
            m_slideAnimation->setEasingCurve(themeAnimation().decelerate);
            connect(m_slideAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
                m_isMovingProgrammatically = true;
                move(val.toPoint());
                m_isMovingProgrammatically = false;
            });
        }

        m_slideAnimation->stop();
        m_slideAnimation->setDuration(themeAnimation().normal);
        m_slideAnimation->setStartValue(oldP);
        m_slideAnimation->setEndValue(newP);
        m_isMovingProgrammatically = true;
        move(oldP); // 从上一个位置平滑滑动到新位置
        m_isMovingProgrammatically = false;
        m_slideAnimation->start();
    }

} // namespace ui::navigation
