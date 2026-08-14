#include "ui/navigation/NavigationIndicator.h"

#include <QDebug>
#include <QPainter>
#include <QPaintEvent>
#include <QShowEvent>
#include <QVariantAnimation>

#include "ui/navigation/NavigationMetrics.h"
#include "ui/navigation/NavigationWidget.h"

namespace ui::navigation {

    fluent::FluentElement::Theme NavigationIndicator::cachedEffectiveTheme() const {
        if (!m_themeValid) {
            m_effectiveTheme = effectiveTheme();
            m_themeValid = true;
        }
        return m_effectiveTheme;
    }

    const fluent::FluentElement::Colors& NavigationIndicator::colorsRef() const {
        return fluent::ThemeRegistry::instance().colors(cachedEffectiveTheme() == fluent::FluentElement::Theme::Dark);
    }

    void NavigationIndicator::onThemeUpdated() {
        m_themeValid = false;
        update();
    }

    NavigationIndicator::NavigationIndicator(QWidget* parent)
        : QWidget(parent)
        , m_startRect(0, 0, kSelectionIndicatorWidth, kSelectionIndicatorHeight)
        , m_targetRect(m_startRect)
        , m_currentRect(m_startRect)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        hide();

        m_flightAnimation = new QVariantAnimation(this);
        m_flightAnimation->setEasingCurve(themeAnimation().decelerate);
        connect(m_flightAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            const qreal p = v.toDouble();

            if (m_animMode == AnimationMode::PortalReturn) {
                // Portal Return: 水平指示条从左向右展开恢复
                const qreal morphW = qMax(0.0, m_targetRect.width() * p);
                m_currentRect = QRectF(m_targetRect.x(), m_targetRect.y(), morphW, m_targetRect.height());
            }
            else if (m_animMode == AnimationMode::CrossWindowPortal) {
                // Cross Window Portal: 按宿主方向分派形态，顶栏与浮层各放独立动效。
                if (m_position == NavigationPosition::Top) {
                    // 顶栏水平条：向左收缩消失（左侧固定，右边界左移，宽度衰减到 0）
                    const qreal morphW = qMax(0.0, m_startRect.width() * (1.0 - p));
                    m_currentRect = QRectF(m_startRect.x(), m_startRect.y(), morphW, m_startRect.height());
                }
                else {
                    // 浮层垂直条：从顶部生成向下生长（顶部固定，高度从 0 增长到目标高度）
                    const qreal growH = m_targetRect.height() * p;
                    m_currentRect = QRectF(m_targetRect.x(), m_targetRect.top(), m_targetRect.width(), growH);
                }
            }
            else if (m_position == NavigationPosition::Top) {
                // Top 模式：水平胶囊，X 轴滑行，完全镜像 Left 模式的 Y 轴逻辑（X↔Y 对换）
                const bool portalMode = qAbs(m_startRect.y() - m_targetRect.y()) > 0.5;
                const bool movingRight = m_targetRect.x() >= m_startRect.x();

                if (portalMode) {
                    // 跨行（理论上 Top 模式不应出现，但保留兜底）：先收缩宽度到 0，再在目标位置展开
                    if (p < 0.5) {
                        const qreal lp     = p * 2.0;
                        const qreal morphW = qMax(0.0, m_startRect.width() * (1.0 - lp));
                        m_currentRect.setY(m_startRect.y());
                        m_currentRect.setHeight(m_startRect.height());
                        if (movingRight) {
                            m_currentRect.setLeft(m_startRect.right() - morphW);
                            m_currentRect.setWidth(morphW);
                        } else {
                            m_currentRect.setLeft(m_startRect.left());
                            m_currentRect.setWidth(morphW);
                        }
                    } else {
                        const qreal lp     = (p - 0.5) * 2.0;
                        const qreal morphW = qMax(0.0, m_targetRect.width() * lp);
                        m_currentRect.setY(m_targetRect.y());
                        m_currentRect.setHeight(m_targetRect.height());
                        if (movingRight) {
                            m_currentRect.setLeft(m_targetRect.left());
                            m_currentRect.setWidth(morphW);
                        } else {
                            m_currentRect.setLeft(m_targetRect.right() - morphW);
                            m_currentRect.setWidth(morphW);
                        }
                    }
                } else {
                    // 同行（正常情况）：Y 固定，X 插值滑动，宽高固定
                    m_currentRect.setX(m_startRect.x() + (m_targetRect.x() - m_startRect.x()) * p);
                    m_currentRect.setY(m_startRect.y());
                    m_currentRect.setWidth(m_startRect.width() + (m_targetRect.width() - m_startRect.width()) * p);
                    m_currentRect.setHeight(m_startRect.height());
                }
            } else {
                // Left 模式：跨层级（X 不同）时用"穿越门"效果
                const bool portalMode = qAbs(m_startRect.x() - m_targetRect.x()) > 0.5;
                const bool movingUp   = m_targetRect.y() <= m_startRect.y();

                if (portalMode) {
                    if (p < 0.5) {
                        const qreal lp    = p * 2.0;
                        const qreal morphH = qMax(0.0, m_startRect.height() * (1.0 - lp));
                        m_currentRect.setX(m_startRect.x());
                        m_currentRect.setWidth(m_startRect.width());
                        if (movingUp) {
                            m_currentRect.setTop(m_startRect.top());
                            m_currentRect.setHeight(morphH);
                        } else {
                            m_currentRect.setTop(m_startRect.bottom() - morphH);
                            m_currentRect.setHeight(morphH);
                        }
                    } else {
                        const qreal lp    = (p - 0.5) * 2.0;
                        const qreal morphH = qMax(0.0, m_targetRect.height() * lp);
                        m_currentRect.setX(m_targetRect.x());
                        m_currentRect.setWidth(m_targetRect.width());
                        if (movingUp) {
                            m_currentRect.setTop(m_targetRect.bottom() - morphH);
                            m_currentRect.setHeight(morphH);
                        } else {
                            m_currentRect.setTop(m_targetRect.top());
                            m_currentRect.setHeight(morphH);
                        }
                    }
                } else {
                    m_currentRect.setX(m_startRect.x() + (m_targetRect.x() - m_startRect.x()) * p);
                    m_currentRect.setY(m_startRect.y() + (m_targetRect.y() - m_startRect.y()) * p);
                    m_currentRect.setWidth(m_startRect.width());
                    m_currentRect.setHeight(m_startRect.height());
                }
            }

            setGeometry(m_startRect.united(m_targetRect).toRect().adjusted(-5, -5, 5, 5));
            update();
        });
        connect(m_flightAnimation, &QVariantAnimation::finished, this, [this]() {
            qDebug() << "[NavIndicator] flight finished! finalRect:" << m_targetRect;
            const AnimationMode finishedMode = m_animMode;
            // 同步为目标几何，避免动画/无动画路径终点不一致导致后续 isSamePosition 误判
            m_animMode = AnimationMode::Normal;
            m_startRect = m_targetRect;
            m_currentRect = m_targetRect;
            hide();
            // Portal 动画由调用方各自处理收尾（顶栏释放归属 / 浮层恢复原生指示条），不发 flightFinished
            if (finishedMode == AnimationMode::CrossWindowPortal)
                return;
            emit flightFinished();
        });
    }

    void NavigationIndicator::setNavigationPosition(NavigationPosition pos) {
        if (m_position == pos) return;
        m_position = pos;
        update();
    }

    void NavigationIndicator::activateAt(const QRectF& targetRect, int durationMs, bool animated)
    {
        m_animMode = AnimationMode::Normal;
        const bool samePos = isSamePosition(targetRect);
        qDebug() << "[NavIndicator] activateAt targetRect:" << targetRect
                 << "currentRect:" << m_currentRect << "samePos:" << samePos
                 << "durationMs:" << durationMs << "animated:" << animated;

        if (samePos) {
            // 位置未变，但仍需让 item 恢复常驻指示条（可能刚被清空）
            m_flightAnimation->stop();
            emit flightFinished();
            return;
        }

        raiseToTop();

        if (!animated || durationMs <= 0 || NavigationWidget::isReducedMotion()) {
            qDebug() << "[NavIndicator] activateAt instant snap";
            m_flightAnimation->stop();
            m_startRect = targetRect;
            m_targetRect = targetRect;
            m_currentRect = targetRect;
            hide();
            emit flightFinished();
            return;
        }

        m_startRect = m_currentRect;
        m_targetRect = targetRect;
        m_flightAnimation->stop();
        m_flightAnimation->setEasingCurve(themeAnimation().decelerate);
        m_flightAnimation->setStartValue(0.0);
        m_flightAnimation->setEndValue(1.0);
        m_flightAnimation->setDuration(durationMs);
        setGeometry(m_startRect.united(m_targetRect).toRect().adjusted(-5, -5, 5, 5));
        show();
        emit flightStarted();
        m_flightAnimation->start();
    }

    void NavigationIndicator::playPortalReturn(const QRectF& targetRect, int durationMs)
    {
        qDebug() << "[NavIndicator] playPortalReturn targetRect:" << targetRect << "durationMs:" << durationMs;
        if (NavigationWidget::isReducedMotion() || durationMs <= 0) {
            m_startRect = targetRect;
            m_targetRect = targetRect;
            m_currentRect = targetRect;
            hide();
            emit flightFinished();
            return;
        }

        raiseToTop();
        m_animMode = AnimationMode::PortalReturn;
        // 起始位置：目标宽度为0
        m_startRect = QRectF(targetRect.x(), targetRect.y(), 0, targetRect.height());
        m_targetRect = targetRect;
        m_currentRect = m_startRect;
        m_flightAnimation->stop();
        m_flightAnimation->setEasingCurve(themeAnimation().decelerate);
        m_flightAnimation->setStartValue(0.0);
        m_flightAnimation->setEndValue(1.0);
        m_flightAnimation->setDuration(durationMs);
        setGeometry(targetRect.toRect().adjusted(-5, -5, 5, 5));
        show();
        emit flightStarted();
        m_flightAnimation->start();
    }

    void NavigationIndicator::playCrossWindowPortal(const QRectF& startRect, const QRectF& targetRect, int durationMs)
    {
        qDebug() << "[NavIndicator] playCrossWindowPortal startRect:" << startRect << "targetRect:" << targetRect << "durationMs:" << durationMs;
        if (NavigationWidget::isReducedMotion() || durationMs <= 0) {
            m_startRect = targetRect;
            m_targetRect = targetRect;
            m_currentRect = targetRect;
            hide();
            // Portal 动画不发 flightFinished（由调用方各自处理收尾）
            return;
        }

        raiseToTop();
        m_animMode = AnimationMode::CrossWindowPortal;
        m_startRect = startRect;
        m_targetRect = targetRect;
        m_flightAnimation->stop();
        m_flightAnimation->setEasingCurve(themeAnimation().decelerate);
        m_flightAnimation->setStartValue(0.0);
        m_flightAnimation->setEndValue(1.0);
        m_flightAnimation->setDuration(durationMs);

        // 几何缓冲与初始帧按宿主方向分派：顶栏收缩只需起始矩形，
        // 浮层向下生长（顶部固定、高度 0）只需覆盖目标矩形。
        if (m_position == NavigationPosition::Top) {
            m_currentRect = m_startRect;
            setGeometry(m_startRect.toRect().adjusted(-5, -5, 5, 5));
        } else {
            // 顶部固定、高度 0 的初始帧
            m_currentRect = QRectF(m_targetRect.x(), m_targetRect.top(), m_targetRect.width(), 0.0);
            setGeometry(m_targetRect.toRect().adjusted(-5, -5, 5, 5));
        }
        show();
        emit flightStarted();
        m_flightAnimation->start();
    }

    void NavigationIndicator::hideIndicator()
    {
        if (m_flightAnimation)
            m_flightAnimation->stop();
        hide();
    }

    bool NavigationIndicator::isFlying() const
    {
        return m_flightAnimation && m_flightAnimation->state() == QAbstractAnimation::Running;
    }

    bool NavigationIndicator::isSamePosition(const QRectF& targetRect) const
    {
        // 目标几何与当前几何几乎相同（<1px）时视为"位置未变"，短路避免无谓动画。
        return qAbs(targetRect.x() - m_currentRect.x()) < 1.0
            && qAbs(targetRect.y() - m_currentRect.y()) < 1.0;
    }

    void NavigationIndicator::raiseToTop()
    {
        raise();
    }

    void NavigationIndicator::paintEvent(QPaintEvent* /*event*/)
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(colorsRef().accentDefault);

        // geometry 比实际 rect 大一圈（-5,-5,5,5），需映射回局部坐标
        const QPointF offset = -geometry().topLeft();
        const QRectF local = m_currentRect.translated(offset);
        painter.drawRoundedRect(local.adjusted(0, 0, -1, -1),
                                ::CornerRadius::Indicator, ::CornerRadius::Indicator);
    }

    void NavigationIndicator::showEvent(QShowEvent* event)
    {
        QWidget::showEvent(event);
        raiseToTop();
    }

} // namespace ui::navigation
