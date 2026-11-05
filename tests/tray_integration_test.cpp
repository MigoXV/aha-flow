#include "ui/main_window.h"
#include <QAction>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusArgument>
#include <QProcess>
#include <QProcessEnvironment>
#include <QDBusContext>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QMenu>
#include <QSignalSpy>
#include <QSystemTrayIcon>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

// A desktop tray host on an isolated session bus. Exercise Qt's real SNI backend,
// rather than replacing isSystemTrayAvailable or assuming a running process has an icon.
class TrayHost final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierWatcher")
    Q_PROPERTY(bool IsStatusNotifierHostRegistered READ hostRegistered)
    Q_PROPERTY(int ProtocolVersion READ protocolVersion)
    Q_PROPERTY(QStringList RegisteredStatusNotifierItems READ items)
  public:
    bool hostRegistered() const
    {
        return true;
    }
    int protocolVersion() const
    {
        return 0;
    }
    QStringList items() const
    {
        return registered;
    }
    QString service, path;
    QStringList registered;
  public slots:
    void RegisterStatusNotifierItem(const QString &item)
    {
        service = item.startsWith('/') ? message().service() : item;
        path = item.startsWith('/') ? item : QString("/StatusNotifierItem");
        registered.append(service + path);
        QTextStream(stdout) << "REGISTERED " << service << ' ' << path << Qt::endl;
        emit StatusNotifierItemRegistered(service + path);
    }
    void RegisterStatusNotifierHost(const QString &) {}
  signals:
    void StatusNotifierItemRegistered(const QString &service);
    void StatusNotifierItemUnregistered(const QString &service);
    void StatusNotifierHostRegistered();
};

// Qt 6.4 introspects the watcher synchronously: a watcher in the UI thread
// deadlocks. A separate host process also matches how real desktop panels run.
class TrayHostProcess : public QObject
{
  public:
    TrayHostProcess()
    {
        connect(&process, &QProcess::readyReadStandardOutput, this, [this] {
            output += process.readAllStandardOutput();
            while (output.contains('\n')) {
                const int end = output.indexOf('\n');
                const auto parts = output.left(end).trimmed().split(' ');
                output.remove(0, end + 1);
                if (parts.value(0) == "READY")
                    ready = true;
                if (parts.value(0) == "REGISTERED") {
                    service = QString::fromUtf8(parts.value(1));
                    path = QString::fromUtf8(parts.value(2));
                }
            }
        });
        process.start(QCoreApplication::applicationFilePath(), {"--tray-host"});
    }
    ~TrayHostProcess() override
    {
        stop();
    }
    void stop()
    {
        if (process.state() != QProcess::NotRunning) {
            process.terminate();
            if (!process.waitForFinished(1000)) {
                process.kill();
                process.waitForFinished(1000);
            }
        }
    }
    QProcess process;
    bool ready = false;
    QString service, path;
    QByteArray output;
};

namespace
{
int actionId(const QDBusArgument &layout, const QString &label)
{
    int id = -1;
    QVariantMap properties;
    layout.beginStructure();
    layout >> id >> properties;
    int match = properties.value("label").toString() == label ? id : -1;
    layout.beginArray();
    while (!layout.atEnd()) {
        QDBusVariant child;
        layout >> child;
        const int nested = actionId(qvariant_cast<QDBusArgument>(child.variant()), label);
        if (nested >= 0)
            match = nested;
    }
    layout.endArray();
    layout.endStructure();
    return match;
}
} // namespace

