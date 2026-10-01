#pragma once

#include "network/engine_client.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace aha {

class MainWindow final : public QWidget {
public:
    explicit MainWindow(const EngineConfig &config, QWidget *parent = nullptr);
    void refreshModels();

private:
    EngineConfig config_;
    EngineClient engine_;
    QLineEdit *address_;
    QCheckBox *allowUntrusted_;
    QComboBox *models_;
    QPushButton *refresh_;
    QLabel *status_;
};

} // namespace aha
