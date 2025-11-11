#include "farmerpage.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <QTimer>
#include <QFont>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QDialog>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <QCheckBox>

FarmerPage::FarmerPage(ArchivasNodeManager* nodeManager, ConfigManager* configManager, QWidget *parent)
    : QWidget(parent)
    , m_nodeManager(nodeManager)
    , m_configManager(configManager)
    , m_startButton(nullptr)
    , m_stopButton(nullptr)
    , m_restartButton(nullptr)
    , m_statusLabel(nullptr)
    , m_plotCountLabel(nullptr)
    , m_plotsPathEdit(nullptr)
    , m_farmerPrivkeyPathEdit(nullptr)
    , m_logTextEdit(nullptr)
    , m_statusTimer(nullptr)
{
    setupUi();

    // Connect signals
    connect(m_nodeManager, &ArchivasNodeManager::farmerStarted, this, &FarmerPage::onFarmerStarted);
    connect(m_nodeManager, &ArchivasNodeManager::farmerStopped, this, &FarmerPage::onFarmerStopped);
    connect(m_nodeManager, &ArchivasNodeManager::farmerLog, this, &FarmerPage::onFarmerLog);
    connect(m_nodeManager, &ArchivasNodeManager::statusUpdated, this, &FarmerPage::updateStatus);

    // Status update timer
    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &FarmerPage::updateStatus);
    m_statusTimer->start(5000); // Update every 5 seconds

    // Load config
    FarmerConfig config = m_configManager->getFarmerConfig();
    m_plotsPathEdit->setText(config.plotsPath);
    m_farmerPrivkeyPathEdit->setText(config.farmerPrivkeyPath);
    m_farmerPrivkeyPathEdit->setEchoMode(QLineEdit::Password);

    updateStatus();
}

FarmerPage::~FarmerPage()
{
}

void FarmerPage::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Controls Group
    QGroupBox* controlsGroup = new QGroupBox("Farmer Controls", this);
    QVBoxLayout* controlsLayout = new QVBoxLayout(controlsGroup);

    // Plots path
    QHBoxLayout* plotsPathLayout = new QHBoxLayout();
    plotsPathLayout->addWidget(new QLabel("Plots Path:", controlsGroup));
    m_plotsPathEdit = new QLineEdit(controlsGroup);
    plotsPathLayout->addWidget(m_plotsPathEdit);
    QPushButton* browsePlotsButton = new QPushButton("Browse...", controlsGroup);
    connect(browsePlotsButton, &QPushButton::clicked, [this]() {
        QString path = QFileDialog::getExistingDirectory(this, "Select Plots Directory");
        if (!path.isEmpty()) {
            m_plotsPathEdit->setText(path);
            FarmerConfig config = m_configManager->getFarmerConfig();
            config.plotsPath = path;
            m_configManager->setFarmerConfig(config);
            m_configManager->saveConfig();
        }
    });
    plotsPathLayout->addWidget(browsePlotsButton);
    controlsLayout->addLayout(plotsPathLayout);

    // Farmer private key path
    QHBoxLayout* keyPathLayout = new QHBoxLayout();
    keyPathLayout->addWidget(new QLabel("Farmer Private Key Path:", controlsGroup));
    m_farmerPrivkeyPathEdit = new QLineEdit(controlsGroup);
    keyPathLayout->addWidget(m_farmerPrivkeyPathEdit);
    QPushButton* browseKeyButton = new QPushButton("Browse...", controlsGroup);
    connect(browseKeyButton, &QPushButton::clicked, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Select Farmer Private Key", "", "Key Files (*.key *)");
        if (!path.isEmpty()) {
            m_farmerPrivkeyPathEdit->setText(path);
            FarmerConfig config = m_configManager->getFarmerConfig();
            config.farmerPrivkeyPath = path;
            m_configManager->setFarmerConfig(config);
            m_configManager->saveConfig();
        }
    });
    keyPathLayout->addWidget(browseKeyButton);
    controlsLayout->addLayout(keyPathLayout);

    // Status
    QHBoxLayout* statusLayout = new QHBoxLayout();
    statusLayout->addWidget(new QLabel("Status:", controlsGroup));
    m_statusLabel = new QLabel("Stopped", controlsGroup);
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(new QLabel("Plot Count:", controlsGroup));
    m_plotCountLabel = new QLabel("0", controlsGroup);
    statusLayout->addWidget(m_plotCountLabel);
    controlsLayout->addLayout(statusLayout);

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_startButton = new QPushButton("Start Farmer", controlsGroup);
    m_stopButton = new QPushButton("Stop Farmer", controlsGroup);
    m_restartButton = new QPushButton("Restart Farmer", controlsGroup);
    m_createPlotButton = new QPushButton("Create Plot...", controlsGroup);
    connect(m_startButton, &QPushButton::clicked, this, &FarmerPage::onStartFarmer);
    connect(m_stopButton, &QPushButton::clicked, this, &FarmerPage::onStopFarmer);
    connect(m_restartButton, &QPushButton::clicked, this, &FarmerPage::onRestartFarmer);
    connect(m_createPlotButton, &QPushButton::clicked, this, &FarmerPage::onCreatePlot);
    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_stopButton);
    buttonLayout->addWidget(m_restartButton);
    buttonLayout->addWidget(m_createPlotButton);
    buttonLayout->addStretch();
    controlsLayout->addLayout(buttonLayout);

    mainLayout->addWidget(controlsGroup);

    // Logs Group
    QGroupBox* logsGroup = new QGroupBox("Farmer Logs", this);
    QVBoxLayout* logsLayout = new QVBoxLayout(logsGroup);
    m_logTextEdit = new QPlainTextEdit(logsGroup);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setFont(QFont("Monospace", 9));
    logsLayout->addWidget(m_logTextEdit);
    mainLayout->addWidget(logsGroup);

    updateControls();
}

