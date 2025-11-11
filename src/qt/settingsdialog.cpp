#include "settingsdialog.h"
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QFileDialog>

SettingsDialog::SettingsDialog(ConfigManager* configManager, QWidget *parent)
    : QDialog(parent)
    , m_configManager(configManager)
{
    setWindowTitle("Settings");
    setModal(true);
    resize(600, 500);

    setupUi();
    loadConfig();
}

SettingsDialog::~SettingsDialog()
{
}

void SettingsDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QTabWidget* tabWidget = new QTabWidget(this);

    // Node tab
    QWidget* nodeTab = new QWidget();
    QFormLayout* nodeLayout = new QFormLayout(nodeTab);
    nodeLayout->setSpacing(10);

    QHBoxLayout* nodeExecLayout = new QHBoxLayout();
    m_nodeExecutableEdit = new QLineEdit(nodeTab);
    QPushButton* nodeExecBrowse = new QPushButton("Browse...", nodeTab);
    connect(nodeExecBrowse, &QPushButton::clicked, this, &SettingsDialog::onBrowseNodeExecutable);
    nodeExecLayout->addWidget(m_nodeExecutableEdit);
    nodeExecLayout->addWidget(nodeExecBrowse);
    nodeLayout->addRow("Executable Path:", nodeExecLayout);

    m_nodeNetworkEdit = new QLineEdit(nodeTab);
    nodeLayout->addRow("Network:", m_nodeNetworkEdit);

    m_nodeRpcBindEdit = new QLineEdit(nodeTab);
    nodeLayout->addRow("RPC Bind:", m_nodeRpcBindEdit);

    QHBoxLayout* nodeDataDirLayout = new QHBoxLayout();
    m_nodeDataDirEdit = new QLineEdit(nodeTab);
    QPushButton* nodeDataDirBrowse = new QPushButton("Browse...", nodeTab);
    connect(nodeDataDirBrowse, &QPushButton::clicked, this, &SettingsDialog::onBrowseDataDir);
    nodeDataDirLayout->addWidget(m_nodeDataDirEdit);
    nodeDataDirLayout->addWidget(nodeDataDirBrowse);
    nodeLayout->addRow("Data Directory:", nodeDataDirLayout);

    m_nodeBootnodesEdit = new QLineEdit(nodeTab);
    nodeLayout->addRow("Bootnodes:", m_nodeBootnodesEdit);

    m_nodeAutoStartCheck = new QCheckBox(nodeTab);
    nodeLayout->addRow("Auto-start:", m_nodeAutoStartCheck);

    tabWidget->addTab(nodeTab, "Node");

    // Farmer tab
    QWidget* farmerTab = new QWidget();
    QFormLayout* farmerLayout = new QFormLayout(farmerTab);
    farmerLayout->setSpacing(10);

    QHBoxLayout* farmerExecLayout = new QHBoxLayout();
    m_farmerExecutableEdit = new QLineEdit(farmerTab);
    QPushButton* farmerExecBrowse = new QPushButton("Browse...", farmerTab);
    connect(farmerExecBrowse, &QPushButton::clicked, this, &SettingsDialog::onBrowseFarmerExecutable);
    farmerExecLayout->addWidget(m_farmerExecutableEdit);
    farmerExecLayout->addWidget(farmerExecBrowse);
    farmerLayout->addRow("Executable Path:", farmerExecLayout);

    QHBoxLayout* farmerPlotsLayout = new QHBoxLayout();
    m_farmerPlotsPathEdit = new QLineEdit(farmerTab);
    QPushButton* farmerPlotsBrowse = new QPushButton("Browse...", farmerTab);
    connect(farmerPlotsBrowse, &QPushButton::clicked, this, &SettingsDialog::onBrowsePlotsPath);
    farmerPlotsLayout->addWidget(m_farmerPlotsPathEdit);
    farmerPlotsLayout->addWidget(farmerPlotsBrowse);
    farmerLayout->addRow("Plots Path:", farmerPlotsLayout);

    QHBoxLayout* farmerKeyLayout = new QHBoxLayout();
    m_farmerPrivkeyPathEdit = new QLineEdit(farmerTab);
    m_farmerPrivkeyPathEdit->setEchoMode(QLineEdit::Password);
    QPushButton* farmerKeyBrowse = new QPushButton("Browse...", farmerTab);
    connect(farmerKeyBrowse, &QPushButton::clicked, this, &SettingsDialog::onBrowseFarmerKey);
    farmerKeyLayout->addWidget(m_farmerPrivkeyPathEdit);
    farmerKeyLayout->addWidget(farmerKeyBrowse);
    farmerLayout->addRow("Farmer Private Key Path:", farmerKeyLayout);

    m_farmerNodeUrlEdit = new QLineEdit(farmerTab);
    farmerLayout->addRow("Node URL:", m_farmerNodeUrlEdit);

    m_farmerAutoStartCheck = new QCheckBox(farmerTab);
    farmerLayout->addRow("Auto-start:", m_farmerAutoStartCheck);

    tabWidget->addTab(farmerTab, "Farmer");

    // RPC tab
    QWidget* rpcTab = new QWidget();
    QFormLayout* rpcLayout = new QFormLayout(rpcTab);
    rpcLayout->setSpacing(10);

    m_rpcUrlEdit = new QLineEdit(rpcTab);
    rpcLayout->addRow("RPC URL:", m_rpcUrlEdit);

    m_rpcFallbackUrlEdit = new QLineEdit(rpcTab);
    rpcLayout->addRow("Fallback URL:", m_rpcFallbackUrlEdit);

    m_rpcPollIntervalEdit = new QLineEdit(rpcTab);
    rpcLayout->addRow("Poll Interval (ms):", m_rpcPollIntervalEdit);

    tabWidget->addTab(rpcTab, "RPC");

    // UI tab
    QWidget* uiTab = new QWidget();
    QFormLayout* uiLayout = new QFormLayout(uiTab);
    uiLayout->setSpacing(10);

    m_uiThemeEdit = new QLineEdit(uiTab);
    uiLayout->addRow("Theme:", m_uiThemeEdit);

    m_uiLanguageEdit = new QLineEdit(uiTab);
    uiLayout->addRow("Language:", m_uiLanguageEdit);

    m_uiMinimizeToTrayCheck = new QCheckBox(uiTab);
    uiLayout->addRow("Minimize to Tray:", m_uiMinimizeToTrayCheck);

    tabWidget->addTab(uiTab, "UI");

    mainLayout->addWidget(tabWidget);

    // Buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void SettingsDialog::loadConfig()
{
    NodeConfig nodeConfig = m_configManager->getNodeConfig();
    m_nodeExecutableEdit->setText(nodeConfig.executablePath);
    m_nodeNetworkEdit->setText(nodeConfig.network);
    m_nodeRpcBindEdit->setText(nodeConfig.rpcBind);
    m_nodeDataDirEdit->setText(nodeConfig.dataDir);
    m_nodeBootnodesEdit->setText(nodeConfig.bootnodes);
    m_nodeAutoStartCheck->setChecked(nodeConfig.autoStart);

    FarmerConfig farmerConfig = m_configManager->getFarmerConfig();
    m_farmerExecutableEdit->setText(farmerConfig.executablePath);
    m_farmerPlotsPathEdit->setText(farmerConfig.plotsPath);
    m_farmerPrivkeyPathEdit->setText(farmerConfig.farmerPrivkeyPath);
    m_farmerNodeUrlEdit->setText(farmerConfig.nodeUrl);
    m_farmerAutoStartCheck->setChecked(farmerConfig.autoStart);

    RpcConfig rpcConfig = m_configManager->getRpcConfig();
    m_rpcUrlEdit->setText(rpcConfig.url);
    m_rpcFallbackUrlEdit->setText(rpcConfig.fallbackUrl);
    m_rpcPollIntervalEdit->setText(QString::number(rpcConfig.pollIntervalMs));

    UiConfig uiConfig = m_configManager->getUiConfig();
    m_uiThemeEdit->setText(uiConfig.theme);
    m_uiLanguageEdit->setText(uiConfig.language);
    m_uiMinimizeToTrayCheck->setChecked(uiConfig.minimizeToTray);
}

