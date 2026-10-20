#include "ui/main_window.h"
#include "ui/settings_window.h"
#include "ui/theme.h"
#include <QCheckBox>
#include <QAbstractTextDocumentLayout>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QFontInfo>
#include <QGlyphRun>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QRawFont>
#include <QScrollBar>
#include <QScrollArea>
#include <QScreen>
#include <QLayout>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>
#include <QTabWidget>
#include <QTabBar>
#include <QToolButton>
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
        QApplication::setFont(aha::uiFont());
    }
    void fontsSurviveStylingAndDpi()
    {
#ifdef Q_OS_WIN
        if (QGuiApplication::platformName() != QStringLiteral("windows"))
            QSKIP("Windows glyph diagnostics require the native windows font backend.");
#endif
        qInfo("Checking native font selection and widget styling");
        aha::AppSettings settings;
        settings.theme = QStringLiteral("vallum");
        settings.engine.baseUrl = QStringLiteral("http://127.0.0.1:1");
        aha::MainWindow window(settings, false);
        window.show();
        window.showSettings(true);
        const QFont expected = aha::uiFont();
        const auto face = QRawFont::fromFont(expected);
        qInfo("UI family: %s; physical face: %s", qPrintable(expected.family()), qPrintable(face.familyName()));
        const bool hasChinese = face.supportsCharacter(QChar(0x4E2D)) && face.supportsCharacter(QChar(0x6587));
        QCOMPARE(QApplication::font().family(), expected.family());
        QCOMPARE(expected.hintingPreference(), QFont::PreferDefaultHinting);
#ifdef Q_OS_WIN
        const auto installed = QFontDatabase::families();
        if (!installed.contains(QStringLiteral("Noto Sans SC"), Qt::CaseInsensitive) &&
            !installed.contains(QStringLiteral("Noto Sans CJK SC"), Qt::CaseInsensitive) &&
            !installed.contains(QStringLiteral("Source Han Sans SC"), Qt::CaseInsensitive) &&
            installed.contains(QStringLiteral("Microsoft YaHei UI"), Qt::CaseInsensitive))
            QCOMPARE(expected.family().toLower(), QStringLiteral("microsoft yahei ui"));
        if (!installed.contains(QStringLiteral("Inter"), Qt::CaseInsensitive) &&
            installed.contains(QStringLiteral("Segoe UI"), Qt::CaseInsensitive))
            QCOMPARE(aha::uiFont(12, false, true).family().toLower(), QStringLiteral("segoe ui"));
#endif
        if (!hasChinese)
            qWarning("This test host has no preferred CJK font; inspect glyph fallbacks in fonts.json.");
        auto *dialog = window.findChild<QDialog *>("settingsDialog");
        auto *transcript = window.findChild<QTextEdit *>("transcript");
        window.session()->transcriptChanged({"font", "中文转写设置录音", true, false, false, {}});
        QJsonArray report;
        for (const QString &theme : {QStringLiteral("vallum"), QStringLiteral("abyssus")}) {
            window.findChild<QPushButton *>(theme == "vallum" ? "themeVallum" : "themeAbyssus")->click();
            QTest::qWait(30);
            QList<QWidget *> controls{dialog,
                                      transcript,
                                      window.findChild<QLabel *>("recognitionStatus"),
                                      window.findChild<QToolButton *>("recordButton"),
                                      window.findChild<QComboBox *>("engineAddress"),
                                      window.findChild<QComboBox *>("engineAddress")->lineEdit(),
                                      window.findChild<QComboBox *>("model"),
                                      window.findChild<QTabWidget *>("settingsTabs")->tabBar()};
            for (auto *label : dialog->findChildren<QLabel *>())
                controls.append(label);
            for (auto *edit : dialog->findChildren<QLineEdit *>())
                controls.append(edit);
            for (auto *button : dialog->findChildren<QPushButton *>())
                controls.append(button);
            for (auto *control : controls) {
                QVERIFY(control);
                const QFont font = control->font();
                qInfo("Checking %s/%s, font %s", control->metaObject()->className(), qPrintable(control->objectName()),
                      qPrintable(font.family()));
                QCOMPARE(font.family(), expected.family());
                QCOMPARE(font.hintingPreference(), QFont::PreferDefaultHinting);
                QTextLayout layout(QStringLiteral("中文转写设置录音"), font);
                layout.beginLayout();
                auto line = layout.createLine();
                QVERIFY(line.isValid());
                line.setLineWidth(1000);
                layout.endLayout();
                QJsonArray glyphFamilies;
                const auto runs = layout.glyphRuns();
                QVERIFY(!runs.isEmpty());
                for (const auto &run : runs) {
                    glyphFamilies.append(run.rawFont().familyName());
                    if (hasChinese) {
                        QCOMPARE(run.rawFont().familyName(), face.familyName());
                        for (const auto glyph : run.glyphIndexes())
                            QVERIFY(glyph != 0);
                    }
                }
                report.append(QJsonObject{{"theme", theme},
                                          {"widget", control->metaObject()->className()},
                                          {"name", control->objectName()},
                                          {"family", font.family()},
                                          {"resolvedFamily", QFontInfo(font).family()},
                                          {"pixelSize", font.pixelSize()},
                                          {"chineseGlyphFamilies", glyphFamilies}});
            }
            QCOMPARE(transcript->document()->defaultFont().family(), expected.family());
            QCOMPARE(transcript->document()->firstBlock().begin().fragment().charFormat().font().family(),
                     expected.family());
        }
        const auto screenshot = window.grab();
        QCOMPARE(window.size(), QSize(360, 300));
        verifySettingsGeometry(dialog);
        QCOMPARE(screenshot.size(),
                 QSize(qRound(360 * screenshot.devicePixelRatio()), qRound(300 * screenshot.devicePixelRatio())));
        const QString screenshots = qEnvironmentVariable("AHA_FLOW_SCREENSHOT_DIR");
        if (!screenshots.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshots));
            QFile output(QDir(screenshots).filePath("fonts.json"));
            QVERIFY(output.open(QIODevice::WriteOnly));
            const QJsonDocument document(QJsonObject{{"platform", QGuiApplication::platformName()},
                                                     {"devicePixelRatio", screenshot.devicePixelRatio()},
                                                     {"preferredChineseAvailable", hasChinese},
                                                     {"controls", report}});
            QCOMPARE(output.write(document.toJson()), qint64(document.toJson().size()));
        }
    }
    void compactSettingsAndTranscript()
    {
        aha::AppSettings settings;
        settings.cache.rootDirectory = settingsDirectory_.path();
        settings.engine.baseUrl = QStringLiteral("http://127.0.0.1:1");
        aha::MainWindow window(settings, false);
        window.show();
        QTest::qWait(30);
        QCOMPARE(window.size(), QSize(360, 232));
        const QString capture = qEnvironmentVariable("AHA_FLOW_SCREENSHOT_DIR");
        if (!capture.isEmpty()) {
            QDir().mkpath(capture);
            window.session()->started();
            window.grab().save(QDir(capture).filePath("waiting.png"));
        }
        window.resize(410, 350);
        window.toggleCompact();
        QCOMPARE(window.size(), QSize(360, 44));
        window.toggleCompact();
        QCOMPARE(window.size(), QSize(410, 350));
        window.showSettings(true);
        auto *dialog = window.findChild<QDialog *>("settingsDialog");
        QVERIFY(dialog->isWindow());
        QVERIFY(dialog->isVisible());
        verifySettingsGeometry(dialog);
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
        QCOMPARE(window.findChild<QLabel *>("recognitionStatus")->text(), QStringLiteral("等待续说"));
        const QString screenshots = qEnvironmentVariable("AHA_FLOW_SCREENSHOT_DIR");
        if (!screenshots.isEmpty()) {
            QDir().mkpath(screenshots);
            window.resize(360, 300);
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
        QCOMPARE(first.blockFormat().topMargin(), 18.0);
        QCOMPARE(first.blockFormat().bottomMargin(), 6.0);
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
        QCOMPARE(nextBaseline - lastBaseline, 40.0);
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
        verifySettingsGeometry(dialog);
        auto *model = window.findChild<QComboBox *>("model");
        QVERIFY(model->width() < window.findChild<QComboBox *>("engineAddress")->width());
        auto *enginePage = qobject_cast<QScrollArea *>(window.findChild<QTabWidget *>("settingsTabs")->currentWidget());
        enginePage->ensureWidgetVisible(model);
        QVERIFY(enginePage->viewport()->rect().contains(
            QRect(model->mapTo(enginePage->viewport(), QPoint()), model->size())));
        window.findChild<QTabWidget *>("settingsTabs")->setCurrentIndex(2);
        auto *toggle = window.findChild<QCheckBox *>("cacheEnabled");
        toggle->setFocus();
        QTest::keyClick(toggle, Qt::Key_Space);
        QVERIFY(!toggle->isChecked());
        QSettings store;
        QVERIFY(!aha::AppSettings::load(store).cache.enabled);
        auto *directory = window.findChild<QLineEdit *>("cacheDirectory");
        QTest::qWait(20);
        const QSize cacheSize = dialog->size();
        const QString path = settingsDirectory_.path() + "/" + QString(300, QLatin1Char('a'));
        directory->setText(path);
        QCOMPARE(directory->toolTip(), path);
        QTest::qWait(20);
        QCOMPARE(dialog->size(), cacheSize);
        verifySettingsGeometry(dialog);
        QTest::keyClick(dialog, Qt::Key_Escape);
        QVERIFY(!dialog->isVisible());
        QVERIFY(window.isVisible());
    }
    void settingsFollowContentAndKeepFooterVisible()
    {
        aha::initializeUiResources();
        for (const QString &theme : {QStringLiteral("vallum"), QStringLiteral("abyssus")}) {
            aha::AppSettings settings;
            settings.theme = theme;
            aha::SettingsWindow dialog(settings, nullptr);
            dialog.show();
            QTest::qWait(30);
            verifySettingsGeometry(&dialog);
            const int engineHeight = dialog.height();
            QCOMPARE(engineHeight, qMin(360, int(dialog.screen()->availableGeometry().height() * 0.85)));
            auto *tabs = dialog.findChild<QTabWidget *>("settingsTabs");
            for (int tab : {1, 2, 3, 0}) {
                tabs->setCurrentIndex(tab);
                QTest::qWait(20);
                QCOMPARE(dialog.height(), engineHeight);
                verifySettingsGeometry(&dialog);
            }
            auto *error = dialog.controls().modelsError;
            error->setText(QStringLiteral("无法获取模型列表，请检查服务地址。\n").repeated(100));
            error->show();
            auto *enginePage = qobject_cast<QScrollArea *>(tabs->currentWidget());
            QTRY_VERIFY(enginePage->verticalScrollBar()->maximum() > 0);
            QTRY_COMPARE(dialog.height(), engineHeight);
            verifySettingsGeometry(&dialog);
            enginePage->ensureWidgetVisible(dialog.controls().enableAha);
            const QRect toggleBounds(dialog.controls().enableAha->mapTo(enginePage->viewport(), QPoint()),
                                     dialog.controls().enableAha->size());
            QVERIFY(enginePage->viewport()->rect().contains(toggleBounds));
            error->hide();
            QTRY_COMPARE(dialog.height(), engineHeight);
            verifySettingsGeometry(&dialog);
            // Reopening a short tab must not retain the previous engine page's height.
            tabs->setCurrentIndex(1);
            QTest::qWait(20);
            const QSize shortSize = dialog.size();
            dialog.hide();
            dialog.show();
            QTest::qWait(20);
            QCOMPARE(dialog.size(), shortSize);
        }
    }
    void stableChromeAndEdgeResize()
    {
        aha::AppSettings settings;
        settings.engine.baseUrl = QStringLiteral("http://127.0.0.1:1");
        aha::MainWindow window(settings, false);
        window.show();
        QTest::qWait(30);
        QCOMPARE(window.size(), QSize(360, 232));
        auto *clock = window.findChild<QLabel *>("sessionTime");
        auto *record = window.findChild<QToolButton *>("recordButton");
        QCOMPARE(clock->width(), 36);
        QCOMPARE(record->size(), QSize(104, 36));
        const QRect clockBounds = clock->geometry();
        const QRect actionsBounds(record->mapTo(&window, QPoint()), record->size());
        for (const QString &state :
             {QStringLiteral("连接中"), QStringLiteral("等待中"), QStringLiteral("语义轮次未结束"),
              QStringLiteral("错误"), QStringLiteral("待机")}) {
            window.session()->statusChanged(state,
                                            state == QStringLiteral("错误") ? QStringLiteral("服务不可达") : QString());
            QTest::qWait(5);
            QCOMPARE(window.size(), QSize(360, 232));
            QCOMPARE(clock->geometry(), clockBounds);
            QCOMPARE(QRect(record->mapTo(&window, QPoint()), record->size()), actionsBounds);
        }
        window.session()->transcriptChanged({"first", "实时内容", false, true, false, {}});
        QCOMPARE(window.size(), QSize(360, 300));
        for (int i = 0; i < 40; ++i)
            window.session()->transcriptChanged(
                {QString::number(i), "转写正文持续增长但窗口不跳高。", true, false, false, {}});
        QCOMPARE(window.size(), QSize(360, 300));
        auto *transcript = window.findChild<QTextEdit *>("transcript");
        QTest::qWait(20);
        QVERIFY(transcript->verticalScrollBar()->maximum() > 0);
        if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
            const auto drag = [&window](const char *name, const QPoint &delta) {
                auto *handle = window.findChild<QWidget *>(name);
                QVERIFY(handle && handle->isVisible());
                const QPoint local = handle->rect().center();
                const QPoint global = handle->mapToGlobal(local);
                QMouseEvent press(QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton,
                                  Qt::NoModifier);
                QApplication::sendEvent(handle, &press);
                QMouseEvent move(QEvent::MouseMove, local + delta, global + delta, Qt::NoButton, Qt::LeftButton,
                                 Qt::NoModifier);
                QApplication::sendEvent(handle, &move);
                QMouseEvent release(QEvent::MouseButtonRelease, local + delta, global + delta, Qt::LeftButton,
                                    Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(handle, &release);
            };
            const int right = window.geometry().right();
            drag("resizeLeft", QPoint(-40, 0));
            QCOMPARE(window.width(), 400);
            QCOMPARE(window.geometry().right(), right);
            const int left = window.x();
            drag("resizeRight", QPoint(30, 0));
            QCOMPARE(window.width(), 430);
            QCOMPARE(window.x(), left);
            drag("resizeBottom", QPoint(0, 40));
            QCOMPARE(window.height(), 340);
            drag("resizeBottom", QPoint(0, -1000));
            QCOMPARE(window.height(), 220);
        }
        const QSize manual = window.size();
        window.toggleCompact();
        QCOMPARE(window.size(), QSize(360, 44));
        window.toggleCompact();
        QCOMPARE(window.size(), manual);
        window.session()->transcriptChanged({"more", "继续转写", true, false, false, {}});
        QCOMPARE(window.size(), manual);
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
    static void verifySettingsGeometry(QDialog *dialog)
    {
        QCOMPARE(dialog->width(), 480);
        QVERIFY(dialog->height() <= int(dialog->screen()->availableGeometry().height() * 0.85));
        auto *footer = dialog->findChild<QWidget *>("settingsFooter");
        QVERIFY(footer);
        QVERIFY(footer->isVisible());
        const QRect bounds(footer->mapTo(dialog, QPoint()), footer->size());
        QVERIFY(dialog->rect().contains(bounds));
    }
    QTemporaryDir settingsDirectory_;
};
QTEST_MAIN(UiTest)
#include "ui_test.moc"
