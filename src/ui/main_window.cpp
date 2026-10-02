#include "ui/main_window.h"
#include "ui/settings_window.h"
#include "ui/theme.h"
#include "ui/widgets.h"

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
#include <QScreen>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTextBlock>
#include <QTextBlockFormat>
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
QToolButton *button(const QString &name, const QString &tip, QWidget *parent)
{
    auto *b = new QToolButton(parent);
    b->setProperty("iconName", name);
    b->setIcon(uiIcon(name, false));
    b->setIconSize(QSize(20, 20));
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    b->setFixedSize(32, 32);
    return b;
}
QLabel *caption(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setFont(uiFont(12));
    label->setProperty("role", "caption");
    label->setMinimumHeight(18);
    return label;
}
} // namespace
MainWindow::MainWindow(const AppSettings &settings, bool autoStart, QWidget *parent)
    : QWidget(parent), settings_(settings)
{
    initializeUiResources();
    setObjectName("floatingWindow");
    setWindowTitle("aha-flow");
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFont(uiFont());
    setMinimumSize(360, 300);
    resize(expandedSize_);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *shell = new QFrame(this);
    shell->setObjectName("shell");
    outer->addWidget(shell);
    auto *layout = new QVBoxLayout(shell);
    layout->setContentsMargins(11, 11, 11, 11);
    layout->setSpacing(8);
    header_ = new QWidget(shell);
    header_->setFixedHeight(32);
    auto *headerLayout = new QHBoxLayout(header_);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);
    dot_ = new QLabel(header_);
    dot_->setFixedSize(6, 6);
    headerLayout->addWidget(dot_);
    dot_->hide();
    brand_ = new QWidget(header_);
    auto *brandLayout = new QHBoxLayout(brand_);
    brandLayout->setContentsMargins(0, 0, 0, 0);
    brandLayout->setSpacing(8);
    auto *title = new QLabel("aha-flow", brand_);
    title->setObjectName("title");
    title->setFont(uiFont(18, true, true));
    brandLayout->addWidget(title);
    version_ = caption(QCoreApplication::applicationVersion(), brand_);
    version_->setObjectName("version");
    version_->setFont(uiFont(12, false, true));
    brandLayout->addWidget(version_);
    brandLayout->addStretch();
    headerLayout->addWidget(brand_, 1);
    caption_ = new ElidedLabel(header_);
    caption_->setMinimumWidth(0);
    caption_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    headerLayout->addWidget(caption_, 1);
    caption_->hide();
    compactTime_ = caption("00:00", header_);
    compactTime_->setFont(uiFont(12, true, true));
    headerLayout->addWidget(compactTime_);
    compactTime_->hide();
    compactRecord_ = button("mic", QStringLiteral("开始识别"), header_);
    compactRecord_->setObjectName("compactRecordButton");
    compactRecord_->setProperty("primary", true);
    headerLayout->addWidget(compactRecord_);
    compactRecord_->hide();
    settingsButton_ = button("settings", QStringLiteral("设置"), header_);
    settingsButton_->setObjectName("settingsButton");
    headerLayout->addWidget(settingsButton_);
    expandButton_ = button("collapse", QStringLiteral("缩小"), header_);
    expandButton_->setObjectName("compactButton");
    headerLayout->addWidget(expandButton_);
    auto *close = button("close", QStringLiteral("关闭窗口"), header_);
    headerLayout->addWidget(close);
    layout->addWidget(header_);
    for (QWidget *drag : {header_, brand_, static_cast<QWidget *>(title), static_cast<QWidget *>(version_),
                          static_cast<QWidget *>(caption_), static_cast<QWidget *>(dot_)})
        drag->installEventFilter(this);

    body_ = new QWidget(shell);
    auto *bodyLayout = new QVBoxLayout(body_);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(8);
    auto *controls = new QHBoxLayout;
    controls->setSpacing(8);
    statusDot_ = new QLabel(body_);
    statusDot_->setFixedSize(6, 6);
    controls->addWidget(statusDot_);
    status_ = caption(QStringLiteral("待机"), body_);
    status_->setProperty("role", "status");
    status_->setObjectName("recognitionStatus");
    status_->setMinimumHeight(24);
    controls->addWidget(status_, 1);
    time_ = caption("00:00", body_);
    time_->setFont(uiFont(12, true, true));
    controls->addWidget(time_);
    bodyLayout->addLayout(controls);
    auto *divider = new QFrame(body_);
    divider->setProperty("role", "divider");
    divider->setFixedHeight(1);
    bodyLayout->addWidget(divider);
    statusDetail_ = caption({}, body_);
    statusDetail_->setProperty("role", "status");
    statusDetail_->setObjectName("statusDetail");
    statusDetail_->setWordWrap(true);
    statusDetail_->setMinimumWidth(0);
    statusDetail_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    statusDetail_->setMaximumHeight(88);
    statusDetail_->hide();
    bodyLayout->addWidget(statusDetail_);
    transcript_ = new QTextEdit(body_);
    transcript_->setObjectName("transcript");
    transcript_->setFont(uiFont());
    transcript_->document()->setDefaultFont(uiFont());
    transcript_->document()->setDocumentMargin(0);
    transcript_->setReadOnly(true);
    transcript_->setUndoRedoEnabled(false);
    transcript_->setFrameShape(QFrame::NoFrame);
    transcript_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    transcript_->setPlaceholderText(QStringLiteral("开始说话，文字会显示在这里。"));
    bodyLayout->addWidget(transcript_, 1);
    historyHint_ = caption(QStringLiteral("仅显示最近 1000 轮／约 1 MiB 文字"), body_);
    historyHint_->setObjectName("historyHint");
    historyHint_->hide();
    bodyLayout->addWidget(historyHint_);
    auto *footer = new QVBoxLayout;
    footer->setSpacing(8);
    meter_ = new QProgressBar(body_);
    meter_->setRange(0, 100);
    meter_->setValue(0);
    meter_->setTextVisible(false);
    meter_->setFixedHeight(4);
    meter_->setAccessibleName(QStringLiteral("语音电平"));
    footer->addWidget(meter_);
    auto *actions = new QHBoxLayout;
    actions->setSpacing(16);
    auto *meta = new QVBoxLayout;
    meta->setSpacing(0);
    modelHint_ = caption(settings_.engine.model, body_);
    modelHint_->setFont(uiFont(12, false, true));
    modelHint_->setMinimumWidth(0);
    modelHint_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    cacheHint_ = caption({}, body_);
    meta->addWidget(modelHint_);
    meta->addWidget(cacheHint_);
    actions->addLayout(meta, 1);
    record_ = button("mic", QStringLiteral("开始识别"), body_);
    record_->setObjectName("recordButton");
    record_->setProperty("primary", true);
    record_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    record_->setFont(uiFont(14, true));
    record_->setFixedSize(128, 32);
    actions->addWidget(record_);
    footer->addLayout(actions);
    bodyLayout->addLayout(footer);
    layout->addWidget(body_, 1);
    resizeHandle_ = new QToolButton(this);
    resizeHandle_->setObjectName("resizeHandle");
    resizeHandle_->setFixedSize(14, 10);
    resizeHandle_->setCursor(Qt::SizeBDiagCursor);
    resizeHandle_->setAccessibleName(QStringLiteral("拖拽调整窗口大小"));
    resizeHandle_->setToolTip(QStringLiteral("拖拽调整窗口大小"));
    resizeHandle_->installEventFilter(this);
    settingsWindow_ = new SettingsWindow(settings_, this);
    const auto &widgets = settingsWindow_->controls();
    address_ = widgets.address;
    models_ = widgets.models;
    modelsError_ = widgets.modelsError;
    language_ = widgets.language;
    apiKey_ = widgets.apiKey;
    allowUntrusted_ = widgets.allowUntrusted;
    enableAha_ = widgets.enableAha;
    enableCorrection_ = widgets.enableCorrection;
    correctionUrl_ = widgets.correctionUrl;
    enableCache_ = widgets.enableCache;
    cacheDirectory_ = widgets.cacheDirectory;
    sliceSeconds_ = widgets.sliceSeconds;
    auto *browse = widgets.browse;
    modelsTimer_ = new QTimer(this);
    modelsTimer_->setSingleShot(true);
    modelsTimer_->setInterval(300);
    connect(modelsTimer_, &QTimer::timeout, this, &MainWindow::refreshModels);
    connect(address_, &QComboBox::editTextChanged, this, [this](const QString &value) {
        settings_.engine.baseUrl = value;
        runtimeEngine_.clear();
        saveSettings();
        if (settingsWindow_->isVisible())
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
        if (settingsWindow_->isVisible())
            modelsTimer_->start();
    });
    connect(allowUntrusted_, &QCheckBox::toggled, this, [this](bool value) {
        settings_.engine.allowUntrustedCertificate = value;
        runtimeSelfSigned_ = false;
        saveSettings();
        if (settingsWindow_->isVisible())
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
    connect(settingsButton_, &QToolButton::clicked, this, [this] { showSettings(true); });
    connect(settingsWindow_, &QDialog::finished, modelsTimer_, &QTimer::stop);
    connect(settingsWindow_, &SettingsWindow::themeSelected, this, [this](const QString &name) {
        if (settings_.theme == name)
            return;
        settings_.theme = name;
        saveSettings();
        applyTheme();
    });
    connect(close, &QToolButton::clicked, this, &QWidget::close);
    connect(&engine_, &EngineClient::modelsReady, this, [this](const QStringList &models) {
        const QSignalBlocker blocker(models_);
        models_->clear();
        if (!models.contains(settings_.engine.model))
            models_->addItem(settings_.engine.model);
        models_->addItems(models);
        models_->setCurrentText(settings_.engine.model);
        modelsError_->setText(models.isEmpty() ? QStringLiteral("服务端没有可用模型") : QString());
        modelsError_->setVisible(models.isEmpty());
    });
    connect(&engine_, &EngineClient::requestFailed, this, [this](const QString &message) {
        modelsError_->setText(message);
        modelsError_->show();
    });
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
    applyTheme();
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
    updateSessionMetadata();
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
    modelsError_->show();
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
    activeModel_ = snapshot.engine.model;
    activeCacheEnabled_ = snapshot.cache.enabled;
    session_.start(snapshot);
    updateSessionMetadata();
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
    if (visible) {
        if (!settingsWindow_->isVisible()) {
            settingsWindow_->setScreen(screen());
            settingsWindow_->fitToContents();
            const QRect available = screen()->availableGeometry();
            settingsWindow_->move(
                qMax(available.left(), qMin(geometry().right() + 16, available.right() - settingsWindow_->width() + 1)),
                qMax(available.top(), qMin(y(), available.bottom() - settingsWindow_->height() + 1)));
        }
        settingsWindow_->show();
        settingsWindow_->raise();
        settingsWindow_->activateWindow();
        modelsTimer_->start();
    } else {
        settingsWindow_->hide();
        modelsTimer_->stop();
    }
}
void MainWindow::toggleCompact()
{
    const QRect old = geometry();
    compact_ = !compact_;
    auto *shellLayout = findChild<QFrame *>("shell")->layout();
    shellLayout->setContentsMargins(11, compact_ ? 7 : 11, 11, compact_ ? 7 : 11);
    if (compact_) {
        expandedSize_ = size();
        body_->hide();
        brand_->hide();
        settingsButton_->hide();
        showSettings(false);
        setMinimumSize(360, 48);
        setMaximumHeight(48);
        resize(360, 48);
    } else {
        setMaximumHeight(QWIDGETSIZE_MAX);
        setMinimumSize(360, 300);
        resize(expandedSize_);
    }
    move(old.right() - width() + 1, old.top());
    body_->setVisible(!compact_);
    brand_->setVisible(!compact_);
    resizeHandle_->setVisible(!compact_);
    settingsButton_->setVisible(!compact_);
    dot_->setVisible(compact_);
    caption_->setVisible(compact_);
    compactTime_->setVisible(compact_);
    compactRecord_->setVisible(compact_);
    expandButton_->setToolTip(compact_ ? QStringLiteral("展开") : QStringLiteral("缩小"));
    expandButton_->setAccessibleName(expandButton_->toolTip());
    expandButton_->setProperty("iconName", compact_ ? "expand" : "collapse");
    expandButton_->setIcon(uiIcon(compact_ ? "expand" : "collapse", settings_.theme == "abyssus"));
}
void MainWindow::setStatus(const QString &label, const QString &detail)
{
    statusLabel_ = label;
    statusDetailText_ = detail;
    const auto &t = theme(settings_.theme == "abyssus");
    const bool error = label == QStringLiteral("错误"), idle = label == QStringLiteral("待机");
    const bool busy = label == QStringLiteral("连接中") || label == QStringLiteral("停止中");
    if (busy || idle || error)
        meter_->setValue(0);
    transcript_->setPlaceholderText(label == QStringLiteral("连接中")
                                        ? QStringLiteral("正在连接识别引擎…\n连接完成后才会打开麦克风。")
                                    : error ? QString()
                                            : QStringLiteral("开始说话，文字会显示在这里。"));
    const QColor color = error                                       ? t.danger
                         : idle || label == QStringLiteral("等待中") ? t.secondaryText
                         : label == QStringLiteral("停止中")         ? t.warning
                         : label == QStringLiteral("聆听中")         ? t.success
                                                                     : t.info;
    status_->setText(label);
    status_->setToolTip(detail.isEmpty() ? label : detail);
    for (QLabel *dot : {dot_, statusDot_}) {
        dot->setStyleSheet(QStringLiteral("background:%1;border-radius:3px;").arg(color.name()));
        dot->setToolTip(detail.isEmpty() ? label : detail);
    }
    statusDetail_->setVisible(error && !detail.isEmpty());
    statusDetail_->setText(detail.left(240) + QStringLiteral("\n检查配置后重新开始。"));
    statusDetail_->setToolTip(detail);
    if (error || idle)
        recording_ = false;
    const QString action =
        busy         ? (label == QStringLiteral("连接中") ? QStringLiteral("连接中…") : QStringLiteral("保存中…"))
        : recording_ ? QStringLiteral("停止识别")
        : error      ? QStringLiteral("重新开始")
                     : QStringLiteral("开始识别");
    for (QToolButton *b : {record_, compactRecord_}) {
        b->setEnabled(!busy);
        b->setProperty("iconName", recording_ ? "stop" : "mic");
        b->setIcon(uiIcon(recording_ ? "stop" : "mic", settings_.theme == "abyssus"));
        b->setToolTip(action);
        b->setAccessibleName(action);
    }
    record_->setText(action);
    if (items_.isEmpty())
        caption_->setText(label);
    updateSessionMetadata();
}
void MainWindow::updateSessionMetadata()
{
    const bool active = session_.active();
    modelHint_->setText(active ? activeModel_ : settings_.engine.model);
    modelHint_->setToolTip(modelHint_->text());
    cacheHint_->setText((active ? activeCacheEnabled_ : settings_.cache.enabled) ? QStringLiteral("录音缓存已开启")
                                                                                 : QStringLiteral("录音缓存已关闭"));
}
void MainWindow::applyTheme()
{
    const bool dark = settings_.theme == "abyssus";
    setProperty("theme", settings_.theme);
    setPalette(themePalette(dark));
    setStyleSheet(themeStyleSheet(dark));
    settingsWindow_->applyTheme(settings_.theme);
    QPixmap grip(14, 10);
    grip.fill(Qt::transparent);
    {
        QPainter painter(&grip);
        painter.setPen(theme(dark).strongBorder);
        painter.drawLine(2, 2, 10, 10);
        painter.drawLine(2, 6, 6, 10);
    }
    static_cast<QToolButton *>(resizeHandle_)->setIcon(QIcon(grip));
    static_cast<QToolButton *>(resizeHandle_)->setIconSize(QSize(14, 10));
    for (auto *b : findChildren<QToolButton *>()) {
        const QString name = b->property("iconName").toString();
        if (!name.isEmpty())
            b->setIcon(uiIcon(name, dark));
    }
    const int scrollPosition = transcript_->verticalScrollBar()->value();
    const int selectionStart = transcript_->textCursor().anchor();
    const int selectionEnd = transcript_->textCursor().position();
    QTextCursor batch(transcript_->document());
    batch.beginEditBlock();
    for (int i = 0; i < items_.size(); ++i)
        renderTranscript(i);
    batch.endEditBlock();
    QTextCursor selection(transcript_->document());
    selection.setPosition(selectionStart);
    selection.setPosition(selectionEnd, QTextCursor::KeepAnchor);
    transcript_->setTextCursor(selection);
    transcript_->verticalScrollBar()->setValue(scrollPosition);
    setStatus(statusLabel_, statusDetailText_);
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
    const auto &t = theme(settings_.theme == "abyssus");
    format.setFont(uiFont());
    format.setForeground(item.interim && !item.correcting ? t.secondaryText : t.text);
    QTextBlockFormat paragraph;
    paragraph.setTopMargin(0);
    paragraph.setBottomMargin(0);
    paragraph.setLineHeight(22, QTextBlockFormat::FixedHeight);
    cursor.setBlockFormat(paragraph);
    // Keep one document block per round even when a result contains line breaks.
    QString text = item.text;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace('\r', QChar(0x2028));
    text.replace('\n', QChar(0x2028));
    text.replace(QChar(0x2029), QChar(0x2028));
    int start = 0, end = static_cast<int>(text.size());
    while (start < end && text[start] == QChar(0x2028))
        ++start;
    while (end > start && text[end - 1] == QChar(0x2028))
        --end;
    text = text.mid(start, end - start);
    cursor.insertText(text, format);
    if (!item.correctionError.isEmpty()) {
        format.setForeground(t.text);
        format.setFont(uiFont(12));
        QString error = item.correctionError;
        error.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        error.replace('\r', QChar(0x2028));
        error.replace('\n', QChar(0x2028));
        cursor.insertText(QStringLiteral("\u2028纠错失败，已保留原文：") + error, format);
    } else if (item.correcting) {
        format.setFont(uiFont(12));
        format.setForeground(t.text);
        cursor.insertText(QStringLiteral("\u2028正在纠错，结果会继续更新"), format);
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
    caption_->setText(last.text.isEmpty() ? (last.correcting ? QStringLiteral("纠错中…") : statusLabel_)
                                          : last.text.simplified());
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
            const int width = qMax(360, resizeOrigin_.width() - delta.x()),
                      height = qMax(300, resizeOrigin_.height() + delta.y());
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
    if (resizeHandle_)
        resizeHandle_->move(4, height() - 14);
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
    showSettings(false);
    setStatus(QStringLiteral("停止中"), QStringLiteral("正在保存录音并关闭窗口"));
    session_.shutdown();
}
} // namespace aha
