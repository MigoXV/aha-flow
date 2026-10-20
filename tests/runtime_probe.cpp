#include <QAudioDevice>
#include <QAudioSource>
#include <QCoreApplication>
#include <QEventLoop>
#include <QIODevice>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QSslSocket>
#include <QTextStream>
#include <QTimer>

// 在无系统 Qt 的容器中验证 dlopen 后端；不连接引擎或打开麦克风。
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (!QSslSocket::supportsSsl()) {
        QTextStream(stderr) << "TLS backend unavailable\n";
        return 1;
    }
    QMediaPlayer player;
    if (!player.isAvailable()) {
        QTextStream(stderr) << "Multimedia backend unavailable\n";
        return 1;
    }
    QTextStream(stdout) << "TLS: " << QSslSocket::sslLibraryVersionString()
                        << "; multimedia available; audio inputs: "
                        << QMediaDevices::audioInputs().size() << '\n';
    if (qEnvironmentVariableIsSet("AHA_TEST_AUDIO")) {
        const QAudioDevice device = QMediaDevices::defaultAudioInput();
        if (device.isNull())
            return 2;
        QAudioSource source(device, device.preferredFormat());
        QIODevice *stream = source.start();
        if (!stream)
            return 3;
        QEventLoop loop;
        qint64 bytes = 0;
        QObject::connect(stream, &QIODevice::readyRead, &loop, [&] {
            bytes += stream->readAll().size();
            if (bytes > 0)
                loop.quit();
        });
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
        source.stop();
        QTextStream(stdout) << "Captured bytes from virtual audio input: " << bytes << '\n';
        if (bytes == 0)
            return 4;
    }
    return 0;
}
