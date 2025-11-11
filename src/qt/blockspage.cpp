#include "blockspage.h"
#include <QClipboard>
#include <QApplication>
#include <QHeaderView>

BlocksPage::BlocksPage(ArchivasRpcClient* rpcClient, QWidget *parent)
    : QWidget(parent)
    , m_rpcClient(rpcClient)
    , m_tableWidget(nullptr)
    , m_refreshTimer(nullptr)
{
    setupUi();

    connect(m_rpcClient, &ArchivasRpcClient::blocksUpdated, this, &BlocksPage::onBlocksUpdated);

    // Auto-refresh every 10 seconds
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, [this]() {
        m_rpcClient->getRecentBlocks(20);
    });
    m_refreshTimer->start(10000);

    // Initial load
    m_rpcClient->getRecentBlocks(20);
}

BlocksPage::~BlocksPage()
{
}

void BlocksPage::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(6);
    m_tableWidget->setHorizontalHeaderLabels({"Height", "Hash", "Farmer", "Tx Count", "Timestamp", "Difficulty"});
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    m_tableWidget->setAlternatingRowColors(true);

    connect(m_tableWidget, &QTableWidget::cellDoubleClicked, this, &BlocksPage::onTableDoubleClicked);

    mainLayout->addWidget(m_tableWidget);
}

void BlocksPage::onBlocksUpdated(const QList<BlockInfo>& blocks)
{
    updateTable(blocks);
}

void BlocksPage::updateTable(const QList<BlockInfo>& blocks)
{
    m_tableWidget->setRowCount(blocks.size());

    for (int i = 0; i < blocks.size(); ++i) {
        const BlockInfo& block = blocks[i];

        // Height
        QTableWidgetItem* heightItem = new QTableWidgetItem(block.height);
        m_tableWidget->setItem(i, 0, heightItem);

        // Hash (truncated)
        QString shortHash = block.hash;
        if (shortHash.length() > 16) {
            shortHash = shortHash.left(8) + "..." + shortHash.right(8);
        }
        QTableWidgetItem* hashItem = new QTableWidgetItem(shortHash);
        hashItem->setData(Qt::UserRole, block.hash); // Store full hash
        hashItem->setToolTip(block.hash);
        m_tableWidget->setItem(i, 1, hashItem);

        // Farmer
        QString shortFarmer = block.farmer;
        if (shortFarmer.length() > 16) {
            shortFarmer = shortFarmer.left(8) + "..." + shortFarmer.right(8);
        }
        QTableWidgetItem* farmerItem = new QTableWidgetItem(shortFarmer);
        farmerItem->setToolTip(block.farmer);
        m_tableWidget->setItem(i, 2, farmerItem);

        // Tx Count
        QTableWidgetItem* txCountItem = new QTableWidgetItem(QString::number(block.txCount));
        m_tableWidget->setItem(i, 3, txCountItem);

        // Timestamp
        QTableWidgetItem* timestampItem = new QTableWidgetItem(block.timestamp);
        m_tableWidget->setItem(i, 4, timestampItem);

        // Difficulty
        QTableWidgetItem* difficultyItem = new QTableWidgetItem(block.difficulty);
        m_tableWidget->setItem(i, 5, difficultyItem);
    }

    m_tableWidget->resizeColumnsToContents();
}

void BlocksPage::onTableDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    if (row < 0 || row >= m_tableWidget->rowCount()) {
        return;
    }

    // Copy hash to clipboard
    QTableWidgetItem* hashItem = m_tableWidget->item(row, 1);
    if (hashItem) {
        QString fullHash = hashItem->data(Qt::UserRole).toString();
        if (fullHash.isEmpty()) {
            fullHash = hashItem->text();
        }
        QClipboard* clipboard = QApplication::clipboard();
        clipboard->setText(fullHash);
    }
}

