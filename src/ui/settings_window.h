#pragma once

#include "core/settings.h"
#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTabWidget;
class QToolButton;

namespace aha
{
struct SettingsControls {
    QComboBox *address, *models;
    QLineEdit *language, *apiKey, *correctionUrl, *cacheDirectory;
    QCheckBox *allowUntrusted, *enableAha, *enableCorrection, *enableCache;
    QSpinBox *sliceSeconds;
    QLabel *modelsError;
    QToolButton *browse;
};

class SettingsWindow final : public QDialog
{
    Q_OBJECT
  public:
    explicit SettingsWindow(const AppSettings &settings, QWidget *parent);
    const SettingsControls &controls() const
    {
        return controls_;
    }
    void applyTheme(const QString &name);
  signals:
    void themeSelected(const QString &name);

  protected:
    bool eventFilter(QObject *object, QEvent *event) override;

  private:
    SettingsControls controls_{};
    QTabWidget *tabs_;
    bool dragging_ = false;
    QPoint dragStart_, windowStart_;
};
} // namespace aha
