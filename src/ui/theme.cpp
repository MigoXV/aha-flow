#include "ui/theme.h"

#include <QFontDatabase>
#include <QRawFont>
#include <QResource>
#include <QPainter>
#include <QSvgRenderer>

static void registerUiResources()
{
    Q_INIT_RESOURCE(aha_ui_assets);
}

namespace aha
{
namespace
{
QString installedFontFamily(const QStringList &preferred, bool chinese = false)
{
    static const QStringList available = QFontDatabase::families();
    for (const QString &candidate : preferred)
        for (const QString &family : available)
            if (family.compare(candidate, Qt::CaseInsensitive) == 0) {
                if (chinese) {
                    QFont probe(family);
                    probe.setPixelSize(14);
                    const auto face = QRawFont::fromFont(probe);
                    if (!face.isValid() || !face.supportsCharacter(QChar(0x4E2D)) ||
                        !face.supportsCharacter(QChar(0x6587)))
                        continue;
                }
                return family;
            }
    return QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
}
} // namespace

void initializeUiResources()
{
    static const bool initialized = [] {
        registerUiResources();
        return true;
    }();
    Q_UNUSED(initialized);
}

const Theme &theme(bool dark)
{
    static const Theme vallum{QColor("#F8F7F2"), QColor("#F3F1EA"), QColor("#FCFBF8"), QColor("#EEECE5"),
                              QColor("#EDF1EE"), QColor("#D8DCD7"), QColor("#C7CDC8"), QColor("#252A2A"),
                              QColor("#656C69"), QColor("#AFC2BC"), QColor("#5D706C"), QColor("#78968F"),
                              QColor("#252A2A"), QColor("#F8F7F2")};
    static const Theme abyssus{QColor("#080A0D"), QColor("#0B0F13"), QColor("#111820"), QColor("#151E27"),
                               QColor("#16212A"), QColor("#273746"), QColor("#3A5268"), QColor("#E7EEF4"),
                               QColor("#AAB7C3"), QColor("#9FC4E2"), QColor("#B4D2E9"), QColor("#789FBE"),
                               QColor("#9FC4E2"), QColor("#080A0D")};
    return dark ? abyssus : vallum;
}

QFont uiFont(int pixels, bool medium, bool latin)
{
    // Stylesheets can retain only the primary family. Resolve an installed face first
    // so missing Noto fonts do not turn into serif substitutes on Windows.
    static const QString chinese = installedFontFamily(
        {
#ifdef Q_OS_WIN
            "Microsoft YaHei UI",
            "微软雅黑 UI",
            "Microsoft YaHei",
            "微软雅黑",
            "DengXian",
            "等线",
#endif
            "Noto Sans SC",
            "Noto Sans CJK SC",
            "Source Han Sans SC",
            "思源黑体",
            "Microsoft YaHei UI",
            "微软雅黑 UI",
            "Microsoft YaHei",
            "微软雅黑",
            "DengXian",
            "等线",
            "PingFang SC",
            "苹方-简",
            "Heiti SC",
            "黑体-简",
            "WenQuanYi Micro Hei",
            "WenQuanYi Zen Hei"},
        true);
    static const QString western = installedFontFamily({
#ifdef Q_OS_WIN
        "Segoe UI",
#endif
        "Inter", "Segoe UI", "Noto Sans", "DejaVu Sans", "Liberation Sans", "Arial"});
    QFont font(latin ? western : chinese);
    font.setStyleHint(QFont::SansSerif);
    font.setPixelSize(pixels);
    font.setWeight(medium ? QFont::Medium : QFont::Normal);
    // Qt's Windows font database selects DirectWrite at high DPI with default
    // hinting. Forcing full hinting instead selects the GDI path in Qt 6.8.
    return font;
}

QString assetPath(const QString &name, bool dark)
{
    return QStringLiteral(":/aha/ui/%1/%2.svg").arg(dark ? "abyssus" : "vallum", name);
}

QIcon uiIcon(const QString &name, bool dark)
{
    // Render bundled SVGs directly; distribution-specific SVG icon plugins are optional.
    QSvgRenderer renderer(assetPath(name, dark));
    QPixmap pixmap(40, 40);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter, QRectF(0, 0, 20, 20));
    return QIcon(pixmap);
}

