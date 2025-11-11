#include "overviewpage.h"
#include "configmanager.h"
#include <QGroupBox>
#include <QFrame>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>

OverviewPage::OverviewPage(ArchivasRpcClient* rpcClient, ArchivasNodeManager* nodeManager, QWidget *parent)
    : QWidget(parent)
    , m_rpcClient(rpcClient)
    , m_nodeManager(nodeManager)
    , m_chainHeightLabel(nullptr)
    , m_chainHashLabel(nullptr)
    , m_difficultyLabel(nullptr)
    , m_timestampLabel(nullptr)
    , m_nodeStatusLabel(nullptr)
    , m_farmerStatusLabel(nullptr)
    , m_networkLabel(nullptr)
    , m_rpcStatusLabel(nullptr)
    , m_updateTimer(nullptr)
    , m_nodeRunning(false)
    , m_farmerRunning(false)
    , m_rpcConnected(false)
{
    setupUi();

    // Connect signals
    connect(m_rpcClient, &ArchivasRpcClient::chainTipUpdated, this, &OverviewPage::onChainTipUpdated);
    connect(m_rpcClient, &ArchivasRpcClient::connected, this, &OverviewPage::onRpcConnected);
    connect(m_rpcClient, &ArchivasRpcClient::disconnected, this, &OverviewPage::onRpcDisconnected);
    connect(m_nodeManager, &ArchivasNodeManager::nodeStarted, this, &OverviewPage::onNodeStatusChanged);
    connect(m_nodeManager, &ArchivasNodeManager::nodeStopped, this, &OverviewPage::onNodeStatusChanged);
    connect(m_nodeManager, &ArchivasNodeManager::farmerStarted, this, &OverviewPage::onFarmerStatusChanged);
    connect(m_nodeManager, &ArchivasNodeManager::farmerStopped, this, &OverviewPage::onFarmerStatusChanged);
    connect(m_nodeManager, &ArchivasNodeManager::statusUpdated, this, &OverviewPage::updateDisplay);

    // Update timer
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &OverviewPage::updateDisplay);
    m_updateTimer->start(5000); // Update every 5 seconds

    // Initial update
    updateDisplay();
    updateStatusIndicators();
}

OverviewPage::~OverviewPage()
{
}

void OverviewPage::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Chain Status Group
    QGroupBox* chainGroup = new QGroupBox("Chain Status", this);
    QGridLayout* chainLayout = new QGridLayout(chainGroup);

    chainLayout->addWidget(new QLabel("Height:", chainGroup), 0, 0);
    m_chainHeightLabel = new QLabel("—", chainGroup);
    chainLayout->addWidget(m_chainHeightLabel, 0, 1);

    chainLayout->addWidget(new QLabel("Hash:", chainGroup), 1, 0);
    m_chainHashLabel = new QLabel("—", chainGroup);
    m_chainHashLabel->setWordWrap(true);
    chainLayout->addWidget(m_chainHashLabel, 1, 1);

    chainLayout->addWidget(new QLabel("Difficulty:", chainGroup), 2, 0);
    m_difficultyLabel = new QLabel("—", chainGroup);
    chainLayout->addWidget(m_difficultyLabel, 2, 1);

    chainLayout->addWidget(new QLabel("Timestamp:", chainGroup), 3, 0);
    m_timestampLabel = new QLabel("—", chainGroup);
    chainLayout->addWidget(m_timestampLabel, 3, 1);

    mainLayout->addWidget(chainGroup);

    // Service Status Group
    QGroupBox* serviceGroup = new QGroupBox("Service Status", this);
    QGridLayout* serviceLayout = new QGridLayout(serviceGroup);

    serviceLayout->addWidget(new QLabel("Node:", serviceGroup), 0, 0);
    m_nodeStatusLabel = new QLabel("Stopped", serviceGroup);
    serviceLayout->addWidget(m_nodeStatusLabel, 0, 1);

    serviceLayout->addWidget(new QLabel("Farmer:", serviceGroup), 1, 0);
    m_farmerStatusLabel = new QLabel("Stopped", serviceGroup);
    serviceLayout->addWidget(m_farmerStatusLabel, 1, 1);

    serviceLayout->addWidget(new QLabel("Network:", serviceGroup), 2, 0);
    ConfigManager* config = ConfigManager::instance();
    NodeConfig nodeConfig = config->getNodeConfig();
    m_networkLabel = new QLabel(nodeConfig.network, serviceGroup);
    serviceLayout->addWidget(m_networkLabel, 2, 1);

    serviceLayout->addWidget(new QLabel("RPC Connection:", serviceGroup), 3, 0);
    m_rpcStatusLabel = new QLabel("Disconnected", serviceGroup);
    serviceLayout->addWidget(m_rpcStatusLabel, 3, 1);

    mainLayout->addWidget(serviceGroup);

    mainLayout->addStretch();
}

