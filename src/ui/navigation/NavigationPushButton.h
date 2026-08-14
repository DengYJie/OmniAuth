#pragma once

#include "ui/navigation/NavigationWidget.h"
#include <QString>

namespace ui::navigation {

/**
 * @brief 基础图文导航按钮
 * 
 * 实现标准的 Fluent 图标+文字渲染、悬停背景及折叠过渡中的缩进插值。
 */
class NavigationPushButton : public NavigationWidget {
    Q_OBJECT

public:
    explicit NavigationPushButton(const QString& iconGlyph, const QString& text,
                                  bool isSelectable = true, QWidget* parent = nullptr);
    ~NavigationPushButton() override = default;

    QString text() const { return m_text; }
    void setText(const QString& text);

    QString iconGlyph() const { return m_iconGlyph; }
    void setIconGlyph(const QString& glyph);

    int iconSize() const { return m_iconSize; }
    void setIconSize(int size);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

    /// 随折叠进度消除子项缩进以契合 48px 紧凑栏。
    virtual int iconDrawX() const;

protected:
    QString m_iconGlyph;
    QString m_text;
    int m_iconSize = Typography::IconSize::Standard;
};

} // namespace ui::navigation
