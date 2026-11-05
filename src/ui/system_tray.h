#pragma once

#include <QIcon>
#include <QObject>
#include <QTimer>
#include <functional>

class QAction;
class QMenu;
class QSystemTrayIcon;
class QWidget;

namespace aha
{
// Owns desktop integration only; recording and window lifetime stay with MainWindow.
class SystemTray final : public QObject
{
    Q_OBJECT
  public:
    using Availability = std::function<bool()>;
    explicit SystemTray(const QIcon &icon, QWidget *window, Availability availability = {});
    bool available() const;
    void refreshAvailability();
    void updateState(const QString &status, bool active, bool busy, bool closing);
    void updateWindowVisibility(bool visible);
    void hide();

  signals:
    void windowToggleRequested();
    void windowRestoreRequested();
    void recordingToggleRequested();
    void settingsRequested();
    void exitRequested();
    void availabilityChanged(bool available);

  private:
    QSystemTrayIcon *icon_;
    QMenu *menu_;
    QAction *windowAction_, *recordAction_, *settingsAction_, *exitAction_;
    Availability availability_;
    QTimer availabilityTimer_;
    bool available_ = false;
};
} // namespace aha