void FarmerPage::onStartFarmer()
{
    FarmerConfig config = m_configManager->getFarmerConfig();
    config.plotsPath = m_plotsPathEdit->text();
    config.farmerPrivkeyPath = m_farmerPrivkeyPathEdit->text();
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    m_logTextEdit->appendPlainText(QString("[%1] Starting farmer...").arg(timestamp));
    m_logTextEdit->appendPlainText(QString("[%1] Plots: %2, Node URL: %3").arg(timestamp, config.plotsPath, config.nodeUrl));
    
    bool success = m_nodeManager->startFarmer(config.nodeUrl, config.plotsPath, config.farmerPrivkeyPath);
    if (!success) {
        QString error = "ERROR: Failed to start farmer. Check the logs above for details.";
        m_logTextEdit->appendPlainText(error);
    } else {
        m_logTextEdit->appendPlainText(QString("[%1] Farmer start command sent successfully. Waiting for farmer to start...").arg(timestamp));
    }
    
    // Save config
    m_configManager->setFarmerConfig(config);
    m_configManager->saveConfig();
}

void FarmerPage::onStopFarmer()
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    m_logTextEdit->appendPlainText(QString("[%1] Stopping farmer...").arg(timestamp));
    m_nodeManager->stopFarmer();
}

void FarmerPage::onRestartFarmer()
{
    m_nodeManager->stopFarmer();
    QTimer::singleShot(1000, this, &FarmerPage::onStartFarmer);
}

void FarmerPage::onFarmerStarted()
{
    updateStatus();
    updateControls();
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    m_logTextEdit->appendPlainText(QString("[%1] Farmer started successfully").arg(timestamp));
}

void FarmerPage::onFarmerStopped()
{
    updateStatus();
    updateControls();
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    m_logTextEdit->appendPlainText(QString("[%1] Farmer stopped").arg(timestamp));
}

void FarmerPage::onFarmerLog(const QString &level, const QString &message)
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

void FarmerPage::updateStatus()
{
    bool running = m_nodeManager->isFarmerRunning();
    if (running) {
        m_statusLabel->setText("Running");
        m_statusLabel->setStyleSheet("color: green; font-weight: bold;");
    } else {
        m_statusLabel->setText("Stopped");
        m_statusLabel->setStyleSheet("color: red; font-weight: bold;");
    }
    
    // Update plot count
    int plotCount = m_nodeManager->getPlotCount();
    m_plotCountLabel->setText(QString::number(plotCount));
}

