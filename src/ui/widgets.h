#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>
#include <QElapsedTimer>
#include <QTimer>

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

// Audio-driven target with fast attack and smooth release, rendered independently of input callbacks.
class InputLevelMeter final : public QProgressBar
{
  public:
    explicit InputLevelMeter(QWidget *parent);
    void setLevel(float level);
    void clearLevel();

  protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

  private:
    void advanceFrame();
    void startFrames();
    QTimer frameTimer_;
    QElapsedTimer clock_;
    qint64 lastFrameNs_ = 0, lastSampleNs_ = 0;
    qreal displayedLevel_ = 0;
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
