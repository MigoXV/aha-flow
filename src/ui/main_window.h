#pragma once

#include "network/engine_client.h"
#include "session/session_controller.h"
#include <QElapsedTimer>
#include <QSettings>
#include <QWidget>
#include "ui/system_tray.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;
class QStackedLayout;
class QTextEdit;
class QToolButton;
class QTimer;

namespace aha
{
class SettingsWindow;
class InputLevelMeter;
class MainWindow final : public QWidget
{
    Q_OBJECT
  public:
    explicit MainWindow(const AppSettings &settings, bool autoStart = true, QWidget *parent = nullptr);
    void setRuntimeOverrides(const QString &engine, const QString &apiKey, bool allowSelfSigned, bool apiKeyProvided);
    void refreshModels();
    void startRecognition();
    void toggleCompact();
    void enableSystemTray(const QIcon &icon, SystemTray::Availability availability = {});
    void restoreWindow();
    void requestExit();
    void showSettings(bool visible);
    SessionController *session()
    {
        return &session_;
    }
  signals:
    void firstFrameRendered();
    void exitReady();

  protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *event) override;

  private:
    void saveSettings();
    void updateTrayState();
    void toggleTrayWindow();
    void setStatus(const QString &label, const QString &detail);
    void updateTranscript(const Transcript &result);
    void renderTranscript(int index);
    void updateElapsed();
    void toggleRecording();
    void applyTheme();
    void updateSessionMetadata();
    AppSettings settings_;
    QSettings store_;
    EngineClient engine_;
    SessionController session_;
    QElapsedTimer elapsed_;
    QList<Transcript> items_;
    QString statusLabel_ = QStringLiteral("待机");
    QString statusDetailText_;
    bool activeCacheEnabled_ = true;
    QString runtimeEngine_, runtimeKey_;
    bool runtimeSelfSigned_ = false;
    bool runtimeKeyProvided_ = false;
    bool compact_ = false, recording_ = false, closing_ = false, canClose_ = false;
    bool dragging_ = false, resizing_ = false;
    bool firstPaint_ = true, manuallySized_ = false;
    Qt::Edges resizeEdges_;
    QList<QWidget *> resizeHandles_;
    QPoint dragOrigin_;
    QRect resizeOrigin_;
    QSize expandedSize_{360, 232};
    QStackedLayout *contentStack_;
    QLabel *emptyTitle_, *emptyHelp_;
    QWidget *header_, *body_, *brand_;
    QLabel *dot_, *caption_, *status_, *time_, *modelsError_, *historyHint_;
    QLabel *statusDot_, *statusDetail_, *cacheHint_;
    QToolButton *record_, *compactRecord_, *settingsButton_, *expandButton_;
    QComboBox *address_, *models_;
    QLineEdit *language_, *apiKey_, *correctionUrl_, *cacheDirectory_;
    QCheckBox *allowUntrusted_, *enableAha_, *enableCorrection_, *enableCache_;
    QSlider *sliceSeconds_;
    SystemTray *tray_ = nullptr;
    QToolButton *closeButton_ = nullptr;
    SettingsWindow *settingsWindow_ = nullptr;
    QTextEdit *transcript_;
    InputLevelMeter *meter_;
    QTimer *modelsTimer_;
};
} // namespace aha
