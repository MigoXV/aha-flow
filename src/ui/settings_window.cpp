#include "ui/settings_window.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <QButtonGroup>
#include <QCoreApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QShowEvent>
#include <QSlider>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>
#include <QtMath>

namespace aha
{
namespace
{
QLabel *label(const QString &text, QWidget *parent, bool caption = false)
{
    auto *l = new QLabel(text, parent);
    l->setFont(uiFont(caption ? 11 : 12, !caption));
    l->setProperty("role", caption ? "caption" : "label");
    l->setMinimumHeight(caption ? 16 : 18);
    l->setWordWrap(true);
    return l;
}

QWidget *field(const QString &name, QWidget *control, QWidget *parent)
{
    auto *group = new QWidget(parent);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    auto *l = label(name, group);
    l->setBuddy(control);
    control->setAccessibleName(name);
    control->setFixedHeight(32);
    control->setFont(uiFont(12));
    layout->addWidget(l);
    layout->addWidget(control);
    return group;
}

QWidget *inlineField(const QString &name, QWidget *control, QWidget *parent)
{
    auto *group = new QWidget(parent);
    auto *layout = new QHBoxLayout(group);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    auto *l = label(name, group);
    l->setWordWrap(false);
    l->setBuddy(control);
    control->setAccessibleName(name);
    control->setFont(uiFont(12));
    control->setFixedHeight(32);
    layout->addWidget(l);
    layout->addWidget(control, 1);
    return group;
}

QCheckBox *preference(QVBoxLayout *layout, const QString &name, const QString &help, bool checked)
{
    auto *row = new QWidget;
    auto *horizontal = new QHBoxLayout(row);
    horizontal->setContentsMargins(0, 0, 0, 0);
    horizontal->setSpacing(12);
    auto *copy = new QVBoxLayout;
    copy->setSpacing(0);
    copy->addWidget(label(name, row));
    copy->addWidget(label(help, row, true));
    horizontal->addLayout(copy, 1);
    auto *toggle = new ToggleSwitch(name, row);
    toggle->setChecked(checked);
    horizontal->addWidget(toggle);
    layout->addWidget(row);
    return toggle;
}

QVBoxLayout *page(QTabWidget *tabs, const QString &title)
{
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *contents = new QWidget;
    auto *layout = new QVBoxLayout(contents);
    layout->setContentsMargins(0, 8, 0, 6);
    layout->setSpacing(10);
    layout->setAlignment(Qt::AlignTop);
    layout->setSizeConstraint(QLayout::SetMinimumSize);
    scroll->setWidget(contents);
    tabs->addTab(scroll, title);
    return layout;
}

} // namespace

SettingsWindow::SettingsWindow(const AppSettings &settings, QWidget *parent) : QDialog(parent)
{
    realtimeDraft_ = settings.engine.realtimeDraft;
    setObjectName("settingsDialog");
    setWindowTitle(QStringLiteral("aha-flow 设置"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFont(uiFont(12));
    setFixedWidth(480);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *shell = new QFrame(this);
    shell->setObjectName("settingsShell");
    outer->addWidget(shell);
    auto *layout = new QVBoxLayout(shell);
    layout->setContentsMargins(15, 7, 15, 7); // 16px including the shell border.
    layout->setSpacing(0);
    auto *header = new QWidget(shell);
    header->setFixedHeight(36);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);
    auto *title = label(QStringLiteral("设置"), header);
    title->setFont(uiFont(14, true));
    title->setProperty("role", "heading");
    headerLayout->addWidget(title, 1);
    auto *close = new QToolButton(header);
    close->setProperty("iconName", "close");
    close->setToolTip(QStringLiteral("关闭设置"));
    close->setAccessibleName(QStringLiteral("关闭设置"));
    close->setIconSize(QSize(16, 16));
    close->setFixedSize(28, 28);
    headerLayout->addWidget(close);
    layout->addWidget(header);
    header->setProperty("settingsDragHandle", true);
    title->setProperty("settingsDragHandle", true);
    header->installEventFilter(this);
    title->installEventFilter(this);
    connect(close, &QToolButton::clicked, this, &QDialog::reject);

    tabs_ = new QTabWidget(shell);
    tabs_->setObjectName("settingsTabs");
    tabs_->tabBar()->setFont(uiFont(13, true));
    tabs_->setDocumentMode(true);
    tabs_->setUsesScrollButtons(false);
    tabs_->tabBar()->setDrawBase(false);
    tabs_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    layout->addWidget(tabs_, 1);

    auto *enginePage = page(tabs_, QStringLiteral("识别引擎"));
    auto *engine = enginePage;
    controls_.address = new IconComboBox;
    controls_.address->setObjectName("engineAddress");
    controls_.address->setEditable(true);
    controls_.address->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    controls_.address->setMinimumContentsLength(16);
    controls_.address->addItems(settings.recentServers);
    controls_.address->setEditText(settings.engine.baseUrl);
    engine->addWidget(field(QStringLiteral("服务基地址"), controls_.address, shell));
    auto *pair = new QWidget;
    auto *pairLayout = new QHBoxLayout(pair);
    pairLayout->setContentsMargins(0, 0, 0, 0);
    pairLayout->setSpacing(12);
    controls_.models = new IconComboBox(pair);
    controls_.models->setObjectName("model");
    controls_.models->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    controls_.models->setMinimumContentsLength(12);
    controls_.models->addItem(settings.engine.model);
    pairLayout->addWidget(inlineField(QStringLiteral("模型"), controls_.models, pair), 1);
    controls_.language = new QLineEdit(settings.engine.language, pair);
    controls_.language->setPlaceholderText(QStringLiteral("留空默认"));
    controls_.language->setToolTip(QStringLiteral("留空使用服务端默认值"));
    auto *languageField = inlineField(QStringLiteral("语言"), controls_.language, pair);
    languageField->setFixedWidth(160);
    pairLayout->addWidget(languageField);
    engine->addWidget(pair);
    controls_.modelsError = label({}, shell, true);
    controls_.modelsError->setObjectName("modelsError");
    controls_.modelsError->setMinimumWidth(0);
    controls_.modelsError->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    controls_.modelsError->hide();
    engine->addWidget(controls_.modelsError);
    auto *security = enginePage;
    controls_.apiKey = new QLineEdit(settings.engine.apiKey);
    controls_.apiKey->setEchoMode(QLineEdit::Password);
    controls_.apiKey->setPlaceholderText(QStringLiteral("可选；仅用于识别引擎"));
    security->addWidget(inlineField(QStringLiteral("API Key"), controls_.apiKey, shell));
    controls_.allowUntrusted =
        preference(security, QStringLiteral("允许自签名证书"), QStringLiteral("仅用于你信任的局域网服务。"),
                   settings.engine.allowUntrustedCertificate);
    controls_.enableAha = preference(security, QStringLiteral("AHA 扩展协议"),
                                     QStringLiteral("显示中间预览与语义轮次状态。"), settings.engine.enableAha);

    const auto group = [this, engine](const QString &title) {
        auto *header = new QToolButton;
        header->setText(title);
        header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        header->setArrowType(Qt::RightArrow);
        header->setCheckable(true);
        header->setMinimumHeight(32);
        header->setAccessibleName(title);
        engine->addWidget(header);
        auto *body = new QWidget;
        auto *layout = new QVBoxLayout(body);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);
        engine->addWidget(body);
        body->hide();
        connect(header, &QToolButton::toggled, body, [header, body](bool open) {
            body->setVisible(open);
            header->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        });
        return layout;
    };
    const auto initial = [this, settings](const QString &path, QJsonValue fallback) {
        if (realtimeDraft_.contains(path))
            return realtimeDraft_.value(path).toObject();
        QJsonValue wire = settings.engine.realtimeOptions;
        for (const auto &part : path.split('/'))
            wire = wire.toObject().value(part);
        QJsonObject entry{{"enabled", !wire.isUndefined()}, {"value", wire.isUndefined() ? fallback : wire}};
        realtimeDraft_.insert(path, entry);
        return entry;
    };
    const auto optional = [this, initial](QVBoxLayout *layout, const QString &title, const QString &path,
                                          QWidget *editor, QJsonValue fallback) {
        const auto entry = initial(path, fallback);
        auto *toggle = preference(layout, title, QStringLiteral("关闭此项则不发送，使用服务端设置。"),
                                  entry.value("enabled").toBool());
        toggle->setObjectName(path + "/send");
        editor->setEnabled(toggle->isChecked());
        layout->addWidget(editor);
        connect(toggle, &QCheckBox::toggled, this, [this, path, editor](bool enabled) {
            editor->setEnabled(enabled);
            auto entry = realtimeDraft_.value(path).toObject();
            entry.insert("enabled", enabled);
            realtimeDraft_.insert(path, entry);
            rebuildRealtimeOptions();
        });
    };
    const auto setValue = [this](const QString &path, QJsonValue value) {
        auto entry = realtimeDraft_.value(path).toObject();
        entry.insert("value", value);
        realtimeDraft_.insert(path, entry);
        rebuildRealtimeOptions();
    };
    const auto numeric = [this, initial, optional, setValue](QVBoxLayout *layout, const QString &title,
                                                             const QString &path, int minimum, int maximum,
                                                             int fallback, int scale, const QString &unit) {
        auto *row = new QWidget;
        auto *vertical = new QVBoxLayout(row);
        vertical->setContentsMargins(0, 0, 0, 0);
        vertical->setSpacing(0);
        auto *horizontal = new QHBoxLayout;
        horizontal->setContentsMargins(0, 0, 0, 0);
        vertical->addLayout(horizontal);
        auto *slider = new QSlider(Qt::Horizontal, row);
        slider->setObjectName(path);
        slider->setAccessibleName(title);
        slider->setRange(minimum, maximum);
        slider->setSingleStep(1);
        slider->setValue(qRound(initial(path, double(fallback) / scale).value("value").toDouble() * scale));
        slider->setMinimumHeight(32);
        auto *value = label({}, row);
        value->setMinimumWidth(72);
        const auto update = [slider, value, scale, unit] {
            value->setText(QString::number(double(slider->value()) / scale, 'f', scale == 100 ? 2 : 0) + unit);
        };
        update();
        slider->setToolTip(QStringLiteral("范围 %1–%2%3；方向键微调，Home/End 选择端点")
                               .arg(double(minimum) / scale)
                               .arg(double(maximum) / scale)
                               .arg(unit));
        horizontal->addWidget(slider, 1);
        horizontal->addWidget(value);
        vertical->addWidget(
            label(QStringLiteral("范围 %1–%2%3").arg(double(minimum) / scale).arg(double(maximum) / scale).arg(unit),
                  row, true));
        slider->installEventFilter(this);
        optional(layout, title, path, row, double(fallback) / scale);
        connect(slider, &QSlider::valueChanged, this, [setValue, path, scale, update](int value) {
            update();
            setValue(path, double(value) / scale);
        });
    };
    const auto mode = [this](QVBoxLayout *layout, const QString &title, const QString &key, const QStringList &labels,
                             const QStringList &values, const QString &fallback) {
        const QString selected = realtimeDraft_.value(key).toString(fallback);
        realtimeDraft_.insert(key, selected);
        auto *combo = new IconComboBox;
        combo->setObjectName(key);
        for (int i = 0; i < values.size(); ++i)
            combo->addItem(labels[i], values[i]);
        combo->setCurrentIndex(qMax(0, values.indexOf(selected)));
        layout->addWidget(field(title, combo, this));
        connect(combo, &QComboBox::currentIndexChanged, this, [this, combo, key] {
            realtimeDraft_.insert(key, combo->currentData().toString());
            rebuildRealtimeOptions();
        });
        return combo;
    };
    auto *vad = group(QStringLiteral("语音活动检测"));
    const auto wireVad = settings.engine.realtimeOptions.value("turn_detection");
    auto *vadMode = mode(vad, QStringLiteral("断句方式"), "__vad",
                         {"不发送 · 服务端设置", "音量断句", "语义断句", "关闭 · 手动提交"},
                         {"omit", "server_vad", "semantic_vad", "off"},
                         wireVad.isNull() ? "off" : wireVad.toObject().value("type").toString("omit"));
    auto *server = new QWidget;
    auto *serverLayout = new QVBoxLayout(server);
    serverLayout->setContentsMargins(0, 0, 0, 0);
    vad->addWidget(server);
    numeric(serverLayout, QStringLiteral("触发阈值"), "turn_detection/threshold", 0, 100, 50, 100, "");
    numeric(serverLayout, QStringLiteral("句首前置音频"), "turn_detection/prefix_padding_ms", 0, 5000, 300, 1, " ms");
    numeric(serverLayout, QStringLiteral("结束静音时长"), "turn_detection/silence_duration_ms", 100, 10000, 500, 1,
            " ms");
    auto *semantic = new QWidget;
    auto *semanticLayout = new QVBoxLayout(semantic);
    semanticLayout->setContentsMargins(0, 0, 0, 0);
    vad->addWidget(semantic);
    const auto segmented = [this, initial, optional, setValue](QVBoxLayout *layout, const QString &title,
                                                               const QString &path, const QStringList &labels,
                                                               const QStringList &values, const QString &fallback) {
        auto *row = new QWidget;
        auto *horizontal = new QHBoxLayout(row);
        horizontal->setContentsMargins(0, 0, 0, 0);
        horizontal->setSpacing(2);
        auto *buttons = new QButtonGroup(row);
        buttons->setExclusive(true);
        const QString selected = initial(path, fallback).value("value").toString();
        for (int i = 0; i < values.size(); ++i) {
            auto *button = new QPushButton(labels[i], row);
            button->setCheckable(true);
            button->setMinimumHeight(32);
            button->setObjectName(path + "/" + values[i]);
            button->setChecked(values[i] == selected);
            button->setAccessibleName(title + " · " + labels[i]);
            buttons->addButton(button);
            horizontal->addWidget(button, 1);
            connect(button, &QPushButton::clicked, this,
                    [setValue, path, value = values[i]] { setValue(path, value); });
        }
        optional(layout, title, path, row, fallback);
    };
    segmented(semanticLayout, QStringLiteral("语义断句积极程度"), "turn_detection/eagerness",
              {"自动", "低", "中", "高"}, {"auto", "low", "medium", "high"}, "auto");
    const auto showVad = [vadMode, server, semantic] {
        server->setVisible(vadMode->currentData() == "server_vad");
        semantic->setVisible(vadMode->currentData() == "semantic_vad");
    };
    showVad();
    connect(vadMode, &QComboBox::currentIndexChanged, this, showVad);
    auto *recognition = group(QStringLiteral("识别选项"));
    auto *languageMode =
        mode(recognition, QStringLiteral("语言发送方式"), "__language", {"不发送", "单语言", "多语言 · 逗号分隔"},
             {"omit", "single", "multiple"}, settings.engine.languageMode);
    controls_.language->setEnabled(languageMode->currentData() != "omit");
    connect(languageMode, &QComboBox::currentIndexChanged, this,
            [this, languageMode] { controls_.language->setEnabled(languageMode->currentData() != "omit"); });
    for (const auto &path : {QString("transcription/prompt"), QString("transcription/keywords")}) {
        auto *text = new QPlainTextEdit;
        text->setObjectName(path);
        text->setFixedHeight(76);
        const auto entry = initial(path, path.endsWith("keywords") ? QJsonValue(QJsonArray{}) : QJsonValue(""));
        if (path.endsWith("keywords")) {
            QStringList words;
            for (auto word : entry.value("value").toArray())
                words.append(word.toString());
            text->setPlainText(words.join('\n'));
        } else
            text->setPlainText(entry.value("value").toString());
        const QString title =
            path.endsWith("keywords") ? QStringLiteral("关键词 · 每行一个") : QStringLiteral("识别提示");
        text->setAccessibleName(title);
        optional(recognition, title, path, text, "");
        connect(text, &QPlainTextEdit::textChanged, this, [text, path, setValue] {
            if (path.endsWith("keywords")) {
                QJsonArray words;
                for (const auto &word : text->toPlainText().split('\n', Qt::SkipEmptyParts))
                    words.append(word.trimmed());
                setValue(path, words);
            } else
                setValue(path, text->toPlainText());
        });
    }
    segmented(recognition, QStringLiteral("出字延迟"), "transcription/delay", {"极低", "低", "中", "高", "极高"},
              {"minimal", "low", "medium", "high", "xhigh"}, "medium");
    auto *probability =
        preference(recognition, QStringLiteral("请求识别对数概率"), QStringLiteral("诊断输出；是否支持由服务端决定。"),
                   initial("logprobs", true).value("enabled").toBool());
    probability->setObjectName("logprobs/send");
    connect(probability, &QCheckBox::toggled, this, [this](bool enabled) {
        realtimeDraft_.insert("logprobs", QJsonObject{{"enabled", enabled}, {"value", true}});
        rebuildRealtimeOptions();
    });
    auto *noise = group(QStringLiteral("输入降噪"));
    const auto wireNoise = settings.engine.realtimeOptions.value("noise_reduction");
    auto *noiseMode = mode(noise, QStringLiteral("降噪方式"), "__noise",
                           {"不发送 · 服务端设置", "关闭", "近场 · 耳麦", "远场 · 会议麦克风"},
                           {"omit", "off", "near_field", "far_field"},
                           wireNoise.isNull() ? "off" : wireNoise.toObject().value("type").toString("omit"));
    auto *strength = new QWidget;
    auto *strengthLayout = new QVBoxLayout(strength);
    strengthLayout->setContentsMargins(0, 0, 0, 0);
    noise->addWidget(strength);
    numeric(strengthLayout, QStringLiteral("降噪强度"), "noise_reduction/strength", 0, 100, 100, 100, "");
    const auto showNoise = [noiseMode, strength] {
        strength->setVisible(noiseMode->currentData() == "near_field" || noiseMode->currentData() == "far_field");
    };
    showNoise();
    connect(noiseMode, &QComboBox::currentIndexChanged, this, showNoise);
    auto *inspect = new QPushButton(QStringLiteral("查看请求与服务端有效配置"));
    engine->addWidget(inspect);
    connect(inspect, &QPushButton::clicked, this, [this] {
        auto *dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(QStringLiteral("会话配置 · 最近一次连接"));
        dialog->resize(600, 420);
        auto *layout = new QVBoxLayout(dialog);
        auto *text = new QPlainTextEdit(dialog);
        text->setReadOnly(true);
        text->setPlainText(QStringLiteral("请求配置\n%1\n服务端有效配置\n%2")
                               .arg(QString::fromUtf8(QJsonDocument(requested_).toJson()),
                                    QString::fromUtf8(QJsonDocument(effective_).toJson())));
        layout->addWidget(text);
        dialog->show();
    });

    auto *correction = page(tabs_, QStringLiteral("文本纠错"));
    controls_.enableCorrection =
        preference(correction, QStringLiteral("启用文本纠错"), QStringLiteral("完成转写后，按轮次优化文字。"),
                   settings.correction.enabled);
    controls_.correctionUrl = new QLineEdit(settings.correction.responsesUrl);
    correction->addWidget(field(QStringLiteral("Responses API 地址"), controls_.correctionUrl, shell));
    auto *recovery = label(QStringLiteral("纠错超时、截断或请求失败时，会恢复识别原文并显示原因。"), shell, true);
    recovery->setFont(uiFont(11));
    correction->addWidget(recovery);

    auto *cache = page(tabs_, QStringLiteral("录音缓存"));
    controls_.enableCache = preference(cache, QStringLiteral("启用录音缓存"),
                                       QStringLiteral("保存原始音频与 VAD 分段。"), settings.cache.enabled);
    controls_.enableCache->setObjectName("cacheEnabled");
    controls_.cacheDirectory = new QLineEdit(
        settings.cache.rootDirectory.isEmpty() ? AppSettings::defaultCacheDirectory() : settings.cache.rootDirectory);
    controls_.cacheDirectory->setObjectName("cacheDirectory");
    controls_.cacheDirectory->setFixedHeight(32);
    controls_.cacheDirectory->setFont(uiFont(12));
    controls_.cacheDirectory->setCursorPosition(0);
    controls_.cacheDirectory->setToolTip(controls_.cacheDirectory->text());
    auto *directoryRow = new QWidget;
    auto *directoryLayout = new QHBoxLayout(directoryRow);
    directoryLayout->setContentsMargins(0, 0, 0, 0);
    directoryLayout->setSpacing(8);
    directoryLayout->addWidget(controls_.cacheDirectory, 1);
    directoryRow->setFocusProxy(controls_.cacheDirectory);
    controls_.browse = new QToolButton(directoryRow);
    controls_.browse->setProperty("iconName", "folder");
    controls_.browse->setFixedSize(32, 32);
    controls_.browse->setIconSize(QSize(16, 16));
    controls_.browse->setAccessibleName(QStringLiteral("选择录音缓存目录"));
    controls_.browse->setToolTip(QStringLiteral("选择录音缓存目录"));
    directoryLayout->addWidget(controls_.browse);
    cache->addWidget(field(QStringLiteral("缓存目录"), directoryRow, shell));
    cache->addWidget(label(QStringLiteral("请选择绝对路径；不存在的目录会自动创建。"), shell, true));
    connect(controls_.cacheDirectory, &QLineEdit::textChanged, controls_.cacheDirectory, &QWidget::setToolTip);
    connect(controls_.cacheDirectory, &QLineEdit::editingFinished, this,
            [this] { controls_.cacheDirectory->setCursorPosition(0); });
    auto *duration = new QWidget;
    auto *durationLayout = new QHBoxLayout(duration);
    durationLayout->setContentsMargins(0, 0, 0, 0);
    durationLayout->setSpacing(8);
    controls_.sliceSeconds = new QSlider(Qt::Horizontal, duration);
    controls_.sliceSeconds->setObjectName("cacheSliceSeconds");
    controls_.sliceSeconds->setRange(1, 3600);
    controls_.sliceSeconds->installEventFilter(this);
    controls_.sliceSeconds->setValue(settings.cache.rawSliceSeconds);
    controls_.sliceSeconds->setMinimumWidth(100);
    controls_.sliceSeconds->setMinimumHeight(32);
    duration->setFocusProxy(controls_.sliceSeconds);
    durationLayout->addWidget(controls_.sliceSeconds, 1);
    auto *seconds = label(QString::number(settings.cache.rawSliceSeconds) + QStringLiteral(" 秒"), duration);
    seconds->setMinimumWidth(62);
    durationLayout->addWidget(seconds);
    connect(controls_.sliceSeconds, &QSlider::valueChanged, this,
            [seconds](int value) { seconds->setText(QString::number(value) + QStringLiteral(" 秒")); });
    cache->addWidget(field(QStringLiteral("原始录音切片时长 · 1–3600 秒"), duration, shell));
    controls_.sliceSeconds->setAccessibleName(QStringLiteral("原始录音切片时长，秒"));
    auto *retention = label(QStringLiteral("缓存按会话分别保存，历史录音不会自动清理。"), shell, true);
    retention->setFont(uiFont(11));
    cache->addWidget(retention);

    auto *appearance = page(tabs_, QStringLiteral("外观"));
    appearance->addWidget(label(QStringLiteral("界面主题"), shell));
    auto *choices = new QHBoxLayout;
    choices->setSpacing(16);
    auto *themeGroup = new QButtonGroup(this);
    themeGroup->setExclusive(true);
    for (const bool dark : {false, true}) {
        auto *choice = new ThemeChoice(dark, shell);
        choices->addWidget(choice, 1);
        themeGroup->addButton(choice);
        connect(choice, &QPushButton::clicked, this, [this, dark] { emit themeSelected(dark ? "abyssus" : "vallum"); });
    }
    appearance->addLayout(choices);
    auto *immediate = label(QStringLiteral("主题立即生效；两套主题使用相同的布局和操作。"), shell, true);
    immediate->setFont(uiFont(11));
    appearance->addWidget(immediate);
    appearance->addWidget(label(QStringLiteral("展开窗适合查看完整转写，紧凑窗便于与其他应用并行使用。"), shell, true));

    appearance->addWidget(
        label(QStringLiteral("版本信息 · aha-flow v%1").arg(QCoreApplication::applicationVersion()), shell, true));
    auto *footer = new QWidget(shell);
    footer->setObjectName("settingsFooter");
    auto *footerLayout = new QVBoxLayout(footer);
    footerLayout->setContentsMargins(0, 0, 0, 0);
    footerLayout->setSpacing(0);
    auto *divider = new QFrame(footer);
    divider->setProperty("role", "divider");
    divider->setFixedHeight(1);
    footerLayout->addWidget(divider);
    auto *actions = new QHBoxLayout;
    actions->setContentsMargins(0, 0, 0, 0);
    actions->addWidget(label(QStringLiteral("自动保存 · 识别配置下次开始时生效"), footer, true), 1);
    auto *done = new QPushButton(QStringLiteral("完成"), footer);
    done->setObjectName("settingsDone");
    done->setFixedSize(72, 32);
    done->setFont(uiFont(13, true));
    done->setAccessibleName(QStringLiteral("完成设置"));
    actions->addWidget(done);
    footerLayout->addLayout(actions);
    layout->addWidget(footer);
    connect(done, &QPushButton::clicked, this, &QDialog::accept);
    connect(tabs_, &QTabWidget::currentChanged, this, &SettingsWindow::scheduleFit);
    for (int i = 0; i < tabs_->count(); ++i)
        static_cast<QScrollArea *>(tabs_->widget(i))->widget()->installEventFilter(this);
    controls_.modelsError->installEventFilter(this);
    applyTheme(settings.theme);
    fitToContents();
}

void SettingsWindow::rebuildRealtimeOptions()
{
    QJsonObject options, transcription, detection, noise;
    const QString vad = realtimeDraft_.value("__vad").toString("omit");
    const QString reduction = realtimeDraft_.value("__noise").toString("omit");
    for (auto it = realtimeDraft_.begin(); it != realtimeDraft_.end(); ++it) {
        const auto entry = it.value().toObject();
        if (!entry.value("enabled").toBool())
            continue;
        const auto parts = it.key().split('/');
        const auto value = entry.value("value");
        if (parts.size() == 2) {
            if (parts[0] == "transcription")
                transcription.insert(parts[1], value);
            if (parts[0] == "turn_detection" && ((vad == "semantic_vad" && parts[1] == "eagerness") ||
                                                 (vad == "server_vad" && parts[1] != "eagerness")))
                detection.insert(parts[1], value);
            if (parts[0] == "noise_reduction")
                noise.insert(parts[1], value);
        } else if (it.key() == "logprobs")
            options.insert("logprobs", value);
    }
    if (!transcription.isEmpty())
        options.insert("transcription", transcription);
    if (vad == "off")
        options.insert("turn_detection", QJsonValue::Null);
    else if (vad != "omit") {
        detection.insert("type", vad);
        options.insert("turn_detection", detection);
    }
    if (reduction == "off")
        options.insert("noise_reduction", QJsonValue::Null);
    else if (reduction != "omit") {
        noise.insert("type", reduction);
        options.insert("noise_reduction", noise);
    }
    emit realtimeChanged(options, realtimeDraft_, realtimeDraft_.value("__language").toString("single"));
}
void SettingsWindow::setSessionConfiguration(const QJsonObject &requested, const QJsonObject &effective)
{
    requested_ = requested;
    effective_ = effective;
}

void SettingsWindow::scheduleFit()
{
    if (fitPending_)
        return;
    fitPending_ = true;
    QTimer::singleShot(0, this, [this] {
        fitPending_ = false;
        if (isVisible())
            fitToContents();
    });
}

void SettingsWindow::fitToContents()
{
    // Keep title, tabs and footer stable across tabs and validation messages.
    // Only the active page scrolls; small screens constrain the outer height.
    const QRect available = screen()->availableGeometry();
    setFixedHeight(qMin(360, qFloor(available.height() * 0.85)));
    if (isVisible())
        move(qBound(available.left(), x(), qMax(available.left(), available.right() - width() + 1)),
             qBound(available.top(), y(), qMax(available.top(), available.bottom() - height() + 1)));
}

void SettingsWindow::watchScreen(QScreen *screen)
{
    disconnect(geometryConnection_);
    disconnect(dpiConnection_);
    geometryConnection_ = connect(screen, &QScreen::availableGeometryChanged, this, &SettingsWindow::scheduleFit);
    dpiConnection_ = connect(screen, &QScreen::logicalDotsPerInchChanged, this, &SettingsWindow::scheduleFit);
    scheduleFit();
}

void SettingsWindow::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    setFocus(Qt::OtherFocusReason);
    if (!screenConnected_) {
        connect(windowHandle(), &QWindow::screenChanged, this, &SettingsWindow::watchScreen);
        screenConnected_ = true;
    }
    watchScreen(screen());
}

