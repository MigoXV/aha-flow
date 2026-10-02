#include "ui/main_window.h"
#include "ui/theme.h"
#include "network/realtime_client.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QIcon>
#include <QSettings>
#include <QTextStream>
#include <QTimer>
#include <memory>

int main(int argc, char *argv[])
{
    QElapsedTimer startup;
    startup.start();
    bool headless = false;
    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        if (argument == "--check-engine" || argument == "--check-session" || argument == "--replay-pcm" ||
            argument.startsWith("--replay-pcm=") || argument == "--help" || argument == "--version")
            headless = true;
    }
    std::unique_ptr<QCoreApplication> app;
    if (headless)
        app = std::make_unique<QCoreApplication>(argc, argv);
    else {
        app = std::make_unique<QApplication>(argc, argv);
        QApplication::setFont(aha::uiFont());
        QIcon icon;
        for (const int size : {16, 24, 32, 48, 64, 128, 256, 512})
            icon.addFile(QStringLiteral(":/aha/app/aha-flow-%1.png").arg(size), QSize(size, size));
        QApplication::setWindowIcon(icon);
        QApplication::setDesktopFileName(QStringLiteral("aha-flow"));
    }
    QCoreApplication::setApplicationName("aha-flow");
    QCoreApplication::setApplicationVersion(AHA_FLOW_VERSION);
    QCoreApplication::setOrganizationName("aha-flow");
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("aha-flow 原生语音转写客户端"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"engine", QStringLiteral("本次运行的引擎 HTTP/HTTPS 基地址"), "url"});
    parser.addOption({"allow-self-signed", QStringLiteral("本次运行允许不受信任的引擎 TLS 证书")});
    parser.addOption({"check-engine", QStringLiteral("无界面获取模型列表后退出")});
    parser.addOption({"check-session", QStringLiteral("无界面验证 Realtime 会话配置，不采集音频")});
    parser.addOption({"replay-pcm", QStringLiteral("回放 24 kHz 单声道 PCM16LE 文件，不打开麦克风"), "file"});
    parser.addOption({"cache-dir", QStringLiteral("本次回放使用的缓存目录"), "directory"});
    parser.addOption({"no-cache", QStringLiteral("本次回放禁用录音缓存")});
    parser.addOption({"no-autostart", QStringLiteral("显示窗口但不自动开始识别")});
    parser.addOption({"smoke-test", QStringLiteral("显示窗口后退出，不连接引擎")});
    parser.addOption({"benchmark", QStringLiteral("不连接引擎，输出首帧时间并在 5 秒后退出")});
    parser.process(*app);
    QSettings store;
    const aha::AppSettings saved = aha::AppSettings::load(store);
    aha::AppSettings effective = saved;
    if (parser.isSet("engine"))
        effective.engine.baseUrl = parser.value("engine");
    if (parser.isSet("allow-self-signed"))
        effective.engine.allowUntrustedCertificate = true;
    if (qEnvironmentVariableIsSet("AHA_FLOW_API_KEY"))
        effective.engine.apiKey = qEnvironmentVariable("AHA_FLOW_API_KEY");
    if (parser.isSet("cache-dir"))
        effective.cache.rootDirectory = parser.value("cache-dir");
    if (parser.isSet("no-cache"))
        effective.cache.enabled = false;
    if (headless) {
        if (parser.isSet("check-engine")) {
            aha::EngineClient engine;
            QObject::connect(&engine, &aha::EngineClient::modelsReady, app.get(), [&app](const QStringList &models) {
                QTextStream(stdout) << models.join('\n') << Qt::endl;
                app->exit(0);
            });
            QObject::connect(&engine, &aha::EngineClient::requestFailed, app.get(), [&app](const QString &message) {
                QTextStream(stderr) << message << Qt::endl;
                app->exit(1);
            });
            QTimer::singleShot(0, &engine, [&engine, effective] { engine.fetchModels(effective.engine); });
            return app->exec();
        }
        if (parser.isSet("check-session")) {
            aha::RealtimeClient client;
            QObject::connect(&client, &aha::RealtimeClient::ready, app.get(), [&] {
                QTextStream(stdout) << "Realtime transcription session ready" << Qt::endl;
                client.stop();
                app->exit(0);
            });
            QObject::connect(&client, &aha::RealtimeClient::failed, app.get(), [&](const QString &message) {
                QTextStream(stderr) << message << Qt::endl;
                app->exit(1);
            });
            QTimer::singleShot(0, &client, [&] { client.start(effective.engine); });
            return app->exec();
        }
        if (parser.isSet("replay-pcm")) {
            aha::SessionController session;
            bool failed = false;
            QObject::connect(&session, &aha::SessionController::statusChanged, app.get(),
                             [&](const QString &label, const QString &detail) {
                                 QTextStream(stderr)
                                     << label << (detail.isEmpty() ? QString() : "：" + detail) << Qt::endl;
                                 if (label == QStringLiteral("错误")) {
                                     failed = true;
                                     session.shutdown();
                                 }
                             });
            QObject::connect(&session, &aha::SessionController::cacheDirectory, app.get(), [](const QString &path) {
                if (!path.isEmpty())
                    QTextStream(stderr) << "Cache: " << path << Qt::endl;
            });
            QObject::connect(&session, &aha::SessionController::transcriptChanged, app.get(),
                             [](const aha::Transcript &result) {
                                 QTextStream(stdout) << QJsonDocument(QJsonObject{{"item_id", result.itemId},
                                                                                  {"transcript", result.text},
                                                                                  {"final", result.final},
                                                                                  {"interim", result.interim},
                                                                                  {"correcting", result.correcting},
                                                                                  {"error", result.correctionError}})
                                                            .toJson(QJsonDocument::Compact)
                                                     << Qt::endl;
                             });
            QObject::connect(&session, &aha::SessionController::finished, &session, &aha::SessionController::shutdown);
            QObject::connect(&session, &aha::SessionController::shutdownReady, app.get(),
                             [&] { app->exit(failed ? 1 : 0); });
            const QString replay = parser.value("replay-pcm");
            QTimer::singleShot(0, &session, [&] { session.start(effective, replay); });
            return app->exec();
        }
        return 0;
    }
    const bool smoke = parser.isSet("smoke-test"), benchmark = parser.isSet("benchmark");
    aha::MainWindow window(saved, !smoke && !benchmark && !parser.isSet("no-autostart"));
    window.setRuntimeOverrides(parser.isSet("engine") ? parser.value("engine") : QString(),
                               qEnvironmentVariable("AHA_FLOW_API_KEY"), parser.isSet("allow-self-signed"),
                               qEnvironmentVariableIsSet("AHA_FLOW_API_KEY"));
    if (benchmark)
        QObject::connect(&window, &aha::MainWindow::firstFrameRendered, &window,
                         [&] { QTextStream(stdout) << "first_frame_ms=" << startup.elapsed() << Qt::endl; });
    window.show();
    if (smoke)
        QTimer::singleShot(100, &window, &QWidget::close);
    if (benchmark)
        QTimer::singleShot(5000, &window, &QWidget::close);
    return app->exec();
}
