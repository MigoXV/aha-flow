#include "ui/system_tray.h"
#include <QAction>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QWidget>
#include <utility>

namespace aha
{
SystemTray::SystemTray(const QIcon &icon, QWidget *window, Availability availability)
    : QObject(window), availability_(availability ? std::move(availability) : QSystemTrayIcon::isSystemTrayAvailable)
{
    setObjectName(QStringLiteral("systemTrayController"));
    icon_ = new QSystemTrayIcon(icon, this);
    icon_->setObjectName(QStringLiteral("systemTrayIcon"));
    menu_ = new QMenu(window);
    menu_->setObjectName(QStringLiteral("systemTrayMenu"));
    windowAction_ = menu_->addAction(QStringLiteral("隐藏窗口"));
    windowAction_->setObjectName(QStringLiteral("trayWindow"));
    recordAction_ = menu_->addAction(QStringLiteral("开始识别"));
    recordAction_->setObjectName(QStringLiteral("trayRecording"));
    settingsAction_ = menu_->addAction(QStringLiteral("设置"));
    settingsAction_->setObjectName(QStringLiteral("traySettings"));
    menu_->addSeparator();
    exitAction_ = menu_->addAction(QStringLiteral("退出 aha-flow"));
    exitAction_->setObjectName(QStringLiteral("trayExit"));
    connect(windowAction_, &QAction::triggered, this, &SystemTray::windowToggleRequested);
    connect(recordAction_, &QAction::triggered, this, &SystemTray::recordingToggleRequested);
    connect(settingsAction_, &QAction::triggered, this, &SystemTray::settingsRequested);
    connect(exitAction_, &QAction::triggered, this, &SystemTray::exitRequested);
    connect(icon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            emit windowRestoreRequested();
    });
    icon_->setContextMenu(menu_);
    icon_->setToolTip(QStringLiteral("aha-flow · 待机"));
    // Register even if the host starts later. Qt restores the icon when a host appears.
    if (!icon.isNull())
        icon_->show();
    availabilityTimer_.setInterval(2000);
    connect(&availabilityTimer_, &QTimer::timeout, this, &SystemTray::refreshAvailability);
    availabilityTimer_.start();
    refreshAvailability();
}
bool SystemTray::available() const
{
    return icon_->isVisible() && !icon_->icon().isNull() && availability_();
}
void SystemTray::refreshAvailability()
{
    const bool now = available();
    if (now == available_)
        return;
    available_ = now;
    emit availabilityChanged(now);
}
void SystemTray::updateState(const QString &status, bool active, bool busy, bool closing)
{
    icon_->setToolTip(QStringLiteral("aha-flow · %1").arg(status));
    recordAction_->setText(active ? QStringLiteral("停止识别") : QStringLiteral("开始识别"));
    recordAction_->setEnabled(!busy && !closing);
    windowAction_->setEnabled(!closing);
    settingsAction_->setEnabled(!closing);
    exitAction_->setEnabled(!closing);
}
void SystemTray::updateWindowVisibility(bool visible)
{
    windowAction_->setText(visible ? QStringLiteral("隐藏窗口") : QStringLiteral("显示窗口"));
}
void SystemTray::hide()
{
    availabilityTimer_.stop();
    icon_->hide();
}
} // namespace aha
