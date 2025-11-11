#include "nodepage.h"
#include <QScrollBar>
#include <QTimer>
#include <QFont>
#include <QDateTime>

NodePage::NodePage(ArchivasNodeManager* nodeManager, ConfigManager* configManager, QWidget *parent)
    : QWidget(parent)
    , m_nodeManager(nodeManager)
    , m_configManager(configManager)
    , m_startButton(nullptr)
    , m_stopButton(nullptr)
    , m_restartButton(nullptr)
    , m_statusLabel(nullptr)
    , m_peerCountLabel(nullptr)
    , m_logTextEdit(nullptr)
    , m_statusTimer(nullptr)
{
    setupUi();

    // Connect signals
    connect(m_nodeManager, &ArchivasNodeManager::nodeStarted, this, &NodePage::onNodeStarted);
    connect(m_nodeManager, &ArchivasNodeManager::nodeStopped, this, &NodePage::onNodeStopped);
    connect(m_nodeManager, &ArchivasNodeManager::nodeLog, this, &NodePage::onNodeLog);
    connect(m_nodeManager, &ArchivasNodeManager::statusUpdated, this, &NodePage::updateStatus);

    // Status update timer
    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &NodePage::updateStatus);
    m_statusTimer->start(5000); // Update every 5 seconds

    updateStatus();
}

NodePage::~NodePage()
{
}

void NodePage::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Controls Group
    QGroupBox* controlsGroup = new QGroupBox("Node Controls", this);
    QVBoxLayout* controlsLayout = new QVBoxLayout(controlsGroup);

    // Status
    QHBoxLayout* statusLayout = new QHBoxLayout();
    statusLayout->addWidget(new QLabel("Status:", controlsGroup));
    m_statusLabel = new QLabel("Stopped", controlsGroup);
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(new QLabel("Peer Count:", controlsGroup));
    m_peerCountLabel = new QLabel("0", controlsGroup);
    statusLayout->addWidget(m_peerCountLabel);
    controlsLayout->addLayout(statusLayout);

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_startButton = new QPushButton("Start Node", controlsGroup);
    m_stopButton = new QPushButton("Stop Node", controlsGroup);
    m_restartButton = new QPushButton("Restart Node", controlsGroup);
    connect(m_startButton, &QPushButton::clicked, this, &NodePage::onStartNode);
    connect(m_stopButton, &QPushButton::clicked, this, &NodePage::onStopNode);
    connect(m_restartButton, &QPushButton::clicked, this, &NodePage::onRestartNode);
    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_stopButton);
    buttonLayout->addWidget(m_restartButton);
    buttonLayout->addStretch();
    controlsLayout->addLayout(buttonLayout);

    mainLayout->addWidget(controlsGroup);

    // Logs Group
    QGroupBox* logsGroup = new QGroupBox("Node Logs", this);
    QVBoxLayout* logsLayout = new QVBoxLayout(logsGroup);
    m_logTextEdit = new QPlainTextEdit(logsGroup);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setFont(QFont("Monospace", 9));
    logsLayout->addWidget(m_logTextEdit);
    mainLayout->addWidget(logsGroup);

    updateControls();
}

void NodePage::onStartNode()
{
    NodeConfig config = m_configManager->getNodeConfig();
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    m_logTextEdit->appendPlainText(QString("[%1] Starting Archivas node...").arg(timestamp));
    m_logTextEdit->appendPlainText(QString("[%1] Network: %2, RPC Bind: %3, Data Dir: %4")
        .arg(timestamp, config.network, config.rpcBind, config.dataDir));
    
    // Extract genesis file from Qt resources
    QString genesisPath = extractGenesisFile();
    bool success = m_nodeManager->startNode(config.network, config.rpcBind, 
                                           config.dataDir, config.bootnodes, genesisPath);
    if (!success) {
        QString error = "ERROR: Failed to start node. Check the logs above for details.";
        m_logTextEdit->appendPlainText(error);
    } else {
        m_logTextEdit->appendPlainText(QString("[%1] Node start command sent successfully. Waiting for node to start...").arg(timestamp));
    }
    
    // Save config
    m_configManager->setNodeConfig(config);
    m_configManager->saveConfig();
}

void NodePage::onStopNode()
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    m_logTextEdit->appendPlainText(QString("[%1] Stopping Archivas node...").arg(timestamp));
    m_nodeManager->stopNode();
}

void NodePage::onRestartNode()
{
    onStopNode();
    QTimer::singleShot(1000, this, &NodePage::onStartNode);
}

QString NodePage::extractGenesisFile()
{
    // Try to load from Qt resources first
    QFile resourceFile(":/genesis/devnet.genesis.json");
    if (resourceFile.exists()) {
        // Extract to a temporary file in the data directory
        NodeConfig config = m_configManager->getNodeConfig();
        QString dataDir = config.dataDir;
        QDir dir;
        dir.mkpath(dataDir);
        
        QString tempPath = dataDir + "/genesis.json";
        if (resourceFile.open(QIODevice::ReadOnly)) {
            QFile tempFile(tempPath);
            if (tempFile.open(QIODevice::WriteOnly)) {
                tempFile.write(resourceFile.readAll());
                tempFile.close();
                resourceFile.close();
                return tempPath;
            }
            resourceFile.close();
        }
    }
    
    // Fallback: try relative path
    QString fallbackPath = "genesis/devnet.genesis.json";
    if (QFile::exists(fallbackPath)) {
        return fallbackPath;
    }
    
    // Last resort: return empty (Go code will try alternatives)
    return fallbackPath;
}

void NodePage::onNodeStarted()
{
    updateStatus();
    updateControls();
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    m_logTextEdit->appendPlainText(QString("[%1] Node started successfully").arg(timestamp));
}

void NodePage::onNodeStopped()
{
    updateStatus();
    updateControls();
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    m_logTextEdit->appendPlainText(QString("[%1] Node stopped").arg(timestamp));
}

void NodePage::onNodeLog(const QString &level, const QString &message)
{
    if (message.isEmpty()) {
        return;
    }
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    m_logTextEdit->appendPlainText(QString("[%1] [%2] %3").arg(timestamp, level, message));
    // Auto-scroll to bottom
    QScrollBar* scrollBar = m_logTextEdit->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void NodePage::updateStatus()
{
    bool running = m_nodeManager->isNodeRunning();
    if (running) {
        m_statusLabel->setText("Running");
        m_statusLabel->setStyleSheet("color: green; font-weight: bold;");
    } else {
        m_statusLabel->setText("Stopped");
        m_statusLabel->setStyleSheet("color: red; font-weight: bold;");
    }
    
    // Update peer count
    int peerCount = m_nodeManager->getPeerCount();
    m_peerCountLabel->setText(QString::number(peerCount));
}

void NodePage::updateControls()
{
    bool running = m_nodeManager->isNodeRunning();
    m_startButton->setEnabled(!running);
    m_stopButton->setEnabled(running);
    m_restartButton->setEnabled(running);
}
