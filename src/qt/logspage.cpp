#include "logspage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QFont>
#include <QScrollBar>
#include <QLabel>
#include <QFile>
#include <QIODevice>

LogsPage::LogsPage(ArchivasNodeManager* nodeManager, QWidget *parent)
    : QWidget(parent)
    , m_nodeManager(nodeManager)
    , m_tabWidget(nullptr)
    , m_nodeLogs(nullptr)
    , m_farmerLogs(nullptr)
    , m_searchEdit(nullptr)
    , m_autoScrollCheck(nullptr)
    , m_autoScroll(true)
{
    setupUi();
    
    // Connect to node manager signals
    connect(m_nodeManager, &ArchivasNodeManager::nodeLog, this, &LogsPage::onNodeLog);
    connect(m_nodeManager, &ArchivasNodeManager::farmerLog, this, &LogsPage::onFarmerLog);
}

LogsPage::~LogsPage()
{
}

void LogsPage::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Search bar
    QHBoxLayout* searchLayout = new QHBoxLayout();
    searchLayout->addWidget(new QLabel("Search:", this));
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search logs...");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &LogsPage::onSearchChanged);
    searchLayout->addWidget(m_searchEdit);
    
    m_autoScrollCheck = new QCheckBox("Auto-scroll", this);
    m_autoScrollCheck->setChecked(m_autoScroll);
    connect(m_autoScrollCheck, &QCheckBox::toggled, [this](bool checked) {
        m_autoScroll = checked;
    });
    searchLayout->addWidget(m_autoScrollCheck);
    searchLayout->addStretch();
    
    mainLayout->addLayout(searchLayout);

    // Tab widget for node and farmer logs
    m_tabWidget = new QTabWidget(this);
    
    // Node logs tab
    QWidget* nodeTab = new QWidget();
    QVBoxLayout* nodeTabLayout = new QVBoxLayout(nodeTab);
    m_nodeLogs = new QPlainTextEdit(nodeTab);
    m_nodeLogs->setReadOnly(true);
    m_nodeLogs->setFont(QFont("Monospace", 9));
    nodeTabLayout->addWidget(m_nodeLogs);
    
    QHBoxLayout* nodeButtonsLayout = new QHBoxLayout();
    QPushButton* clearNodeButton = new QPushButton("Clear", nodeTab);
    QPushButton* saveNodeButton = new QPushButton("Save", nodeTab);
    connect(clearNodeButton, &QPushButton::clicked, this, &LogsPage::onClearNodeLogs);
    connect(saveNodeButton, &QPushButton::clicked, this, &LogsPage::onSaveNodeLogs);
    nodeButtonsLayout->addWidget(clearNodeButton);
    nodeButtonsLayout->addWidget(saveNodeButton);
    nodeButtonsLayout->addStretch();
    nodeTabLayout->addLayout(nodeButtonsLayout);
    
    m_tabWidget->addTab(nodeTab, "Node Logs");
    
    // Farmer logs tab
    QWidget* farmerTab = new QWidget();
    QVBoxLayout* farmerTabLayout = new QVBoxLayout(farmerTab);
    m_farmerLogs = new QPlainTextEdit(farmerTab);
    m_farmerLogs->setReadOnly(true);
    m_farmerLogs->setFont(QFont("Monospace", 9));
    farmerTabLayout->addWidget(m_farmerLogs);
    
    QHBoxLayout* farmerButtonsLayout = new QHBoxLayout();
    QPushButton* clearFarmerButton = new QPushButton("Clear", farmerTab);
    QPushButton* saveFarmerButton = new QPushButton("Save", farmerTab);
    connect(clearFarmerButton, &QPushButton::clicked, this, &LogsPage::onClearFarmerLogs);
    connect(saveFarmerButton, &QPushButton::clicked, this, &LogsPage::onSaveFarmerLogs);
    farmerButtonsLayout->addWidget(clearFarmerButton);
    farmerButtonsLayout->addWidget(saveFarmerButton);
    farmerButtonsLayout->addStretch();
    farmerTabLayout->addLayout(farmerButtonsLayout);
    
    m_tabWidget->addTab(farmerTab, "Farmer Logs");
    
    mainLayout->addWidget(m_tabWidget);
}

void LogsPage::addNodeLog(const QString &level, const QString &message)
{
    onNodeLog(level, message);
}

void LogsPage::addFarmerLog(const QString &level, const QString &message)
{
    onFarmerLog(level, message);
}

void LogsPage::onNodeLog(const QString &level, const QString &message)
{
    if (message.isEmpty()) {
        return;
    }
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString logLine = QString("[%1] [%2] %3").arg(timestamp, level, message);
    m_nodeLogs->appendPlainText(logLine);
    
    if (m_autoScroll) {
        QScrollBar* scrollBar = m_nodeLogs->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }
}

void LogsPage::onFarmerLog(const QString &level, const QString &message)
{
    if (message.isEmpty()) {
        return;
    }
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString logLine = QString("[%1] [%2] %3").arg(timestamp, level, message);
    m_farmerLogs->appendPlainText(logLine);
    
    if (m_autoScroll) {
        QScrollBar* scrollBar = m_farmerLogs->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }
}

void LogsPage::onClearNodeLogs()
{
    m_nodeLogs->clear();
}

void LogsPage::onClearFarmerLogs()
{
    m_farmerLogs->clear();
}

void LogsPage::onSaveNodeLogs()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save Node Logs", "", "Text Files (*.txt);;All Files (*)");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(m_nodeLogs->toPlainText().toUtf8());
            file.close();
            QMessageBox::information(this, "Success", "Node logs saved successfully.");
        } else {
            QMessageBox::warning(this, "Error", "Failed to save node logs.");
        }
    }
}

void LogsPage::onSaveFarmerLogs()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save Farmer Logs", "", "Text Files (*.txt);;All Files (*)");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(m_farmerLogs->toPlainText().toUtf8());
            file.close();
            QMessageBox::information(this, "Success", "Farmer logs saved successfully.");
        } else {
            QMessageBox::warning(this, "Error", "Failed to save farmer logs.");
        }
    }
}

void LogsPage::onSearchChanged(const QString& text)
{
    // Simple search implementation - highlight matching text
    // TODO: Implement more sophisticated search/filtering
    Q_UNUSED(text);
}