class TrayIntegrationTest final : public QObject
{
    Q_OBJECT
  private slots:
    void registersIconAndRestoresWindow()
    {
        QTemporaryDir directory;
        QCoreApplication::setOrganizationName("aha-flow-tests");
        QCoreApplication::setApplicationName("tray-integration");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        TrayHostProcess host;
        QTRY_VERIFY(host.ready);
        QTRY_VERIFY(QSystemTrayIcon::isSystemTrayAvailable());
        QPixmap pixels(32, 32);
        pixels.fill(Qt::darkBlue);
        aha::AppSettings settings;
        settings.cache.enabled = false;
        aha::MainWindow window(settings, false);
        window.enableSystemTray(QIcon(pixels));
        window.show();
        QTRY_VERIFY(!host.service.isEmpty());
        auto *tray = window.findChild<QSystemTrayIcon *>();
        QVERIFY(tray);
        QVERIFY(tray->isVisible());
        // Icon pixmaps and a menu must actually be exported on D-Bus.
        auto properties =
            QDBusMessage::createMethodCall(host.service, host.path, "org.freedesktop.DBus.Properties", "GetAll");
        properties << "org.kde.StatusNotifierItem";
        QDBusPendingCallWatcher reply(bus.asyncCall(properties));
        QTRY_VERIFY(reply.isFinished());
        QDBusPendingReply<QVariantMap> result = reply;
        QVERIFY2(!result.isError(), qPrintable(result.error().message()));
        const auto pixmaps = qvariant_cast<QDBusArgument>(result.value().value("IconPixmap"));
        pixmaps.beginArray();
        QVERIFY(!pixmaps.atEnd());
        int width = 0, height = 0;
        QByteArray iconBytes;
        pixmaps.beginStructure();
        pixmaps >> width >> height >> iconBytes;
        pixmaps.endStructure();
        QVERIFY(width > 0 && height > 0);
        QCOMPARE(iconBytes.size(), width * height * 4);
        const auto menuPath = qvariant_cast<QDBusObjectPath>(result.value().value("Menu")).path();
        QVERIFY(menuPath != "/" && !menuPath.isEmpty());
        QSignalSpy exiting(&window, &aha::MainWindow::exitReady);
        QSignalSpy lastWindow(qApp, &QApplication::lastWindowClosed);
        window.close();
        QVERIFY(!window.isVisible());
        QCOMPARE(lastWindow.size(), 0);
        QCOMPARE(exiting.size(), 0);
        auto activate =
            QDBusMessage::createMethodCall(host.service, host.path, "org.kde.StatusNotifierItem", "Activate");
        activate << 0 << 0;
        bus.asyncCall(activate);
        QTRY_VERIFY(window.isVisible());
        window.close();
        host.stop();
        QTRY_VERIFY(!QSystemTrayIcon::isSystemTrayAvailable());
        QTRY_VERIFY_WITH_TIMEOUT(window.isVisible(), 5000);
        window.requestExit();
        QTRY_COMPARE(exiting.size(), 1);
        QVERIFY(!tray->isVisible());
    }
    void applicationExportsTrayAndExitsFromMenu()
    {
        QTemporaryDir directory;
        auto bus = QDBusConnection::sessionBus();
        TrayHostProcess host;
        QTRY_VERIFY(host.ready);
        QProcess app;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("XDG_CONFIG_HOME", directory.path());
        environment.remove("AHA_FLOW_API_KEY");
        app.setProcessEnvironment(environment);
        const QString executable = qEnvironmentVariable("AHA_FLOW_TRAY_TEST_APP", QStringLiteral(AHA_FLOW_EXECUTABLE));
        app.start(executable, {"--no-autostart"});
        QTRY_COMPARE(app.state(), QProcess::Running);
        QTRY_VERIFY(!host.service.isEmpty());
        auto get = QDBusMessage::createMethodCall(host.service, host.path, "org.freedesktop.DBus.Properties", "GetAll");
        get << "org.kde.StatusNotifierItem";
        QDBusPendingCallWatcher properties(bus.asyncCall(get));
        QTRY_VERIFY(properties.isFinished());
        QDBusPendingReply<QVariantMap> props = properties;
        QVERIFY2(!props.isError(), qPrintable(props.error().message()));
        const auto iconPixels = qvariant_cast<QDBusArgument>(props.value().value("IconPixmap"));
        iconPixels.beginArray();
        QVERIFY(!iconPixels.atEnd());
        const auto menuPath = qvariant_cast<QDBusObjectPath>(props.value().value("Menu")).path();
        QVERIFY(!menuPath.isEmpty() && menuPath != "/");
        auto query = QDBusMessage::createMethodCall(host.service, menuPath, "com.canonical.dbusmenu", "GetLayout");
        query << 0 << -1 << QStringList{"label"};
        QDBusPendingCallWatcher menuReply(bus.asyncCall(query));
        QTRY_VERIFY(menuReply.isFinished());
        QVERIFY(menuReply.reply().type() != QDBusMessage::ErrorMessage);
        const auto layout = qvariant_cast<QDBusArgument>(menuReply.reply().arguments().value(1));
        const int exitId = actionId(layout, QStringLiteral("退出 aha-flow"));
        QVERIFY(exitId >= 0);
        auto click = QDBusMessage::createMethodCall(host.service, menuPath, "com.canonical.dbusmenu", "Event");
        click << exitId << "clicked" << QVariant::fromValue(QDBusVariant(0)) << uint(0);
        bus.asyncCall(click);
        QTRY_COMPARE(app.state(), QProcess::NotRunning);
        QCOMPARE(app.exitStatus(), QProcess::NormalExit);
        QCOMPARE(app.exitCode(), 0);
        host.stop();
    }
};
int main(int argc, char **argv)
{
    if (argc == 2 && QByteArray(argv[1]) == "--tray-host") {
        QCoreApplication app(argc, argv);
        auto bus = QDBusConnection::sessionBus();
        TrayHost host;
        if (!bus.registerObject("/StatusNotifierWatcher", &host,
                                QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals |
                                    QDBusConnection::ExportAllProperties) ||
            !bus.registerService("org.kde.StatusNotifierWatcher"))
            return 1;
        QTextStream(stdout) << "READY" << Qt::endl;
        return app.exec();
    }
    QApplication app(argc, argv);
    TrayIntegrationTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "tray_integration_test.moc"
