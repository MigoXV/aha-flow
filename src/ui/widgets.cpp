#include "ui/widgets.h"
#include "ui/theme.h"

#include <QPainter>
#include <QSvgRenderer>

namespace aha
{
void IconComboBox::paintEvent(QPaintEvent *event)
{
    QComboBox::paintEvent(event);
    QPainter painter(this);
    QSvgRenderer renderer(assetPath("chevron", property("dark").toBool()));
    renderer.render(&painter, QRectF(width() - 36, (height() - 20) / 2.0, 20, 20));
}

void ElidedLabel::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setPen(palette().color(QPalette::WindowText));
    painter.setFont(font());
    painter.drawText(contentsRect(), Qt::AlignVCenter | Qt::AlignLeft,
                     fontMetrics().elidedText(text(), Qt::ElideLeft, contentsRect().width()));
}

ToggleSwitch::ToggleSwitch(const QString &label, QWidget *parent) : QCheckBox(parent)
{
    setAccessibleName(label);
    setToolTip(label);
    setFixedSize(40, 32);
    setCursor(Qt::PointingHandCursor);
}

bool ToggleSwitch::hitButton(const QPoint &point) const
{
    return rect().contains(point);
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    const bool dark = property("dark").toBool();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (!isEnabled())
        painter.setOpacity(0.55);
    QSvgRenderer renderer(assetPath(isChecked() ? "toggle-on" : "toggle-off", dark));
    renderer.render(&painter, QRectF(2, 6, 36, 20));
    if (hasFocus()) {
        painter.setPen(QPen(theme(dark).focus, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(1, 1, 38, 30), 6, 6);
    }
}

ThemeChoice::ThemeChoice(bool darkChoice, QWidget *parent) : QPushButton(parent), darkChoice_(darkChoice)
{
    setObjectName(darkChoice ? "themeAbyssus" : "themeVallum");
    setAccessibleName(darkChoice ? QStringLiteral("苍渊，深色主题") : QStringLiteral("白垣，浅色主题"));
    setCheckable(true);
    setFixedHeight(88);
    setMinimumWidth(190);
    setCursor(Qt::PointingHandCursor);
}

void ThemeChoice::paintEvent(QPaintEvent *)
{
    const auto &t = theme(property("dark").toBool());
    const auto &preview = theme(darkChoice_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(underMouse() ? t.hover : t.surface);
    p.setPen(QPen(isChecked() || hasFocus() ? t.focus : t.border, isChecked() || hasFocus() ? 2 : 1));
    p.drawRoundedRect(QRectF(1, 1, width() - 2, height() - 2), 8, 8);
    p.setBrush(preview.background);
    p.setPen(QPen(preview.border, 1));
    p.drawRoundedRect(QRectF(16, 24, 32, 40), 6, 6);
    p.fillRect(QRect(20, 30, 24, 3), preview.secondaryText);
    p.fillRect(QRect(20, 37, 20, 2), preview.accent);
    p.setFont(uiFont(14, true));
    p.setPen(t.text);
    p.drawText(QRect(64, 23, width() - 80, 22), Qt::AlignVCenter,
               darkChoice_ ? QStringLiteral("苍渊") : QStringLiteral("白垣"));
    p.setFont(uiFont(12));
    p.setPen(t.secondaryText);
    const QString description = isChecked()   ? QStringLiteral("已选择")
                                : darkChoice_ ? QStringLiteral("深色与微光")
                                              : QStringLiteral("米灰与青灰");
    p.drawText(QRect(64, 49, width() - 80, 18), Qt::AlignVCenter, description);
}
} // namespace aha
