#include "mainwindow.h"
#include "settingsdialog.h"
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QMessageBox>
#include <QApplication>
#include <QIcon>
#include <QKeySequence>
#include <QTimer>
#include <QListWidget>
#include <QStackedWidget>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWindow>
#include <QDebug>
#include <QFile>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_splitter(nullptr)
    , m_sidebar(nullptr)
    , m_stackedWidget(nullptr)
    , m_overviewPage(nullptr)
    , m_nodePage(nullptr)
    , m_farmerPage(nullptr)
    , m_blocksPage(nullptr)
    , m_transactionsPage(nullptr)
    , m_logsPage(nullptr)
    , m_nodeManager(nullptr)
    , m_rpcClient(nullptr)
    , m_configManager(nullptr)
    , m_pollTimer(nullptr)
    , m_nodeRunning(false)
    , m_farmerRunning(false)
    , m_rpcConnected(false)
{
    // Initialize config manager
    m_configManager = ConfigManager::instance();
    bool isFirstRun = !QFile(m_configManager->getConfigPath()).exists();
    m_configManager->loadConfig();
    
    // Show welcome message on first run (delayed to avoid blocking initialization)
    if (isFirstRun) {
        QTimer::singleShot(2000, this, [this]() {
            QMessageBox::information(this, "Welcome to Archivas Core", 
                "Welcome to Archivas Core!\n\n"
                "The node and farmer are starting automatically with default settings.\n"
                "You can configure them in Settings if needed.\n\n"
                "The application is ready to use!");
        });
    }

    // Initialize node manager (cgo bridge to Go code)
    m_nodeManager = new ArchivasNodeManager(this);
    connect(m_nodeManager, &ArchivasNodeManager::nodeStarted, this, &MainWindow::onNodeStatusChanged);
    connect(m_nodeManager, &ArchivasNodeManager::nodeStopped, this, &MainWindow::onNodeStatusChanged);
    connect(m_nodeManager, &ArchivasNodeManager::farmerStarted, this, &MainWindow::onFarmerStatusChanged);
    connect(m_nodeManager, &ArchivasNodeManager::farmerStopped, this, &MainWindow::onFarmerStatusChanged);
    // Log connections will be set up after logs page is created

    // Initialize RPC client
    RpcConfig rpcConfig = m_configManager->getRpcConfig();
    m_rpcClient = new ArchivasRpcClient(this);
    m_rpcClient->setBaseUrl(rpcConfig.url);
    m_rpcClient->setFallbackUrl(rpcConfig.fallbackUrl);
    connect(m_rpcClient, &ArchivasRpcClient::chainTipUpdated, this, &MainWindow::onChainTipUpdated);
    connect(m_rpcClient, &ArchivasRpcClient::connected, this, &MainWindow::onRpcConnected);
    connect(m_rpcClient, &ArchivasRpcClient::disconnected, this, &MainWindow::onRpcDisconnected);

    setupUi();
    setupMenuBar();
    setupStatusBar();
    setupPages();  // Must create pages BEFORE sidebar to avoid crash
    setupSidebar();

    // Delay polling start until after window is shown
    // Use QTimer::singleShot to start after event loop is running
    QTimer::singleShot(100, this, &MainWindow::startPolling);

    // Auto-start node and farmer on launch (out-of-the-box experience)
    // Delay auto-start slightly to ensure UI is ready
    QTimer::singleShot(500, this, [this]() {
        NodeConfig nodeConfig = m_configManager->getNodeConfig();
        FarmerConfig farmerConfig = m_configManager->getFarmerConfig();
        
        // Always auto-start node with default config (unless explicitly disabled)
        if (nodeConfig.autoStart) {
            qDebug() << "Auto-starting Archivas node...";
            // Extract genesis file from Qt resources to a temporary file
            QString genesisPath = extractGenesisFile();
            m_nodeManager->startNode(nodeConfig.network, nodeConfig.rpcBind, 
                                    nodeConfig.dataDir, nodeConfig.bootnodes, genesisPath);
        }
        
        // Auto-start farmer after a short delay (node should be ready first)
        if (farmerConfig.autoStart) {
            QTimer::singleShot(1000, this, [this, farmerConfig]() {
                qDebug() << "Auto-starting Archivas farmer...";
                m_nodeManager->startFarmer(farmerConfig.nodeUrl, farmerConfig.plotsPath,
                                          farmerConfig.farmerPrivkeyPath);
            });
        }
    });
}

