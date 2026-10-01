#include "ui/main_window.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>

#include <memory>

int main(int argc, char *argv[])
{
    bool headless = false;
    for (int index = 1; index < argc; ++index) {
        if (QString::fromLocal8Bit(argv[index]) == QStringLiteral("--check-engine")) {
            headless = true;
        }
    }
    std::unique_ptr<QCoreApplication> app;
    if (headless) {
        app = std::make_unique<QCoreApplication>(argc, argv);
    } else {
        app = std::make_unique<QApplication>(argc, argv);
    }
    QCoreApplication::setApplicationName(QStringLiteral("aha-flow"));
    QCoreApplication::setApplicationVersion(QStringLiteral(AHA_FLOW_VERSION));
    QCoreApplication::setOrganizationName(QStringLiteral("aha-flow"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("aha-flow 原生语音转写客户端"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("engine"), QStringLiteral("引擎 HTTP/HTTPS 基地址"), QStringLiteral("url")});
    parser.addOption({QStringLiteral("allow-self-signed"), QStringLiteral("允许引擎使用不受信任的 TLS 证书")});
    parser.addOption({QStringLiteral("check-engine"), QStringLiteral("无界面获取模型列表后退出")});
    parser.addOption({QStringLiteral("smoke-test"), QStringLiteral("显示窗口后退出，不连接引擎")});
    parser.process(*app);

    aha::EngineConfig config;
    if (parser.isSet(QStringLiteral("engine"))) {
        config.baseUrl = parser.value(QStringLiteral("engine"));
    }
    config.allowUntrustedCertificate = parser.isSet(QStringLiteral("allow-self-signed"));
    // 密钥不进入命令行参数、配置文件或日志。
    config.apiKey = qEnvironmentVariable("AHA_FLOW_API_KEY");

    if (headless) {
        aha::EngineClient engine;
        QObject::connect(&engine, &aha::EngineClient::modelsReady, app.get(), [&app](const QStringList &models) {
            QTextStream(stdout) << models.join(QLatin1Char('\n')) << Qt::endl;
            app->exit(0);
        });
        QObject::connect(&engine, &aha::EngineClient::requestFailed, app.get(), [&app](const QString &message) {
            QTextStream(stderr) << message << Qt::endl;
            app->exit(1);
        });
        QTimer::singleShot(0, &engine, [&engine, config] { engine.fetchModels(config); });
        return app->exec();
    }

    aha::MainWindow window(config);
    window.show();
    if (parser.isSet(QStringLiteral("smoke-test"))) {
        QTimer::singleShot(100, app.get(), &QCoreApplication::quit);
    } else {
        QTimer::singleShot(0, &window, &aha::MainWindow::refreshModels);
    }
    return app->exec();
}
