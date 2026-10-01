#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>

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
