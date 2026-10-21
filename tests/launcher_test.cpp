#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTest>

class LauncherTest final : public QObject
{
    Q_OBJECT
  private slots:
    void nativeEntryPointPreservesArgumentsAndEnvironment()
    {
        QTemporaryDir directory;
        const QString original = directory.filePath(QStringLiteral("original package"));
        const QString moved = directory.filePath(QStringLiteral("移动后的 包"));
        QVERIFY(QDir().mkpath(original + "/bin"));
        QVERIFY(QFile::copy(QStringLiteral(AHA_LAUNCHER_PATH), original + "/aha-flow"));
        QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(), original + "/bin/aha-flow"));
        QVERIFY(QDir().rename(original, moved));
        const QString entry = moved + "/aha-flow";
        QFile file(entry);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.read(4), QByteArray::fromHex("7f454c46"));
        const auto mime = QMimeDatabase().mimeTypeForFile(entry);
        QVERIFY(mime.inherits("application/x-executable") || mime.name() == "application/x-pie-executable");
        QVERIFY(QFileInfo(entry).isExecutable());

        QProcess process;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("LD_LIBRARY_PATH", "/existing libraries");
        environment.insert("XDG_CACHE_HOME", directory.filePath("cache"));
        process.setProcessEnvironment(environment);
        process.setWorkingDirectory(directory.path());
        const QStringList arguments{"--launcher-payload", QStringLiteral("中文 空格"),
                                    "$(not-a-shell)",     "semi;colon",
                                    "quote\"value",       "--version"};
        process.start(entry, arguments);
        QVERIFY(process.waitForStarted());
        const qint64 pid = process.processId();
        QVERIFY(process.waitForFinished());
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 23);
        const auto result = QJsonDocument::fromJson(process.readAllStandardOutput()).object();
        QCOMPARE(result.value("pid").toVariant().toLongLong(), pid); // exec, with no shell or terminal child.
        QCOMPARE(result.value("arguments").toArray().toVariantList(),
                 QJsonArray::fromStringList(arguments.mid(1)).toVariantList());
        QCOMPARE(result.value("LD_LIBRARY_PATH").toString(), moved + "/lib:/existing libraries");
        QCOMPARE(result.value("QT_PLUGIN_PATH").toString(), moved + "/plugins");
        QCOMPARE(result.value("QT_QPA_PLATFORM_PLUGIN_PATH").toString(), moved + "/plugins/platforms");
        QCOMPARE(result.value("QT_MEDIA_BACKEND").toString(), QStringLiteral("gstreamer"));
        QCOMPARE(result.value("GST_PLUGIN_SYSTEM_PATH_1_0").toString(), moved + "/lib/gstreamer-1.0");
        QCOMPARE(result.value("GST_PLUGIN_PATH_1_0").toString(), moved + "/lib/gstreamer-1.0");
        QCOMPARE(result.value("GST_PLUGIN_SCANNER_1_0").toString(), moved + "/libexec/gst-plugin-scanner");
        QCOMPARE(result.value("GST_REGISTRY_1_0").toString(),
                 directory.filePath("cache/aha-flow/gstreamer-registry.bin"));
        QCOMPARE(result.value("HOME").toString(), QString::fromLocal8Bit(qgetenv("HOME")));
    }
    void missingPayloadFailsWithoutSpawningAnotherProgram()
    {
        QTemporaryDir directory;
        const QString entry = directory.filePath("aha-flow");
        QVERIFY(QFile::copy(QStringLiteral(AHA_LAUNCHER_PATH), entry));
        QProcess process;
        process.start(entry, {"--version"});
        QVERIFY(process.waitForFinished());
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 127);
        QVERIFY(process.readAllStandardError().contains("start bundled application"));
    }
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().value(1) == "--launcher-payload") {
        QJsonObject result{{"pid", app.applicationPid()},
                           {"arguments", QJsonArray::fromStringList(app.arguments().mid(2))}};
        for (const char *name : {"LD_LIBRARY_PATH", "QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "QT_MEDIA_BACKEND",
                                 "GST_PLUGIN_SYSTEM_PATH_1_0", "GST_PLUGIN_PATH_1_0", "GST_PLUGIN_SCANNER_1_0",
                                 "GST_REGISTRY_1_0", "HOME"})
            result.insert(QString::fromLatin1(name), QString::fromLocal8Bit(qgetenv(name)));
        QFile output;
        if (!output.open(stdout, QIODevice::WriteOnly))
            return 24;
        output.write(QJsonDocument(result).toJson(QJsonDocument::Compact));
        return 23;
    }
    LauncherTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "launcher_test.moc"