void SettingsWindow::applyTheme(const QString &name)
{
    const bool dark = name == "abyssus";
    setProperty("theme", name);
    setPalette(themePalette(dark));
    setStyleSheet(themeStyleSheet(dark));
    for (auto *toggle : findChildren<QCheckBox *>()) {
        toggle->setProperty("dark", dark);
        toggle->update();
    }
    for (auto *combo : findChildren<QComboBox *>()) {
        combo->setProperty("dark", dark);
        combo->update();
    }
    for (auto *choice : findChildren<QPushButton *>()) {
        if (!choice->objectName().startsWith("theme"))
            continue;
        choice->setProperty("dark", dark);
        choice->setChecked(choice->objectName() == (dark ? "themeAbyssus" : "themeVallum"));
        choice->update();
    }
    for (auto *button : findChildren<QToolButton *>()) {
        const QString name = button->property("iconName").toString();
        if (!name.isEmpty())
            button->setIcon(uiIcon(name, dark));
    }
    scheduleFit();
}

bool SettingsWindow::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::Wheel && qobject_cast<QSlider *>(object) &&
        !static_cast<QWidget *>(object)->hasFocus()) {
        event->ignore();
        return true;
    }
    if (event->type() == QEvent::LayoutRequest ||
        (object == controls_.modelsError && (event->type() == QEvent::Show || event->type() == QEvent::Hide)))
        scheduleFit();
    if (!object->property("settingsDragHandle").toBool())
        return QDialog::eventFilter(object, event);
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            dragStart_ = mouse->globalPosition().toPoint();
            windowStart_ = pos();
            dragging_ = !(windowHandle() && windowHandle()->startSystemMove());
            return true;
        }
    } else if (event->type() == QEvent::MouseMove && dragging_) {
        move(windowStart_ + static_cast<QMouseEvent *>(event)->globalPosition().toPoint() - dragStart_);
        return true;
    } else if (event->type() == QEvent::MouseButtonRelease)
        dragging_ = false;
    return false;
}
} // namespace aha
