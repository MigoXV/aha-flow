#include "ui/main_window.h"
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QMenu>
#include <QPixmap>
#include <QSignalSpy>
#include <QSystemTrayIcon>
#include <QTemporaryDir>
#include <QTest>
#include <QWebSocket>
#include <QWebSocketServer>

namespace
{
QIcon testIcon()
{
    QPixmap pixels(32, 32);
    pixels.fill(QColor("#252A2A"));
    return QIcon(pixels);
}
QAction *action(aha::MainWindow &window, const char *name)
{
    return window.findChild<QAction *>(QString::fromLatin1(name));
}
} // namespace
class TrayTest final : public QObject
{
    Q_OBJECT
  private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("aha-flow-tests");
        QCoreApplication::setApplicationName("tray-tests");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory_.path());
    }
    void visibilityAndHostRecovery_data()
    {
        QTest::addColumn<QString>("theme");
        QTest::newRow("light") << QString("vallum");
        QTest::newRow("dark") << QString("abyssus");
    }
    void visibilityAndHostRecovery()
    {
        QFETCH(QString, theme);
        aha::AppSettings settings;
        settings.theme = theme;
        settings.cache.enabled = false;
        settings.engine.baseUrl = "http://127.0.0.1:1";
        aha::MainWindow window(settings, false);
        bool host = true;
        window.enableSystemTray(testIcon(), [&host] { return host; });
        window.show();
        QSignalSpy stopped(window.session(), &aha::SessionController::shutdownReady);
        QSignalSpy exiting(&window, &aha::MainWindow::exitReady);
        auto *controller = window.findChild<aha::SystemTray *>();
        auto *icon = window.findChild<QSystemTrayIcon *>();
        QVERIFY(controller);
        QVERIFY(icon);
        QVERIFY(icon->isVisible());
        QVERIFY(!icon->icon().isNull());
        QVERIFY(controller->available());
        QVERIFY(icon->contextMenu());
        QVERIFY(action(window, "trayWindow"));
        QVERIFY(action(window, "trayRecording"));
        QVERIFY(action(window, "traySettings"));
        QVERIFY(action(window, "trayExit"));
        QCOMPARE(action(window, "trayWindow")->text(), QString("隐藏窗口"));
        const QString screenshots = qEnvironmentVariable("AHA_FLOW_SCREENSHOT_DIR");
        if (!screenshots.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshots));
            icon->contextMenu()->popup(window.mapToGlobal(QPoint(12, 40)));
            QTest::qWait(30);
            icon->contextMenu()->grab().save(QDir(screenshots).filePath(QString("tray-%1.png").arg(theme)));
            icon->contextMenu()->hide();
        }
        window.resize(410, 350);
        window.toggleCompact();
        const auto size = window.size();
        const auto position = window.pos();
        window.close();
        QVERIFY(!window.isVisible());
        QCOMPARE(stopped.size(), 0);
        QCOMPARE(exiting.size(), 0);
        QCOMPARE(action(window, "trayWindow")->text(), QString("显示窗口"));
        action(window, "trayWindow")->trigger();
        QVERIFY(window.isVisible());
        QCOMPARE(window.size(), size);
        QCOMPARE(window.pos(), position);
        icon->activated(QSystemTrayIcon::Context);
        QVERIFY(window.isVisible());
        icon->activated(QSystemTrayIcon::Trigger);
        QVERIFY(window.isVisible());
        action(window, "trayWindow")->trigger();
        QVERIFY(!window.isVisible());
        icon->activated(QSystemTrayIcon::DoubleClick);
        QVERIFY(window.isVisible());
        window.showMinimized();
        icon->activated(QSystemTrayIcon::Trigger);
        QVERIFY(window.isVisible());
        QVERIFY(!window.isMinimized());
        window.close();
        action(window, "traySettings")->trigger();
        QVERIFY(window.isVisible());
        QVERIFY(window.findChild<QDialog *>("settingsDialog")->isVisible());
        window.close();
        QVERIFY(!window.findChild<QDialog *>("settingsDialog")->isVisible());
        host = false;
        controller->refreshAvailability();
        QVERIFY(window.isVisible());
        host = true;
        controller->refreshAvailability();
        window.close();
        QVERIFY(!window.isVisible());
        host = false;
        // Closing must check current host availability, even before the next poll.
        window.restoreWindow();
        window.close();
        QTRY_COMPARE(exiting.size(), 1);
        QCOMPARE(stopped.size(), 1);
        QVERIFY(!icon->isVisible());
    }
    void unavailableOrMissingIcon_data()
    {
        QTest::addColumn<bool>("host");
        QTest::newRow("no-host") << false;
        QTest::newRow("no-icon") << true;
    }
    void unavailableOrMissingIcon()
    {
        QFETCH(bool, host);
        aha::AppSettings settings;
        aha::MainWindow window(settings, false);
        window.enableSystemTray(host ? QIcon() : testIcon(), [host] { return host; });
        window.show();
        QVERIFY(!window.findChild<aha::SystemTray *>()->available());
        QSignalSpy exiting(&window, &aha::MainWindow::exitReady);
        window.close();
        QTRY_COMPARE(exiting.size(), 1);
        QVERIFY(!window.isVisible());
    }
    void exitWaitsForFinalTranscriptAndCache_data()
    {
        QTest::addColumn<bool>("stopViaMenu");
        QTest::newRow("exit-during-recording") << false;
        QTest::newRow("stop-then-exit") << true;
    }
    void exitWaitsForFinalTranscriptAndCache()
    {
        QFETCH(bool, stopViaMenu);
        QFile pcm(directory_.filePath("input.pcm"));
        QVERIFY(pcm.open(QIODevice::WriteOnly));
        pcm.write(QByteArray(48000, '\0'));
        pcm.close();
        QWebSocketServer server("tray-test", QWebSocketServer::NonSecureMode);
        QVERIFY(server.listen(QHostAddress::LocalHost));
        bool committed = false;
        int chunks = 0;
        connect(&server, &QWebSocketServer::newConnection, &server, [&] {
            auto *peer = server.nextPendingConnection();
            connect(peer, &QWebSocket::disconnected, peer, &QObject::deleteLater);
            connect(peer, &QWebSocket::textMessageReceived, peer, [&, peer](const QString &text) {
                const auto event = QJsonDocument::fromJson(text.toUtf8()).object();
                const auto send = [peer](QJsonObject event) {
                    peer->sendTextMessage(QString::fromUtf8(QJsonDocument(event).toJson()));
                };
                if (event.value("type") == "session.update")
                    send({{"type", "session.updated"}, {"session", event.value("session")}});
                if (event.value("type") == "input_audio_buffer.append")
                    ++chunks;
                if (event.value("type") == "input_audio_buffer.commit") {
                    committed = true;
                    send({{"type", "input_audio_buffer.committed"}, {"item_id", "final"}});
                    QTimer::singleShot(100, peer, [send] {
                        send({{"type", "conversation.item.input_audio_transcription.completed"},
                              {"item_id", "final"},
                              {"transcript", "托盘退出前完成转写"}});
                    });
                }
            });
            peer->sendTextMessage("{\"type\":\"session.created\"}");
        });
        aha::AppSettings settings;
        settings.cache.rootDirectory = directory_.filePath(stopViaMenu ? "cache-stop" : "cache-exit");
        settings.engine.baseUrl = QString("http://127.0.0.1:%1").arg(server.serverPort());
        settings.engine.realtimeOptions = {{"turn_detection", QJsonValue::Null}};
        aha::MainWindow window(settings, false);
        window.enableSystemTray(testIcon(), [] { return true; });
        window.show();
        QSignalSpy exiting(&window, &aha::MainWindow::exitReady);
        QSignalSpy transcript(window.session(), &aha::SessionController::transcriptChanged);
        QSignalSpy cache(window.session(), &aha::SessionController::cacheDirectory);
        window.session()->start(settings, pcm.fileName());
        QVERIFY(!action(window, "trayRecording")->isEnabled());
        QTRY_VERIFY(chunks >= 1);
        QCOMPARE(action(window, "trayRecording")->text(), QString("停止识别"));
        window.close();
        QVERIFY(!window.isVisible());
        QVERIFY(window.session()->active());
        QCOMPARE(exiting.size(), 0);
        if (stopViaMenu) {
            QSignalSpy finished(window.session(), &aha::SessionController::finished);
            action(window, "trayRecording")->trigger();
            QVERIFY(!action(window, "trayRecording")->isEnabled());
            QTRY_COMPARE(finished.size(), 1);
            QCOMPARE(exiting.size(), 0);
            QVERIFY(window.findChild<QSystemTrayIcon *>()->isVisible());
            QVERIFY(action(window, "trayRecording")->isEnabled());
            QCOMPARE(action(window, "trayRecording")->text(), QString("开始识别"));
        }
        action(window, "trayExit")->trigger();
        QVERIFY(!window.findChild<QSystemTrayIcon *>()->isVisible());
        action(window, "trayExit")->trigger();
        window.requestExit();
        QVERIFY(!action(window, "trayRecording")->isEnabled());
        QVERIFY(!action(window, "traySettings")->isEnabled());
        QCOMPARE(exiting.size(), 0);
        QTRY_VERIFY(committed);
        QTRY_COMPARE(exiting.size(), 1);
        QCOMPARE(transcript.size(), 1);
        QCOMPARE(qvariant_cast<aha::Transcript>(transcript.first().first()).text, QString("托盘退出前完成转写"));
        QVERIFY(!window.session()->active());
        QCOMPARE(cache.size(), 1);
        const QString path = cache.first().first().toString();
        QVERIFY(QFile::exists(path + "/000001.flac"));
        QVERIFY(!QFile::exists(path + "/pending.pcm"));
        QVERIFY(!window.findChild<QSystemTrayIcon *>()->isVisible());
    }
    void recordingActionAndStatus()
    {
        aha::AppSettings settings;
        settings.cache.enabled = false;
        settings.engine.baseUrl = "http://127.0.0.1:1";
        aha::MainWindow window(settings, false);
        bool host = false;
        window.enableSystemTray(testIcon(), [&host] { return host; });
        window.show();
        host = true;
        window.findChild<aha::SystemTray *>()->refreshAvailability();
        QVERIFY(window.findChild<aha::SystemTray *>()->available());
        QSignalSpy status(window.session(), &aha::SessionController::statusChanged);
        action(window, "trayRecording")->trigger();
        QVERIFY(!status.isEmpty());
        QCOMPARE(status.first().first().toString(), QString("连接中"));
        QVERIFY(!action(window, "trayRecording")->isEnabled());
        QTRY_VERIFY(!window.session()->active());
        QVERIFY(action(window, "trayRecording")->isEnabled());
        QCOMPARE(action(window, "trayRecording")->text(), QString("开始识别"));
        QVERIFY(window.findChild<QSystemTrayIcon *>()->toolTip().contains(QStringLiteral("错误")));
        QSignalSpy exiting(&window, &aha::MainWindow::exitReady);
        window.close();
        action(window, "trayExit")->trigger();
        QTRY_COMPARE(exiting.size(), 1);
    }

  private:
    QTemporaryDir directory_;
};
QTEST_MAIN(TrayTest)
#include "tray_test.moc"
