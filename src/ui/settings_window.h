#pragma once

#include "core/settings.h"
#include <QDialog>
#include <QMetaObject>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QScreen;
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
    void fitToContents();
  signals:
    void themeSelected(const QString &name);

  protected:
    bool eventFilter(QObject *object, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

  private:
    SettingsControls controls_{};
    QTabWidget *tabs_;
    void scheduleFit();
    void watchScreen(QScreen *screen);
    QMetaObject::Connection geometryConnection_, dpiConnection_;
    bool fitPending_ = false, screenConnected_ = false;
    bool dragging_ = false;
    QPoint dragStart_, windowStart_;
};
} // namespace aha
