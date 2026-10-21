#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>

namespace aha
{
class IconComboBox final : public QComboBox
{
  public:
    using QComboBox::QComboBox;

  protected:
    void paintEvent(QPaintEvent *event) override;
};

class ElidedLabel final : public QLabel
{
  public:
    using QLabel::QLabel;

  protected:
    void paintEvent(QPaintEvent *event) override;
};

// Fixed digit cells keep the session clock stable even with proportional fonts.
class SessionTimeLabel final : public QLabel
{
  public:
    using QLabel::QLabel;

  protected:
    void paintEvent(QPaintEvent *event) override;
};

// Paint every measured change without native progress-bar chunk/repaint thresholds.
class InputLevelMeter final : public QProgressBar
{
  public:
    explicit InputLevelMeter(QWidget *parent);
    void setLevel(float level);

  protected:
    void paintEvent(QPaintEvent *event) override;
};

// Draw the live-result label outside selectable text, keeping finalized text plain.
class TranscriptView final : public QTextEdit
{
  public:
    using QTextEdit::QTextEdit;

  protected:
    void paintEvent(QPaintEvent *event) override;
};

class ToggleSwitch final : public QCheckBox
{
  public:
    explicit ToggleSwitch(const QString &label, QWidget *parent);

  protected:
    void paintEvent(QPaintEvent *event) override;
    bool hitButton(const QPoint &point) const override;
};

class ThemeChoice final : public QPushButton
{
  public:
    explicit ThemeChoice(bool darkChoice, QWidget *parent);

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    bool darkChoice_;
};
} // namespace aha
