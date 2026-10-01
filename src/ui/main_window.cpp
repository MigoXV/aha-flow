#include "ui/main_window.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QProgressBar>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

namespace aha
{
namespace
{
QIcon icon(const QString &name, const QColor &color = QColor("#111111"))
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (name == "mic") {
        p.drawRoundedRect(QRectF(9, 3, 6, 12), 3, 3);
        p.drawArc(QRectF(6, 6, 12, 13), 180 * 16, 180 * 16);
        p.drawLine(12, 19, 12, 22);
        p.drawLine(8, 22, 16, 22);
    } else if (name == "stop") {
        p.setBrush(color);
        p.drawRoundedRect(QRectF(7, 7, 10, 10), 1, 1);
    } else if (name == "close") {
        p.drawLine(6, 6, 18, 18);
        p.drawLine(18, 6, 6, 18);
    } else if (name == "settings") {
        p.drawEllipse(QRectF(5, 5, 14, 14));
        p.drawEllipse(QRectF(9, 9, 6, 6));
        for (int i = 0; i < 8; ++i) {
            p.save();
            p.translate(12, 12);
            p.rotate(i * 45);
            p.drawLine(0, -7, 0, -10);
            p.restore();
        }
    } else if (name == "resize") {
        p.drawLine(5, 15, 13, 23);
        p.drawLine(5, 20, 8, 23);
    } else {
        p.drawLine(5, 5, 10, 10);
        p.drawLine(14, 14, 19, 19);
        p.drawLine(5, 5, 5, 10);
        p.drawLine(5, 5, 10, 5);
        p.drawLine(19, 19, 14, 19);
        p.drawLine(19, 19, 19, 14);
    }
    return QIcon(pixmap);
}
QToolButton *button(const QString &name, const QString &tip, QWidget *parent)
{
    auto *b = new QToolButton(parent);
    b->setIcon(icon(name));
    b->setIconSize(QSize(18, 18));
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    b->setFixedSize(26, 26);
    return b;
}
} // namespace
MainWindow::MainWindow(const AppSettings &settings, bool autoStart, QWidget *parent)
    : QWidget(parent), settings_(settings)
{
    setObjectName("floatingWindow");
    setWindowTitle("aha-flow");
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumSize(280, 220);
    resize(expandedSize_);
    setStyleSheet(QStringLiteral(
        "QWidget{font-family:'Segoe UI','Noto Sans',sans-serif;font-size:11px;color:#111111;}"
        "QFrame#shell{background:white;border:1px solid rgba(32,38,46,40);border-radius:8px;}"
        "QToolButton{border:0;background:#f2f2f2;border-radius:5px;}QToolButton:hover{background:#e5e5e5;}"
        "QToolButton:disabled{background:#f5f5f5;}"
        "QToolButton#recordButton,QToolButton#compactRecordButton{background:#111111;}"
        "QToolButton#recordButton:hover,QToolButton#compactRecordButton:hover{background:#000000;}"
        "QToolButton#recordButton:disabled,QToolButton#compactRecordButton:disabled{background:#a6a6a6;}"
        "QLineEdit,QComboBox,QSpinBox{background:white;border:1px solid #dddddd;border-radius:4px;padding:3px;}"
        "QLabel#title{font-size:13px;font-weight:600;}QLabel#version{font-size:9px;color:#8a8a8a;}"
        "QTextEdit{border:0;background:white;font-size:12px;}QScrollArea{border:0;background:white;}"
        "QProgressBar{border:0;background:#eeeeee;height:3px;border-radius:1px;}QProgressBar::chunk{background:#111111;"
        "}"
        "QLabel#historyHint{font-size:9px;color:#777777;}"));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 6, 6, 6);
    auto *shell = new QFrame(this);
    shell->setObjectName("shell");
    outer->addWidget(shell);
    auto *layout = new QVBoxLayout(shell);
    layout->setContentsMargins(10, 7, 8, 4);
    layout->setSpacing(5);
    header_ = new QWidget(shell);
    auto *headerLayout = new QHBoxLayout(header_);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(5);
    dot_ = new QLabel(header_);
    dot_->setFixedSize(7, 7);
    headerLayout->addWidget(dot_);
    brand_ = new QWidget(header_);
    auto *brandLayout = new QVBoxLayout(brand_);
    brandLayout->setContentsMargins(0, 0, 0, 0);
    brandLayout->setSpacing(0);
    auto *title = new QLabel("aha-flow", brand_);
    title->setObjectName("title");
    brandLayout->addWidget(title);
    version_ = new QLabel("v" + QCoreApplication::applicationVersion(), brand_);
    version_->setObjectName("version");
    brandLayout->addWidget(version_);
    headerLayout->addWidget(brand_, 1);
    caption_ = new QLabel(header_);
    caption_->setMinimumWidth(0);
    caption_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    headerLayout->addWidget(caption_, 1);
    caption_->hide();
    compactTime_ = new QLabel("00:00", header_);
    headerLayout->addWidget(compactTime_);
    compactTime_->hide();
    compactRecord_ = button("mic", QStringLiteral("开始识别"), header_);
    compactRecord_->setObjectName("compactRecordButton");
    headerLayout->addWidget(compactRecord_);
    compactRecord_->hide();
    expandButton_ = button("expand", QStringLiteral("缩小"), header_);
    expandButton_->setObjectName("compactButton");
    headerLayout->addWidget(expandButton_);
    settingsButton_ = button("settings", QStringLiteral("设置"), header_);
    settingsButton_->setObjectName("settingsButton");
    headerLayout->addWidget(settingsButton_);
    auto *close = button("close", QStringLiteral("关闭窗口"), header_);
    headerLayout->addWidget(close);
    layout->addWidget(header_);
    for (QWidget *drag : {header_, brand_, static_cast<QWidget *>(title), static_cast<QWidget *>(version_),
                          static_cast<QWidget *>(caption_), static_cast<QWidget *>(dot_)})
        drag->installEventFilter(this);

    body_ = new QWidget(shell);
    auto *bodyLayout = new QVBoxLayout(body_);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(5);
    auto *controls = new QHBoxLayout;
    status_ = new QLabel(QStringLiteral("待机"), body_);
    status_->setObjectName("recognitionStatus");
    status_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    time_ = new QLabel("00:00", body_);
    controls->addWidget(status_, 1);
    controls->addWidget(time_);
    record_ = button("mic", QStringLiteral("开始识别"), body_);
    record_->setObjectName("recordButton");
    record_->setFixedSize(34, 34);
    controls->addWidget(record_);
    bodyLayout->addLayout(controls);
    settingsPanel_ = new QScrollArea(body_);
    settingsPanel_->setObjectName("settingsPanel");
    settingsPanel_->setWidgetResizable(true);
    settingsPanel_->hide();
    auto *panel = new QWidget;
    panel->setStyleSheet("background:white;");
    auto *form = new QVBoxLayout(panel);
    form->setContentsMargins(0, 0, 5, 0);
    form->setSpacing(5);
    const auto field = [form, panel](const QString &label, QWidget *widget) {
        form->addWidget(new QLabel(label, panel));
        form->addWidget(widget);
    };
    address_ = new QComboBox(panel);
    address_->setEditable(true);
    address_->setObjectName("engineAddress");
    address_->addItems(settings_.recentServers);
    address_->setEditText(settings_.engine.baseUrl);
    field(QStringLiteral("服务基地址"), address_);
    models_ = new QComboBox(panel);
    models_->setObjectName("model");
    models_->addItem(settings_.engine.model);
    field(QStringLiteral("模型"), models_);
    modelsError_ = new QLabel(panel);
    modelsError_->setWordWrap(true);
    form->addWidget(modelsError_);
    language_ = new QLineEdit(settings_.engine.language, panel);
    language_->setPlaceholderText(QStringLiteral("留空使用服务端默认值"));
    field(QStringLiteral("语言"), language_);
    apiKey_ = new QLineEdit(settings_.engine.apiKey, panel);
    apiKey_->setEchoMode(QLineEdit::Password);
    field(QStringLiteral("API Key（可选）"), apiKey_);
    const auto check = [panel, form](const QString &text, bool value) {
        auto *b = new QCheckBox(text, panel);
        b->setChecked(value);
        form->addWidget(b);
        return b;
    };
    allowUntrusted_ = check(QStringLiteral("允许自签名证书"), settings_.engine.allowUntrustedCertificate);
    enableAha_ = check(QStringLiteral("启用 AHA 扩展协议"), settings_.engine.enableAha);
    enableCorrection_ = check(QStringLiteral("启用文本纠错"), settings_.correction.enabled);
    correctionUrl_ = new QLineEdit(settings_.correction.responsesUrl, panel);
    field(QStringLiteral("文本纠错 Responses API 地址"), correctionUrl_);
    enableCache_ = check(QStringLiteral("启用录音缓存（原始音频与 VAD 分段）"), settings_.cache.enabled);
    enableCache_->setObjectName("cacheEnabled");
    cacheDirectory_ = new QLineEdit(settings_.cache.rootDirectory.isEmpty() ? AppSettings::defaultCacheDirectory()
                                                                            : settings_.cache.rootDirectory,
                                    panel);
    cacheDirectory_->setObjectName("cacheDirectory");
    auto *directoryRow = new QWidget(panel);
    auto *directoryLayout = new QHBoxLayout(directoryRow);
    directoryLayout->setContentsMargins(0, 0, 0, 0);
    directoryLayout->addWidget(cacheDirectory_, 1);
    auto *browse = new QToolButton(panel);
    browse->setText(QStringLiteral("浏览…"));
    directoryLayout->addWidget(browse);
    field(QStringLiteral("缓存目录"), directoryRow);
    sliceSeconds_ = new QSpinBox(panel);
    sliceSeconds_->setObjectName("cacheSliceSeconds");
    sliceSeconds_->setRange(1, 3600);
    sliceSeconds_->setSuffix(QStringLiteral(" 秒"));
    sliceSeconds_->setValue(settings_.cache.rawSliceSeconds);
    field(QStringLiteral("原始录音切片时长"), sliceSeconds_);
    auto *note =
        new QLabel(QStringLiteral("配置自动保存，下次开始识别时生效；目录在开始前检查。不自动清理历史录音。"), panel);
    note->setWordWrap(true);
    note->setStyleSheet("color:#777777;font-size:9px;");
    form->addWidget(note);
    settingsPanel_->setWidget(panel);
    bodyLayout->addWidget(settingsPanel_);
    meter_ = new QProgressBar(body_);
    meter_->setRange(0, 100);
    meter_->setValue(0);
    meter_->setTextVisible(false);
    meter_->setFixedHeight(3);
    meter_->setAccessibleName(QStringLiteral("语音电平"));
    bodyLayout->addWidget(meter_);
    transcript_ = new QTextEdit(body_);
    transcript_->setObjectName("transcript");
    transcript_->setReadOnly(true);
    transcript_->setUndoRedoEnabled(false);
    transcript_->setPlaceholderText(QStringLiteral("转写结果会显示在这里"));
    bodyLayout->addWidget(transcript_, 1);
    historyHint_ = new QLabel(QStringLiteral("仅显示最近 1000 轮／约 1 MiB 文字"), body_);
    historyHint_->setObjectName("historyHint");
    historyHint_->hide();
    bodyLayout->addWidget(historyHint_);
    layout->addWidget(body_, 1);
    resizeHandle_ = button("resize", QStringLiteral("拖拽调整窗口大小"), shell);
    resizeHandle_->setFixedSize(14, 10);
    resizeHandle_->setCursor(Qt::SizeBDiagCursor);
    resizeHandle_->installEventFilter(this);
    auto *bottom = new QHBoxLayout;
    bottom->setContentsMargins(0, 0, 0, 0);
    bottom->addWidget(resizeHandle_);
    bottom->addStretch();
    layout->addLayout(bottom);

    modelsTimer_ = new QTimer(this);
    modelsTimer_->setSingleShot(true);
    modelsTimer_->setInterval(300);
    connect(modelsTimer_, &QTimer::timeout, this, &MainWindow::refreshModels);
    connect(address_, &QComboBox::editTextChanged, this, [this](const QString &value) {
        settings_.engine.baseUrl = value;
        runtimeEngine_.clear();
        saveSettings();
        if (settingsPanel_->isVisible())
            modelsTimer_->start();
    });
    connect(models_, &QComboBox::currentTextChanged, this, [this](const QString &value) {
        if (!value.isEmpty()) {
            settings_.engine.model = value;
            saveSettings();
        }
    });
    connect(language_, &QLineEdit::textChanged, this, [this](const QString &value) {
        settings_.engine.language = value;
        saveSettings();
    });
    connect(apiKey_, &QLineEdit::textChanged, this, [this](const QString &value) {
        settings_.engine.apiKey = value;
        runtimeKey_.clear();
        runtimeKeyProvided_ = false;
        saveSettings();
        if (settingsPanel_->isVisible())
            modelsTimer_->start();
    });
    connect(allowUntrusted_, &QCheckBox::toggled, this, [this](bool value) {
        settings_.engine.allowUntrustedCertificate = value;
        runtimeSelfSigned_ = false;
        saveSettings();
        if (settingsPanel_->isVisible())
            modelsTimer_->start();
    });
    connect(enableAha_, &QCheckBox::toggled, this, [this](bool value) {
        settings_.engine.enableAha = value;
        saveSettings();
    });
    connect(enableCorrection_, &QCheckBox::toggled, this, [this](bool value) {
        settings_.correction.enabled = value;
        saveSettings();
    });
    connect(correctionUrl_, &QLineEdit::textChanged, this, [this](const QString &value) {
        settings_.correction.responsesUrl = value;
        saveSettings();
    });
    connect(enableCache_, &QCheckBox::toggled, this, [this](bool value) {
        settings_.cache.enabled = value;
        saveSettings();
    });
    connect(cacheDirectory_, &QLineEdit::textChanged, this, [this](const QString &value) {
        settings_.cache.rootDirectory = value;
        saveSettings();
    });
    connect(sliceSeconds_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        settings_.cache.rawSliceSeconds = value;
        saveSettings();
    });
    connect(browse, &QToolButton::clicked, this, [this] {
        const QString directory =
            QFileDialog::getExistingDirectory(this, QStringLiteral("选择录音缓存目录"), cacheDirectory_->text());
        if (!directory.isEmpty())
            cacheDirectory_->setText(directory);
    });
    connect(record_, &QToolButton::clicked, this, &MainWindow::toggleRecording);
    connect(compactRecord_, &QToolButton::clicked, this, &MainWindow::toggleRecording);
    connect(expandButton_, &QToolButton::clicked, this, &MainWindow::toggleCompact);
    connect(settingsButton_, &QToolButton::clicked, this, [this] { showSettings(!settingsPanel_->isVisible()); });
    connect(close, &QToolButton::clicked, this, &QWidget::close);
    connect(&engine_, &EngineClient::modelsReady, this, [this](const QStringList &models) {
        const QSignalBlocker blocker(models_);
        models_->clear();
        if (!models.contains(settings_.engine.model))
            models_->addItem(settings_.engine.model);
        models_->addItems(models);
        models_->setCurrentText(settings_.engine.model);
        modelsError_->setText(models.isEmpty() ? QStringLiteral("服务端没有可用模型") : QString());
    });
    connect(&engine_, &EngineClient::requestFailed, modelsError_, &QLabel::setText);
    connect(&session_, &SessionController::statusChanged, this, &MainWindow::setStatus);
    connect(&session_, &SessionController::transcriptChanged, this, &MainWindow::updateTranscript);
    connect(&session_, &SessionController::levelChanged, this,
            [this](float value) { meter_->setValue(qRound(value * 100)); });
    connect(&session_, &SessionController::connected, this, [this](const QString &address) {
        AppSettings effective = settings_;
        effective.engine.baseUrl = address;
        effective.rememberServer();
        settings_.recentServers = effective.recentServers;
        saveSettings();
        const QSignalBlocker blocker(address_);
        address_->clear();
        address_->addItems(settings_.recentServers);
        address_->setEditText(settings_.engine.baseUrl);
    });
    connect(&session_, &SessionController::started, this, [this] {
        recording_ = true;
        elapsed_.start();
        setStatus(QStringLiteral("等待中"), {});
    });
    connect(&session_, &SessionController::finished, this, [this] {
        recording_ = false;
        updateElapsed();
    });
    connect(
        &session_, &SessionController::shutdownReady, this,
        [this] {
            canClose_ = true;
            this->close();
        },
        Qt::QueuedConnection);
    auto *clock = new QTimer(this);
    connect(clock, &QTimer::timeout, this, &MainWindow::updateElapsed);
    clock->start(1000);
    setStatus(QStringLiteral("待机"), {});
    if (autoStart)
        QTimer::singleShot(0, this, &MainWindow::startRecognition);
}
void MainWindow::setRuntimeOverrides(const QString &engine, const QString &key, bool allowSelfSigned,
                                     bool apiKeyProvided)
{
    runtimeEngine_ = engine;
    runtimeKey_ = key;
    runtimeKeyProvided_ = apiKeyProvided;
    runtimeSelfSigned_ = allowSelfSigned;
    if (!engine.isEmpty())
        address_->setToolTip(QStringLiteral("本次运行覆盖服务地址：%1").arg(engine));
}
void MainWindow::saveSettings()
{
    settings_.save(store_);
}
void MainWindow::refreshModels()
{
    EngineConfig config = settings_.engine;
    if (!runtimeEngine_.isEmpty())
        config.baseUrl = runtimeEngine_;
    if (runtimeKeyProvided_)
        config.apiKey = runtimeKey_;
    if (runtimeSelfSigned_)
        config.allowUntrustedCertificate = true;
    modelsError_->setText(QStringLiteral("正在读取模型…"));
    engine_.fetchModels(config);
}
void MainWindow::startRecognition()
{
    AppSettings snapshot = settings_;
    if (!runtimeEngine_.isEmpty())
        snapshot.engine.baseUrl = runtimeEngine_;
    if (runtimeKeyProvided_)
        snapshot.engine.apiKey = runtimeKey_;
    if (runtimeSelfSigned_)
        snapshot.engine.allowUntrustedCertificate = true;
    const QString invalid = snapshot.validate();
    if (!invalid.isEmpty()) {
        setStatus(QStringLiteral("错误"), invalid);
        return;
    }
    items_.clear();
    transcript_->clear();
    historyHint_->hide();
    caption_->clear();
    recording_ = false;
    elapsed_.invalidate();
    time_->setText("00:00");
    compactTime_->setText("00:00");
    session_.start(snapshot);
}
void MainWindow::toggleRecording()
{
    if (session_.active())
        session_.stop();
    else
        startRecognition();
}
void MainWindow::showSettings(bool visible)
{
    settingsPanel_->setVisible(visible);
    settingsPanel_->setMaximumHeight(qMax(70, height() / 2 - 20));
    if (visible)
        modelsTimer_->start();
    else
        modelsTimer_->stop();
}
void MainWindow::toggleCompact()
{
    const QRect old = geometry();
    compact_ = !compact_;
    if (compact_) {
        expandedSize_ = size();
        showSettings(false);
        setMinimumSize(280, 58);
        setMaximumHeight(58);
        resize(300, 58);
    } else {
        setMaximumHeight(QWIDGETSIZE_MAX);
        setMinimumSize(280, 220);
        resize(expandedSize_);
    }
    move(old.right() - width() + 1, old.top());
    body_->setVisible(!compact_);
    brand_->setVisible(!compact_);
    resizeHandle_->setVisible(!compact_);
    settingsButton_->setVisible(!compact_);
    caption_->setVisible(compact_);
    compactTime_->setVisible(compact_);
    compactRecord_->setVisible(compact_);
    expandButton_->setToolTip(compact_ ? QStringLiteral("展开") : QStringLiteral("缩小"));
}
void MainWindow::setStatus(const QString &label, const QString &detail)
{
    statusLabel_ = label;
    const bool error = label == QStringLiteral("错误"), boundary = label == QStringLiteral("语义轮次未结束"),
               idle = label == QStringLiteral("待机");
    const QString background = error ? "#fff3f2" : boundary ? "#eff6ff" : idle ? "#f7f8fa" : "#f1f8f4";
    const QString color = error ? "#c34747" : boundary ? "#2563eb" : idle ? "#9aa3af" : "#4b9a68";
    status_->setText(label);
    status_->setToolTip(detail.isEmpty() ? label : detail);
    status_->setStyleSheet(
        QStringLiteral("background:%1;border:1px solid %2;border-radius:5px;padding:5px;color:#20262e;")
            .arg(background, color));
    dot_->setStyleSheet(QStringLiteral("background:%1;border-radius:3px;").arg(color));
    dot_->setToolTip(detail.isEmpty() ? label : detail);
    const bool busy = label == QStringLiteral("连接中") || label == QStringLiteral("停止中");
    if (error || idle)
        recording_ = false;
    for (QToolButton *b : {record_, compactRecord_}) {
        b->setEnabled(!busy);
        b->setIcon(icon(recording_ ? "stop" : "mic", Qt::white));
        b->setToolTip(recording_ ? QStringLiteral("停止识别") : QStringLiteral("开始识别"));
    }
    if (items_.isEmpty())
        caption_->setText(label);
}
void MainWindow::updateElapsed()
{
    const qint64 seconds = elapsed_.isValid() ? elapsed_.elapsed() / 1000 : 0;
    const QString text =
        QStringLiteral("%1:%2").arg(seconds / 60, 2, 10, QLatin1Char('0')).arg(seconds % 60, 2, 10, QLatin1Char('0'));
    if (recording_) {
        time_->setText(text);
        compactTime_->setText(text);
    }
}
void MainWindow::renderTranscript(int index)
{
    QTextCursor cursor(transcript_->document());
    if (index >= transcript_->document()->blockCount()) {
        cursor.movePosition(QTextCursor::End);
        cursor.insertBlock();
    }
    const QTextBlock block = transcript_->document()->findBlockByNumber(index);
    cursor = QTextCursor(block);
    cursor.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
    const Transcript &item = items_[index];
    QTextCharFormat format;
    format.setForeground(QColor(item.correcting ? "#a77713" : item.interim ? "#777777" : "#111111"));
    // Keep one document block per round even when a result contains line breaks.
    QString text = item.text;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace('\r', QChar(0x2028));
    text.replace('\n', QChar(0x2028));
    cursor.insertText(text, format);
    if (!item.correctionError.isEmpty()) {
        format.setForeground(QColor("#996b25"));
        format.setFontPointSize(8);
        QString error = item.correctionError;
        error.replace('\r', QChar(0x2028));
        error.replace('\n', QChar(0x2028));
        cursor.insertText(QStringLiteral("\u2028纠错失败，已保留原文：") + error, format);
    }
}
void MainWindow::updateTranscript(const Transcript &result)
{
    session_.acknowledgeTranscript(result.itemId);
    int index = -1;
    for (int i = 0; i < items_.size(); ++i)
        if (items_[i].itemId == result.itemId) {
            index = i;
            break;
        }
    if (index < 0) {
        if (result.text.trimmed().isEmpty() && !result.correcting)
            return;
        index = static_cast<int>(items_.size());
        items_.append(result);
    } else
        items_[index] = result;
    const bool scroll = transcript_->verticalScrollBar()->value() >= transcript_->verticalScrollBar()->maximum() - 5;
    qsizetype characters = 0;
    for (const Transcript &item : items_)
        characters += item.text.size() + item.correctionError.size();
    bool removed = false;
    while (items_.size() > 1 && (items_.size() > 1000 || characters > 512 * 1024)) {
        characters -= items_.first().text.size() + items_.first().correctionError.size();
        items_.removeFirst();
        removed = true;
    }
    if (characters > 512 * 1024 && !items_.isEmpty()) {
        items_.last().text = items_.last().text.right(512 * 1024);
        removed = true;
    }
    if (removed) {
        transcript_->clear();
        for (int i = 0; i < items_.size(); ++i)
            renderTranscript(i);
        historyHint_->show();
    } else
        renderTranscript(index);
    if (scroll)
        transcript_->verticalScrollBar()->setValue(transcript_->verticalScrollBar()->maximum());
    const Transcript &last = items_.last();
    caption_->setText(last.text.isEmpty()     ? last.correcting ? QStringLiteral("纠错中…") : statusLabel_
                      : last.text.size() > 20 ? "..." + last.text.right(20)
                                              : last.text);
    caption_->setStyleSheet(last.correcting ? "color:#a77713;" : "color:#111111;");
    caption_->setToolTip(last.text);
}
bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton)
            return false;
        dragOrigin_ = mouse->globalPosition().toPoint();
        resizeOrigin_ = geometry();
        if (object == resizeHandle_) {
            resizing_ = !compact_;
            if (resizing_ && windowHandle() && windowHandle()->startSystemResize(Qt::LeftEdge | Qt::BottomEdge))
                resizing_ = false;
        } else {
            dragging_ = true;
            if (windowHandle() && windowHandle()->startSystemMove())
                dragging_ = false;
        }
        return true;
    }
    if (event->type() == QEvent::MouseMove) {
        const QPoint delta = static_cast<QMouseEvent *>(event)->globalPosition().toPoint() - dragOrigin_;
        if (resizing_) {
            const int width = qMax(280, resizeOrigin_.width() - delta.x()),
                      height = qMax(220, resizeOrigin_.height() + delta.y());
            setGeometry(resizeOrigin_.right() - width + 1, resizeOrigin_.top(), width, height);
            return true;
        }
        if (dragging_) {
            move(resizeOrigin_.topLeft() + delta);
            return true;
        }
    }
    if (event->type() == QEvent::MouseButtonRelease) {
        dragging_ = resizing_ = false;
    }
    return QWidget::eventFilter(object, event);
}
void MainWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (settingsPanel_)
        settingsPanel_->setMaximumHeight(qMax(70, height() / 2 - 20));
}
void MainWindow::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    if (firstPaint_) {
        firstPaint_ = false;
        QTimer::singleShot(0, this, [this] { emit firstFrameRendered(); });
    }
}
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (canClose_) {
        event->accept();
        return;
    }
    event->ignore();
    if (closing_)
        return;
    closing_ = true;
    setStatus(QStringLiteral("停止中"), QStringLiteral("正在保存录音并关闭窗口"));
    session_.shutdown();
}
} // namespace aha
