#include "ui/main_window.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextDocument>
#include <QVBoxLayout>

namespace aha {

MainWindow::MainWindow(const EngineConfig &config, QWidget *parent)
    : QWidget(parent), config_(config)
{
    setWindowTitle(QStringLiteral("aha-flow"));
    setWindowFlag(Qt::WindowStaysOnTopHint);
    resize(380, 340);
    setMinimumSize(320, 260);

    auto *layout = new QVBoxLayout(this);
    auto *title = new QLabel(QStringLiteral("aha-flow · 实时语音转写"), this);
    layout->addWidget(title);

    auto *form = new QFormLayout;
    address_ = new QLineEdit(config_.baseUrl, this);
    form->addRow(QStringLiteral("引擎地址"), address_);
    models_ = new QComboBox(this);
    models_->addItem(config_.model);
    form->addRow(QStringLiteral("模型"), models_);
    layout->addLayout(form);

    allowUntrusted_ = new QCheckBox(QStringLiteral("允许不受信任的 TLS 证书（局域网）"), this);
    allowUntrusted_->setChecked(config_.allowUntrustedCertificate);
    allowUntrusted_->setToolTip(QStringLiteral("开启后，本次引擎请求会跳过 TLS 证书错误检查。"));
    layout->addWidget(allowUntrusted_);

    refresh_ = new QPushButton(QStringLiteral("获取模型列表"), this);
    layout->addWidget(refresh_);
    status_ = new QLabel(QStringLiteral("等待连接"), this);
    status_->setWordWrap(true);
    layout->addWidget(status_);

    auto *transcript = new QPlainTextEdit(this);
    transcript->setReadOnly(true);
    transcript->document()->setMaximumBlockCount(1000);
    transcript->setPlaceholderText(QStringLiteral("项目已初始化；麦克风采集和实时转写将在后续接入。"));
    layout->addWidget(transcript, 1);

    connect(refresh_, &QPushButton::clicked, this, &MainWindow::refreshModels);
    connect(&engine_, &EngineClient::modelsReady, this, [this](const QStringList &models) {
        refresh_->setEnabled(true);
        const QString selected = models_->currentText();
        models_->clear();
        models_->addItems(models);
        const int index = models_->findText(selected);
        if (index >= 0) {
            models_->setCurrentIndex(index);
        }
        status_->setText(QStringLiteral("引擎已连接，共 %1 个模型").arg(models.size()));
    });
    connect(&engine_, &EngineClient::requestFailed, this, [this](const QString &message) {
        refresh_->setEnabled(true);
        status_->setText(message);
    });
}

void MainWindow::refreshModels()
{
    config_.baseUrl = address_->text();
    config_.allowUntrustedCertificate = allowUntrusted_->isChecked();
    refresh_->setEnabled(false);
    status_->setText(QStringLiteral("正在获取模型列表…"));
    engine_.fetchModels(config_);
}

} // namespace aha
