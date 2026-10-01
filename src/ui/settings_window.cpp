#include "ui/settings_window.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

namespace aha
{
namespace
{
QLabel *label(const QString &text, QWidget *parent, bool caption = false)
{
    auto *l = new QLabel(text, parent);
    l->setFont(uiFont(caption ? 12 : 14, !caption));
    l->setProperty("role", caption ? "caption" : "label");
    l->setMinimumHeight(caption ? 18 : 22);
    l->setWordWrap(true);
    return l;
}

QWidget *field(const QString &name, QWidget *control, QWidget *parent)
{
    auto *group = new QWidget(parent);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    auto *l = label(name, group);
    l->setBuddy(control);
    control->setAccessibleName(name);
    control->setFixedHeight(40);
    layout->addWidget(l);
    layout->addWidget(control);
    return group;
}

QCheckBox *preference(QVBoxLayout *layout, const QString &name, const QString &help, bool checked)
{
    auto *row = new QWidget;
    auto *horizontal = new QHBoxLayout(row);
    horizontal->setContentsMargins(0, 0, 0, 0);
    horizontal->setSpacing(16);
    auto *copy = new QVBoxLayout;
    copy->setSpacing(4);
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
    layout->setContentsMargins(0, 16, 0, 0);
    layout->setSpacing(16);
    scroll->setWidget(contents);
    tabs->addTab(scroll, title);
    return layout;
}
} // namespace

SettingsWindow::SettingsWindow(const AppSettings &settings, QWidget *parent) : QDialog(parent)
{
    setObjectName("settingsDialog");
    setWindowTitle(QStringLiteral("aha-flow 设置"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFont(uiFont());
    resize(480, 640);
    setMinimumSize(480, 480);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *shell = new QFrame(this);
    shell->setObjectName("settingsShell");
    outer->addWidget(shell);
    auto *layout = new QVBoxLayout(shell);
    layout->setContentsMargins(23, 23, 23, 23);
    layout->setSpacing(16);
    auto *header = new QWidget(shell);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);
    auto *title = label(QStringLiteral("设置"), header);
    title->setFont(uiFont(18, true));
    title->setProperty("role", "heading");
    headerLayout->addWidget(title, 1);
    auto *close = new QToolButton(header);
    close->setProperty("iconName", "close");
    close->setToolTip(QStringLiteral("关闭设置"));
    close->setAccessibleName(QStringLiteral("关闭设置"));
    close->setIconSize(QSize(20, 20));
    close->setFixedSize(32, 32);
    headerLayout->addWidget(close);
    layout->addWidget(header);
    header->installEventFilter(this);
    title->installEventFilter(this);
    connect(close, &QToolButton::clicked, this, &QDialog::reject);
    layout->addWidget(label(QStringLiteral("配置自动保存；识别配置在下次开始时生效。"), shell, true));

    tabs_ = new QTabWidget(shell);
    tabs_->setObjectName("settingsTabs");
    tabs_->tabBar()->setFont(uiFont(14, true));
    tabs_->setDocumentMode(true);
    tabs_->setUsesScrollButtons(false);
    tabs_->tabBar()->setDrawBase(false);
    layout->addWidget(tabs_, 1);

    auto *engine = page(tabs_, QStringLiteral("识别引擎"));
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
    pairLayout->setSpacing(16);
    controls_.models = new IconComboBox(pair);
    controls_.models->setObjectName("model");
    controls_.models->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    controls_.models->setMinimumContentsLength(12);
    controls_.models->addItem(settings.engine.model);
    pairLayout->addWidget(field(QStringLiteral("模型"), controls_.models, pair), 1);
    controls_.language = new QLineEdit(settings.engine.language, pair);
    controls_.language->setPlaceholderText(QStringLiteral("留空默认"));
    controls_.language->setToolTip(QStringLiteral("留空使用服务端默认值"));
    auto *languageField = field(QStringLiteral("语言"), controls_.language, pair);
    languageField->setFixedWidth(120);
    pairLayout->addWidget(languageField);
    engine->addWidget(pair);
    controls_.modelsError = label({}, shell, true);
    controls_.modelsError->setObjectName("modelsError");
    controls_.modelsError->setMinimumWidth(0);
    controls_.modelsError->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    controls_.modelsError->hide();
    engine->addWidget(controls_.modelsError);
    controls_.apiKey = new QLineEdit(settings.engine.apiKey);
    controls_.apiKey->setEchoMode(QLineEdit::Password);
    controls_.apiKey->setPlaceholderText(QStringLiteral("可选；仅用于识别引擎"));
    engine->addWidget(field(QStringLiteral("API Key"), controls_.apiKey, shell));
    controls_.allowUntrusted =
        preference(engine, QStringLiteral("允许自签名证书"), QStringLiteral("仅用于你信任的局域网服务。"),
                   settings.engine.allowUntrustedCertificate);
    controls_.enableAha = preference(engine, QStringLiteral("AHA 扩展协议"),
                                     QStringLiteral("显示中间预览与语义轮次状态。"), settings.engine.enableAha);
    engine->addStretch();

    auto *correction = page(tabs_, QStringLiteral("文本纠错"));
    controls_.enableCorrection =
        preference(correction, QStringLiteral("启用文本纠错"), QStringLiteral("完成转写后，按轮次优化文字。"),
                   settings.correction.enabled);
    controls_.correctionUrl = new QLineEdit(settings.correction.responsesUrl);
    correction->addWidget(field(QStringLiteral("Responses API 地址"), controls_.correctionUrl, shell));
    auto *recovery = label(QStringLiteral("纠错超时、截断或请求失败时，会恢复识别原文并显示原因。"), shell, true);
    recovery->setFont(uiFont(14));
    correction->addWidget(recovery);
    correction->addStretch();

    auto *cache = page(tabs_, QStringLiteral("录音缓存"));
    controls_.enableCache = preference(cache, QStringLiteral("启用录音缓存"),
                                       QStringLiteral("保存原始音频与 VAD 分段。"), settings.cache.enabled);
    controls_.enableCache->setObjectName("cacheEnabled");
    controls_.cacheDirectory = new QLineEdit(
        settings.cache.rootDirectory.isEmpty() ? AppSettings::defaultCacheDirectory() : settings.cache.rootDirectory);
    controls_.cacheDirectory->setObjectName("cacheDirectory");
    controls_.cacheDirectory->setFixedHeight(40);
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
    controls_.browse->setIconSize(QSize(20, 20));
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
    controls_.sliceSeconds = new QSpinBox(duration);
    controls_.sliceSeconds->setObjectName("cacheSliceSeconds");
    controls_.sliceSeconds->setButtonSymbols(QAbstractSpinBox::NoButtons);
    controls_.sliceSeconds->setRange(1, 3600);
    controls_.sliceSeconds->setValue(settings.cache.rawSliceSeconds);
    controls_.sliceSeconds->setFixedSize(112, 40);
    duration->setFocusProxy(controls_.sliceSeconds);
    durationLayout->addWidget(controls_.sliceSeconds);
    durationLayout->addWidget(label(QStringLiteral("秒"), duration));
    durationLayout->addWidget(label(QStringLiteral("1–3600 秒"), duration, true));
    durationLayout->addStretch();
    cache->addWidget(field(QStringLiteral("原始录音切片时长"), duration, shell));
    controls_.sliceSeconds->setAccessibleName(QStringLiteral("原始录音切片时长，秒"));
    auto *retention = label(QStringLiteral("缓存按会话分别保存，历史录音不会自动清理。"), shell, true);
    retention->setFont(uiFont(14));
    cache->addWidget(retention);
    cache->addStretch();

    auto *appearance = page(tabs_, QStringLiteral("外观"));
    appearance->addWidget(label(QStringLiteral("界面主题"), shell));
    auto *choices = new QHBoxLayout;
    choices->setSpacing(16);
    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    for (const bool dark : {false, true}) {
        auto *choice = new ThemeChoice(dark, shell);
        choices->addWidget(choice, 1);
        group->addButton(choice);
        connect(choice, &QPushButton::clicked, this, [this, dark] { emit themeSelected(dark ? "abyssus" : "vallum"); });
    }
    appearance->addLayout(choices);
    auto *immediate = label(QStringLiteral("主题立即生效；两套主题使用相同的布局和操作。"), shell, true);
    immediate->setFont(uiFont(14));
    appearance->addWidget(immediate);
    appearance->addWidget(label(QStringLiteral("展开窗适合查看完整转写，紧凑窗便于与其他应用并行使用。"), shell, true));
    appearance->addStretch();

    auto *footer = new QHBoxLayout;
    footer->addWidget(label(QStringLiteral("自动保存"), shell, true), 1);
    auto *done = new QPushButton(QStringLiteral("完成"), shell);
    done->setFixedSize(72, 40);
    done->setAccessibleName(QStringLiteral("完成设置"));
    footer->addWidget(done);
    layout->addLayout(footer);
    connect(done, &QPushButton::clicked, this, &QDialog::accept);
    applyTheme(settings.theme);
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
}

bool SettingsWindow::eventFilter(QObject *, QEvent *event)
{
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