void SettingsDialog::saveConfig()
{
    NodeConfig nodeConfig;
    nodeConfig.executablePath = m_nodeExecutableEdit->text();
    nodeConfig.network = m_nodeNetworkEdit->text();
    nodeConfig.rpcBind = m_nodeRpcBindEdit->text();
    nodeConfig.dataDir = m_nodeDataDirEdit->text();
    nodeConfig.bootnodes = m_nodeBootnodesEdit->text();
    nodeConfig.autoStart = m_nodeAutoStartCheck->isChecked();
    m_configManager->setNodeConfig(nodeConfig);

    FarmerConfig farmerConfig;
    farmerConfig.executablePath = m_farmerExecutableEdit->text();
    farmerConfig.plotsPath = m_farmerPlotsPathEdit->text();
    farmerConfig.farmerPrivkeyPath = m_farmerPrivkeyPathEdit->text();
    farmerConfig.nodeUrl = m_farmerNodeUrlEdit->text();
    farmerConfig.autoStart = m_farmerAutoStartCheck->isChecked();
    m_configManager->setFarmerConfig(farmerConfig);

    RpcConfig rpcConfig;
    rpcConfig.url = m_rpcUrlEdit->text();
    rpcConfig.fallbackUrl = m_rpcFallbackUrlEdit->text();
    rpcConfig.pollIntervalMs = m_rpcPollIntervalEdit->text().toInt();
    m_configManager->setRpcConfig(rpcConfig);

    UiConfig uiConfig;
    uiConfig.theme = m_uiThemeEdit->text();
    uiConfig.language = m_uiLanguageEdit->text();
    uiConfig.minimizeToTray = m_uiMinimizeToTrayCheck->isChecked();
    m_configManager->setUiConfig(uiConfig);

    if (!m_configManager->saveConfig()) {
        QMessageBox::critical(this, "Error", "Failed to save configuration.");
    }
}

void SettingsDialog::onAccepted()
{
    saveConfig();
    accept();
}

void SettingsDialog::onBrowseNodeExecutable()
{
    QString path = QFileDialog::getOpenFileName(this, "Select Node Executable", "", "Executable (*)");
    if (!path.isEmpty()) {
        m_nodeExecutableEdit->setText(path);
    }
}

void SettingsDialog::onBrowseFarmerExecutable()
{
    QString path = QFileDialog::getOpenFileName(this, "Select Farmer Executable", "", "Executable (*)");
    if (!path.isEmpty()) {
        m_farmerExecutableEdit->setText(path);
    }
}

void SettingsDialog::onBrowsePlotsPath()
{
    QString path = QFileDialog::getExistingDirectory(this, "Select Plots Directory");
    if (!path.isEmpty()) {
        m_farmerPlotsPathEdit->setText(path);
    }
}

void SettingsDialog::onBrowseFarmerKey()
{
    QString path = QFileDialog::getOpenFileName(this, "Select Farmer Private Key", "", "Key Files (*.key *)");
    if (!path.isEmpty()) {
        m_farmerPrivkeyPathEdit->setText(path);
    }
}

void SettingsDialog::onBrowseDataDir()
{
    QString path = QFileDialog::getExistingDirectory(this, "Select Data Directory");
    if (!path.isEmpty()) {
        m_nodeDataDirEdit->setText(path);
    }
}