void FarmerPage::onCreatePlot()
{
    // Get farmer config for default paths
    FarmerConfig config = m_configManager->getFarmerConfig();
    QString plotsPath = m_plotsPathEdit->text().isEmpty() ? config.plotsPath : m_plotsPathEdit->text();
    QString farmerPrivkeyPath = m_farmerPrivkeyPathEdit->text().isEmpty() ? config.farmerPrivkeyPath : m_farmerPrivkeyPathEdit->text();
    
    // Create dialog for plot creation
    QDialog dialog(this);
    dialog.setWindowTitle("Create New Plot");
    dialog.setMinimumWidth(400);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    // Plot path
    QHBoxLayout* pathLayout = new QHBoxLayout();
    pathLayout->addWidget(new QLabel("Plot File Path:", &dialog));
    QLineEdit* plotPathEdit = new QLineEdit(&dialog);
    // Default plot name based on timestamp
    QString defaultPlotName = QString("plot_%1.arcv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    plotPathEdit->setText(QDir(plotsPath).filePath(defaultPlotName));
    pathLayout->addWidget(plotPathEdit);
    QPushButton* browsePlotButton = new QPushButton("Browse...", &dialog);
    connect(browsePlotButton, &QPushButton::clicked, [&plotPathEdit, plotsPath]() {
        QString path = QFileDialog::getSaveFileName(nullptr, "Save Plot As", plotsPath, "Plot Files (*.arcv)");
        if (!path.isEmpty()) {
            if (!path.endsWith(".arcv")) {
                path += ".arcv";
            }
            plotPathEdit->setText(path);
        }
    });
    pathLayout->addWidget(browsePlotButton);
    layout->addLayout(pathLayout);
    
    // K Size
    QHBoxLayout* kSizeLayout = new QHBoxLayout();
    kSizeLayout->addWidget(new QLabel("K Size:", &dialog));
    QSpinBox* kSizeSpinBox = new QSpinBox(&dialog);
    kSizeSpinBox->setMinimum(20);
    kSizeSpinBox->setMaximum(32);
    kSizeSpinBox->setValue(25); // Default k=25 (smaller for testing, mainnet uses k=28)
    kSizeLayout->addWidget(kSizeSpinBox);
    kSizeLayout->addWidget(new QLabel("(k=25 recommended for testing, k=28 for mainnet)", &dialog));
    kSizeLayout->addStretch();
    layout->addLayout(kSizeLayout);
    
    // Info label
    QLabel* infoLabel = new QLabel(&dialog);
    infoLabel->setWordWrap(true);
    infoLabel->setText("Plot creation may take several minutes depending on k-size. "
                       "Larger k-sizes create larger plots but are more efficient for farming.");
    infoLabel->setStyleSheet("color: #666; padding: 10px;");
    layout->addWidget(infoLabel);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* createButton = new QPushButton("Create Plot", &dialog);
    QPushButton* cancelButton = new QPushButton("Cancel", &dialog);
    connect(createButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    buttonLayout->addStretch();
    buttonLayout->addWidget(createButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);
    
    if (dialog.exec() == QDialog::Accepted) {
        QString plotPath = plotPathEdit->text();
        unsigned int kSize = static_cast<unsigned int>(kSizeSpinBox->value());
        
        if (plotPath.isEmpty()) {
            QMessageBox::warning(this, "Invalid Input", "Please specify a plot file path.");
            return;
        }
        
        if (farmerPrivkeyPath.isEmpty()) {
            QMessageBox::warning(this, "Invalid Input", "Please specify a farmer private key path.");
            return;
        }
        
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        m_logTextEdit->appendPlainText(QString("[%1] Creating plot: %2 (kSize=%3)").arg(timestamp, plotPath, QString::number(kSize)));
        m_logTextEdit->appendPlainText(QString("[%1] This may take several minutes...").arg(timestamp));
        
        // Create plot in a separate thread to avoid blocking UI
        // For now, we'll do it synchronously but show a message
        QMessageBox::information(this, "Plot Creation", 
            QString("Creating plot with kSize=%1.\n\n"
                   "This may take several minutes. Progress will be shown in the logs below.")
            .arg(kSize));
        
        bool success = m_nodeManager->createPlot(plotPath, kSize, farmerPrivkeyPath);
        
        if (success) {
            m_logTextEdit->appendPlainText(QString("[%1] Plot created successfully: %2").arg(timestamp, plotPath));
            QMessageBox::information(this, "Success", QString("Plot created successfully:\n%1").arg(plotPath));
            
            // Update plot count
            updateStatus();
        } else {
            m_logTextEdit->appendPlainText(QString("[%1] ERROR: Failed to create plot. Check logs above for details.").arg(timestamp));
            QMessageBox::critical(this, "Error", "Failed to create plot. Check the logs for details.");
        }
    }
}

void FarmerPage::updateControls()
{
    bool running = m_nodeManager->isFarmerRunning();
    m_startButton->setEnabled(!running);
    m_stopButton->setEnabled(running);
    m_restartButton->setEnabled(running);
    m_plotsPathEdit->setEnabled(!running);
    m_farmerPrivkeyPathEdit->setEnabled(!running);
    // Plot creation can be done anytime
    m_createPlotButton->setEnabled(true);
}
