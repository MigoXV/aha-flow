#pragma once

#include "network/engine_client.h"
#include "session/session_controller.h"
#include <QElapsedTimer>
#include <QSettings>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QScrollArea;
class QSpinBox;
class QTextEdit;
class QToolButton;
class QTimer;

namespace aha
{
class MainWindow final : public QWidget
{
    Q_OBJECT
  public:
    explicit MainWindow(const AppSettings &settings, bool autoStart = true, QWidget *parent = nullptr);
    void setRuntimeOverrides(const QString &engine, const QString &apiKey, bool allowSelfSigned, bool apiKeyProvided);
    void refreshModels();
    void startRecognition();
    void toggleCompact();
    void showSettings(bool visible);
    SessionController *session()
    {
        return &session_;
    }
  signals:
    void firstFrameRendered();

  protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

  private:
    void saveSettings();
    void setStatus(const QString &label, const QString &detail);
    void updateTranscript(const Transcript &result);
    void renderTranscript(int index);
    void updateElapsed();
    void toggleRecording();
    AppSettings settings_;
    QSettings store_;
    EngineClient engine_;
    SessionController session_;
    QElapsedTimer elapsed_;
    QList<Transcript> items_;
    QString statusLabel_ = QStringLiteral("待机");
    QString runtimeEngine_, runtimeKey_;
    bool runtimeSelfSigned_ = false;
    bool runtimeKeyProvided_ = false;
    bool compact_ = false, recording_ = false, closing_ = false, canClose_ = false;
    bool dragging_ = false, resizing_ = false;
    bool firstPaint_ = true;
    QPoint dragOrigin_;
    QRect resizeOrigin_;
    QSize expandedSize_{300, 250};
    QWidget *header_, *body_, *brand_, *resizeHandle_;
    QLabel *dot_, *caption_, *version_, *status_, *time_, *compactTime_, *modelsError_, *historyHint_;
    QToolButton *record_, *compactRecord_, *settingsButton_, *expandButton_;
    QComboBox *address_, *models_;
    QLineEdit *language_, *apiKey_, *correctionUrl_, *cacheDirectory_;
    QCheckBox *allowUntrusted_, *enableAha_, *enableCorrection_, *enableCache_;
    QSpinBox *sliceSeconds_;
    QScrollArea *settingsPanel_ = nullptr;
    QTextEdit *transcript_;
    QProgressBar *meter_;
    QTimer *modelsTimer_;
};
} // namespace aha
