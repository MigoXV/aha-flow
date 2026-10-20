#include <QAudioDevice>
#include <QCoreApplication>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QSslSocket>
#include <QTextStream>

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
    return 0;
}