QPalette themePalette(bool dark)
{
    const auto &t = theme(dark);
    QPalette p;
    p.setColor(QPalette::Window, t.background);
    p.setColor(QPalette::WindowText, t.text);
    p.setColor(QPalette::Base, t.surface);
    p.setColor(QPalette::AlternateBase, t.secondaryBackground);
    p.setColor(QPalette::Text, t.text);
    p.setColor(QPalette::PlaceholderText, t.secondaryText);
    p.setColor(QPalette::Button, t.surface);
    p.setColor(QPalette::ButtonText, t.text);
    p.setColor(QPalette::Highlight, t.strongAccent);
    p.setColor(QPalette::HighlightedText, dark ? t.background : t.onPrimaryButton);
    p.setColor(QPalette::Disabled, QPalette::Text, t.secondaryText);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, t.secondaryText);
    return p;
}

QString themeStyleSheet(bool dark)
{
    const auto &t = theme(dark);
    QString css = QStringLiteral(R"(
        QWidget { color: @text; background: transparent; }
        QFrame#shell, QFrame#settingsShell {
            background: @bg; border: 1px solid @border; border-radius: 12px;
        }
        QLabel[role="caption"], QLabel#version { color: @secondary; }
        QLabel#title { font-size: 18px; font-weight: 600; }
        QLabel[role="heading"] { font-size: 18px; font-weight: 500; }
        QFrame[role="divider"] { background: @border; border: 0; }
        QToolButton {
            background: transparent; border: 2px solid transparent; border-radius: 6px;
        }
        QToolButton:hover { background: @hover; }
        QToolButton:focus { border-color: @focus; }
        QToolButton[primary="true"] { background: @primary; color: @onPrimary; }
        QToolButton[primary="true"]:hover { background: @primary; }
        QToolButton[primary="true"]:disabled { background: @subtle; color: @secondary; }
        QToolButton#recordButton { border-radius: 8px; }
        QToolButton#resizeHandle { border: 0; }
        QPushButton {
            background: @surface; border: 1px solid @border; border-radius: 8px; padding: 0 16px;
        }
        QPushButton:hover { background: @hover; }
        QPushButton:focus { border: 2px solid @focus; }
        QLineEdit, QComboBox, QSpinBox {
            background: @surface; border: 1px solid @border; border-radius: 8px; padding: 0 16px;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border: 2px solid @focus; }
        QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled { background: @subtle; color: @secondary; }
        QComboBox { padding-right: 36px; }
        QComboBox QLineEdit { background: transparent; border: 0; padding: 0; }
        QComboBox::drop-down { border: 0; width: 36px; }
        QComboBox::down-arrow { image: none; width: 0; height: 0; }
        QComboBox QAbstractItemView { background: @surface; selection-background-color: @hover; color: @text; }
        QTabWidget::pane { border: 0; }
        QTabBar { border-bottom: 1px solid @border; }
        QTabBar::tab { color: @secondary; border-bottom: 2px solid transparent; padding: 0 0 14px 0;
                        margin-right: 24px; min-height: 24px; }
        QTabBar::tab:selected { color: @text; border-bottom-color: @strongAccent; }
        QTabBar::tab:focus { background: @hover; }
        QTextEdit, QScrollArea { border: 0; background: transparent; padding: 0; }
        QProgressBar { border: 0; background: @border; border-radius: 0; }
        QProgressBar::chunk { background: @strongAccent; }
        QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }
        QScrollBar::handle:vertical { background: @strongBorder; min-height: 24px; border-radius: 3px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QLabel#statusDetail { border-left: 2px solid @danger; padding-left: 16px; }
    )");
    const QList<QPair<QString, QString>> values{{"@bg", t.background.name()},
                                                {"@border", t.border.name()},
                                                {"@strongBorder", t.strongBorder.name()},
                                                {"@text", t.text.name()},
                                                {"@secondary", t.secondaryText.name()},
                                                {"@hover", t.hover.name()},
                                                {"@surface", t.surface.name()},
                                                {"@subtle", t.subtle.name()},
                                                {"@focus", t.focus.name()},
                                                {"@onPrimary", t.onPrimaryButton.name()},
                                                {"@primary", t.primaryButton.name()},
                                                {"@strongAccent", t.strongAccent.name()},
                                                {"@danger", t.danger.name()}};
    for (const auto &value : values)
        css.replace(value.first, value.second);
    return css;
}
} // namespace aha
