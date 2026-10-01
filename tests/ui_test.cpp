#include "ui/main_window.h"
#include <QCheckBox>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>
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
        aha::MainWindow window(settings, false);
        window.show();
        QTest::qWait(30);
        QCOMPARE(window.size(), QSize(300, 250));
        window.resize(410, 350);
        window.toggleCompact();
        QCOMPARE(window.size(), QSize(300, 58));
        window.toggleCompact();
        QCOMPARE(window.size(), QSize(410, 350));
        window.showSettings(true);
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
        QVERIFY(window.findChild<QLabel *>("recognitionStatus")->styleSheet().contains("#eff6ff"));
        const QString screenshots = qEnvironmentVariable("AHA_FLOW_SCREENSHOT_DIR");
        if (!screenshots.isEmpty()) {
            QDir().mkpath(screenshots);
            QTest::qWait(30);
            window.grab().save(QDir(screenshots).filePath("expanded.png"));
            window.toggleCompact();
            QTest::qWait(30);
            window.grab().save(QDir(screenshots).filePath("compact.png"));
            window.toggleCompact();
            window.showSettings(true);
            QTest::qWait(30);
            window.grab().save(QDir(screenshots).filePath("settings.png"));
        }
        QSignalSpy closed(window.session(), &aha::SessionController::shutdownReady);
        window.close();
        QTRY_COMPARE(closed.size(), 1);
    }

  private:
    QTemporaryDir settingsDirectory_;
};
QTEST_MAIN(UiTest)
#include "ui_test.moc"
