#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QFileDialog>
#include "configmanager.h"

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(ConfigManager* configManager, QWidget *parent = nullptr);
    ~SettingsDialog();

private slots:
    void onAccepted();
    void onBrowseNodeExecutable();
    void onBrowseFarmerExecutable();
    void onBrowsePlotsPath();
    void onBrowseFarmerKey();
    void onBrowseDataDir();

private:
    void setupUi();
    void loadConfig();
    void saveConfig();

    ConfigManager* m_configManager;

    // Node settings
    QLineEdit* m_nodeExecutableEdit;
    QLineEdit* m_nodeNetworkEdit;
    QLineEdit* m_nodeRpcBindEdit;
    QLineEdit* m_nodeDataDirEdit;
    QLineEdit* m_nodeBootnodesEdit;
    QCheckBox* m_nodeAutoStartCheck;

    // Farmer settings
    QLineEdit* m_farmerExecutableEdit;
    QLineEdit* m_farmerPlotsPathEdit;
    QLineEdit* m_farmerPrivkeyPathEdit;
    QLineEdit* m_farmerNodeUrlEdit;
    QCheckBox* m_farmerAutoStartCheck;

    // RPC settings
    QLineEdit* m_rpcUrlEdit;
    QLineEdit* m_rpcFallbackUrlEdit;
    QLineEdit* m_rpcPollIntervalEdit;

    // UI settings
    QLineEdit* m_uiThemeEdit;
    QLineEdit* m_uiLanguageEdit;
    QCheckBox* m_uiMinimizeToTrayCheck;
};

#endif // SETTINGSDIALOG_H

