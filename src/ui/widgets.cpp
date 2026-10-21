#include "ui/widgets.h"
#include "ui/theme.h"

#include <QPainter>
#include <QSvgRenderer>
#include <QTextBlock>
#include <QTextCursor>
#include <cmath>

namespace aha
{
void IconComboBox::paintEvent(QPaintEvent *event)
{
    QComboBox::paintEvent(event);
    QPainter painter(this);
    QSvgRenderer renderer(assetPath("chevron", property("dark").toBool()));
    renderer.render(&painter, QRectF(width() - 26, (height() - 16) / 2.0, 16, 16));
}

void ElidedLabel::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setPen(palette().color(QPalette::WindowText));
    painter.setFont(font());
    painter.drawText(contentsRect(), Qt::AlignVCenter | Qt::AlignLeft,
                     fontMetrics().elidedText(text(), Qt::ElideLeft, contentsRect().width()));
}

void SessionTimeLabel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setPen(palette().color(QPalette::WindowText));
    p.setFont(font());
    const QString value = text();
    const qreal digitWidth = value.size() <= 5 ? 8.0 : 32.0 / (value.size() - 1);
    qreal x = 0;
    for (const QChar c : value) {
        const qreal cell = c == QLatin1Char(':') ? 4.0 : digitWidth;
        p.drawText(QRectF(x, 0, cell, height()), Qt::AlignCenter, QString(c));
        x += cell;
    }
}

InputLevelMeter::InputLevelMeter(QWidget *parent) : QProgressBar(parent)
{
    setRange(0, 1000);
    setValue(0);
    setTextVisible(false);
    setFixedHeight(4);
    setMinimumWidth(64);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    clock_.start();
    frameTimer_.setTimerType(Qt::PreciseTimer);
    frameTimer_.setInterval(16);
    connect(&frameTimer_, &QTimer::timeout, this, &InputLevelMeter::advanceFrame);
}

void InputLevelMeter::setLevel(float level)
{
    setValue(std::isfinite(level) ? qRound(qBound(0.0f, level, 1.0f) * maximum()) : 0);
    lastSampleNs_ = clock_.nsecsElapsed();
    startFrames();
}

void InputLevelMeter::clearLevel()
{
    frameTimer_.stop();
    setValue(0);
    displayedLevel_ = 0;
    update();
}

void InputLevelMeter::startFrames()
{
    if (isVisible() && !frameTimer_.isActive() && (value() > 0 || displayedLevel_ > 0)) {
        lastFrameNs_ = clock_.nsecsElapsed();
        frameTimer_.start();
    }
}

void InputLevelMeter::advanceFrame()
{
    const qint64 now = clock_.nsecsElapsed();
    const qreal elapsedMs = (now - lastFrameNs_) / 1e6;
    lastFrameNs_ = now;
    // Missing input must decay instead of holding a stale microphone level indefinitely.
    if (now - lastSampleNs_ > 150000000)
        setValue(0);
    const qreal target = qreal(value()) / maximum();
    const qreal timeConstantMs = target > displayedLevel_ ? 12 : 80;
    displayedLevel_ += (target - displayedLevel_) * -std::expm1(-elapsedMs / timeConstantMs);
    if (qAbs(displayedLevel_ - target) * width() < 0.1) {
        displayedLevel_ = target;
        if (target == 0)
            frameTimer_.stop();
    }
    update();
}

void InputLevelMeter::showEvent(QShowEvent *event)
{
    QProgressBar::showEvent(event);
    if (clock_.nsecsElapsed() - lastSampleNs_ > 150000000)
        clearLevel();
    else
        startFrames();
}

void InputLevelMeter::hideEvent(QHideEvent *event)
{
    frameTimer_.stop();
    QProgressBar::hideEvent(event);
}

void InputLevelMeter::paintEvent(QPaintEvent *)
{
    const auto &t = theme(property("dark").toBool());
    QPainter p(this);
    p.fillRect(rect(), t.border);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(t.strongAccent);
    p.drawRect(QRectF(0, 0, width() * displayedLevel_, height()));
}

void TranscriptView::paintEvent(QPaintEvent *event)
{
    QTextEdit::paintEvent(event);
    QPainter p(viewport());
    p.setFont(uiFont(11));
    p.setPen(theme(property("dark").toBool()).secondaryText);
    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        const QRect line = cursorRect(QTextCursor(block));
        if (line.top() > viewport()->height() + 18)
            break;
        if (line.bottom() < 0 || block.text().isEmpty() || block.userState() != 1)
            continue;
        p.drawText(QRect(0, line.top() - 18, viewport()->width(), 16), Qt::AlignVCenter, QStringLiteral("实时转写"));
    }
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
    renderer.render(&painter, QRectF(4, 8, 32, 16));
    if (hasFocus()) {
        painter.setPen(QPen(theme(dark).focus, 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(1, 1, 38, 30), 3, 3);
    }
}

ThemeChoice::ThemeChoice(bool darkChoice, QWidget *parent) : QPushButton(parent), darkChoice_(darkChoice)
{
    setObjectName(darkChoice ? "themeAbyssus" : "themeVallum");
    setAccessibleName(darkChoice ? QStringLiteral("苍渊，深色主题") : QStringLiteral("白垣，浅色主题"));
    setCheckable(true);
    setFixedHeight(64);
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
    p.setPen(QPen(isChecked() || hasFocus() ? t.focus : t.border, hasFocus() ? 1.5 : 1));
    p.drawRoundedRect(QRectF(1, 1, width() - 2, height() - 2), 4, 4);
    p.setBrush(preview.background);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(16, 12, 32, 40), 3, 3);
    p.fillRect(QRect(20, 18, 24, 3), preview.secondaryText);
    p.fillRect(QRect(20, 25, 20, 2), preview.accent);
    p.setFont(uiFont(12, true));
    p.setPen(t.text);
    p.drawText(QRect(64, 13, width() - 80, 18), Qt::AlignVCenter,
               darkChoice_ ? QStringLiteral("苍渊") : QStringLiteral("白垣"));
    p.setFont(uiFont(11));
    p.setPen(t.secondaryText);
    const QString description = isChecked()   ? QStringLiteral("已选择")
                                : darkChoice_ ? QStringLiteral("深色与微光")
                                              : QStringLiteral("米灰与青灰");
    p.drawText(QRect(64, 33, width() - 80, 18), Qt::AlignVCenter, description);
}
} // namespace aha