void OverviewPage::onChainTipUpdated(const ChainTip& tip)
{
    m_currentTip = tip;
    updateDisplay();
}

void OverviewPage::onNodeStatusChanged()
{
    m_nodeRunning = m_nodeManager->isNodeRunning();
    updateStatusIndicators();
}

void OverviewPage::onFarmerStatusChanged()
{
    m_farmerRunning = m_nodeManager->isFarmerRunning();
    updateStatusIndicators();
}

void OverviewPage::onRpcConnected()
{
    m_rpcConnected = true;
    updateStatusIndicators();
}

void OverviewPage::onRpcDisconnected()
{
    m_rpcConnected = false;
    updateStatusIndicators();
}

void OverviewPage::updateDisplay()
{
    // Update chain info from node manager (primary source)
    int height = m_nodeManager->getCurrentHeight();
    if (height > 0) {
        m_chainHeightLabel->setText(QString::number(height));
    } else if (!m_currentTip.height.isEmpty()) {
        // Fallback to RPC data if node manager doesn't have data yet
        m_chainHeightLabel->setText(m_currentTip.height);
    } else {
        m_chainHeightLabel->setText("—");
    }

    QString tipHash = m_nodeManager->getTipHash();
    if (!tipHash.isEmpty() && tipHash != "0000000000000000000000000000000000000000000000000000000000000000") {
        QString shortHash = tipHash;
        if (shortHash.length() > 16) {
            shortHash = shortHash.left(8) + "..." + shortHash.right(8);
        }
        m_chainHashLabel->setText(shortHash);
        m_chainHashLabel->setToolTip(tipHash); // Show full hash on hover
    } else if (!m_currentTip.hash.isEmpty()) {
        // Fallback to RPC data
        QString shortHash = m_currentTip.hash;
        if (shortHash.length() > 16) {
            shortHash = shortHash.left(8) + "..." + shortHash.right(8);
        }
        m_chainHashLabel->setText(shortHash);
        m_chainHashLabel->setToolTip(m_currentTip.hash);
    } else {
        m_chainHashLabel->setText("—");
        m_chainHashLabel->setToolTip("");
    }

    // Difficulty from RPC (node manager doesn't provide this yet)
    if (!m_currentTip.difficulty.isEmpty()) {
        m_difficultyLabel->setText(m_currentTip.difficulty);
    } else {
        m_difficultyLabel->setText("—");
    }

    if (!m_currentTip.timestamp.isEmpty()) {
        m_timestampLabel->setText(m_currentTip.timestamp);
    } else {
        m_timestampLabel->setText("—");
    }

    // Update service status from node manager
    m_nodeRunning = m_nodeManager->isNodeRunning();
    m_farmerRunning = m_nodeManager->isFarmerRunning();
    updateStatusIndicators();
}

void OverviewPage::updateStatusIndicators()
{
    // Node status
    if (m_nodeRunning) {
        m_nodeStatusLabel->setText("Running");
        m_nodeStatusLabel->setStyleSheet("color: green;");
    } else {
        m_nodeStatusLabel->setText("Stopped");
        m_nodeStatusLabel->setStyleSheet("color: red;");
    }

    // Farmer status
    if (m_farmerRunning) {
        m_farmerStatusLabel->setText("Running");
        m_farmerStatusLabel->setStyleSheet("color: green;");
    } else {
        m_farmerStatusLabel->setText("Stopped");
        m_farmerStatusLabel->setStyleSheet("color: red;");
    }

    // RPC status
    if (m_rpcConnected) {
        m_rpcStatusLabel->setText("Connected");
        m_rpcStatusLabel->setStyleSheet("color: green;");
    } else {
        m_rpcStatusLabel->setText("Disconnected");
        m_rpcStatusLabel->setStyleSheet("color: red;");
    }
}

