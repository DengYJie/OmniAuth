#include "ui/navigation/NavigationPushButton.h"
#include <FluentQt/Design.h>
#include <QPainter>

#include "ui/navigation/NavigationMetrics.h"

namespace ui::navigation {

    NavigationPushButton::NavigationPushButton(const QString& iconGlyph, const QString& text, bool isSelectable, QWidget* parent)
        : NavigationWidget(isSelectable, parent)
        , m_iconGlyph(iconGlyph)
        , m_text(text)
    {
        setCursor(Qt::PointingHandCursor);
        if (!text.isEmpty()) {
            setAccessibleItemName(text);
        }
    }

    void NavigationPushButton::setText(const QString& text) {
        m_text = text;
        if (!text.isEmpty()) {
            setAccessibleItemName(text);
        }
        update();
    }

    void NavigationPushButton::setIconGlyph(const QString& glyph) {
        m_iconGlyph = glyph;
        update();
    }

    void NavigationPushButton::setIconSize(int size) {
        if (m_iconSize == size) return;
        m_iconSize = size;
        updateGeometry();
        update();
    }

    QSize NavigationPushButton::sizeHint() const {
        if (m_orientation == Orientation::Horizontal) {
            if (isFooterItem() || m_text.isEmpty()) {
                return QSize(kTopBarItemHeight, kTopBarItemHeight);
            }
            int w = kTopBarItemHorizontalPadding * 2;
            if (!m_iconGlyph.isEmpty()) {
                w += m_iconSize;
            }
            if (!m_text.isEmpty()) {
                QFont f = themeFont(Typography::FontRole::Body).toQFont();
                QFontMetrics fm(f);
                if (!m_iconGlyph.isEmpty()) {
                    w += kTopBarButtonSpacing; // 图标与文字的间距
                }
                w += fm.horizontalAdvance(m_text);
            }
            return QSize(w, kTopBarItemHeight);
        }
        return QSize(0, kItemHeight);
    }

    int NavigationPushButton::iconDrawX() const {
        if (m_orientation == Orientation::Horizontal) {
            if (isFooterItem() || m_text.isEmpty()) {
                return qMax(0, (width() - m_iconSize) / 2);
            }
            return kTopBarItemHorizontalPadding;
        }
        const qreal expandedLeft = kRowLeftInset + kContentStart + m_nodeDepth * themeSpacing().large * m_expandProgress;
        return qRound(expandedLeft);
    }

    void NavigationPushButton::paintEvent(QPaintEvent* /*event*/) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const auto& colors = colorsRef();
        const auto radius = themeRadius().control;
        const auto spacing = themeSpacing();

        const int iconX = iconDrawX();

        QColor bg = currentBackgroundColor();
        if (bg.alpha() > 0) {
            QRectF itemRect;
            if (m_orientation == Orientation::Horizontal) {
                if (m_text.isEmpty() || isFooterItem()) {
                    // Top 纯图标/Footer 项：1:1 正方形卡片
                    const int cardSize = height() - kItemBgPaddingV * 2;
                    const qreal cardX = (width() - cardSize) / 2.0;
                    itemRect = QRectF(cardX, kItemBgPaddingV, cardSize, cardSize);
                } else {
                    itemRect = QRectF(spacing.xSmall, kItemBgPaddingV,
                        width() - spacing.xSmall * 2,
                        height() - kItemBgPaddingV * 2);
                }
            } else {
                if (isCompacted()) {
                    // 紧凑模式：1:1 正方形卡片
                    const int cardSize = qMin(width() - spacing.xSmall * 2, height() - kItemBgPaddingV * 2);
                    const qreal cardX = (width() - cardSize) / 2.0;
                    const qreal cardY = (height() - cardSize) / 2.0;
                    itemRect = QRectF(cardX, cardY, cardSize, cardSize);
                } else {
                    itemRect = QRectF(spacing.xSmall, kItemBgPaddingV,
                        width() - spacing.xSmall * 2,
                        height() - kItemBgPaddingV * 2);
                }
            }
            painter.setPen(Qt::NoPen);
            painter.setBrush(bg);
            painter.drawRoundedRect(itemRect, radius, radius);
        }

        if (!m_iconGlyph.isEmpty()) {
            painter.setFont(Typography::Icons::font(m_iconSize));
            painter.setPen(m_isSelected ? colors.textPrimary : colors.textSecondary);
            const QString glyph = Typography::Icons::glyphForSize(m_iconGlyph, m_iconSize);
            painter.drawText(QRect(iconX, 0, m_iconSize, height()), Qt::AlignCenter, glyph);
        }

        const float alpha = currentTextAlpha();
        if (alpha > 0.0f && !m_text.isEmpty() && !(m_orientation == Orientation::Horizontal && isFooterItem())) {
            painter.setFont(themeFont(m_isSelected ? Typography::FontRole::BodyStrong
                : Typography::FontRole::Body).toQFont());
            QColor textCol = colors.textPrimary;
            textCol.setAlphaF(alpha);
            painter.setPen(textCol);

            int textLeft = 0;
            int maxTextWidth = 0;
            if (m_orientation == Orientation::Horizontal) {
                textLeft = kTopBarItemHorizontalPadding;
                if (!m_iconGlyph.isEmpty()) {
                    textLeft += m_iconSize + kTopBarButtonSpacing;
                }
                maxTextWidth = qMax(0, width() - textLeft - kTopBarItemHorizontalPadding);
            }
            else {
                textLeft = kTextLeftOffset + qRound(m_nodeDepth * spacing.large * m_expandProgress);
                maxTextWidth = qMax(0, width() - textLeft - spacing.small);
            }
            painter.drawText(QRect(textLeft, 0, maxTextWidth, height()),
                Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                m_text);
        }

        if (hasKeyboardFocus()) {
            drawFocusVisual(painter, QRectF(0, 0, width(), height()));
        }
    }

} // namespace ui::navigation