QString MainWindow::extractGenesisFile()
{
    // Try to load from Qt resources first
    QFile resourceFile(":/genesis/devnet.genesis.json");
    if (resourceFile.exists()) {
        // Extract to a temporary file in the data directory
        QString dataDir = m_configManager->getNodeConfig().dataDir;
        QDir dir;
        dir.mkpath(dataDir);
        
        QString tempPath = dataDir + "/genesis.json";
        if (resourceFile.open(QIODevice::ReadOnly)) {
            QFile tempFile(tempPath);
            if (tempFile.open(QIODevice::WriteOnly)) {
                tempFile.write(resourceFile.readAll());
                tempFile.close();
                resourceFile.close();
                qDebug() << "Extracted genesis file to:" << tempPath;
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
    qWarning() << "Could not extract genesis file from resources, using fallback";
    return fallbackPath;
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{
    setWindowTitle("Archivas Core");
    
    // Set window icon (should already be set in ArchivasApplication, but ensure it's applied)
    QIcon appIcon = QApplication::windowIcon();
    if (!appIcon.isNull()) {
        setWindowIcon(appIcon);
    }
    
    // Set window properties for proper taskbar integration
    // These help the window manager match the window to the desktop file
    setProperty("WM_CLASS", QByteArray("archivas-core"));
    setProperty("_NET_WM_DESKTOP_FILE", QByteArray("archivas-core.desktop"));
    
    resize(1200, 800);

    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    m_splitter = new QSplitter(Qt::Horizontal, m_centralWidget);
    QHBoxLayout* layout = new QHBoxLayout(m_centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_splitter);
}

void MainWindow::setupMenuBar()
{
    QMenu* fileMenu = menuBar()->addMenu("&File");
    m_settingsAction = fileMenu->addAction("&Settings", this, &MainWindow::showSettings);
    m_settingsAction->setShortcut(QKeySequence::Preferences);
    fileMenu->addSeparator();
    m_exitAction = fileMenu->addAction("E&xit", this, &QWidget::close);
    m_exitAction->setShortcut(QKeySequence::Quit);

    QMenu* helpMenu = menuBar()->addMenu("&Help");
    m_aboutAction = helpMenu->addAction("&About", this, &MainWindow::about);
    m_aboutAction->setShortcut(QKeySequence::HelpContents);
}

void MainWindow::setupStatusBar()
{
    updateStatusBar();
}

void MainWindow::setupSidebar()
{
    m_sidebar = new QListWidget(m_splitter);
    m_sidebar->setMaximumWidth(200);
    m_sidebar->setSpacing(2);

    m_sidebar->addItem("Overview");
    m_sidebar->addItem("Node");
    m_sidebar->addItem("Farmer");
    m_sidebar->addItem("Blocks");
    m_sidebar->addItem("Transactions");
    m_sidebar->addItem("Logs");

    connect(m_sidebar, &QListWidget::currentRowChanged, this, &MainWindow::onPageChanged);
    m_sidebar->setCurrentRow(0);

    m_splitter->addWidget(m_sidebar);
}

void MainWindow::setupPages()
{
    m_stackedWidget = new QStackedWidget(m_splitter);
    m_splitter->addWidget(m_stackedWidget);

    // Create pages
    m_overviewPage = new OverviewPage(m_rpcClient, m_nodeManager, this);
    m_nodePage = new NodePage(m_nodeManager, m_configManager, this);
    m_farmerPage = new FarmerPage(m_nodeManager, m_configManager, this);
    m_blocksPage = new BlocksPage(m_rpcClient, this);
    m_transactionsPage = new TransactionsPage(m_rpcClient, this);
    m_logsPage = new LogsPage(m_nodeManager, this);
    
    // Connect logs after logs page is created
    connect(m_nodeManager, &ArchivasNodeManager::nodeLog, m_logsPage, &LogsPage::addNodeLog);
    connect(m_nodeManager, &ArchivasNodeManager::farmerLog, m_logsPage, &LogsPage::addFarmerLog);

    // Add pages to stack
    m_stackedWidget->addWidget(m_overviewPage);
    m_stackedWidget->addWidget(m_nodePage);
    m_stackedWidget->addWidget(m_farmerPage);
    m_stackedWidget->addWidget(m_blocksPage);
    m_stackedWidget->addWidget(m_transactionsPage);
    m_stackedWidget->addWidget(m_logsPage);
}

void MainWindow::onPageChanged(int index)
{
    if (m_stackedWidget && index >= 0 && index < m_stackedWidget->count()) {
        m_stackedWidget->setCurrentIndex(index);
    }
}

void MainWindow::onNodeStatusChanged()
{
    m_nodeRunning = m_nodeManager->isNodeRunning();
    updateStatusBar();
}

void MainWindow::onFarmerStatusChanged()
{
    m_farmerRunning = m_nodeManager->isFarmerRunning();
    updateStatusBar();
}

void MainWindow::onChainTipUpdated(const ChainTip& tip)
{
    Q_UNUSED(tip);
    updateStatusBar();
}

void MainWindow::onRpcConnected()
{
    m_rpcConnected = true;
    updateStatusBar();
}

void MainWindow::onRpcDisconnected()
{
    m_rpcConnected = false;
    updateStatusBar();
}

void MainWindow::updateStatusBar()
{
    QString status;
    status += QString("Node: %1 | ").arg(m_nodeRunning ? "Running" : "Stopped");
    status += QString("Farmer: %1 | ").arg(m_farmerRunning ? "Running" : "Stopped");
    status += QString("RPC: %1").arg(m_rpcConnected ? "Connected" : "Disconnected");

    statusBar()->showMessage(status);
}

void MainWindow::startPolling()
{
    if (m_pollTimer) {
        return; // Already started
    }
    
    RpcConfig rpcConfig = m_configManager->getRpcConfig();
    m_pollTimer = new QTimer(this);
    m_pollTimer->setSingleShot(false);
    connect(m_pollTimer, &QTimer::timeout, [this]() {
        if (m_rpcClient) {
            m_rpcClient->getChainTip();
            m_rpcClient->getRecentBlocks(20);
            m_rpcClient->getRecentTransactions(50);
        }
    });
    m_pollTimer->start(rpcConfig.pollIntervalMs);

    // Initial poll after a short delay to ensure event loop is running
    QTimer::singleShot(500, [this]() {
        if (m_rpcClient) {
            m_rpcClient->getChainTip();
            m_rpcClient->getRecentBlocks(20);
            m_rpcClient->getRecentTransactions(50);
        }
    });
}

void MainWindow::showSettings()
{
    SettingsDialog dialog(m_configManager, this);
    if (dialog.exec() == QDialog::Accepted) {
        // Reload config
        m_configManager->loadConfig();
        RpcConfig rpcConfig = m_configManager->getRpcConfig();
        m_rpcClient->setBaseUrl(rpcConfig.url);
        m_rpcClient->setFallbackUrl(rpcConfig.fallbackUrl);
        if (m_pollTimer) {
            m_pollTimer->setInterval(rpcConfig.pollIntervalMs);
        }
    }
}

void MainWindow::about()
{
    QMessageBox::about(this, "About Archivas Core",
                       "Archivas Core GUI\n\n"
                       "Version 1.0.0\n\n"
                       "A desktop GUI for managing Archivas node and farmer processes.\n\n"
                       "Forked from Bitcoin Core Qt framework.");
}

