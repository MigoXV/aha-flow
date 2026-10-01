#include "ui/main_window.h"
#include "ui/theme.h"
#include <QCheckBox>
#include <QAbstractTextDocumentLayout>
#include <QDialog>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextLayout>
#include <QComboBox>
#include <QTextEdit>

class UiTest final : public QObject
{
    Q_OBJECT
  private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("aha-flow-tests");
        QCoreApplication::setApplicationName("ui-tests");
        QCoreApplication::setApplicationVersion("0.2.0");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory_.path());
    }
    void compactSettingsAndTranscript()
    {
        aha::AppSettings settings;
        settings.cache.rootDirectory = settingsDirectory_.path();
        settings.engine.baseUrl = QStringLiteral("http://127.0.0.1:1");
        aha::MainWindow window(settings, false);
        window.show();
        QTest::qWait(30);
        QCOMPARE(window.size(), QSize(360, 420));
        window.resize(410, 350);
        window.toggleCompact();
        QCOMPARE(window.size(), QSize(360, 64));
        window.toggleCompact();
        QCOMPARE(window.size(), QSize(410, 350));
        window.showSettings(true);
        auto *dialog = window.findChild<QDialog *>("settingsDialog");
        QVERIFY(dialog->isWindow());
        QVERIFY(dialog->isVisible());
        QCOMPARE(dialog->size(), QSize(480, 640));
        QCOMPARE(window.size(), QSize(410, 350));
        window.findChild<QTabWidget *>("settingsTabs")->setCurrentIndex(2);
        window.findChild<QCheckBox *>("cacheEnabled")->setChecked(false);
        window.findChild<QLineEdit *>("cacheDirectory")->setText(settingsDirectory_.filePath("recordings"));
        window.findChild<QSpinBox *>("cacheSliceSeconds")->setValue(42);
        QSettings store;
        const auto loaded = aha::AppSettings::load(store);
        QVERIFY(!loaded.cache.enabled);
        QCOMPARE(loaded.cache.rawSliceSeconds, 42);
        window.showSettings(false);
        window.session()->transcriptChanged({"one", "临时文字", false, true, false, {}});
        window.session()->transcriptChanged({"one", "正式文字", true, false, false, {}});
        QCOMPARE(window.findChild<QTextEdit *>("transcript")->toPlainText(), QStringLiteral("正式文字"));
        window.session()->transcriptChanged({"two", "第二轮", true, false, false, {}});
        window.session()->transcriptChanged({"one", "纠正文字", true, false, false, {}});
        QCOMPARE(window.findChild<QTextEdit *>("transcript")->toPlainText(), QStringLiteral("纠正文字\n第二轮"));
        window.session()->transcriptChanged({"one", "第一行\n第二行", true, false, false, {}});
        window.session()->transcriptChanged({"two", "第二轮更新", true, false, false, {}});
        QCOMPARE(window.findChild<QTextEdit *>("transcript")->toPlainText(),
                 QStringLiteral("第一行\n第二行\n第二轮更新"));
        window.session()->statusChanged(QStringLiteral("语义轮次未结束"), {});
        QCOMPARE(window.findChild<QLabel *>("recognitionStatus")->text(), QStringLiteral("语义轮次未结束"));
        const QString screenshots = qEnvironmentVariable("AHA_FLOW_SCREENSHOT_DIR");
        if (!screenshots.isEmpty()) {
            QDir().mkpath(screenshots);
            window.resize(360, 420);
            window.session()->transcriptChanged(
                {"one", "今天先确认两个调整方向。\n缓存目录可以在设置里调整。", true, false, false, {}});
            window.session()->transcriptChanged(
                {"two", "原始录音按 300 秒切片，\nVAD 分段与原始文字一并保存。", false, true, false, {}});
            window.session()->started();
            window.session()->statusChanged(QStringLiteral("聆听中"), {});
            window.session()->levelChanged(0.37f);
            QTest::qWait(30);
            window.grab().save(QDir(screenshots).filePath("expanded.png"));
            window.toggleCompact();
            QTest::qWait(30);
            window.grab().save(QDir(screenshots).filePath("compact.png"));
            window.toggleCompact();
            window.showSettings(true);
            window.findChild<QTabWidget *>("settingsTabs")->setCurrentIndex(0);
            QTest::qWait(30);
            dialog->grab().save(QDir(screenshots).filePath("settings.png"));
            for (int tab = 1; tab < 4; ++tab) {
                window.findChild<QTabWidget *>("settingsTabs")->setCurrentIndex(tab);
                QTest::qWait(30);
                dialog->grab().save(QDir(screenshots).filePath(QStringLiteral("settings-%1.png").arg(tab)));
            }
            window.findChild<QPushButton *>("themeAbyssus")->click();
            QTest::qWait(30);
            window.grab().save(QDir(screenshots).filePath("expanded-dark.png"));
            dialog->grab().save(QDir(screenshots).filePath("settings-dark.png"));
            window.showSettings(false);
            window.toggleCompact();
            QTest::qWait(30);
            window.grab().save(QDir(screenshots).filePath("compact-dark.png"));
        }
        QSignalSpy closed(window.session(), &aha::SessionController::shutdownReady);
        window.close();
        QTRY_COMPARE(closed.size(), 1);
    }
    void naturalParagraphsAndThemePersistence()
    {
        aha::AppSettings settings;
        settings.cache.rootDirectory = settingsDirectory_.path();
        aha::MainWindow window(settings, false);
        window.show();
        auto *text = window.findChild<QTextEdit *>("transcript");
        window.session()->transcriptChanged({"a", "第一段第一行\n第一段第二行", true, false, false, {}});
        window.session()->transcriptChanged({"b", "第二段", true, false, false, {}});
        QCOMPARE(text->toPlainText(), QStringLiteral("第一段第一行\n第一段第二行\n第二段"));
        auto *document = text->document();
        QCOMPARE(document->blockCount(), 2);
        const auto first = document->firstBlock();
        const auto second = first.next();
        QCOMPARE(first.blockFormat().topMargin(), 0.0);
        QCOMPARE(first.blockFormat().bottomMargin(), 0.0);
        QCOMPARE(first.blockFormat().lineHeight(), 22.0);
        QCOMPARE(first.blockFormat().lineHeightType(), int(QTextBlockFormat::FixedHeight));
        const QRectF firstBounds = document->documentLayout()->blockBoundingRect(first);
        const QRectF secondBounds = document->documentLayout()->blockBoundingRect(second);
        const auto firstLine = first.layout()->lineAt(0);
        const auto lastLine = first.layout()->lineAt(1);
        const auto nextLine = second.layout()->lineAt(0);
        QCOMPARE(lastLine.y() - firstLine.y(), 22.0);
        const qreal lastBaseline = firstBounds.top() + lastLine.y() + lastLine.ascent();
        const qreal nextBaseline = secondBounds.top() + nextLine.y() + nextLine.ascent();
        QCOMPARE(nextBaseline - lastBaseline, 22.0);
        QTextCursor selection(document);
        selection.setPosition(0);
        selection.setPosition(3, QTextCursor::KeepAnchor);
        text->setTextCursor(selection);
        window.findChild<QPushButton *>("themeAbyssus")->click();
        QCOMPARE(text->toPlainText(), QStringLiteral("第一段第一行\n第一段第二行\n第二段"));
        QCOMPARE(text->textCursor().selectedText(), QStringLiteral("第一段"));
        QCOMPARE(first.begin().fragment().charFormat().foreground().color(), QColor("#E7EEF4"));
        QSettings store;
        QCOMPARE(aha::AppSettings::load(store).theme, QStringLiteral("abyssus"));
        aha::MainWindow restored(aha::AppSettings::load(store), false);
        QCOMPARE(restored.property("theme").toString(), QStringLiteral("abyssus"));
        window.session()->transcriptChanged({"b", "正在修订", true, false, true, {}});
        QVERIFY(text->toPlainText().contains(QStringLiteral("正在纠错")));
        window.session()->transcriptChanged({"b", "保留原文", true, false, false, "请求超时"});
        QVERIFY(text->toPlainText().contains(QStringLiteral("纠错失败，已保留原文：请求超时")));
        QVERIFY(!text->toPlainText().contains(QStringLiteral("正在纠错")));
        window.session()->transcriptChanged({"b", "最终文字", true, false, false, {}});
        QCOMPARE(text->toPlainText(), QStringLiteral("第一段第一行\n第一段第二行\n最终文字"));
        window.session()->transcriptChanged({"b", "\r\n最终文字\r\n\r\n", true, false, false, {}});
        QCOMPARE(text->toPlainText(), QStringLiteral("第一段第一行\n第一段第二行\n最终文字"));
        for (int i = 0; i < 30; ++i)
            window.session()->transcriptChanged({QString::number(i), "长记录也保持普通行距。", true, false, false, {}});
        QTest::qWait(20);
        auto *scroll = text->verticalScrollBar();
        QVERIFY(scroll->maximum() > 44);
        scroll->setValue(44);
        window.findChild<QPushButton *>("themeVallum")->click();
        QCOMPARE(scroll->value(), 44);
    }
    void keyboardSettingsAndLongFields()
    {
        aha::AppSettings settings;
        settings.engine.baseUrl = QStringLiteral("http://127.0.0.1:1");
        settings.engine.model = QString(300, QLatin1Char('m'));
        settings.cache.rootDirectory = settingsDirectory_.path();
        aha::MainWindow window(settings, false);
        window.show();
        window.showSettings(true);
        auto *dialog = window.findChild<QDialog *>("settingsDialog");
        QTest::qWait(20);
        QCOMPARE(dialog->size(), QSize(480, 640));
        auto *model = window.findChild<QComboBox *>("model");
        QVERIFY(model->width() < 300);
        window.findChild<QTabWidget *>("settingsTabs")->setCurrentIndex(2);
        auto *toggle = window.findChild<QCheckBox *>("cacheEnabled");
        toggle->setFocus();
        QTest::keyClick(toggle, Qt::Key_Space);
        QVERIFY(!toggle->isChecked());
        QSettings store;
        QVERIFY(!aha::AppSettings::load(store).cache.enabled);
        auto *directory = window.findChild<QLineEdit *>("cacheDirectory");
        const QString path = settingsDirectory_.path() + "/" + QString(300, QLatin1Char('a'));
        directory->setText(path);
        QCOMPARE(directory->toolTip(), path);
        QCOMPARE(dialog->size(), QSize(480, 640));
        QTest::keyClick(dialog, Qt::Key_Escape);
        QVERIFY(!dialog->isVisible());
        QVERIFY(window.isVisible());
    }
    void bundledIconsRenderWithoutOptionalPlugins()
    {
        aha::initializeUiResources();
        for (const bool dark : {false, true})
            for (const QString &name :
                 QStringList{"mic", "stop", "settings", "close", "collapse", "expand", "folder", "chevron"}) {
                const QImage image = aha::uiIcon(name, dark).pixmap(20, 20).toImage();
                QVERIFY2(!image.isNull(), qPrintable(name));
                bool visible = false;
                for (int y = 0; y < image.height(); ++y)
                    for (int x = 0; x < image.width(); ++x)
                        visible |= qAlpha(image.pixel(x, y)) != 0;
                QVERIFY2(visible, qPrintable(name));
            }
    }

  private:
    QTemporaryDir settingsDirectory_;
};
QTEST_MAIN(UiTest)
#include "ui_test.moc"
